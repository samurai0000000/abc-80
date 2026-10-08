/*
 * Abc80WebServer.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80WebServer.hxx>
#include <abc80/FrameOutbox.hxx>
#include <abc80/KeyQueue.hxx>
#include <abc80/WebProtocol.hxx>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <httplib.h>
#include <sys/socket.h>

namespace abc80 {

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

} // namespace

struct Abc80WebServer::Impl {
    // One emulated board and everything that belongs to it. Guarded by mtx.
    struct Session {
        std::string token;
        RomId rom{RomId::Monitor};
        bool ready{false};                    // booted; set under mtx
        Abc80Board board;
        std::unique_ptr<KeyQueue> keys;
        uint64_t frames{0};
        uint64_t cycles{0};
        std::shared_ptr<FrameOutbox> outbox;  // null while detached
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

    std::mutex wakeMtx;
    std::condition_variable wake;

    void setError(const std::string &e)
    {
        std::lock_guard<std::mutex> lock(errorMtx);
        error = e;
    }

    // ---- session lifecycle ----

    // Power-cycles the board with the given ROM. Caller holds s.mtx.
    bool boot(Session &s, RomId rom)
    {
        s.rom = rom;
        const RomProfile &profile = romProfile(rom);
        Abc80Status st;
        if (config.coldBoot) {
            st = s.board.powerOn(config.root + "/" + profile.relPath);
        } else {
            st = s.board.powerOnRom(rom, config.root);
        }
        if (st != Abc80Status::OK) {
            return false;
        }
        s.keys = std::make_unique<KeyQueue>(profile.keys);
        s.ready = true;
        s.frames = 0;
        s.cycles = 0;
        if (config.bootTurbo) {
            s.board.bootTurbo(rom, kBootMaxTStates);
        }
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
                if (session->outbox) {
                    session->outbox->close();  // another tab with this token takes over
                }
                session->outbox = outbox;
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
            if (session->outbox == outbox) {
                session->outbox.reset();
                session->detachedAt = Clock::now();
            }
        }
        attached.fetch_sub(1);
        ws.close();
    }

    void onMessage(Session &s, FrameOutbox &outbox, const std::string &text)
    {
        const ClientMessage m = parseClientMessage(text);
        switch (m.type) {
        case ClientMessage::Type::Invalid:
            outbox.post(buildErrorJson(m.error));
            break;
        case ClientMessage::Type::Hello:
            outbox.post(buildErrorJson("already said hello"));
            break;
        case ClientMessage::Type::Key: {
            std::lock_guard<std::mutex> lock(s.mtx);
            if (m.down) {
                if (!s.keys->press(m.keyCode)) {
                    outbox.post(buildErrorJson("key queue full"));
                }
            } else {
                s.keys->release(m.keyCode);
            }
            break;
        }
        case ClientMessage::Type::Reset: {
            std::lock_guard<std::mutex> lock(s.mtx);
            s.board.reset();
            s.keys->clear();
            break;
        }
        case ClientMessage::Type::SetRom: {
            std::lock_guard<std::mutex> lock(s.mtx);
            if (!boot(s, m.rom)) {
                outbox.post(buildErrorJson("cannot load the ROM"));
            }
            break;
        }
        }
    }

    // ---- the 60 Hz worker ----

    std::string telemetryFor(Session &s)
    {
        TelemetryFrame f;
        f.seq = ++s.frames;
        f.rom = romProfile(s.rom).name;
        s.cycles += ABC80_FRAME_TSTATES;
        f.cycles = s.cycles;
        f.displayMasks = s.board.displayMasks(config.litDutyMin);
        f.displayDigits = s.board.displayDigits(config.litDutyMin);
        f.ep = s.board.epLed();
        f.halt = s.board.haltLed();
        f.speakerLed = s.board.speakerLedFraction();
        const Abc80Ppi::FrameAudio &audio = s.board.frameAudio();
        f.speakerLevel = (s.board.getPpi().getDigitStrobe() & 0x80) != 0;
        f.speakerEdges.assign(audio.offsets.begin(), audio.offsets.begin() + audio.edgeCount);
        f.speakerOverflow = audio.overflow;
        f.pc = s.board.lastFetchAddress();
        f.sp = s.board.getCpu().getSP();
        f.af = s.board.getCpu().getAF();
        f.bc = s.board.getCpu().getBC();
        f.de = s.board.getCpu().getDE();
        f.hl = s.board.getCpu().getHL();
        return buildTelemetryJson(f);
    }

    void expireDetached()
    {
        const auto grace = std::chrono::seconds(config.sessionGraceSeconds);
        const auto now = Clock::now();
        std::lock_guard<std::mutex> reg(registryMtx);
        for (auto it = sessions.begin(); it != sessions.end();) {
            Session &s = *it->second;
            std::unique_lock<std::mutex> lock(s.mtx, std::try_to_lock);
            if (lock && !s.outbox && now - s.detachedAt > grace) {
                lock.unlock();
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    void workerLoop()
    {
        auto next = Clock::now();
        auto lastExpiry = Clock::now();
        while (running.load()) {
            {
                std::unique_lock<std::mutex> lock(wakeMtx);
                wake.wait_for(lock, std::chrono::milliseconds(250), [this] { return !running.load() || attached.load() > 0; });
            }
            if (!running.load()) {
                break;
            }
            if (Clock::now() - lastExpiry > std::chrono::milliseconds(250)) {
                expireDetached();
                lastExpiry = Clock::now();
            }
            if (attached.load() == 0) {
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
                if (!lock || !s.outbox) {
                    continue;  // busy (booting or handling a message) or detached: it simply misses this frame
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
                std::shared_ptr<FrameOutbox> outbox = s.outbox;
                lock.unlock();
                outbox->post(frame);  // never blocks: the newest frame replaces an unsent one
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
        const std::string webDir = config.root + "/" + config.webRoot;

        svr = std::make_unique<httplib::Server>();
        svr->new_task_queue = [] { return new httplib::ThreadPool(32); };  // one thread per open tab plus requests
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
            // Close every attached tab's outbox so its handler returns within a second.
            std::lock_guard<std::mutex> reg(registryMtx);
            for (auto &kv : sessions) {
                std::lock_guard<std::mutex> lock(kv.second->mtx);
                if (kv.second->outbox) {
                    kv.second->outbox->close();
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
