/*
 * Abc80ConfigTest.cxx
 *
 * Unit tests for Abc80Config using libconfig++ (plan Section 2.3.1).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Config.hxx>
#include <abc80/Version.hxx>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

#ifndef ABC80_DEB_PATH
#define ABC80_DEB_PATH ""
#endif

using namespace abc80;

namespace {

std::string runCmd(const std::string &cmd)
{
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return "";
    }
    char buf[512];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        result += buf;
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string readFileText(const std::string &path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
}

struct ScopedEnvVar {
    std::string name;
    bool hadPrev{false};
    std::string prevVal;

    ScopedEnvVar(const char *varName, const char *newVal)
        : name(varName)
    {
        const char *cur = std::getenv(varName);
        if (cur != nullptr) {
            hadPrev = true;
            prevVal = cur;
        }
        if (newVal == nullptr) {
            ::unsetenv(varName);
        } else {
            ::setenv(varName, newVal, 1);
        }
    }

    ~ScopedEnvVar()
    {
        if (hadPrev) {
            ::setenv(name.c_str(), prevVal.c_str(), 1);
        } else {
            ::unsetenv(name.c_str());
        }
    }
};

} // namespace

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

TEST(Abc80Config, DefaultPathUsesEtcAbc80WhenUidIsRoot)
{
    ScopedEnvVar unsetCfg("ABC80_CONFIG_DIR", nullptr);
    ScopedEnvVar unsetSys("ABC80_SYSTEM_DIR", nullptr);
    ScopedEnvVar unsetXdg("XDG_CONFIG_HOME", nullptr);
    ScopedEnvVar setHome("HOME", "/tmp/abc80_test_home");

    STRCMP_EQUAL("/etc/abc-80", Abc80Config::defaultDirForUid(0).string().c_str());
    STRCMP_EQUAL("/etc/abc-80/abc-80.cfg", Abc80Config::defaultPathForUid(0).string().c_str());

    STRCMP_EQUAL("/tmp/abc80_test_home/.config/abc-80",
                 Abc80Config::defaultDirForUid(1000).string().c_str());
    STRCMP_EQUAL("/tmp/abc80_test_home/.config/abc-80/abc-80.cfg",
                 Abc80Config::defaultPathForUid(1000).string().c_str());

    {
        ScopedEnvVar setXdg("XDG_CONFIG_HOME", "/tmp/abc80_test_xdg");
        STRCMP_EQUAL("/tmp/abc80_test_xdg/abc-80/abc-80.cfg",
                     Abc80Config::defaultPathForUid(1000).string().c_str());
        STRCMP_EQUAL("/etc/abc-80/abc-80.cfg",
                     Abc80Config::defaultPathForUid(0).string().c_str());
    }
}

TEST(Abc80Config, ResolveResourcePathFallsBackToSystemDir)
{
    char tmpDirTemplate[] = "/tmp/abc80_sysdir_test_XXXXXX";
    char *tmpDir = ::mkdtemp(tmpDirTemplate);
    CHECK_TRUE(tmpDir != nullptr);
    const std::filesystem::path sysRoot(tmpDir);
    const std::filesystem::path sysDir = sysRoot / "etc" / "abc-80";
    const std::filesystem::path userHome = sysRoot / "home" / "tester";

    std::filesystem::create_directories(sysDir / "build");
    std::filesystem::create_directories(sysDir / "vintage");
    std::filesystem::create_directories(sysDir / "web");
    std::filesystem::create_directories(sysDir / "tapes");
    std::filesystem::create_directories(sysDir / "exercises" / "qb");
    std::filesystem::create_directories(userHome);

    {
        std::ofstream(sysDir / "build" / "rom.bin") << "rom";
        std::ofstream(sysDir / "vintage" / "rom.abc") << "abc";
        std::ofstream(sysDir / "web" / "index.html") << "<html></html>";
    }

    ScopedEnvVar unsetCfg("ABC80_CONFIG_DIR", nullptr);
    ScopedEnvVar unsetXdg("XDG_CONFIG_HOME", nullptr);
    ScopedEnvVar setHome("HOME", userHome.string().c_str());
    ScopedEnvVar setSys("ABC80_SYSTEM_DIR", sysDir.string().c_str());
    ScopedEnvVar ignoreCwd("ABC80_IGNORE_CWD_RESOURCES", "1");

    for (uid_t uid : { static_cast<uid_t>(0), static_cast<uid_t>(1000) }) {
        STRCMP_EQUAL((sysDir / "build" / "rom.bin").string().c_str(),
                     Abc80Config::resolveResourcePath("build/rom.bin", uid).string().c_str());
        STRCMP_EQUAL((sysDir / "vintage" / "rom.abc").string().c_str(),
                     Abc80Config::resolveResourcePath("vintage/rom.abc", uid).string().c_str());
        STRCMP_EQUAL((sysDir / "web" / "index.html").string().c_str(),
                     Abc80Config::resolveResourcePath("web/index.html", uid).string().c_str());
        STRCMP_EQUAL((sysDir / "tapes").string().c_str(),
                     Abc80Config::resolveResourcePath("tapes", uid).string().c_str());
        STRCMP_EQUAL((sysDir / "exercises" / "qb").string().c_str(),
                     Abc80Config::resolveResourcePath("exercises/qb", uid).string().c_str());
    }

    std::filesystem::remove_all(sysRoot);
}

TEST(Abc80Config, DebPackageMatchesReadmeVersionAndSystemdSpec)
{
    std::string debPath = ABC80_DEB_PATH;
    CHECK_FALSE_TEXT(debPath.empty(), "ABC80_DEB_PATH must not be empty");

    struct stat st;
    CHECK_EQUAL_TEXT(0, ::stat(debPath.c_str(), &st), "Debian package must exist");
    CHECK_TRUE(st.st_size > 0);

    const std::string expectedFileName =
        std::string("abc-80_") + abc80::versionString() + "_amd64.deb";
    CHECK_TRUE_TEXT(debPath.find(expectedFileName) != std::string::npos, debPath.c_str());

    std::string readme = readFileText("README.md");
    if (readme.empty()) {
        readme = readFileText("../README.md");
    }
    CHECK_FALSE_TEXT(readme.empty(), "README.md must be readable");
    const std::string expectedVerTag =
        std::string("**Version: ") + abc80::versionString() + "**";
    CHECK_TRUE_TEXT(readme.find(expectedVerTag) != std::string::npos,
                    "README.md **Version: <ver>** must match abc80::versionString()");

    std::string pkg = runCmd("dpkg-deb -f '" + debPath + "' Package");
    STRCMP_EQUAL("abc-80", pkg.c_str());

    std::string ver = runCmd("dpkg-deb -f '" + debPath + "' Version");
    STRCMP_EQUAL(abc80::versionString(), ver.c_str());

    std::string arch = runCmd("dpkg-deb -f '" + debPath + "' Architecture");
    STRCMP_EQUAL("amd64", arch.c_str());

    std::string deps = runCmd("dpkg-deb -f '" + debPath + "' Depends");
    CHECK_TRUE(deps.find("libc6") != std::string::npos);
    CHECK_TRUE(deps.find("libconfig++") != std::string::npos);

    char tmpDirTemplate[] = "/tmp/abc80_deb_test_XXXXXX";
    char *tmpDir = ::mkdtemp(tmpDirTemplate);
    CHECK_TRUE(tmpDir != nullptr);
    std::string tmpDirStr(tmpDir);

    runCmd("mkdir -p '" + tmpDirStr + "/DEBIAN' && dpkg-deb -e '" + debPath + "' '" + tmpDirStr + "/DEBIAN'");
    runCmd("mkdir -p '" + tmpDirStr + "/root' && dpkg-deb -x '" + debPath + "' '" + tmpDirStr + "/root'");

    std::string conffiles = readFileText(tmpDirStr + "/DEBIAN/conffiles");
    CHECK_TRUE(conffiles.find("/etc/default/abc-80") != std::string::npos);
    CHECK_TRUE(conffiles.find("/etc/abc-80/abc-80.cfg") != std::string::npos);

    struct stat scriptSt;
    CHECK_EQUAL(0, ::stat((tmpDirStr + "/DEBIAN/postinst").c_str(), &scriptSt));
    CHECK_TRUE((scriptSt.st_mode & 0111) != 0);
    CHECK_EQUAL(0, ::stat((tmpDirStr + "/DEBIAN/prerm").c_str(), &scriptSt));
    CHECK_TRUE((scriptSt.st_mode & 0111) != 0);
    CHECK_EQUAL(0, ::stat((tmpDirStr + "/DEBIAN/postrm").c_str(), &scriptSt));
    CHECK_TRUE((scriptSt.st_mode & 0111) != 0);

    std::string srvBinPath = tmpDirStr + "/root/usr/sbin/abc80_server";
    struct stat binSt;
    CHECK_EQUAL(0, ::stat(srvBinPath.c_str(), &binSt));
    CHECK_TRUE((binSt.st_mode & 0111) != 0);

    std::string cliBinPath = tmpDirStr + "/root/usr/bin/abc80_cli";
    CHECK_EQUAL(0, ::stat(cliBinPath.c_str(), &binSt));
    CHECK_TRUE((binSt.st_mode & 0111) != 0);

    std::string srvVerOutput = runCmd("'" + srvBinPath + "' --version");
    CHECK_TRUE(srvVerOutput.find(abc80::versionString()) != std::string::npos);

    std::string cliVerOutput = runCmd("'" + cliBinPath + "' --version");
    CHECK_TRUE(cliVerOutput.find(abc80::versionString()) != std::string::npos);

    std::string svcContent = readFileText(tmpDirStr + "/root/lib/systemd/system/abc-80.service");
    CHECK_TRUE(svcContent.find("User=root") != std::string::npos);
    CHECK_TRUE(svcContent.find("Group=root") != std::string::npos);
    CHECK_TRUE(svcContent.find("WorkingDirectory=/etc/abc-80") != std::string::npos);
    CHECK_TRUE(svcContent.find("ExecStart=/usr/sbin/abc80_server") != std::string::npos);
    CHECK_TRUE(svcContent.find("EnvironmentFile=-/etc/default/abc-80") != std::string::npos);
    CHECK_TRUE(svcContent.find("KillSignal=SIGTERM") != std::string::npos);

    std::string defaultContent = readFileText(tmpDirStr + "/root/etc/default/abc-80");
    CHECK_TRUE(defaultContent.find("ABC80_OPTS=") != std::string::npos);

    std::string cfgPath = tmpDirStr + "/root/etc/abc-80/abc-80.cfg";
    Abc80Config parsedCfg;
    std::string cfgErr;
    CHECK_TRUE_TEXT(parsedCfg.loadFile(cfgPath, &cfgErr), cfgErr.c_str());
    LONGS_EQUAL(8008, parsedCfg.server().port);
    STRCMP_EQUAL("vintage", parsedCfg.server().rom.c_str());

    CHECK_TRUE(std::filesystem::exists(tmpDirStr + "/root/etc/abc-80/build/rom.bin"));
    CHECK_TRUE(std::filesystem::exists(tmpDirStr + "/root/etc/abc-80/vintage/rom.abc"));
    CHECK_TRUE(std::filesystem::exists(tmpDirStr + "/root/etc/abc-80/web/index.html"));
    CHECK_TRUE(std::filesystem::exists(tmpDirStr + "/root/etc/abc-80/tapes/0001.tape"));
    CHECK_TRUE(std::filesystem::exists(tmpDirStr + "/root/etc/abc-80/exercises/qb"));
    CHECK_TRUE(std::filesystem::exists(tmpDirStr + "/root/etc/abc-80/programs"));

    for (const char *webRel : {"web/index.html", "web/app.js", "web/style.css"}) {
        std::string srcContent = readFileText(webRel);
        if (srcContent.empty()) {
            srcContent = readFileText(std::string("../") + webRel);
        }
        CHECK_FALSE(srcContent.empty());
        std::string pkgContent = readFileText(tmpDirStr + "/root/etc/abc-80/" + webRel);
        LONGS_EQUAL(srcContent.size(), pkgContent.size());
        CHECK_TRUE(srcContent == pkgContent);
    }

    runCmd("rm -rf '" + tmpDirStr + "'");
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

