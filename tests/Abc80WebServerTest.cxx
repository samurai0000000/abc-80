/*
 * Abc80WebServerTest.cxx
 *
 * End-to-end tests of the web server: real sockets on an ephemeral loopback port, real
 * browser-style WebSocket clients, real ROMs behind every session (plan Sections 2.4, 2.5).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80WebServer.hxx>
#include <abc80/AiKeySequencer.hxx>
#include <abc80/AimonGatewayClient.hxx>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <sys/resource.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using json = nlohmann::json;

namespace {

using Clock = std::chrono::steady_clock;

ServerConfig testConfig()
{
    ServerConfig c;
    c.port = 0;               // ephemeral
    c.bootTurbo = true;       // sessions start at the ROM's steady state
    c.sessionGraceSeconds = 300;
    c.createResident = false; // isolated unique sessions
    return c;
}

ServerConfig residentTestConfig()
{
    ServerConfig c;
    c.port = 0;               // ephemeral
    c.bootTurbo = true;       // sessions start at the ROM's steady state
    c.sessionGraceSeconds = 1;
    c.createResident = true;  // resident session enabled
    return c;
}

// One browser tab: a WebSocket to the server.
class Tab {
public:
    explicit Tab(int port)
        : _client("ws://127.0.0.1:" + std::to_string(port) + "/ws")
    {
        _client.set_read_timeout(0, 200000);  // reads give up after 200 ms so waits can poll
    }

    bool connect() { return static_cast<bool>(_client.connect()); }
    void send(const json &j) { _client.send(j.dump()); }
    bool isOpen() { return _client.is_open(); }

    // Reads until the server closes the connection (the client only notices a close by reading).
    bool waitClosed(int timeoutMs)
    {
        const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
        while (Clock::now() < end) {
            std::string msg;
            if (_client.read(msg) == httplib::ws::Fail) return true;
        }
        return false;
    }
    void close() { _client.close(); }

    void hello(const std::string &token, const char *rom = nullptr)
    {
        json j = { { "type", "hello" }, { "token", token } };
        if (rom != nullptr) j["rom"] = rom;
        send(j);
    }

    void key(const char *name, const char *action)
    {
        send({ { "type", "key" }, { "key", name }, { "action", action } });
    }

    // A tap: down then up immediately, as a fast click would send.
    void tap(const char *name)
    {
        key(name, "down");
        key(name, "up");
    }

    void run() { send({ { "type", "run" } }); }
    void pause() { send({ { "type", "pause" } }); }
    void step() { send({ { "type", "step" } }); }
    void reset() { send({ { "type", "reset" } }); }
    void readMem(uint16_t addr, uint16_t count) { send({ { "type", "readMem" }, { "addr", addr }, { "count", count } }); }
    void readDisasm(uint16_t addr, uint16_t count) { send({ { "type", "readDisasm" }, { "addr", addr }, { "count", count } }); }

    // Reads frames until `pred` accepts one (of type `type`) or the time runs out.
    bool waitFor(const char *type, const std::function<bool(const json &)> &pred, int timeoutMs, json *out = nullptr)
    {
        const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
        while (Clock::now() < end) {
            std::string msg;
            const httplib::ws::ReadResult r = _client.read(msg);
            if (r == httplib::ws::Fail) return false;
            if (r != httplib::ws::Text) continue;
            const json j = json::parse(msg, nullptr, false);
            if (j.is_discarded() || !j.is_object() || j.value("type", "") != type) continue;
            if (pred(j)) {
                if (out != nullptr) *out = j;
                return true;
            }
        }
        return false;
    }

    bool anyTelemetry(int timeoutMs, json *out = nullptr)
    {
        return waitFor("telemetry", [](const json &) { return true; }, timeoutMs, out);
    }

private:
    httplib::ws::WebSocketClient _client;
};

bool anyLit(const json &t)
{
    for (const auto &m : t.at("displayMasks")) {
        if (m.get<int>() != 0) return true;
    }
    return false;
}

std::string digits(const json &t)
{
    std::string s;
    for (const auto &d : t.at("displayDigits")) s += d.get<std::string>();
    return s;
}

double cpuSeconds()
{
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    return static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
           static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e6;
}

bool waitUntil(const std::function<bool()> &cond, int timeoutMs)
{
    const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < end) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return cond();
}

} // namespace

TEST_GROUP(Abc80WebServer)
{
};

TEST(Abc80WebServer, DefaultsAreLoopbackAndTheBookMonitor)
{
    const ServerConfig c;
    STRCMP_EQUAL("127.0.0.1", c.bind.c_str());
    CHECK(c.defaultRom == RomId::Monitor);
    LONGS_EQUAL(8, static_cast<long>(c.maxSessions));
    LONGS_EQUAL(300, static_cast<long>(c.sessionGraceSeconds));
}

TEST(Abc80WebServer, StartsOnAnEphemeralPortServesTheStatusAndTheStaticPageThenStops)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    CHECK(server.isRunning());
    CHECK(server.port() > 0);

    httplib::Client http("127.0.0.1", server.port());
    auto status = http.Get("/api/status");
    CHECK(status);
    LONGS_EQUAL(200, status->status);
    const json s = json::parse(status->body);
    LONGS_EQUAL(0, s.at("sessions").get<int>());
    LONGS_EQUAL(0, s.at("attached").get<int>());

    auto page = http.Get("/");
    CHECK(page);
    LONGS_EQUAL(200, page->status);
    CHECK(page->body.find("board-container") != std::string::npos);

    server.stop();
    CHECK(!server.isRunning());
    CHECK(!httplib::Client("127.0.0.1", 1).Get("/"));  // sanity: a closed port fails
}

TEST(Abc80WebServer, StartFailsCleanlyWhenThePortIsTaken)
{
    Abc80WebServer first(testConfig());
    CHECK(first.start());
    ServerConfig c = testConfig();
    c.port = first.port();
    Abc80WebServer second(c);
    CHECK(!second.start());
    CHECK(!second.isRunning());
    CHECK(!second.lastError().empty());
}

TEST(Abc80WebServer, ATabGetsSixtyHertzTelemetryWithAnIncreasingSequence)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("tab-token-0001");
    json first, later;
    CHECK(tab.anyTelemetry(8000, &first));
    STRCMP_EQUAL("monitor", first.at("rom").get<std::string>().c_str());
    LONGS_EQUAL(6, static_cast<long>(first.at("displayMasks").size()));
    CHECK(anyLit(first));  // the monitor was booted to its scan loop
    const auto t0 = Clock::now();
    int frames = 0;
    json last = first;
    while (frames < 30 && Clock::now() - t0 < std::chrono::seconds(5)) {
        if (tab.anyTelemetry(500, &later)) {
            CHECK(later.at("seq").get<uint64_t>() > last.at("seq").get<uint64_t>());
            last = later;
            ++frames;
        }
    }
    CHECK(frames >= 20);  // roughly half a second of frames at 60 Hz, allowing for a loaded machine
    CHECK(last.at("cycles").get<uint64_t>() > first.at("cycles").get<uint64_t>());
    LONGS_EQUAL(1, static_cast<long>(server.attachedCount()));
}

TEST(Abc80WebServer, A7_TabsAreIsolated)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab a(server.port()), b(server.port());
    CHECK(a.connect());
    CHECK(b.connect());
    a.hello("tab-token-000A");
    b.hello("tab-token-000B");
    json bBefore;
    CHECK(b.anyTelemetry(8000, &bBefore));
    CHECK(a.anyTelemetry(8000));
    a.tap("ADRS");
    a.tap("1");
    a.tap("2");
    a.tap("3");
    a.tap("4");
    CHECK(a.waitFor("telemetry", [](const json &t) { return digits(t).compare(0, 4, "1234") == 0; }, 10000));
    json bAfter;
    CHECK(b.anyTelemetry(2000, &bAfter));
    CHECK(bAfter.at("displayMasks") == bBefore.at("displayMasks"));  // tab B never changed
    CHECK(digits(bAfter).compare(0, 4, "1234") != 0);
    LONGS_EQUAL(2, static_cast<long>(server.sessionCount()));
}

TEST(Abc80WebServer, A6_AReloadWithTheSameTokenKeepsTheBoard)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    json before;
    {
        Tab a(server.port());
        CHECK(a.connect());
        a.hello("reload-token-1");
        CHECK(a.anyTelemetry(8000));
        a.tap("ADRS");
        a.tap("7");
        a.tap("8");
        CHECK(a.waitFor("telemetry", [](const json &t) { return digits(t).find("78") != std::string::npos; }, 10000, &before));
        a.close();
    }
    CHECK(waitUntil([&] { return server.attachedCount() == 0; }, 5000));
    LONGS_EQUAL(1, static_cast<long>(server.sessionCount()));  // the board is kept while detached
    Tab again(server.port());
    CHECK(again.connect());
    again.hello("reload-token-1");
    json after;
    CHECK(again.anyTelemetry(8000, &after));
    CHECK(digits(after).find("78") != std::string::npos);        // same address on the display
    CHECK(after.at("cycles").get<uint64_t>() >= before.at("cycles").get<uint64_t>());  // time did not restart
    LONGS_EQUAL(1, static_cast<long>(server.sessionCount()));
}

TEST(Abc80WebServer, ADetachedBoardIsFreedAfterTheGracePeriod)
{
    ServerConfig c = testConfig();
    c.sessionGraceSeconds = 1;
    Abc80WebServer server(c);
    CHECK(server.start());
    {
        Tab a(server.port());
        CHECK(a.connect());
        a.hello("grace-token-01");
        CHECK(a.anyTelemetry(8000));
        a.close();
    }
    CHECK(waitUntil([&] { return server.attachedCount() == 0; }, 5000));
    CHECK(waitUntil([&] { return server.sessionCount() == 0; }, 8000));
}

TEST(Abc80WebServer, A8_NoAttachedTabCostsNoCpu)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    {
        Tab a(server.port());
        CHECK(a.connect());
        a.hello("idle-token-001");
        CHECK(a.anyTelemetry(8000));
        a.close();
    }
    CHECK(waitUntil([&] { return server.attachedCount() == 0; }, 5000));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // let everything settle
    const double cpu0 = cpuSeconds();
    const auto t0 = Clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const double cpu = cpuSeconds() - cpu0;
    const double wall = std::chrono::duration<double>(Clock::now() - t0).count();
    CHECK_TEXT(cpu / wall < 0.02, "idle server must use under 2% of a core");
}

TEST(Abc80WebServer, ARefusesTooManySessions)
{
    ServerConfig c = testConfig();
    c.maxSessions = 2;
    Abc80WebServer server(c);
    CHECK(server.start());
    Tab a(server.port()), b(server.port()), third(server.port());
    CHECK(a.connect() && b.connect() && third.connect());
    a.hello("limit-token-01");
    b.hello("limit-token-02");
    CHECK(a.anyTelemetry(8000));
    CHECK(b.anyTelemetry(8000));
    third.hello("limit-token-03");
    CHECK(third.waitFor("error", [](const json &e) { return e.at("reason").get<std::string>().find("too many") != std::string::npos; }, 5000));
    CHECK(third.waitClosed(5000));
    LONGS_EQUAL(2, static_cast<long>(server.sessionCount()));
    // An existing token can still reattach at the limit.
    a.close();
    CHECK(waitUntil([&] { return server.attachedCount() == 1; }, 5000));
    Tab a2(server.port());
    CHECK(a2.connect());
    a2.hello("limit-token-01");
    CHECK(a2.anyTelemetry(8000));
}

TEST(Abc80WebServer, MalformedMessagesGetAnErrorAndTheSessionSurvives)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab a(server.port());
    CHECK(a.connect());
    a.hello("garbage-token1");
    CHECK(a.anyTelemetry(8000));
    const char *junk[] = { "not json", "[]", "{}", R"({"type":"key","key":"RST","action":"down"})",
                           R"({"type":"fly"})", R"({"type":"setRom","rom":"vic20"})" };
    // Send the raw junk as text frames on a second connection.
    httplib::ws::WebSocketClient raw("ws://127.0.0.1:" + std::to_string(server.port()) + "/ws");
    raw.set_read_timeout(0, 200000);
    CHECK(static_cast<bool>(raw.connect()));
    CHECK(raw.send(R"({"type":"hello","token":"garbage-token2"})"));
    for (const char *j : junk) CHECK(raw.send(j));
    int errors = 0;
    const auto end = Clock::now() + std::chrono::seconds(6);
    while (errors < 6 && Clock::now() < end) {
        std::string msg;
        if (raw.read(msg) != httplib::ws::Text) continue;
        const json m = json::parse(msg, nullptr, false);
        if (!m.is_discarded() && m.value("type", "") == "error") ++errors;
    }
    LONGS_EQUAL(6, errors);
    CHECK(raw.is_open());
    CHECK(a.anyTelemetry(5000));  // the other tab never noticed
}

TEST(Abc80WebServer, ATelemetryFrameBeforeHelloIsRefusedAndTheSocketIsClosed)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab a(server.port());
    CHECK(a.connect());
    a.key("1", "down");  // no hello first
    CHECK(a.waitFor("error", [](const json &) { return true; }, 5000));
    CHECK(a.waitClosed(5000));
    LONGS_EQUAL(0, static_cast<long>(server.sessionCount()));
}

TEST(Abc80WebServer, A11_A12_SwitchingToThe1993RomPowerCyclesToADarkDeafPanel)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab a(server.port());
    CHECK(a.connect());
    a.hello("rom-token-0001");
    CHECK(a.waitFor("telemetry", [](const json &t) { return anyLit(t); }, 8000));
    a.send({ { "type", "setRom" }, { "rom", "1993" } });
    json t;
    CHECK(a.waitFor("telemetry", [](const json &x) { return x.at("rom").get<std::string>() == "1993"; }, 15000, &t));
    CHECK(a.waitFor("telemetry", [](const json &x) { return x.at("rom").get<std::string>() == "1993" && !anyLit(x); }, 8000));
    a.tap("1");
    a.tap("ADRS");
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    CHECK(a.anyTelemetry(3000, &t));
    CHECK(!anyLit(t));  // keys do nothing at the link poll
    a.send({ { "type", "setRom" }, { "rom", "monitor" } });
    CHECK(a.waitFor("telemetry", [](const json &x) { return x.at("rom").get<std::string>() == "monitor" && anyLit(x); }, 15000));
}

TEST(Abc80WebServer, A9_ResetRestartsTheRomAndTheDisplayComesBack)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab a(server.port());
    CHECK(a.connect());
    a.hello("reset-token-01");
    CHECK(a.waitFor("telemetry", [](const json &t) { return anyLit(t); }, 8000));
    a.tap("ADRS");
    a.tap("5");
    CHECK(a.waitFor("telemetry", [](const json &t) { return digits(t).find('5') != std::string::npos; }, 10000));
    a.send({ { "type", "reset" } });
    CHECK(a.waitFor("telemetry", [](const json &t) { return !anyLit(t); }, 5000));   // dark during the chime
    CHECK(a.waitFor("telemetry", [](const json &t) { return anyLit(t) && digits(t).find('5') == std::string::npos; }, 15000));
}

TEST(Abc80WebServer, ACrossOriginPageIsRefused)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    httplib::Headers evil = { { "Origin", "http://evil.example" } };
    httplib::ws::WebSocketClient c("ws://127.0.0.1:" + std::to_string(server.port()) + "/ws", evil);
    c.set_read_timeout(0, 300000);
    const bool opened = static_cast<bool>(c.connect());
    if (opened) {
        c.send(R"({"type":"hello","token":"origin-token-1"})");
        std::string msg;
        const auto end = Clock::now() + std::chrono::seconds(3);
        bool gotTelemetry = false;
        while (Clock::now() < end) {
            if (c.read(msg) == httplib::ws::Text && msg.find("telemetry") != std::string::npos) gotTelemetry = true;
            if (!c.is_open()) break;
        }
        CHECK(!gotTelemetry);
    }
    LONGS_EQUAL(0, static_cast<long>(server.sessionCount()));
}

TEST(Abc80WebServer, StopReturnsPromptlyWithTabsStillAttached)
{
    auto server = std::make_unique<Abc80WebServer>(testConfig());
    CHECK(server->start());
    Tab a(server->port()), b(server->port());
    CHECK(a.connect() && b.connect());
    a.hello("stop-token-001");
    b.hello("stop-token-002");
    CHECK(a.anyTelemetry(8000));
    CHECK(b.anyTelemetry(8000));
    const auto t0 = Clock::now();
    server->stop();
    CHECK(Clock::now() - t0 < std::chrono::seconds(5));
    CHECK(!server->isRunning());
    server.reset();  // destructor after stop is fine
}

TEST(Abc80WebServer, ExecutionControlRunPauseStepAndReset)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("exec-ctrl-001");

    json t;
    // 1. Initial connection: running (paused: false)
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", false);
    }, 8000, &t));
    CHECK(!t.value("paused", true));

    // 2. Pause
    tab.pause();
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.value("paused", false);
    }, 4000, &t));
    CHECK(t.value("paused", false));
    const uint64_t pausedCycles = t.value("cycles", 0ULL);

    // Multiple pauses don't corrupt state
    tab.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Verify cycles held static while paused
    tab.pause();
    CHECK(tab.waitFor("telemetry", [pausedCycles](const json &j) {
        return j.value("paused", false) && j.value("cycles", 0ULL) == pausedCycles;
    }, 2000, &t));

    // 3. Step single instruction
    tab.step();
    CHECK(tab.waitFor("telemetry", [pausedCycles](const json &j) {
        return j.value("paused", false) && j.value("cycles", 0ULL) > pausedCycles;
    }, 4000, &t));
    const uint64_t stepCycles = t.value("cycles", 0ULL);
    // Cycle advance should be less than 100 T-states (single instruction, not a full frame)
    CHECK(stepCycles > pausedCycles);
    CHECK(stepCycles - pausedCycles < 100);

    // 4. Resume running
    tab.run();
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", false);
    }, 4000, &t));
    CHECK(!t.value("paused", true));

    // Step while running is ignored (session stays running)
    tab.step();
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", false);
    }, 2000, &t));
    CHECK(!t.value("paused", true));

    // 5. Pause then Reset: reset restores running
    tab.pause();
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.value("paused", false);
    }, 4000, &t));
    tab.reset();
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", false);
    }, 4000, &t));
    CHECK(!t.value("paused", true));
}

TEST(Abc80WebServer, ReadMemReadDisasmAndStackTelemetry)
{
    Abc80WebServer server(testConfig());
    CHECK(server.start());
    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("mem-disasm-001");

    json t;
    // 1. Initial running state: stackBytes is empty
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", false);
    }, 8000, &t));
    CHECK(!t.value("paused", true));
    CHECK(t.at("stackBytes").empty());

    // 2. Query readMem: read 16 bytes at 0x0000 (Monitor ROM)
    tab.readMem(0x0000, 16);
    json memReply;
    CHECK(tab.waitFor("memData", [](const json &j) {
        return j.value("addr", 0) == 0;
    }, 4000, &memReply));
    LONGS_EQUAL(16, static_cast<long>(memReply.at("data").size()));
    BYTES_EQUAL(0x06, memReply.at("data")[0].get<int>());

    // 3. Query readDisasm: read 4 instructions at 0x0000
    tab.readDisasm(0x0000, 4);
    json disasmReply;
    CHECK(tab.waitFor("disasm", [](const json &j) {
        return j.value("addr", 0) == 0;
    }, 4000, &disasmReply));
    LONGS_EQUAL(4, static_cast<long>(disasmReply.at("lines").size()));
    LONGS_EQUAL(0, disasmReply.at("lines")[0].at("addr").get<long>());

    // 4. Pause: verify stackBytes is populated (32 bytes)
    tab.pause();
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.value("paused", false) && j.at("stackBytes").size() == 32;
    }, 4000, &t));
    LONGS_EQUAL(32, static_cast<long>(t.at("stackBytes").size()));
    CHECK(t.at("stackBaseAddr").get<uint16_t>() == t.at("registers").at("sp").get<uint16_t>());
}

TEST(Abc80WebServer, ResidentSessionCreatedOnStartupAndNeverExpiredByGrace)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());
    LONGS_EQUAL(1, static_cast<long>(server.sessionCount()));
    CHECK(server.isResidentPaused());
    CHECK(server.isResidentAutoPaused());

    // Connect tab to resident session
    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("resident");
    CHECK(tab.anyTelemetry(8000));
    LONGS_EQUAL(1, static_cast<long>(server.sessionCount()));
    LONGS_EQUAL(1, static_cast<long>(server.residentSubscriberCount()));

    // Disconnect tab and wait for grace period (1s)
    tab.close();
    CHECK(waitUntil([&] { return server.attachedCount() == 0; }, 5000));
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Resident session is NEVER destroyed by expireDetached()!
    LONGS_EQUAL(1, static_cast<long>(server.sessionCount()));
    LONGS_EQUAL(0, static_cast<long>(server.residentSubscriberCount()));
    CHECK(server.isResidentAutoPaused());
}

TEST(Abc80WebServer, ResidentSessionAutoPauseAndAutoResume)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());
    CHECK(server.isResidentAutoPaused());

    // Connect tab
    Tab tab1(server.port());
    CHECK(tab1.connect());
    tab1.hello("resident");

    json t;
    CHECK(tab1.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", true);
    }, 8000, &t));
    CHECK(!server.isResidentPaused());
    CHECK(!server.isResidentAutoPaused());

    // Disconnect tab -> auto-pauses
    tab1.close();
    CHECK(waitUntil([&] { return server.isResidentAutoPaused(); }, 5000));
    CHECK(server.isResidentPaused());

    // Reconnect tab -> auto-resumes
    Tab tab2(server.port());
    CHECK(tab2.connect());
    tab2.hello("resident");
    CHECK(tab2.waitFor("telemetry", [](const json &j) {
        return !j.value("paused", true);
    }, 8000, &t));
    CHECK(!server.isResidentPaused());
    CHECK(!server.isResidentAutoPaused());
}

TEST(Abc80WebServer, ResidentSessionPreservesManualPauseAcrossReconnect)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    Tab tab1(server.port());
    CHECK(tab1.connect());
    tab1.hello("resident");
    CHECK(tab1.anyTelemetry(8000));

    // Manually pause
    tab1.pause();
    json t;
    CHECK(tab1.waitFor("telemetry", [](const json &j) {
        return j.value("paused", false);
    }, 4000, &t));
    CHECK(server.isResidentPaused());
    CHECK(!server.isResidentAutoPaused());

    // Disconnect tab
    tab1.close();
    CHECK(waitUntil([&] { return server.attachedCount() == 0; }, 5000));

    // Reconnect: session should stay paused because it was MANUALLY paused!
    Tab tab2(server.port());
    CHECK(tab2.connect());
    tab2.hello("resident");
    CHECK(tab2.waitFor("telemetry", [](const json &j) {
        return j.value("paused", false);
    }, 4000, &t));
    CHECK(server.isResidentPaused());
    CHECK(!server.isResidentAutoPaused());
}

TEST(Abc80WebServer, ResidentSessionMultiSubscriberBroadcast)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    Tab tab1(server.port());
    CHECK(tab1.connect());
    tab1.hello("resident");
    CHECK(tab1.anyTelemetry(8000));

    Tab tab2(server.port());
    CHECK(tab2.connect());
    tab2.hello("resident");
    CHECK(tab2.anyTelemetry(8000));

    LONGS_EQUAL(2, static_cast<long>(server.residentSubscriberCount()));

    // Broadcast AI punch event
    server.broadcastAiPunch("ADRS", true, 3);

    json p1, p2;
    CHECK(tab1.waitFor("aiPunch", [](const json &j) {
        return j.value("key", "") == "ADRS" && j.value("down", false) && j.value("remaining", 0) == 3;
    }, 4000, &p1));
    CHECK(tab2.waitFor("aiPunch", [](const json &j) {
        return j.value("key", "") == "ADRS" && j.value("down", false) && j.value("remaining", 0) == 3;
    }, 4000, &p2));
}

TEST(Abc80WebServer, ResidentSessionAiPunchClockWakeAndAutoPause)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    // Starts auto-paused with 0 viewers
    CHECK(server.isResidentAutoPaused());

    // Wake clock for AI punch
    server.setResidentClockWakeForAi(true);
    CHECK(server.isResidentRunning());

    // Step a few cycles/frames while punch occurs
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // AI punch finishes while 0 viewers attached -> returns to auto-pause!
    server.setResidentClockWakeForAi(false);
    CHECK(server.isResidentAutoPaused());
    CHECK(server.isResidentPaused());
}

TEST(Abc80WebServer, ResidentKeyNameAliasesAccepted)
{
    uint8_t code = 0;
    CHECK(keyCodeFromName("RUN", code));
    BYTES_EQUAL(KEY_RUN, code);
    CHECK(keyCodeFromName("GO", code));
    BYTES_EQUAL(KEY_RUN, code);

    CHECK(keyCodeFromName("ADDR", code));
    BYTES_EQUAL(KEY_ADRS, code);
    CHECK(keyCodeFromName("ADRS", code));
    BYTES_EQUAL(KEY_ADRS, code);

    CHECK(keyCodeFromName("STEP", code));
    BYTES_EQUAL(KEY_STEP, code);
    CHECK(keyCodeFromName("TO TAPE", code));
    BYTES_EQUAL(KEY_STEP, code);

    CHECK(keyCodeFromName("BP", code));
    BYTES_EQUAL(KEY_BP, code);
    CHECK(keyCodeFromName("FROM TAPE", code));
    BYTES_EQUAL(KEY_BP, code);
}

TEST(Abc80WebServer, CancelAiPunchMessageTriggersCallback)
{
    Abc80WebServer server(residentTestConfig());
    std::atomic<bool> cancelled{false};
    server.setOnAiCancel([&] { cancelled.store(true); });
    CHECK(server.start());

    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("resident");
    CHECK(tab.anyTelemetry(8000));

    tab.send({ { "type", "cancelAiPunch" } });
    CHECK(waitUntil([&] { return cancelled.load(); }, 4000));
}

TEST(Abc80WebServer, ResidentSessionPreservesManualPauseAfterAiClockWakeWithViewerAttached)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("resident");
    CHECK(tab.anyTelemetry(8000));

    // Operator manually pauses the resident session while watching
    tab.pause();
    json t;
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.value("paused", false);
    }, 4000, &t));
    CHECK(server.isResidentPaused());
    CHECK(!server.isResidentAutoPaused());

    // AI punch starts -> wakes clock temporarily
    server.setResidentClockWakeForAi(true);
    CHECK(server.isResidentRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // AI punch ends while viewer tab is STILL connected -> must restore manual pause!
    server.setResidentClockWakeForAi(false);
    CHECK(server.isResidentPaused());
    CHECK(!server.isResidentAutoPaused());
}

TEST(Abc80WebServer, ResidentSessionLocksOutHumanInputDuringAiPunch)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("resident");
    CHECK(tab.anyTelemetry(8000));
    CHECK(server.isResidentRunning());

    // Enter AI punch mode
    server.setResidentClockWakeForAi(true);

    // Human attempts to pause or reset while AI punch is active -> must be ignored
    tab.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(server.isResidentRunning());

    server.setResidentClockWakeForAi(false);
}

TEST(Abc80WebServer, ResidentResetAndGatewayToolSchemas)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    AiKeySequencer seq(
        [&server](const std::string &key, bool down, size_t remaining) {
            uint8_t code = 0;
            if (key == "RST") {
                if (down) {
                    server.resetResident();
                }
            } else if (keyCodeFromName(key, code)) {
                server.punchResidentKey(code, down);
            }
            server.broadcastAiPunch(key, down, remaining);
        },
        [&server]() {
            server.setResidentClockWakeForAi(false);
        },
        40,
        15
    );

    AimonClientConfig gwCfg;
    gwCfg.enabled = false;
    gwCfg.webPort = server.port();
    AimonGatewayClient gw(server, seq, gwCfg);

    // 1. Verify abc80_read_cpu schema (autoPaused + flags)
    const json cpuJson = json::parse(gw.handleToolCall("abc80_read_cpu", "{}"));
    CHECK(cpuJson.contains("autoPaused"));
    CHECK(cpuJson.at("autoPaused").get<bool>());
    CHECK(cpuJson.contains("flags"));
    CHECK(cpuJson.at("flags").contains("z"));
    CHECK(cpuJson.at("flags").contains("c"));

    // 2. Verify abc80_read_display schema (display_masks + leds.speaker)
    const json dispJson = json::parse(gw.handleToolCall("abc80_read_display", "{}"));
    CHECK(dispJson.contains("display_masks"));
    LONGS_EQUAL(6, static_cast<long>(dispJson.at("display_masks").size()));
    CHECK(dispJson.at("leds").contains("speaker"));

    // 3. Verify abc80_read_memory schema (hex_dump)
    const json memJson = json::parse(gw.handleToolCall("abc80_read_memory", "{\"addr\":0,\"count\":16}"));
    CHECK(memJson.contains("hex_dump"));
    CHECK(memJson.at("hex_dump").get<std::string>().find("0000:") == 0);

    // 4. Verify abc80_punch_keys with "RST" tactile reset and schema fields
    const json punchJson = json::parse(gw.handleToolCall("abc80_punch_keys", "{\"keys\":[\"RST\"],\"sync\":true}"));
    STRCMP_EQUAL("completed", punchJson.value("status", "").c_str());
    LONGS_EQUAL(1, static_cast<long>(punchJson.value("enqueued_count", 0)));
    CHECK(punchJson.contains("estimated_duration_sec"));
    CHECK(punchJson.contains("hint"));

    // 5. Verify abc80_get_punch_status includes remaining
    const json statusJson = json::parse(gw.handleToolCall("abc80_get_punch_status", "{}"));
    CHECK(statusJson.contains("remaining"));
    LONGS_EQUAL(0, static_cast<long>(statusJson.at("remaining").get<size_t>()));
}

TEST(Abc80WebServer, GatewayClientSendsRegisterAndUnregisterOnShutdown)
{
    Abc80WebServer server(residentTestConfig());
    CHECK(server.start());

    const int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listenFd >= 0);
    int yes = 1;
    ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    CHECK(::bind(listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0);
    CHECK(::listen(listenFd, 1) == 0);

    socklen_t addrLen = sizeof(addr);
    CHECK(::getsockname(listenFd, reinterpret_cast<struct sockaddr *>(&addr), &addrLen) == 0);
    const int gwPort = ntohs(addr.sin_port);

    std::vector<std::string> lines;
    std::mutex linesMutex;
    std::thread acceptThread([listenFd, &lines, &linesMutex]() {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        const int cfd = ::accept(listenFd, reinterpret_cast<struct sockaddr *>(&caddr), &clen);
        if (cfd < 0) return;

        std::string buf;
        char chunk[2048];
        while (true) {
            const ssize_t n = ::read(cfd, chunk, sizeof(chunk));
            if (n <= 0) break;
            buf.append(chunk, static_cast<size_t>(n));
            size_t pos = 0;
            while ((pos = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    std::lock_guard<std::mutex> lk(linesMutex);
                    lines.push_back(line);
                }
            }
        }
        ::close(cfd);
    });

    AiKeySequencer seq(
        [](const std::string &, bool, size_t) {},
        []() {},
        40,
        15
    );

    AimonClientConfig gwCfg;
    gwCfg.enabled = true;
    gwCfg.host = "127.0.0.1";
    gwCfg.port = gwPort;
    gwCfg.reconnectIntervalSec = 1;
    gwCfg.webPort = server.port();

    AimonGatewayClient gw(server, seq, gwCfg);
    CHECK(gw.start());

    const auto deadline = Clock::now() + std::chrono::seconds(5);
    while (Clock::now() < deadline && !gw.isConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CHECK(gw.isConnected());

    gw.stop();
    if (acceptThread.joinable()) {
        acceptThread.join();
    }
    ::close(listenFd);

    std::lock_guard<std::mutex> lk(linesMutex);
    LONGS_EQUAL(2, static_cast<long>(lines.size()));
    const json regMsg = json::parse(lines[0]);
    STRCMP_EQUAL("gateway/register", regMsg.value("method", "").c_str());
    LONGS_EQUAL(server.port(), regMsg["params"].value("web_port", 0));

    const json unregMsg = json::parse(lines[1]);
    STRCMP_EQUAL("gateway/unregister", unregMsg.value("method", "").c_str());
    STRCMP_EQUAL("abc80", unregMsg["params"].value("subsystem", "").c_str());
    LONGS_EQUAL(server.port(), unregMsg["params"].value("web_port", 0));
}

TEST(Abc80WebServer, CassetteTapeDeckWebSocketTransportAndLibrarySeeding)
{
    const std::string tapesDir = "/tmp/abc80_server_tape_test_dir";
    std::filesystem::remove_all(tapesDir);

    ServerConfig cfg = testConfig();
    cfg.tapesDir = tapesDir;
    Abc80WebServer server(cfg);
    CHECK(server.start());

    Tab tab(server.port());
    CHECK(tab.connect());
    tab.hello("tape_session_01");

    json firstTel;
    CHECK(tab.anyTelemetry(4000, &firstTel));
    CHECK(firstTel.contains("tape"));
    STRCMP_EQUAL("stopped", firstTel["tape"].value("state", "").c_str());
    STRCMP_EQUAL("0001", firstTel["tape"].value("fileId", "").c_str());
    STRCMP_EQUAL("BCD Up-Counter (1000H)", firstTel["tape"].value("annotation", "").c_str());

    // Request tape_list and verify 4 seeded default tapes exist (0001..0004)
    tab.send({ { "type", "tape_list" } });
    json listMsg;
    CHECK(tab.waitFor("tape_list", [](const json &j) {
        return j.contains("tapes") && j["tapes"].size() >= 4;
    }, 4000, &listMsg));

    // Annotate an on-demand slot 0042 and select it
    tab.send({
        { "type", "tape_annotate" },
        { "fileId", "0042" },
        { "annotation", "Custom Tape Label 0042" }
    });
    tab.send({
        { "type", "tape_select" },
        { "fileId", "0042" }
    });

    json selTel;
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.contains("tape") && j["tape"].value("fileId", "") == "0042" &&
               j["tape"].value("annotation", "") == "Custom Tape Label 0042";
    }, 4000, &selTel));
    CHECK(std::filesystem::exists(tapesDir + "/0042.tape"));

    // Select seeded tape 0001 and press PLAY -> state becomes "playing" with 1000 Hz leader tone
    tab.send({
        { "type", "tape_transport" },
        { "action", "play" },
        { "fileId", "0001" }
    });

    json playTel;
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.contains("tape") && j["tape"].value("state", "") == "playing" &&
               j["tape"].value("fileId", "") == "0001";
    }, 4000, &playTel));
    LONGS_EQUAL(1000, playTel["tape"].value("toneHz", 0));

    // Press STOP -> state returns to "stopped"
    tab.send({
        { "type", "tape_transport" },
        { "action", "stop" }
    });
    CHECK(tab.waitFor("telemetry", [](const json &j) {
        return j.contains("tape") && j["tape"].value("state", "") == "stopped";
    }, 4000));

    std::filesystem::remove_all(tapesDir);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

