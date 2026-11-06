/*
 * Abc80WebServer.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Config.hxx>
#include <abc80/Abc80WebServer.hxx>
#include <abc80/AimonGatewayClient.hxx>
#include <abc80/FrameOutbox.hxx>
#include <abc80/KeyQueue.hxx>
#include <abc80/WebProtocol.hxx>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>

namespace abc80 {

using json = nlohmann::json;

namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kFramePeriod = std::chrono::nanoseconds(16666667);  // 60 Hz of wall time
constexpr uint64_t kBootMaxTStates = 400000000ULL;                 // cold monitor boot is 178M T
constexpr int kMaxFramesBehind = 5;

// True when no Origin header is present (non-browser client) or it names this server's own host.
bool originAllowed(const httplib::Request &req)
{
    if (!req.has_header("Origin")) {
        return true;
    }
    const std::string origin = req.get_header_value("Origin");
    const std::string host = req.get_header_value("Host");
    const std::string::size_type scheme = origin.find("://");
    if (scheme == std::string::npos || host.empty()) {
        return false;
    }
    return origin.substr(scheme + 3) == host;
}

std::string generateMcpSessionId()
{
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t p1 = dis(gen);
    uint64_t p2 = dis(gen);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << p1 << std::setw(16) << p2;
    return oss.str();
}

} // namespace

struct Abc80WebServer::Impl {
    // One emulated board and everything that belongs to it. Guarded by mtx.
    struct Session {
        std::string token;
        bool isResident{false};
        RomId rom{RomId::Monitor};
        bool ready{false};                     // booted; set under mtx
        bool paused{false};                    // execution paused; set under mtx
        bool autoPaused{false};                // paused automatically due to 0 websocket connections
        bool aiPunching{false};                // AI punch mode active (locks out human input)
        bool resumedForAiPunch{false};         // temporarily woken up to process AI punches
        bool wasManuallyPausedBeforeAi{false}; // true if session was manually paused before AI wake
        Abc80Board board;
        std::unique_ptr<KeyQueue> keys;
        std::unique_ptr<Abc80QbHost> qbHost;
        uint64_t frames{0};
        uint64_t cycles{0};
        std::vector<std::shared_ptr<FrameOutbox>> outboxes;  // multi-subscriber for resident
        Clock::time_point detachedAt{Clock::now()};
        std::mutex mtx;
    };

    explicit Impl(const ServerConfig &c) : config(c) {}

    ServerConfig config;
    std::unique_ptr<httplib::Server> svr;
    std::thread serverThread;
    std::thread workerThread;
    std::atomic<bool> running{false};
    std::atomic<int> boundPort{0};
    std::atomic<size_t> attached{0};
    std::string error;
    mutable std::mutex errorMtx;

    // Lock order: registryMtx, then a session's mtx. Never the reverse.
    mutable std::mutex registryMtx;
    std::map<std::string, std::shared_ptr<Session>> sessions;
    std::shared_ptr<Session> residentSession;

    std::mutex wakeMtx;
    std::condition_variable wake;

    AiCancelCallback onAiCancel;
    mutable std::mutex cancelCbMtx;

    struct McpSessionState {
        std::string sessionId;
        time_t lastActivity{0};
        bool closed{false};
        std::vector<std::string> pendingMessages;
    };
    std::mutex mcpMutex;
    std::condition_variable mcpCv;
    std::map<std::string, McpSessionState> mcpSessions;

    McpToolHandler mcpToolHandler;
    std::mutex mcpHandlerMtx;

    std::string executeResidentMcpTool(const std::string &name, const std::string &argumentsJson)
    {
        nlohmann::json args = nlohmann::json::object();
        try {
            if (!argumentsJson.empty()) {
                args = nlohmann::json::parse(argumentsJson);
            }
        } catch (...) {}

        if (name == "abc80_read_cpu") {
            if (!residentSession) {
                return "{\"error\": \"Resident emulator not initialized\"}";
            }
            std::lock_guard<std::mutex> lock(residentSession->mtx);
            const Abc80Cpu &cpu = residentSession->board.getCpu();
            const uint16_t af = cpu.getAF();
            const uint8_t f = static_cast<uint8_t>(af & 0xFF);
            nlohmann::json res = {
                {"rom", romProfile(residentSession->rom).name},
                {"cycles", residentSession->cycles},
                {"paused", residentSession->paused},
                {"autoPaused", residentSession->autoPaused},
                {"registers", {
                    {"pc", residentSession->board.lastFetchAddress()},
                    {"sp", cpu.getSP()},
                    {"af", af},
                    {"bc", cpu.getBC()},
                    {"de", cpu.getDE()},
                    {"hl", cpu.getHL()},
                    {"ix", cpu.getIX()},
                    {"iy", cpu.getIY()}
                }},
                {"flags", {
                    {"s", (f & 0x80) != 0},
                    {"z", (f & 0x40) != 0},
                    {"yf", (f & 0x20) != 0},
                    {"h", (f & 0x10) != 0},
                    {"xf", (f & 0x08) != 0},
                    {"pv", (f & 0x04) != 0},
                    {"n", (f & 0x02) != 0},
                    {"c", (f & 0x01) != 0}
                }}
            };
            return res.dump(2);
        }

        if (name == "abc80_read_memory") {
            uint16_t addr = static_cast<uint16_t>(args.value("addr", 0));
            uint16_t count = static_cast<uint16_t>(args.value("count", 16));
            if (count > 256) count = 256;
            std::vector<uint8_t> data;
            if (residentSession) {
                std::lock_guard<std::mutex> lock(residentSession->mtx);
                data = residentSession->board.readMemoryBlock(addr, count);
            }
            std::string hexDump;
            char buf[16];
            for (size_t i = 0; i < data.size(); ++i) {
                if (i % 16 == 0) {
                    if (i > 0) {
                        hexDump.push_back('\n');
                    }
                    std::snprintf(buf, sizeof(buf), "%04X:", static_cast<unsigned>((addr + i) & 0xFFFF));
                    hexDump.append(buf);
                }
                std::snprintf(buf, sizeof(buf), " %02X", static_cast<unsigned>(data[i]));
                hexDump.append(buf);
            }
            nlohmann::json res = {
                {"addr", addr},
                {"count", data.size()},
                {"bytes", data},
                {"hex_dump", hexDump}
            };
            return res.dump(2);
        }

        if (name == "abc80_read_display") {
            if (!residentSession) {
                return "{\"error\": \"Resident emulator not initialized\"}";
            }
            std::lock_guard<std::mutex> lock(residentSession->mtx);
            const auto d = residentSession->board.displayDigits(config.litDutyMin);
            const std::string digits(d.begin(), d.end());
            const auto masksArr = residentSession->board.displayMasks(config.litDutyMin);
            nlohmann::json masks = nlohmann::json::array();
            for (uint8_t m : masksArr) {
                masks.push_back(m);
            }
            nlohmann::json res = {
                {"digits", digits},
                {"display_masks", masks},
                {"leds", {
                    {"ep", residentSession->board.epLed()},
                    {"halt", residentSession->board.haltLed()},
                    {"speaker", residentSession->board.speakerLedFraction()}
                }}
            };
            return res.dump(2);
        }

        if (name == "abc80_disassemble") {
            uint16_t addr = static_cast<uint16_t>(args.value("addr", 0));
            uint16_t count = static_cast<uint16_t>(args.value("count", 10));
            if (count > 64) count = 64;
            std::vector<DisasmLine> lines;
            if (residentSession) {
                std::lock_guard<std::mutex> lock(residentSession->mtx);
                lines = residentSession->board.disassemble(addr, count);
            }
            nlohmann::json linesArr = nlohmann::json::array();
            for (const auto &l : lines) {
                linesArr.push_back({
                    {"addr", l.addr},
                    {"bytes", l.bytes},
                    {"mnemonic", l.mnemonic}
                });
            }
            nlohmann::json res = {
                {"addr", addr},
                {"count", lines.size()},
                {"instructions", linesArr}
            };
            return res.dump(2);
        }

        if (name == "abc80_cancel_punching") {
            AiCancelCallback cb;
            {
                std::lock_guard<std::mutex> lock(cancelCbMtx);
                cb = onAiCancel;
            }
            if (cb) {
                cb();
            }
            nlohmann::json res = {
                {"status", "cancelled"},
                {"discarded_keys", 0}
            };
            return res.dump(2);
        }

        if (name == "abc80_get_punch_status") {
            nlohmann::json res = {
                {"is_punching", hasActivePunches()},
                {"queue_depth", 0},
                {"remaining", 0},
                {"current_key", ""}
            };
            return res.dump(2);
        }

        if (name == "abc80_punch_keys") {
            return "{\"error\": \"No key sequencer attached\"}";
        }

        return "{\"error\": \"unknown tool: " + name + "\"}";
    }

    std::string executeMcpTool(const std::string &name, const std::string &argumentsJson)
    {
        McpToolHandler customHandler;
        {
            std::lock_guard<std::mutex> lock(mcpHandlerMtx);
            customHandler = mcpToolHandler;
        }
        if (customHandler) {
            return customHandler(name, argumentsJson);
        }
        return executeResidentMcpTool(name, argumentsJson);
    }

    void setError(const std::string &e)
    {
        std::lock_guard<std::mutex> lock(errorMtx);
        error = e;
    }

    bool hasActivePunches() const
    {
        if (!residentSession) return false;
        std::lock_guard<std::mutex> lock(residentSession->mtx);
        return residentSession->aiPunching || residentSession->resumedForAiPunch;
    }

    std::string resolvedTapesDir() const
    {
        if (!config.tapesDir.empty()) {
            return config.tapesDir;
        }
        return Abc80Config::resolveResourcePath("tapes").string();
    }

    std::string resolvedProgramsDir() const
    {
        if (!config.programsDir.empty()) {
            return config.programsDir;
        }
        return (Abc80Config::defaultDir() / "programs").string();
    }

    std::string resolvedWebDir() const
    {
        std::error_code ec;
        if (!config.root.empty() && config.root != ".") {
            const std::filesystem::path cand = std::filesystem::path(config.root) / config.webRoot;
            if (std::filesystem::exists(cand / "index.html", ec)) {
                return cand.string();
            }
        }
        const std::string rel = config.webRoot.empty() ? "web" : config.webRoot;
        const std::filesystem::path resolvedWeb = Abc80Config::resolveResourcePath(rel);
        if (std::filesystem::exists(resolvedWeb / "index.html", ec)) {
            return resolvedWeb.string();
        }
        if (config.webRoot.empty() || config.webRoot == "web") {
            const std::filesystem::path sysWeb = Abc80Config::systemDir() / "web";
            if (std::filesystem::exists(sysWeb / "index.html", ec)) {
                return sysWeb.string();
            }
        }
        return config.root + "/" + rel;
    }

    // ---- session lifecycle ----

    // Power-cycles the board with the given ROM. Caller holds s.mtx.
    bool boot(Session &s, RomId rom)
    {
        s.rom = rom;
        const RomProfile &profile = romProfile(rom);
        const std::string romPath = (config.root.empty() || config.root == ".")
            ? std::string(profile.relPath)
            : (config.root + "/" + profile.relPath);
        Abc80Status st;
        if (config.coldBoot) {
            st = s.board.powerOn(romPath);
        } else {
            st = s.board.powerOnRom(rom, config.root);
        }
        if (st != Abc80Status::OK) {
            return false;
        }
        s.keys = std::make_unique<KeyQueue>(profile.keys);
        if (!s.qbHost) {
            s.qbHost = std::make_unique<Abc80QbHost>(s.board);
        }
        s.qbHost->setProgramsDir(resolvedProgramsDir());
        s.qbHost->disconnect();
        s.ready = true;
        s.paused = false;
        s.autoPaused = false;
        s.aiPunching = false;
        s.resumedForAiPunch = false;
        s.wasManuallyPausedBeforeAi = false;
        s.frames = 0;
        s.cycles = 0;
        if (config.bootTurbo) {
            s.board.bootTurbo(rom, kBootMaxTStates);
        }
        CassetteDeck &deck = s.board.getCassetteDeck();
        deck.setTapesDir(resolvedTapesDir());
        deck.seedDefaultTapesIfEmpty();
        deck.selectTape(0x0001);
        return true;
    }

    void sendError(httplib::ws::WebSocket &ws, const std::string &reason)
    {
        ws.send(buildErrorJson(reason));
    }

    // ---- the WebSocket handler: one call per connection, on its own thread ----

    void handleConnection(const httplib::Request &req, httplib::ws::WebSocket &ws)
    {
        if (!originAllowed(req)) {
            ws.close(httplib::ws::CloseStatus::PolicyViolation, "cross-origin page refused");
            return;
        }

        ws.set_read_timeout(1, 0);  // poll once a second so shutdown is never delayed
        std::string text;
        httplib::ws::ReadResult first = httplib::ws::Timeout;
        for (int waited = 0; waited < 10 && running.load() && first == httplib::ws::Timeout; ++waited) {
            first = ws.read(text);  // the hello must arrive within 10 seconds
        }
        if (first != httplib::ws::Text) {
            return;
        }
        const ClientMessage hello = parseClientMessage(text);
        if (hello.type != ClientMessage::Type::Hello) {
            sendError(ws, hello.type == ClientMessage::Type::Invalid ? hello.error : "send 'hello' first");
            ws.close(httplib::ws::CloseStatus::PolicyViolation, "hello required");
            return;
        }

        std::shared_ptr<Session> session;
        bool created = false;
        {
            std::lock_guard<std::mutex> lock(registryMtx);
            auto it = sessions.find(hello.token);
            if (it != sessions.end()) {
                session = it->second;
            } else if (sessions.size() >= config.maxSessions) {
                // fall through with session == nullptr
            } else {
                session = std::make_shared<Session>();
                session->token = hello.token;
                sessions.emplace(hello.token, session);
                created = true;
            }
        }
        if (!session) {
            sendError(ws, "too many sessions");
            ws.close(httplib::ws::CloseStatus::PolicyViolation, "too many sessions");
            return;
        }

        auto outbox = std::make_shared<FrameOutbox>();
        enum class Attach { Ok, BootFailed, NotReady } result = Attach::Ok;
        {
            std::lock_guard<std::mutex> lock(session->mtx);  // a new session is booted here, before it can be stepped
            if (created && !boot(*session, hello.hasRom ? hello.rom : config.defaultRom)) {
                result = Attach::BootFailed;
            } else if (!session->ready) {
                result = Attach::NotReady;  // an earlier attempt to create it failed
            } else {
                if (session->isResident) {
                    session->outboxes.push_back(outbox);
                    // Attachment: auto-resume if auto-paused; stay paused if manually paused
                    if (session->autoPaused) {
                        session->paused = false;
                        session->autoPaused = false;
                    }
                } else {
                    for (auto &ob : session->outboxes) {
                        ob->close();  // another tab with this token takes over
                    }
                    session->outboxes.clear();
                    session->outboxes.push_back(outbox);
                    if (session->autoPaused) {
                        session->paused = false;
                        session->autoPaused = false;
                    }
                }
            }
        }
        if (result != Attach::Ok) {
            if (result == Attach::BootFailed) {
                std::lock_guard<std::mutex> reg(registryMtx);  // session lock already released: order is respected
                sessions.erase(hello.token);
            }
            sendError(ws, result == Attach::BootFailed ? "cannot load the ROM" : "session not ready, retry");
            ws.close(httplib::ws::CloseStatus::InternalError, "attach failed");
            return;
        }
        attached.fetch_add(1);
        wake.notify_all();

        // Send initial state if the session is paused so client gets telemetry immediately
        {
            std::lock_guard<std::mutex> lock(session->mtx);
            if (session->paused) {
                outbox->post(telemetryFor(*session, false));
            }
        }

        std::thread sender([&ws, outbox] {
            std::string frame;
            while (!outbox->closed()) {
                if (!outbox->wait(frame, 100)) {
                    continue;
                }
                if (!ws.send(frame)) {
                    outbox->close();
                }
            }
        });

        ws.set_read_timeout(1, 0);  // wake once a second to notice a takeover or shutdown
        while (!outbox->closed()) {
            const httplib::ws::ReadResult r = ws.read(text);
            if (r == httplib::ws::Timeout) {
                continue;
            }
            if (r != httplib::ws::Text) {
                break;
            }
            onMessage(*session, *outbox, text);
        }

        outbox->close();
        sender.join();
        {
            std::lock_guard<std::mutex> lock(session->mtx);
            auto it = std::find(session->outboxes.begin(), session->outboxes.end(), outbox);
            if (it != session->outboxes.end()) {
                session->outboxes.erase(it);
            }
            if (session->outboxes.empty()) {
                session->detachedAt = Clock::now();
                // Detachment: auto-pause if running and not active in AI punch
                if (!session->paused && !session->aiPunching && !session->resumedForAiPunch) {
                    const uint32_t remaining = session->board.finishInstruction();
                    session->cycles += remaining;
                    session->paused = true;
                    session->autoPaused = true;
                }
            }
        }
        attached.fetch_sub(1);
        wake.notify_all();
        ws.close();
    }

    void onMessage(Session &s, FrameOutbox &outbox, const std::string &text)
    {
        const ClientMessage m = parseClientMessage(text);
        switch (m.type) {
        case ClientMessage::Type::Invalid:
            outbox.postEvent(buildErrorJson(m.error));
            break;
        case ClientMessage::Type::Hello:
            outbox.postEvent(buildErrorJson("already said hello"));
            break;
        case ClientMessage::Type::Key: {
            std::lock_guard<std::mutex> lock(s.mtx);
            if (s.isResident && s.aiPunching) {
                break;
            }
            if (m.down) {
                if (!s.keys->press(m.keyCode)) {
                    outbox.postEvent(buildErrorJson("key queue full"));
                }
            } else {
                s.keys->release(m.keyCode);
            }
            break;
        }
        case ClientMessage::Type::Reset: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string frame;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (s.isResident && s.aiPunching) {
                    break;
                }
                s.board.reset();
                s.keys->clear();
                if (s.qbHost) {
                    s.qbHost->disconnect();
                }
                s.paused = false;
                s.autoPaused = false;
                s.resumedForAiPunch = false;
                s.wasManuallyPausedBeforeAi = false;
                frame = telemetryFor(s, false);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(frame);
            }
            break;
        }
        case ClientMessage::Type::SetRom: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string frame;
            bool ok = false;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (s.isResident && s.aiPunching) {
                    break;
                }
                ok = boot(s, m.rom);
                if (ok) {
                    frame = telemetryFor(s, false);
                    targets = s.outboxes;
                }
            }
            if (!ok) {
                outbox.postEvent(buildErrorJson("cannot load the ROM"));
            } else {
                for (auto &ob : targets) {
                    ob->post(frame);
                }
            }
            break;
        }
        case ClientMessage::Type::Run: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string frame;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (s.isResident && s.aiPunching) {
                    break;
                }
                if (s.paused) {
                    s.paused = false;
                    s.autoPaused = false;
                    s.resumedForAiPunch = false;
                    s.wasManuallyPausedBeforeAi = false;
                    frame = telemetryFor(s, false);
                    targets = s.outboxes;
                }
            }
            for (auto &ob : targets) {
                ob->post(frame);
            }
            break;
        }
        case ClientMessage::Type::Pause: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string frame;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (s.isResident && s.aiPunching) {
                    break;
                }
                if (!s.paused) {
                    const uint32_t remaining = s.board.finishInstruction();
                    s.cycles += remaining;
                    s.paused = true;
                }
                s.autoPaused = false;
                s.resumedForAiPunch = false;
                s.wasManuallyPausedBeforeAi = false;
                frame = telemetryFor(s, false);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(frame);
            }
            break;
        }
        case ClientMessage::Type::Step: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string frame;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (s.isResident && s.aiPunching) {
                    break;
                }
                if (s.paused) {
                    const uint32_t stepT = s.board.stepInstruction();
                    s.cycles += stepT;
                    s.autoPaused = false;
                    s.resumedForAiPunch = false;
                    s.wasManuallyPausedBeforeAi = false;
                    frame = telemetryFor(s, false);
                    targets = s.outboxes;
                }
            }
            for (auto &ob : targets) {
                ob->post(frame);
            }
            break;
        }
        case ClientMessage::Type::ReadMem: {
            std::string reply;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                std::vector<uint8_t> data = s.board.readMemoryBlock(m.memAddr, m.memCount);
                reply = buildMemDataJson(m.memAddr, data);
                outbox.postEvent(reply);
            }
            break;
        }
        case ClientMessage::Type::ReadDisasm: {
            std::string reply;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                std::vector<DisasmLine> lines = s.board.disassemble(m.memAddr, m.memCount);
                reply = buildDisasmJson(m.memAddr, lines);
                outbox.postEvent(reply);
            }
            break;
        }
        case ClientMessage::Type::CancelAiPunch: {
            AiCancelCallback cb;
            {
                std::lock_guard<std::mutex> lock(cancelCbMtx);
                cb = onAiCancel;
            }
            if (cb) {
                cb();
            }
            break;
        }
        case ClientMessage::Type::TapeList: {
            std::lock_guard<std::mutex> lock(s.mtx);
            const std::string reply = buildTapeListJson(s.board.getCassetteDeck().listTapes());
            outbox.postEvent(reply);
            break;
        }
        case ClientMessage::Type::TapeSelect: {
            std::lock_guard<std::mutex> lock(s.mtx);
            s.board.getCassetteDeck().selectTape(m.tapeFileId);
            if (s.paused) {
                const std::string frame = telemetryFor(s, false);
                for (auto &ob : s.outboxes) {
                    ob->post(frame);
                }
            }
            break;
        }
        case ClientMessage::Type::TapeAnnotate: {
            std::lock_guard<std::mutex> lock(s.mtx);
            s.board.getCassetteDeck().setAnnotation(m.tapeFileId, m.tapeAnnotation);
            const std::string reply = buildTapeListJson(s.board.getCassetteDeck().listTapes());
            for (auto &ob : s.outboxes) {
                ob->postEvent(reply);
            }
            break;
        }
        case ClientMessage::Type::TapeTransport: {
            std::lock_guard<std::mutex> lock(s.mtx);
            CassetteDeck &deck = s.board.getCassetteDeck();
            if (m.hasTapeFileId) {
                deck.selectTape(m.tapeFileId);
            }
            if (m.tapeAction == "play") {
                if (deck.play() != Abc80Status::OK) {
                    outbox.postEvent(buildErrorJson("selected tape has no recorded audio"));
                    break;
                }
            } else if (m.tapeAction == "record") {
                deck.record(m.hasTapeAnnotation ? m.tapeAnnotation : "");
            } else if (m.tapeAction == "rewind") {
                deck.rewind();
            } else if (m.tapeAction == "stop") {
                deck.stop();
            }
            if (s.paused) {
                const std::string frame = telemetryFor(s, false);
                for (auto &ob : s.outboxes) {
                    ob->post(frame);
                }
            }
            break;
        }
        case ClientMessage::Type::QbConnect: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string qbReply;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (!s.qbHost) {
                    s.qbHost = std::make_unique<Abc80QbHost>(s.board);
                    s.qbHost->setProgramsDir(resolvedProgramsDir());
                }
                (void)s.qbHost->initialAbc();
                qbReply = buildQbStateJson(*s.qbHost);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(qbReply);
            }
            break;
        }
        case ClientMessage::Type::QbStatus: {
            std::string qbReply;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (!s.qbHost) {
                    s.qbHost = std::make_unique<Abc80QbHost>(s.board);
                    s.qbHost->setProgramsDir(resolvedProgramsDir());
                }
                if (s.qbHost->isConnected()) {
                    s.qbHost->updateMemoryTable(s.qbHost->getCurrentAddress());
                }
                qbReply = buildQbStateJson(*s.qbHost);
            }
            outbox.post(qbReply);
            break;
        }
        case ClientMessage::Type::QbKey: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string qbReply;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (!s.qbHost) {
                    s.qbHost = std::make_unique<Abc80QbHost>(s.board);
                    s.qbHost->setProgramsDir(resolvedProgramsDir());
                }
                (void)s.qbHost->pressKey(m.qbKey);
                qbReply = buildQbStateJson(*s.qbHost);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(qbReply);
            }
            break;
        }
        case ClientMessage::Type::QbSave: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string qbReply;
            std::string err;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (!s.qbHost) {
                    s.qbHost = std::make_unique<Abc80QbHost>(s.board);
                    s.qbHost->setProgramsDir(resolvedProgramsDir());
                }
                (void)s.qbHost->saveProgramAbc(m.qbFilename, m.qbStartAddr, m.qbEndAddr, err);
                qbReply = buildQbStateJson(*s.qbHost);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(qbReply);
            }
            break;
        }
        case ClientMessage::Type::QbLoad: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string qbReply;
            std::string err;
            uint16_t loadedStart = 0;
            size_t loadedBytes = 0;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (!s.qbHost) {
                    s.qbHost = std::make_unique<Abc80QbHost>(s.board);
                    s.qbHost->setProgramsDir(resolvedProgramsDir());
                }
                (void)s.qbHost->loadProgramAbc(m.qbFilename, loadedStart, loadedBytes, err);
                qbReply = buildQbStateJson(*s.qbHost);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(qbReply);
            }
            break;
        }
        case ClientMessage::Type::ExtCircuit: {
            std::vector<std::shared_ptr<FrameOutbox>> targets;
            std::string frame;
            {
                std::lock_guard<std::mutex> lock(s.mtx);
                if (s.isResident && s.aiPunching) {
                    break;
                }
                if (m.extCircuitAction == "attach") {
                    s.board.attachCircuit(m.extCircuitId);
                } else if (m.extCircuitAction == "detach") {
                    s.board.detachCircuit();
                } else if (m.extCircuitAction == "reset") {
                    if (s.board.attachedCircuit() != nullptr) {
                        s.board.attachedCircuit()->reset();
                    }
                } else if (m.extCircuitAction == "set_clock") {
                    s.board.memory().writeByte(0x1200, m.clockSs);
                    s.board.memory().writeByte(0x1201, m.clockMm);
                    s.board.memory().writeByte(0x1202, m.clockHh);
                }
                frame = telemetryFor(s, false);
                targets = s.outboxes;
            }
            for (auto &ob : targets) {
                ob->post(frame);
            }
            break;
        }
        }
    }

    // ---- the 60 Hz worker ----

    std::string telemetryFor(Session &s, bool isFrame = true)
    {
        TelemetryFrame f;
        f.seq = ++s.frames;
        f.rom = romProfile(s.rom).name;
        if (isFrame) {
            s.cycles += ABC80_FRAME_TSTATES;
        }
        f.cycles = s.cycles;
        f.paused = s.paused;
        f.displayMasks = s.board.displayMasks(config.litDutyMin);
        f.displayDigits = s.board.displayDigits(config.litDutyMin);
        f.ep = s.board.epLed();
        f.halt = s.board.haltLed();
        f.speakerLed = s.board.speakerLedFraction();
        if (isFrame) {
            const Abc80Ppi::FrameAudio &audio = s.board.frameAudio();
            f.speakerLevel = (s.board.getPpi().getDigitStrobe() & 0x80) != 0;
            f.speakerEdges.assign(audio.offsets.begin(), audio.offsets.begin() + audio.edgeCount);
            f.speakerOverflow = audio.overflow;
        } else {
            f.speakerLevel = (s.board.getPpi().getDigitStrobe() & 0x80) != 0;
            f.speakerEdges.clear();
            f.speakerOverflow = false;
        }
        f.pc = s.board.lastFetchAddress();
        f.sp = s.board.getCpu().getSP();
        if (s.paused) {
            f.stackBaseAddr = f.sp;
            f.stackBytes = s.board.readMemoryBlock(f.sp, 32);
        } else {
            f.stackBaseAddr = 0;
            f.stackBytes.clear();
        }
        f.af = s.board.getCpu().getAF();
        f.bc = s.board.getCpu().getBC();
        f.de = s.board.getCpu().getDE();
        f.hl = s.board.getCpu().getHL();
        f.ix = s.board.getCpu().getIX();
        f.iy = s.board.getCpu().getIY();
        const CassetteDeck &deck = s.board.getCassetteDeck();
        switch (deck.state()) {
        case TapeDeckState::Stopped:
            f.tapeState = "stopped";
            break;
        case TapeDeckState::Playing:
            f.tapeState = "playing";
            break;
        case TapeDeckState::Recording:
            f.tapeState = "recording";
            break;
        }
        f.tapeFileId = deck.selectedFileIdHex();
        f.tapeAnnotation = deck.annotation();
        f.tapeCounter = deck.counter();
        f.tapeProgress = deck.progress();
        f.tapeToneHz = deck.currentToneHz();
        captureBoardExtTelemetry(s.board, f);
        return buildTelemetryJson(f);
    }

    void expireDetached()
    {
        const auto grace = std::chrono::seconds(config.sessionGraceSeconds);
        const auto now = Clock::now();
        std::lock_guard<std::mutex> reg(registryMtx);
        for (auto it = sessions.begin(); it != sessions.end();) {
            Session &s = *it->second;
            if (s.isResident) {
                ++it;  // Resident session is permanent in memory; never destroyed
                continue;
            }
            std::unique_lock<std::mutex> lock(s.mtx, std::try_to_lock);
            if (lock && s.outboxes.empty() && now - s.detachedAt > grace) {
                lock.unlock();
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool hasUniqueSessions() const
    {
        std::lock_guard<std::mutex> lock(registryMtx);
        for (const auto &kv : sessions) {
            if (!kv.second->isResident) {
                return true;
            }
        }
        return false;
    }

    void workerLoop()
    {
        auto next = Clock::now();
        auto lastExpiry = Clock::now();
        while (running.load()) {
            {
                std::unique_lock<std::mutex> lock(wakeMtx);
                if (attached.load() == 0 && !hasActivePunches() && !hasUniqueSessions()) {
                    wake.wait(lock, [this] {
                        return !running.load() || attached.load() > 0 || hasActivePunches();
                    });
                } else {
                    wake.wait_for(lock, std::chrono::seconds(1), [this] {
                        return !running.load() || attached.load() > 0 || hasActivePunches();
                    });
                }
            }
            if (!running.load()) {
                break;
            }
            if (Clock::now() - lastExpiry > std::chrono::seconds(1)) {
                expireDetached();
                lastExpiry = Clock::now();
            }
            if (attached.load() == 0 && !hasActivePunches()) {
                next = Clock::now();
                continue;
            }

            std::vector<std::shared_ptr<Session>> snapshot;
            {
                std::lock_guard<std::mutex> lock(registryMtx);
                snapshot.reserve(sessions.size());
                for (auto &kv : sessions) {
                    snapshot.push_back(kv.second);
                }
            }
            for (auto &sp : snapshot) {
                Session &s = *sp;
                std::unique_lock<std::mutex> lock(s.mtx, std::try_to_lock);
                if (!lock || (s.outboxes.empty() && !s.resumedForAiPunch)) {
                    continue;  // busy or detached without AI punch
                }
                if (s.paused) {
                    continue;
                }
                KeyQueue::Action actions[4];
                const size_t n = s.keys->tick(actions, 4);
                for (size_t i = 0; i < n; ++i) {
                    if (actions[i].down) {
                        s.board.pressKey(actions[i].keyCode);
                    } else {
                        s.board.releaseKey(actions[i].keyCode);
                    }
                }
                s.board.stepFrame();
                const std::string frame = telemetryFor(s);
                std::vector<std::shared_ptr<FrameOutbox>> outboxes = s.outboxes;
                lock.unlock();
                for (auto &ob : outboxes) {
                    ob->post(frame);
                }
            }

            next += kFramePeriod;
            const auto now = Clock::now();
            if (next < now - kMaxFramesBehind * kFramePeriod) {
                next = now;  // fell far behind (a slow machine): drop the backlog instead of racing to catch up
            }
            std::this_thread::sleep_until(next);
        }
    }

    // ---- start / stop ----

    bool start()
    {
        if (running.load()) {
            return true;
        }
        const std::string webDir = resolvedWebDir();

        svr = std::make_unique<httplib::Server>();
        const size_t poolSize = std::max<size_t>(32, config.maxSessions * 2 + 16);
        svr->new_task_queue = [poolSize] { return new httplib::ThreadPool(poolSize); };
        svr->set_write_timeout(static_cast<time_t>(config.sendTimeoutMs / 1000),
                               static_cast<time_t>((config.sendTimeoutMs % 1000) * 1000));
        svr->set_payload_max_length(kMaxClientMessageBytes);
        // SO_REUSEADDR only: the library default also sets SO_REUSEPORT, which would let a second server
        // silently share this port.
        svr->set_socket_options([](int sock) {  // socket_t is int on Linux
            const int yes = 1;
            ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));
        });
        if (!svr->set_mount_point("/", webDir)) {
            setError("web root not found: " + webDir);
            svr.reset();
            return false;
        }
        svr->Get("/api/status", [this](const httplib::Request &, httplib::Response &res) {
            size_t total;
            {
                std::lock_guard<std::mutex> lock(registryMtx);
                total = sessions.size();
            }
            res.set_content("{\"sessions\":" + std::to_string(total) + ",\"attached\":" +
                                std::to_string(attached.load()) + "}",
                            "application/json");
        });
        svr->WebSocket("/ws", [this](const httplib::Request &req, httplib::ws::WebSocket &ws) {
            handleConnection(req, ws);
        });

        auto setMcpCorsHeaders = [](httplib::Response &res) {
            res.headers.erase("Access-Control-Allow-Origin");
            res.headers.erase("Access-Control-Allow-Methods");
            res.headers.erase("Access-Control-Allow-Headers");
            res.headers.erase("Access-Control-Expose-Headers");
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers",
                           "Content-Type, Accept, Authorization, Mcp-Session-Id, MCP-Protocol-Version, Last-Event-ID");
            res.set_header("Access-Control-Expose-Headers", "Mcp-Session-Id");
        };

        auto handleMcpOptions = [setMcpCorsHeaders](const httplib::Request &, httplib::Response &res) {
            setMcpCorsHeaders(res);
            res.status = 204;
        };

        svr->Options("/mcp", handleMcpOptions);
        svr->Options("/sse", handleMcpOptions);
        svr->Options("/message", handleMcpOptions);

        auto handleStreamableHttpPost = [this, setMcpCorsHeaders](const httplib::Request &req, httplib::Response &res) {
            setMcpCorsHeaders(res);

            std::string sessionId;
            if (req.has_header("Mcp-Session-Id")) {
                sessionId = req.get_header_value("Mcp-Session-Id");
            }

            bool isInit = false;
            try {
                auto j = nlohmann::json::parse(req.body);
                if (j.is_object() && j.value("method", "") == "initialize") {
                    isInit = true;
                } else if (j.is_array()) {
                    for (const auto &item : j) {
                        if (item.is_object() && item.value("method", "") == "initialize") {
                            isInit = true;
                            break;
                        }
                    }
                }
            } catch (...) {}

            time_t now = std::time(nullptr);
            if (sessionId.empty() && isInit) {
                sessionId = generateMcpSessionId();
            }

            if (!sessionId.empty()) {
                std::lock_guard<std::mutex> lock(mcpMutex);
                auto it = mcpSessions.find(sessionId);
                if (it != mcpSessions.end()) {
                    it->second.lastActivity = now;
                } else {
                    McpSessionState state;
                    state.sessionId = sessionId;
                    state.lastActivity = now;
                    state.closed = false;
                    mcpSessions[sessionId] = std::move(state);
                }
                res.set_header("Mcp-Session-Id", sessionId);
            }

            std::string resp = AimonGatewayClient::dispatchJsonRpcWithCallback(
                req.body, "2025-03-26",
                [this](const std::string &name, const std::string &args) {
                    return executeMcpTool(name, args);
                });

            if (resp.find("\"code\":-32700") != std::string::npos || resp.find("\"code\": -32700") != std::string::npos) {
                res.status = 400;
                res.set_content(resp, "application/json");
                return;
            }

            if (resp.empty()) {
                res.status = 202;
                res.body.clear();
            } else {
                res.status = 200;
                res.set_content(resp, "application/json");
            }
        };

        svr->Post("/mcp", handleStreamableHttpPost);
        svr->Post("/sse", handleStreamableHttpPost);

        svr->Post("/message", [this, setMcpCorsHeaders](const httplib::Request &req, httplib::Response &res) {
            setMcpCorsHeaders(res);
            std::string resp = AimonGatewayClient::dispatchJsonRpcWithCallback(
                req.body, "2024-11-05",
                [this](const std::string &name, const std::string &args) {
                    return executeMcpTool(name, args);
                });
            if (resp.find("\"code\":-32700") != std::string::npos || resp.find("\"code\": -32700") != std::string::npos) {
                res.status = 400;
                res.set_content(resp, "application/json");
                return;
            }
            if (resp.empty()) {
                res.status = 202;
                res.body.clear();
            } else {
                res.status = 200;
                res.set_content(resp, "application/json");
            }
        });

        svr->Get("/mcp", [this, setMcpCorsHeaders](const httplib::Request &req, httplib::Response &res) {
            setMcpCorsHeaders(res);

            if (req.has_header("Accept")) {
                std::string accept = req.get_header_value("Accept");
                if (accept.find("text/event-stream") == std::string::npos &&
                    accept.find("*/*") == std::string::npos) {
                    res.status = 405;
                    return;
                }
            }

            std::string sessionId;
            if (req.has_header("Mcp-Session-Id")) {
                sessionId = req.get_header_value("Mcp-Session-Id");
            }

            time_t now = std::time(nullptr);
            if (sessionId.empty()) {
                sessionId = generateMcpSessionId();
            }

            {
                std::lock_guard<std::mutex> lock(mcpMutex);
                auto it = mcpSessions.find(sessionId);
                if (it != mcpSessions.end()) {
                    it->second.lastActivity = now;
                } else {
                    McpSessionState state;
                    state.sessionId = sessionId;
                    state.lastActivity = now;
                    state.closed = false;
                    mcpSessions[sessionId] = std::move(state);
                }
            }

            res.set_header("Mcp-Session-Id", sessionId);
            res.set_header("Cache-Control", "no-cache");
            res.set_header("X-Accel-Buffering", "no");

            auto lastKeepalive = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());
            auto initialSent = std::make_shared<bool>(false);

            auto provider = [this, sessionId, lastKeepalive, initialSent](size_t offset, httplib::DataSink &sink) -> bool {
                (void) offset;
                if (!sink.is_writable() || !running.load()) {
                    return false;
                }

                if (!*initialSent) {
                    *initialSent = true;
                    std::string connMsg = ": connected\n\n";
                    return sink.write(connMsg.data(), connMsg.size());
                }

                std::vector<std::string> toSend;
                {
                    std::unique_lock<std::mutex> lock(mcpMutex);
                    mcpCv.wait_for(lock, std::chrono::milliseconds(200), [this, &sessionId]() {
                        if (!running.load()) return true;
                        auto it = mcpSessions.find(sessionId);
                        if (it == mcpSessions.end() || it->second.closed) return true;
                        return !it->second.pendingMessages.empty();
                    });

                    if (!running.load()) {
                        return false;
                    }

                    auto it = mcpSessions.find(sessionId);
                    if (it == mcpSessions.end() || it->second.closed) {
                        return false;
                    }

                    it->second.lastActivity = std::time(nullptr);

                    if (!it->second.pendingMessages.empty()) {
                        toSend = std::move(it->second.pendingMessages);
                        it->second.pendingMessages.clear();
                    }
                }

                if (!toSend.empty()) {
                    std::string batch;
                    for (const auto &msg : toSend) {
                        batch += "event: message\ndata: " + msg + "\n\n";
                    }
                    *lastKeepalive = std::chrono::steady_clock::now();
                    return sink.write(batch.data(), batch.size());
                }

                auto nowTime = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(nowTime - *lastKeepalive).count() >= 15) {
                    *lastKeepalive = nowTime;
                    std::string ping = ": keepalive\n\n";
                    return sink.write(ping.data(), ping.size());
                }

                return true;
            };

            auto releaser = [this, sessionId](bool success) {
                (void) success;
                std::lock_guard<std::mutex> lock(mcpMutex);
                auto it = mcpSessions.find(sessionId);
                if (it != mcpSessions.end()) {
                    it->second.lastActivity = std::time(nullptr);
                }
            };

            res.set_chunked_content_provider("text/event-stream", provider, releaser);
        });

        svr->Get("/sse", [setMcpCorsHeaders](const httplib::Request &, httplib::Response &res) {
            setMcpCorsHeaders(res);
            res.set_header("Content-Type", "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_content("event: endpoint\ndata: /message\n\n", "text/event-stream");
        });

        svr->Delete("/mcp", [this, setMcpCorsHeaders](const httplib::Request &req, httplib::Response &res) {
            setMcpCorsHeaders(res);
            if (req.has_header("Mcp-Session-Id")) {
                std::string sessionId = req.get_header_value("Mcp-Session-Id");
                {
                    std::lock_guard<std::mutex> lock(mcpMutex);
                    auto it = mcpSessions.find(sessionId);
                    if (it != mcpSessions.end()) {
                        it->second.closed = true;
                        mcpSessions.erase(it);
                    }
                }
                mcpCv.notify_all();
            }
            res.status = 204;
        });

        int p;
        if (config.port == 0) {
            p = svr->bind_to_any_port(config.bind);
        } else {
            p = svr->bind_to_port(config.bind, config.port) ? config.port : -1;
        }
        if (p <= 0) {
            setError("cannot bind " + config.bind + ":" + std::to_string(config.port));
            svr.reset();
            return false;
        }
        boundPort.store(p);

        // Pre-create resident emulator session if configured
        if (config.createResident) {
            residentSession = std::make_shared<Session>();
            residentSession->token = "resident";
            residentSession->isResident = true;
            residentSession->rom = config.defaultRom;
            {
                std::lock_guard<std::mutex> lock(registryMtx);
                sessions.emplace("resident", residentSession);
            }
            {
                std::lock_guard<std::mutex> lock(residentSession->mtx);
                if (!boot(*residentSession, config.defaultRom)) {
                    setError("cannot load resident ROM");
                    svr.reset();
                    sessions.clear();
                    residentSession.reset();
                    return false;
                }
                // Resident emulator starts auto-paused until viewers or AI attach
                residentSession->paused = true;
                residentSession->autoPaused = true;
                residentSession->resumedForAiPunch = false;
            }
        }

        running.store(true);
        serverThread = std::thread([this] { svr->listen_after_bind(); });
        svr->wait_until_ready();
        workerThread = std::thread([this] { workerLoop(); });
        return true;
    }

    void stop()
    {
        if (!running.exchange(false)) {
            return;
        }
        wake.notify_all();
        {
            std::lock_guard<std::mutex> lock(mcpMutex);
            for (auto &kv : mcpSessions) {
                kv.second.closed = true;
            }
        }
        mcpCv.notify_all();
        {
            // Close every attached tab's outbox so its handler returns within a second.
            std::lock_guard<std::mutex> reg(registryMtx);
            for (auto &kv : sessions) {
                std::lock_guard<std::mutex> lock(kv.second->mtx);
                for (auto &ob : kv.second->outboxes) {
                    ob->close();
                }
            }
        }
        if (svr) {
            svr->stop();
        }
        if (workerThread.joinable()) {
            workerThread.join();
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
        svr.reset();
        std::lock_guard<std::mutex> reg(registryMtx);
        sessions.clear();
        residentSession.reset();
    }
};

Abc80WebServer::Abc80WebServer(const ServerConfig &config)
    : _impl(std::make_unique<Impl>(config))
{
}

Abc80WebServer::~Abc80WebServer()
{
    stop();
}

bool Abc80WebServer::start()
{
    return _impl->start();
}

void Abc80WebServer::stop()
{
    _impl->stop();
}

bool Abc80WebServer::isRunning() const noexcept
{
    return _impl->running.load();
}

int Abc80WebServer::port() const noexcept
{
    return _impl->boundPort.load();
}

size_t Abc80WebServer::sessionCount() const
{
    std::lock_guard<std::mutex> lock(_impl->registryMtx);
    return _impl->sessions.size();
}

size_t Abc80WebServer::attachedCount() const
{
    return _impl->attached.load();
}

std::string Abc80WebServer::lastError() const
{
    std::lock_guard<std::mutex> lock(_impl->errorMtx);
    return _impl->error;
}

bool Abc80WebServer::isResidentRunning() const
{
    if (!_impl->residentSession) return false;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    return !_impl->residentSession->paused;
}

bool Abc80WebServer::isResidentPaused() const
{
    if (!_impl->residentSession) return false;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    return _impl->residentSession->paused;
}

bool Abc80WebServer::isResidentAutoPaused() const
{
    if (!_impl->residentSession) return false;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    return _impl->residentSession->autoPaused;
}

size_t Abc80WebServer::residentSubscriberCount() const
{
    if (!_impl->residentSession) return 0;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    return _impl->residentSession->outboxes.size();
}

void Abc80WebServer::punchResidentKey(uint8_t keyCode, bool down)
{
    if (!_impl->residentSession) return;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    if (down) {
        _impl->residentSession->board.pressKey(keyCode);
    } else {
        _impl->residentSession->board.releaseKey(keyCode);
    }
}

void Abc80WebServer::resetResident()
{
    if (!_impl->residentSession) return;
    std::vector<std::shared_ptr<FrameOutbox>> targets;
    std::string frame;
    {
        Impl::Session &s = *_impl->residentSession;
        std::lock_guard<std::mutex> lock(s.mtx);
        s.board.reset();
        if (s.keys) {
            s.keys->clear();
        }
        s.paused = false;
        s.autoPaused = false;
        s.wasManuallyPausedBeforeAi = false;
        frame = _impl->telemetryFor(s, false);
        targets = s.outboxes;
    }
    for (auto &ob : targets) {
        ob->post(frame);
    }
    _impl->wake.notify_all();
}

void Abc80WebServer::broadcastAiPunch(const std::string &key, bool down, size_t remaining)
{
    if (!_impl->residentSession) return;
    const std::string frame = buildAiPunchJson(key, down, remaining);
    std::vector<std::shared_ptr<FrameOutbox>> targets;
    {
        std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
        targets = _impl->residentSession->outboxes;
    }
    for (auto &ob : targets) {
        ob->postEvent(frame);
    }
}

void Abc80WebServer::setResidentClockWakeForAi(bool wake)
{
    if (!_impl->residentSession) return;
    std::vector<std::shared_ptr<FrameOutbox>> targets;
    std::string frame;
    {
        Impl::Session &s = *_impl->residentSession;
        std::lock_guard<std::mutex> lock(s.mtx);
        if (wake) {
            if (!s.aiPunching) {
                s.aiPunching = true;
                s.board.clearKeys();
                if (s.keys) {
                    s.keys->clear();
                }
                if (s.paused) {
                    s.wasManuallyPausedBeforeAi = !s.autoPaused;
                    s.paused = false;
                    s.autoPaused = false;
                    s.resumedForAiPunch = true;
                } else {
                    s.wasManuallyPausedBeforeAi = false;
                    s.resumedForAiPunch = false;
                }
            }
        } else {
            if (s.aiPunching || s.resumedForAiPunch) {
                const bool restoreManualPause = s.wasManuallyPausedBeforeAi;
                s.aiPunching = false;
                s.resumedForAiPunch = false;
                s.wasManuallyPausedBeforeAi = false;
                if (restoreManualPause) {
                    if (!s.paused) {
                        const uint32_t remaining = s.board.finishInstruction();
                        s.cycles += remaining;
                    }
                    s.paused = true;
                    s.autoPaused = false;
                    frame = _impl->telemetryFor(s, false);
                    targets = s.outboxes;
                } else if (s.outboxes.empty()) {
                    if (!s.paused) {
                        const uint32_t remaining = s.board.finishInstruction();
                        s.cycles += remaining;
                    }
                    s.paused = true;
                    s.autoPaused = true;
                }
            }
        }
    }
    for (auto &ob : targets) {
        ob->post(frame);
    }
    _impl->wake.notify_all();
}

void Abc80WebServer::setOnAiCancel(AiCancelCallback cb)
{
    std::lock_guard<std::mutex> lock(_impl->cancelCbMtx);
    _impl->onAiCancel = std::move(cb);
}

void Abc80WebServer::cancelAiPunch()
{
    AiCancelCallback cb;
    {
        std::lock_guard<std::mutex> lock(_impl->cancelCbMtx);
        cb = _impl->onAiCancel;
    }
    if (cb) {
        cb();
    }
}

bool Abc80WebServer::readResidentCpu(TelemetryFrame &frame) const
{
    if (!_impl->residentSession) return false;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    const Abc80Cpu &cpu = _impl->residentSession->board.getCpu();
    frame.seq = _impl->residentSession->frames;
    frame.rom = romProfile(_impl->residentSession->rom).name;
    frame.cycles = _impl->residentSession->cycles;
    frame.paused = _impl->residentSession->paused;
    frame.displayMasks = _impl->residentSession->board.displayMasks(_impl->config.litDutyMin);
    frame.displayDigits = _impl->residentSession->board.displayDigits(_impl->config.litDutyMin);
    frame.ep = _impl->residentSession->board.epLed();
    frame.halt = _impl->residentSession->board.haltLed();
    frame.speakerLed = _impl->residentSession->board.speakerLedFraction();
    frame.speakerLevel = (_impl->residentSession->board.getPpi().getDigitStrobe() & 0x80) != 0;
    frame.pc = _impl->residentSession->board.lastFetchAddress();
    frame.sp = cpu.getSP();
    frame.af = cpu.getAF();
    frame.bc = cpu.getBC();
    frame.de = cpu.getDE();
    frame.hl = cpu.getHL();
    frame.ix = cpu.getIX();
    frame.iy = cpu.getIY();
    return true;
}

std::vector<uint8_t> Abc80WebServer::readResidentMemory(uint16_t addr, uint16_t count) const
{
    if (!_impl->residentSession) return {};
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    return _impl->residentSession->board.readMemoryBlock(addr, count);
}

std::vector<DisasmLine> Abc80WebServer::disassembleResident(uint16_t addr, uint16_t count) const
{
    if (!_impl->residentSession) return {};
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    return _impl->residentSession->board.disassemble(addr, count);
}

bool Abc80WebServer::readResidentDisplay(std::string &digits, uint8_t &epLed, uint8_t &haltLed) const
{
    if (!_impl->residentSession) return false;
    std::lock_guard<std::mutex> lock(_impl->residentSession->mtx);
    const auto d = _impl->residentSession->board.displayDigits(_impl->config.litDutyMin);
    digits.assign(d.begin(), d.end());
    epLed = _impl->residentSession->board.epLed() ? 1 : 0;
    haltLed = _impl->residentSession->board.haltLed() ? 1 : 0;
    return true;
}

void Abc80WebServer::setMcpToolHandler(McpToolHandler handler)
{
    std::lock_guard<std::mutex> lock(_impl->mcpHandlerMtx);
    _impl->mcpToolHandler = std::move(handler);
}

} // namespace abc80

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
