/*
 * Abc80ConfigTest.cxx
 *
 * Unit tests for Abc80Config using libconfig++ (plan Section 2.3.1).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Config.hxx>
#include <filesystem>
#include <fstream>
#include <string>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80Config)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(Abc80Config, DefaultValuesMatchSpecification)
{
    Abc80Config cfg;
    LONGS_EQUAL(8080, cfg.server().port);
    STRCMP_EQUAL("web", cfg.server().webRoot.c_str());
    STRCMP_EQUAL("monitor", cfg.server().rom.c_str());
    STRCMP_EQUAL("warm", cfg.server().bootMode.c_str());
    CHECK(!cfg.server().bootTurbo);
    LONGS_EQUAL(8, static_cast<long>(cfg.server().maxSessions));
    LONGS_EQUAL(300, static_cast<long>(cfg.server().sessionGraceSeconds));

    CHECK(cfg.aimon().enabled);
    STRCMP_EQUAL("localhost", cfg.aimon().host.c_str());
    LONGS_EQUAL(3885, cfg.aimon().port);
    LONGS_EQUAL(5, static_cast<long>(cfg.aimon().reconnectIntervalSec));

    LONGS_EQUAL(500, static_cast<long>(cfg.aiKeypad().punchIntervalMs));
    LONGS_EQUAL(120, static_cast<long>(cfg.aiKeypad().holdDurationMs));
}

TEST(Abc80Config, ToServerConfigEnforcesUnconditionalZeroZeroZeroZeroBind)
{
    Abc80Config cfg;
    cfg.server().port = 9090;
    cfg.server().maxSessions = 12;

    ServerConfig sc = cfg.toServerConfig("/custom/root");
    STRCMP_EQUAL("0.0.0.0", sc.bind.c_str());
    LONGS_EQUAL(9090, sc.port);
    LONGS_EQUAL(12, static_cast<long>(sc.maxSessions));
    STRCMP_EQUAL("/custom/root", sc.root.c_str());
    CHECK(sc.createResident);
}

TEST(Abc80Config, LoadFromStringParsesAllSections)
{
    const std::string text =
        "server = {\n"
        "    port = 7777;\n"
        "    web_root = \"static_site\";\n"
        "    rom = \"1993\";\n"
        "    boot_mode = \"cold\";\n"
        "    boot_turbo = true;\n"
        "    max_sessions = 16;\n"
        "    session_grace_seconds = 60;\n"
        "};\n"
        "aimon = {\n"
        "    enabled = false;\n"
        "    host = \"aimon.example.com\";\n"
        "    port = 4000;\n"
        "    reconnect_interval_sec = 10;\n"
        "};\n"
        "ai_keypad = {\n"
        "    punch_interval_ms = 400;\n"
        "    hold_duration_ms = 100;\n"
        "};\n";

    Abc80Config cfg;
    std::string err;
    CHECK(cfg.loadFromString(text, &err));
    CHECK(err.empty());

    LONGS_EQUAL(7777, cfg.server().port);
    STRCMP_EQUAL("static_site", cfg.server().webRoot.c_str());
    STRCMP_EQUAL("1993", cfg.server().rom.c_str());
    STRCMP_EQUAL("cold", cfg.server().bootMode.c_str());
    CHECK(cfg.server().bootTurbo);
    LONGS_EQUAL(16, static_cast<long>(cfg.server().maxSessions));
    LONGS_EQUAL(60, static_cast<long>(cfg.server().sessionGraceSeconds));

    CHECK(!cfg.aimon().enabled);
    STRCMP_EQUAL("aimon.example.com", cfg.aimon().host.c_str());
    LONGS_EQUAL(4000, cfg.aimon().port);
    LONGS_EQUAL(10, static_cast<long>(cfg.aimon().reconnectIntervalSec));

    LONGS_EQUAL(400, static_cast<long>(cfg.aiKeypad().punchIntervalMs));
    LONGS_EQUAL(100, static_cast<long>(cfg.aiKeypad().holdDurationMs));
}

TEST(Abc80Config, StrictValidationRejectsMaxSessionsLessThanOne)
{
    Abc80Config cfg;
    std::string err;

    const std::string textZero = "server = { max_sessions = 0; };";
    CHECK(!cfg.loadFromString(textZero, &err));
    CHECK(err.find("max_sessions must be >= 1") != std::string::npos);

    const std::string textNegative = "server = { max_sessions = -5; };";
    err.clear();
    CHECK(!cfg.loadFromString(textNegative, &err));
    CHECK(err.find("max_sessions must be >= 1") != std::string::npos);
}

TEST(Abc80Config, MaxSessionsClampedToHardLimitWhenExceeded)
{
    Abc80Config cfg;
    std::string err;

    const std::string textLarge = "server = { max_sessions = 128; };";
    CHECK(cfg.loadFromString(textLarge, &err));
    LONGS_EQUAL(64, static_cast<long>(cfg.server().maxSessions));
}

TEST(Abc80Config, AutoGeneratesDefaultConfigFileIfMissing)
{
    const std::string tempDir = "/tmp/abc80_test_config_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    const std::string tempFile = tempDir + "/nested/abc-80.cfg";

    CHECK(!std::filesystem::exists(tempFile));

    Abc80Config cfg;
    std::string err;
    CHECK(cfg.load(tempFile, &err));
    CHECK(std::filesystem::exists(tempFile));

    // Verify it saved valid config
    LONGS_EQUAL(8080, cfg.server().port);
    STRCMP_EQUAL("localhost", cfg.aimon().host.c_str());

    std::filesystem::remove_all(tempDir);
}

TEST(Abc80Config, PropertyArbitraryMaxSessionsAndMalformedStrings)
{
    const bool okBounds = rc::check("arbitrary max_sessions bounds", [](int maxSess) {
        Abc80Config cfg;
        std::string err;
        const std::string text = "server = { max_sessions = " + std::to_string(maxSess) + "; };";
        const bool ok = cfg.loadFromString(text, &err);
        if (maxSess < 1) {
            RC_ASSERT(!ok);
            RC_ASSERT(!err.empty());
        } else {
            RC_ASSERT(ok);
            RC_ASSERT(cfg.server().maxSessions >= 1 && cfg.server().maxSessions <= 64);
        }
    });
    CHECK(okBounds);

    const bool okRaw = rc::check("arbitrary config strings never crash", [](const std::string &raw) {
        Abc80Config cfg;
        std::string err;
        const bool ok = cfg.loadFromString(raw, &err);
        if (ok) {
            RC_ASSERT(cfg.server().maxSessions >= 1 && cfg.server().maxSessions <= 64);
        } else {
            RC_ASSERT(!err.empty());
        }
    });
    CHECK(okRaw);
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

