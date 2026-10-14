/*
 * AimonGatewayClient.hxx
 *
 * Persistent TCP client connecting to aimon gateway (TCP port 3885)
 * for dynamic MCP tool registration and execution on the resident ABC-80 emulator.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef AIMON_GATEWAY_CLIENT_HXX
#define AIMON_GATEWAY_CLIENT_HXX

#include <abc80/Abc80WebServer.hxx>
#include <abc80/AiKeySequencer.hxx>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace abc80 {

struct AimonClientConfig {
    bool enabled{true};
    std::string host{"localhost"};
    int port{3885};
    unsigned reconnectIntervalSec{5};
    int webPort{8080};
};

class AimonGatewayClient {
public:
    AimonGatewayClient(Abc80WebServer &server, AiKeySequencer &sequencer,
                       const AimonClientConfig &config);
    ~AimonGatewayClient();
    AimonGatewayClient(const AimonGatewayClient &) = delete;
    AimonGatewayClient &operator=(const AimonGatewayClient &) = delete;

    bool start();
    void stop();
    bool isConnected() const noexcept;
    std::string handleToolCall(const std::string &name, const std::string &argumentsJson);

private:
    void runLoop();
    bool connectToGateway(int &sockFd);
    bool sendRegistration(int sockFd);
    void processMessages(int sockFd);

    Abc80WebServer &_server;
    AiKeySequencer &_sequencer;
    AimonClientConfig _config;

    std::atomic<bool> _running{false};
    std::atomic<bool> _connected{false};
    std::atomic<int> _currentSock{-1};
    std::thread _thread;
};

} // namespace abc80

#endif /* AIMON_GATEWAY_CLIENT_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
