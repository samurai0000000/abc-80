/*
 * ServerRunner.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/ServerRunner.hxx>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <thread>

namespace abc80 {

namespace {

std::atomic<bool> g_shutdown{false};

extern "C" void onSignal(int)
{
    g_shutdown.store(true);  // async-signal-safe: a lock-free atomic store and nothing else
}

bool parseLong(const char *text, long lo, long hi, long &out)
{
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    const long v = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || v < lo || v > hi) {
        return false;
    }
    out = v;
    return true;
}

} // namespace

const char *serverUsage()
{
    return "ABC-80 web front panel server\n"
           "Copyright (C) 2026, Charles Chiou\n\n"
           "Usage: abc80_server [options]\n\n"
           "Options:\n"
           "  -h, --help           Show this help and exit\n"
           "      --port N         TCP port (default 8080; 0 picks a free port)\n"
           "      --bind ADDR      Address to listen on (default 127.0.0.1)\n"
           "      --web-dir DIR    Static files, relative to --root (default web)\n"
           "      --root DIR       Repository root for the web files and ROMs (default .)\n"
           "      --rom NAME       Default ROM for new sessions: monitor (default) or 1993\n"
           "      --boot MODE      warm (default) or cold start\n"
           "      --boot-turbo     Run each new board's boot unpaced to the ROM's steady state\n"
           "      --max-sessions N Boards alive at once (default 8)\n"
           "      --grace SECONDS  Keep a detached board this long (default 300)\n";
}

bool parseServerArgs(int argc, const char *const *argv, ServerConfig &cfg, std::string &err, bool &help)
{
    help = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](const char *name) -> const char * {
            if (i + 1 >= argc) {
                err = std::string("missing value for ") + name;
                return nullptr;
            }
            return argv[++i];
        };
        long n = 0;
        if (arg == "-h" || arg == "--help") {
            help = true;
        } else if (arg == "--port") {
            const char *v = value("--port");
            if (v == nullptr) return false;
            if (!parseLong(v, 0, 65535, n)) { err = "--port must be 0-65535"; return false; }
            cfg.port = static_cast<int>(n);
        } else if (arg == "--bind") {
            const char *v = value("--bind");
            if (v == nullptr) return false;
            cfg.bind = v;
        } else if (arg == "--web-dir") {
            const char *v = value("--web-dir");
            if (v == nullptr) return false;
            cfg.webRoot = v;
        } else if (arg == "--root") {
            const char *v = value("--root");
            if (v == nullptr) return false;
            cfg.root = v;
        } else if (arg == "--rom") {
            const char *v = value("--rom");
            if (v == nullptr) return false;
            if (!parseRomId(v, cfg.defaultRom)) { err = "--rom must be monitor or 1993"; return false; }
        } else if (arg == "--boot") {
            const char *v = value("--boot");
            if (v == nullptr) return false;
            const std::string mode = v;
            if (mode == "warm") cfg.coldBoot = false;
            else if (mode == "cold") cfg.coldBoot = true;
            else { err = "--boot must be warm or cold"; return false; }
        } else if (arg == "--boot-turbo") {
            cfg.bootTurbo = true;
        } else if (arg == "--max-sessions") {
            const char *v = value("--max-sessions");
            if (v == nullptr) return false;
            if (!parseLong(v, 1, 64, n)) { err = "--max-sessions must be 1-64"; return false; }
            cfg.maxSessions = static_cast<size_t>(n);
        } else if (arg == "--grace") {
            const char *v = value("--grace");
            if (v == nullptr) return false;
            if (!parseLong(v, 0, 86400, n)) { err = "--grace must be 0-86400 seconds"; return false; }
            cfg.sessionGraceSeconds = static_cast<unsigned>(n);
        } else {
            err = "unknown option: " + arg;
            return false;
        }
    }
    return true;
}

int runServerUntilSignalled(const ServerConfig &cfg)
{
    if (cfg.bind != "127.0.0.1" && cfg.bind != "localhost" && cfg.bind != "::1") {
        std::cerr << "warning: listening on " << cfg.bind
                  << "; the server has no authentication, anyone who can reach it can use it\n";
    }

    g_shutdown.store(false);
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    Abc80WebServer server(cfg);
    if (!server.start()) {
        std::cerr << "error: " << server.lastError() << "\n";
        return 1;
    }
    std::cout << "LISTENING " << server.port() << std::endl;  // flushed: test harnesses wait for this line

    while (!g_shutdown.load() && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    server.stop();
    return 0;
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
