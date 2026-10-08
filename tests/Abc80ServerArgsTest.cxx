/*
 * Abc80ServerArgsTest.cxx
 *
 * Command-line parsing of abc80_server: valid options, every rejection, no side effects on failure.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/ServerRunner.hxx>
#include <string>
#include <vector>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

using namespace abc80;

namespace {

bool parse(const std::vector<std::string> &args, ServerConfig &cfg, std::string &err, bool &help)
{
    std::vector<const char *> argv = { "abc80_server" };
    for (const auto &a : args) argv.push_back(a.c_str());
    return parseServerArgs(static_cast<int>(argv.size()), argv.data(), cfg, err, help);
}

} // namespace

TEST_GROUP(Abc80ServerArgs)
{
    ServerConfig cfg;
    std::string err;
    bool help{false};
};

TEST(Abc80ServerArgs, NoArgumentsLeavesTheLoopbackDefaults)
{
    CHECK(parse({}, cfg, err, help));
    CHECK(!help);
    STRCMP_EQUAL("127.0.0.1", cfg.bind.c_str());
    LONGS_EQUAL(8080, cfg.port);
    CHECK(cfg.defaultRom == RomId::Monitor);
    CHECK(!cfg.bootTurbo);
    CHECK(!cfg.coldBoot);
}

TEST(Abc80ServerArgs, EveryOptionIsApplied)
{
    CHECK(parse({ "--port", "0", "--bind", "0.0.0.0", "--web-dir", "site", "--root", "/srv/abc",
                  "--rom", "1993", "--boot", "cold", "--boot-turbo", "--max-sessions", "3", "--grace", "45" },
                cfg, err, help));
    LONGS_EQUAL(0, cfg.port);
    STRCMP_EQUAL("0.0.0.0", cfg.bind.c_str());
    STRCMP_EQUAL("site", cfg.webRoot.c_str());
    STRCMP_EQUAL("/srv/abc", cfg.root.c_str());
    CHECK(cfg.defaultRom == RomId::Vintage1993);
    CHECK(cfg.coldBoot);
    CHECK(cfg.bootTurbo);
    LONGS_EQUAL(3, static_cast<long>(cfg.maxSessions));
    LONGS_EQUAL(45, static_cast<long>(cfg.sessionGraceSeconds));
}

TEST(Abc80ServerArgs, TheUpperAndLowerLimitsAreAccepted)
{
    CHECK(parse({ "--port", "65535", "--max-sessions", "64", "--grace", "86400" }, cfg, err, help));
    LONGS_EQUAL(65535, cfg.port);
    LONGS_EQUAL(64, static_cast<long>(cfg.maxSessions));
    LONGS_EQUAL(86400, static_cast<long>(cfg.sessionGraceSeconds));
    CHECK(parse({ "--port", "0", "--max-sessions", "1", "--grace", "0" }, cfg, err, help));
    LONGS_EQUAL(0, cfg.port);
    LONGS_EQUAL(1, static_cast<long>(cfg.maxSessions));
    LONGS_EQUAL(0, static_cast<long>(cfg.sessionGraceSeconds));
}

TEST(Abc80ServerArgs, HelpIsReported)
{
    CHECK(parse({ "--help" }, cfg, err, help));
    CHECK(help);
    help = false;
    CHECK(parse({ "-h" }, cfg, err, help));
    CHECK(help);
}

TEST(Abc80ServerArgs, BadOptionsAreRejectedWithAReason)
{
    const std::vector<std::vector<std::string>> bad = {
        { "--port" }, { "--port", "abc" }, { "--port", "-1" }, { "--port", "65536" }, { "--port", "80x" },
        { "--rom", "vic20" }, { "--boot", "lukewarm" }, { "--max-sessions", "0" }, { "--max-sessions", "65" },
        { "--grace", "-5" }, { "--grace", "86401" }, { "--bind" }, { "--frobnicate" }, { "extra" },
    };
    for (const auto &args : bad) {
        ServerConfig c;
        std::string e;
        bool h = false;
        CHECK_TEXT(!parse(args, c, e, h), args[0].c_str());
        CHECK_TEXT(!e.empty(), args[0].c_str());
    }
}

// Any argument vector either parses into in-range values or is rejected with a reason.
TEST(Abc80ServerArgs, PropertyArbitraryArgumentsAreAcceptedInRangeOrRejected)
{
    const bool ok = rc::check("arbitrary args", [](const std::vector<std::string> &args) {
        ServerConfig c;
        std::string e;
        bool h = false;
        const bool parsed = parse(args, c, e, h);
        if (parsed) {
            RC_ASSERT(c.port >= 0 && c.port <= 65535);
            RC_ASSERT(c.maxSessions >= 1 && c.maxSessions <= 64);
            RC_ASSERT(c.sessionGraceSeconds <= 86400);
        } else {
            RC_ASSERT(!e.empty());
        }
    });
    CHECK(ok);
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
