/*
 * Abc80WebServerTest.cxx
 *
 * End-to-end tests of the web server: real sockets on an ephemeral loopback port, real
 * browser-style WebSocket clients, real ROMs behind every session (plan Sections 2.4, 2.5).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80WebServer.hxx>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
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

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
