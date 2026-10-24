/*
 * Abc80Config.cxx
 *
 * Configuration loader and validator using libconfig++ (plan Section 2.3.1).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Config.hxx>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <libconfig.h++>

namespace abc80 {

Abc80Config::Abc80Config()
{
}

std::filesystem::path Abc80Config::systemDir()
{
    const char *sysDir = std::getenv("ABC80_SYSTEM_DIR");
    if (sysDir != nullptr && sysDir[0] != '\0') {
        return std::filesystem::path(sysDir);
    }
    return std::filesystem::path("/etc/abc-80");
}

std::filesystem::path Abc80Config::defaultDirForUid(uid_t euid)
{
    const char *cfgDir = std::getenv("ABC80_CONFIG_DIR");
    if (cfgDir != nullptr && cfgDir[0] != '\0') {
        return std::filesystem::path(cfgDir);
    }
    if (euid == 0) {
        return systemDir();
    }
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return std::filesystem::path(xdg) / "abc-80";
    }
    const char *home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / ".config" / "abc-80";
    }
    struct passwd *pw = ::getpwuid(euid);
    if (pw != nullptr && pw->pw_dir != nullptr && pw->pw_dir[0] != '\0') {
        return std::filesystem::path(pw->pw_dir) / ".config" / "abc-80";
    }
    return std::filesystem::path("abc-80");
}

std::filesystem::path Abc80Config::defaultDir()
{
    return defaultDirForUid(::geteuid());
}

std::filesystem::path Abc80Config::defaultPathForUid(uid_t euid)
{
    return defaultDirForUid(euid) / "abc-80.cfg";
}

std::filesystem::path Abc80Config::defaultPath()
{
    return defaultPathForUid(::geteuid());
}

std::filesystem::path Abc80Config::resolveResourcePath(const std::filesystem::path &relPath, uid_t euid)
{
    if (relPath.empty() || relPath.is_absolute()) {
        return relPath;
    }

    const std::filesystem::path cleanRel = relPath.lexically_normal();
    std::error_code ec;

    const char *ignoreCwd = std::getenv("ABC80_IGNORE_CWD_RESOURCES");
    const bool skipCwd = (ignoreCwd != nullptr && std::string(ignoreCwd) == "1");
    if (!skipCwd && std::filesystem::exists(relPath, ec)) {
        return relPath;
    }

    const std::filesystem::path userCand = defaultDirForUid(euid) / cleanRel;
    if (std::filesystem::exists(userCand, ec)) {
        return userCand;
    }

    const std::filesystem::path sysCand = systemDir() / cleanRel;
    if (std::filesystem::exists(sysCand, ec)) {
        return sysCand;
    }

    if (euid == 0) {
        return userCand;
    }
    return relPath;
}

bool Abc80Config::parseAndValidate(void *configHandle, std::string *error)
{
    libconfig::Config *cfg = static_cast<libconfig::Config *>(configHandle);
    if (!cfg) {
        if (error) *error = "null config handle";
        return false;
    }

    // Top-level server setting fallbacks (e.g. port = 8008; rom = "vintage";)
    {
        int topPort = _server.port;
        if (cfg->lookupValue("port", topPort)) {
            _server.port = topPort;
        }
        std::string topWebRoot;
        if (cfg->lookupValue("web_root", topWebRoot)) {
            _server.webRoot = topWebRoot;
        }
        std::string topRom;
        if (cfg->lookupValue("rom", topRom)) {
            _server.rom = topRom;
        }
        std::string topBootMode;
        if (cfg->lookupValue("boot_mode", topBootMode)) {
            _server.bootMode = topBootMode;
        }
        bool topBootTurbo = _server.bootTurbo;
        if (cfg->lookupValue("boot_turbo", topBootTurbo)) {
            _server.bootTurbo = topBootTurbo;
        }
    }

    // Server settings
    if (cfg->exists("server")) {
        int port = _server.port;
        if (cfg->lookupValue("server.port", port)) {
            _server.port = port;
        }

        std::string webRoot;
        if (cfg->lookupValue("server.web_root", webRoot)) {
            _server.webRoot = webRoot;
        }

        std::string rom;
        if (cfg->lookupValue("server.rom", rom)) {
            _server.rom = rom;
        }

        std::string bootMode;
        if (cfg->lookupValue("server.boot_mode", bootMode)) {
            _server.bootMode = bootMode;
        }

        bool bootTurbo = _server.bootTurbo;
        if (cfg->lookupValue("server.boot_turbo", bootTurbo)) {
            _server.bootTurbo = bootTurbo;
        }

        int maxSessions = static_cast<int>(_server.maxSessions);
        if (cfg->lookupValue("server.max_sessions", maxSessions)) {
            if (maxSessions < 1) {
                if (error) {
                    *error = "max_sessions must be >= 1 (got " + std::to_string(maxSessions) + ")";
                }
                return false;
            }
            if (static_cast<size_t>(maxSessions) > kMaxSessionsHardLimit) {
                std::cerr << "abc80: warning: max_sessions " << maxSessions
                          << " exceeds upper bound " << kMaxSessionsHardLimit
                          << "; clamping to " << kMaxSessionsHardLimit << std::endl;
                _server.maxSessions = kMaxSessionsHardLimit;
            } else {
                _server.maxSessions = static_cast<size_t>(maxSessions);
            }
        }

        int grace = static_cast<int>(_server.sessionGraceSeconds);
        if (cfg->lookupValue("server.session_grace_seconds", grace)) {
            if (grace < 0) {
                if (error) *error = "session_grace_seconds cannot be negative";
                return false;
            }
            _server.sessionGraceSeconds = static_cast<unsigned>(grace);
        }
    }

    // Aimon settings
    if (cfg->exists("aimon")) {
        bool enabled = _aimon.enabled;
        if (cfg->lookupValue("aimon.enabled", enabled)) {
            _aimon.enabled = enabled;
        }

        std::string host;
        if (cfg->lookupValue("aimon.host", host)) {
            _aimon.host = host;
        }

        int port = _aimon.port;
        if (cfg->lookupValue("aimon.port", port)) {
            _aimon.port = port;
        }

        int reconnect = static_cast<int>(_aimon.reconnectIntervalSec);
        if (cfg->lookupValue("aimon.reconnect_interval_sec", reconnect)) {
            if (reconnect < 1) reconnect = 1;
            _aimon.reconnectIntervalSec = static_cast<unsigned>(reconnect);
        }
    }

    // AI Keypad settings
    if (cfg->exists("ai_keypad")) {
        int punchInterval = static_cast<int>(_aiKeypad.punchIntervalMs);
        if (cfg->lookupValue("ai_keypad.punch_interval_ms", punchInterval)) {
            if (punchInterval < 50) punchInterval = 50;
            _aiKeypad.punchIntervalMs = static_cast<unsigned>(punchInterval);
        }

        int holdDuration = static_cast<int>(_aiKeypad.holdDurationMs);
        if (cfg->lookupValue("ai_keypad.hold_duration_ms", holdDuration)) {
            if (holdDuration < 10) holdDuration = 10;
            _aiKeypad.holdDurationMs = static_cast<unsigned>(holdDuration);
        }
    }

    return true;
}

bool Abc80Config::load(const std::string &path, std::string *error)
{
    const std::string resolved = path.empty() ? defaultPath().string() : path;

    if (!std::filesystem::exists(resolved)) {
        // Auto-generate default configuration file if it does not exist
        if (!save(resolved, error)) {
            return false;
        }
    }

    libconfig::Config cfg;
    try {
        cfg.readFile(resolved.c_str());
    } catch (const libconfig::FileIOException &) {
        if (error) *error = "I/O error while reading config file: " + resolved;
        return false;
    } catch (const libconfig::ParseException &pex) {
        if (error) {
            std::ostringstream oss;
            oss << "Config parse error at " << pex.getFile() << ":" << pex.getLine()
                << " - " << pex.getError();
            *error = oss.str();
        }
        return false;
    } catch (const std::exception &ex) {
        if (error) *error = std::string("Config error: ") + ex.what();
        return false;
    }

    return parseAndValidate(&cfg, error);
}

bool Abc80Config::loadFromString(const std::string &content, std::string *error)
{
    libconfig::Config cfg;
    try {
        cfg.readString(content);
    } catch (const libconfig::ParseException &pex) {
        if (error) {
            std::ostringstream oss;
            oss << "Config string parse error at line " << pex.getLine()
                << " - " << pex.getError();
            *error = oss.str();
        }
        return false;
    } catch (const std::exception &ex) {
        if (error) *error = std::string("Config error: ") + ex.what();
        return false;
    }

    return parseAndValidate(&cfg, error);
}

std::string Abc80Config::toString() const
{
    std::ostringstream oss;
    oss << "# ABC-80 Microcomputer Emulator Configuration\n"
        << "# Generated by Abc80Config\n\n"
        << "server = {\n"
        << "    port = " << _server.port << ";\n"
        << "    web_root = \"" << _server.webRoot << "\";\n"
        << "    rom = \"" << _server.rom << "\";\n"
        << "    boot_mode = \"" << _server.bootMode << "\";\n"
        << "    boot_turbo = " << (_server.bootTurbo ? "true" : "false") << ";\n"
        << "    max_sessions = " << _server.maxSessions << ";\n"
        << "    session_grace_seconds = " << _server.sessionGraceSeconds << ";\n"
        << "};\n\n"
        << "aimon = {\n"
        << "    enabled = " << (_aimon.enabled ? "true" : "false") << ";\n"
        << "    host = \"" << _aimon.host << "\";\n"
        << "    port = " << _aimon.port << ";\n"
        << "    reconnect_interval_sec = " << _aimon.reconnectIntervalSec << ";\n"
        << "};\n\n"
        << "ai_keypad = {\n"
        << "    punch_interval_ms = " << _aiKeypad.punchIntervalMs << ";\n"
        << "    hold_duration_ms = " << _aiKeypad.holdDurationMs << ";\n"
        << "};\n";
    return oss.str();
}

bool Abc80Config::save(const std::string &path, std::string *error) const
{
    const std::string resolved = path.empty() ? defaultPath().string() : path;
    const std::filesystem::path p(resolved);

    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) {
            if (error) *error = "cannot create directory " + p.parent_path().string() + ": " + ec.message();
            return false;
        }
    }

    std::ofstream ofs(resolved, std::ios::trunc);
    if (!ofs.is_open()) {
        if (error) *error = "cannot open file for writing: " + resolved;
        return false;
    }

    ofs << toString();
    if (!ofs.good()) {
        if (error) *error = "write error writing config to: " + resolved;
        return false;
    }

    return true;
}

ServerConfig Abc80Config::toServerConfig(const std::string &root) const
{
    ServerConfig c;
    // Server unconditionally binds to "0.0.0.0" across all local network interfaces
    c.bind = "0.0.0.0";
    c.port = _server.port;
    c.webRoot = _server.webRoot;
    c.root = root;
    c.defaultRom = (_server.rom == "1993" || _server.rom == "vintage")
        ? RomId::Vintage1993
        : RomId::Monitor;
    c.coldBoot = (_server.bootMode == "cold");
    c.bootTurbo = _server.bootTurbo;
    c.createResident = true;
    c.aimonEnabled = _aimon.enabled;
    c.maxSessions = _server.maxSessions;
    c.sessionGraceSeconds = _server.sessionGraceSeconds;
    return c;
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
