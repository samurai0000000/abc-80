/*
 * Abc80WebServer.hxx
 *
 * HTTP and WebSocket server for the browser front panel (plan Section 2.4). One WebSocket
 * session owns one emulated board; boards are stepped at 60 Hz only while a tab is attached.
 * httplib and nlohmann stay out of this header (pImpl).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_WEB_SERVER_HXX
#define ABC80_WEB_SERVER_HXX

#include <abc80/Abc80Rom.hxx>
#include <cstddef>
#include <memory>
#include <string>

namespace abc80 {

struct ServerConfig {
    std::string bind{"127.0.0.1"};
    int port{8080};                     // 0 = kernel-chosen ephemeral port
    std::string webRoot{"web"};         // static files, relative to root
    std::string root{"."};              // repository root: web root and ROM paths resolve here
    RomId defaultRom{RomId::Monitor};
    bool bootTurbo{false};              // run a new session's boot unpaced to the ROM's steady state
    bool coldBoot{false};               // cold start (RAM cleared, no warm flag) instead of warm
    size_t maxSessions{8};
    unsigned sessionGraceSeconds{300};  // a detached board is kept this long
    unsigned sendTimeoutMs{200};        // socket write timeout; a stalled client never blocks others
    double litDutyMin{0.05};            // share of a frame a segment must be on to count as lit
};

class Abc80WebServer {
public:
    explicit Abc80WebServer(const ServerConfig &config = ServerConfig());
    ~Abc80WebServer();
    Abc80WebServer(const Abc80WebServer &) = delete;
    Abc80WebServer &operator=(const Abc80WebServer &) = delete;

    // Binds, then serves on background threads. Returns false (see lastError()) if the web root
    // is missing or the port cannot be bound. Not blocking.
    bool start();

    // Closes every connection and joins all threads. Safe to call twice.
    void stop();

    bool isRunning() const noexcept;
    int port() const noexcept;          // the bound port (useful with port 0)
    size_t sessionCount() const;        // boards alive (attached or within the grace period)
    size_t attachedCount() const;       // sessions with a tab attached
    std::string lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace abc80

#endif /* ABC80_WEB_SERVER_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
