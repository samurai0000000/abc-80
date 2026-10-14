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
#include <abc80/WebProtocol.hxx>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace abc80 {

struct ServerConfig {
    std::string bind{"127.0.0.1"};
    int port{8080};                     // 0 = kernel-chosen ephemeral port
    std::string webRoot{"web"};         // static files, relative to root
    std::string root{"."};              // repository root: web root and ROM paths resolve here
    RomId defaultRom{RomId::Monitor};
    bool bootTurbo{false};              // run a new session's boot unpaced to the ROM's steady state
    bool coldBoot{false};               // cold start (RAM cleared, no warm flag) instead of warm
    bool createResident{true};          // pre-create permanent resident emulator instance ("resident")
    bool aimonEnabled{true};            // register resident instance with aimon gateway
    bool aimonCliOverride{false};       // true when --aimon or --no-aimon was passed on CLI
    size_t maxSessions{8};
    unsigned sessionGraceSeconds{300};  // a detached board is kept this long
    unsigned sendTimeoutMs{200};        // socket write timeout; a stalled client never blocks others
    double litDutyMin{0.05};            // share of a frame a segment must be on to count as lit
    std::string tapesDir;               // empty = default ~/.config/abc-80/tapes
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

    // Resident session inspection and control
    bool isResidentRunning() const;
    bool isResidentPaused() const;
    bool isResidentAutoPaused() const;
    size_t residentSubscriberCount() const;

    // AI Punching and Clock Control for resident emulator
    void punchResidentKey(uint8_t keyCode, bool down);
    void resetResident();
    void broadcastAiPunch(const std::string &key, bool down, size_t remaining);
    void setResidentClockWakeForAi(bool wake);

    // Callbacks for emergency cancellation (e.g. Cancel AI Actions button in Web UI)
    using AiCancelCallback = std::function<void()>;
    void setOnAiCancel(AiCancelCallback cb);
    void cancelAiPunch();

    // Resident state inspection for aimon gateway tools
    bool readResidentCpu(TelemetryFrame &frame) const;
    std::vector<uint8_t> readResidentMemory(uint16_t addr, uint16_t count) const;
    std::vector<DisasmLine> disassembleResident(uint16_t addr, uint16_t count) const;
    bool readResidentDisplay(std::string &digits, uint8_t &epLed, uint8_t &haltLed) const;

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
