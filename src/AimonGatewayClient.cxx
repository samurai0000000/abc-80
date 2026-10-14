/*
 * AimonGatewayClient.cxx
 *
 * Persistent TCP client connecting to aimon gateway (TCP port 3885)
 * for dynamic MCP tool registration and execution on the resident ABC-80 emulator.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/AimonGatewayClient.hxx>
#include <abc80/WebProtocol.hxx>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace abc80 {

using json = nlohmann::json;

namespace {

bool sendLine(int sockFd, const std::string &line)
{
    if (sockFd < 0) return false;
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n') {
        msg.push_back('\n');
    }
    size_t total = 0;
    while (total < msg.size()) {
        ssize_t n = ::write(sockFd, msg.data() + total, msg.size() - total);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

json buildToolsArray()
{
    return json::array({
        {
            {"name", "abc80_read_cpu"},
            {"description", "Read all Z80 CPU registers (PC, SP, AF, BC, DE, HL, IX, IY), flags, execution cycle counter, and paused state from the resident ABC-80 emulator."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        },
        {
            {"name", "abc80_read_memory"},
            {"description", "Read a block of bytes from the resident ABC-80 memory bus (RAM, ROM, or memory-mapped I/O)."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"addr", {{"type", "integer"}, {"description", "Start memory address (0-65535, e.g. 0x0000 for ROM or 0x1000 for User RAM)"}}},
                    {"count", {{"type", "integer"}, {"description", "Number of bytes to read (1-256)"}}}
                }},
                {"required", json::array({"addr", "count"})}
            }}
        },
        {
            {"name", "abc80_read_display"},
            {"description", "Read the current 6-digit seven-segment display digits, raw segment masks, and LED states (EP, HALT, Audio) from the resident ABC-80."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        },
        {
            {"name", "abc80_disassemble"},
            {"description", "Disassemble Z80 machine code into assembly instructions starting from address on the resident ABC-80."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"addr", {{"type", "integer"}, {"description", "Starting address for disassembly (0-65535)"}}},
                    {"count", {{"type", "integer"}, {"description", "Number of instructions to disassemble (1-64)"}}}
                }},
                {"required", json::array({"addr", "count"})}
            }}
        },
        {
            {"name", "abc80_punch_keys"},
            {"description", "Punches a sequence of hex keypad and control keys into the resident ABC-80 emulator at authentic human cadence (1 key per 0.5s), animated live on the web front panel. Valid keys: 0-9, A-F, +, -, GO, DATA, ADRS, TO TAPE, FROM TAPE, RST."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"keys", {
                        {"type", "array"},
                        {"items", {{"type", "string"}}},
                        {"description", "Ordered list of key names to punch sequentially, e.g. [\"ADRS\", \"1\", \"0\", \"0\", \"0\", \"DATA\", \"3\", \"E\", \"+\"]"}
                    }},
                    {"sync", {
                        {"type", "boolean"},
                        {"description", "If true (default), blocks until key sequence finishes punching. If false, returns immediately after enqueuing."}
                    }}
                }},
                {"required", json::array({"keys"})}
            }}
        },
        {
            {"name", "abc80_cancel_punching"},
            {"description", "Cancels any in-progress AI keypad sequence on the resident ABC-80, clears pending keys, and releases held keys."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        },
        {
            {"name", "abc80_get_punch_status"},
            {"description", "Get current status of AI keypad sequencing on the resident ABC-80 (is_punching, queue_depth, current_key)."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        }
    });
}

} // namespace

AimonGatewayClient::AimonGatewayClient(Abc80WebServer &server, AiKeySequencer &sequencer,
                                       const AimonClientConfig &config)
    : _server(server),
      _sequencer(sequencer),
      _config(config)
{
}

AimonGatewayClient::~AimonGatewayClient()
{
    stop();
}

bool AimonGatewayClient::start()
{
    if (!_config.enabled) {
        return true;
    }
    if (_running.load()) {
        return true;
    }
    _running.store(true);
    _thread = std::thread(&AimonGatewayClient::runLoop, this);
    return true;
}

void AimonGatewayClient::stop()
{
    if (!_running.exchange(false)) {
        return;
    }
    const bool wasConnected = _connected.exchange(false);
    int sock = _currentSock.exchange(-1);
    if (sock >= 0) {
        if (wasConnected) {
            sendUnregistration(sock);
            ::shutdown(sock, SHUT_WR);
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 50000;
            if (::select(sock + 1, &rfds, nullptr, nullptr, &tv) > 0) {
                char drain[512];
                (void)::read(sock, drain, sizeof(drain));
            }
        }
        ::shutdown(sock, SHUT_RDWR);
        ::close(sock);
    }
    if (_thread.joinable()) {
        _thread.join();
    }
}

bool AimonGatewayClient::isConnected() const noexcept
{
    return _connected.load();
}

bool AimonGatewayClient::connectToGateway(int &sockFd)
{
    sockFd = -1;
    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string portStr = std::to_string(_config.port);
    int err = ::getaddrinfo(_config.host.c_str(), portStr.c_str(), &hints, &res);
    if (err != 0 || !res) {
        return false;
    }

    for (struct addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
        int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        int yes = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

        if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            sockFd = fd;
            break;
        }
        ::close(fd);
    }
    ::freeaddrinfo(res);

    if (sockFd < 0) {
        return false;
    }

    _currentSock.store(sockFd);
    return true;
}

bool AimonGatewayClient::sendRegistration(int sockFd)
{
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "gateway/register"},
        {"params", {
            {"subsystem", "abc80"},
            {"display_name", "ABC-80 Z80 Trainer"},
            {"short_name", "abc80"},
            {"priority", 40},
            {"web_port", _config.webPort},
            {"web_path", "/"},
            {"tools", buildToolsArray()}
        }}
    };
    return sendLine(sockFd, req.dump());
}

bool AimonGatewayClient::sendUnregistration(int sockFd)
{
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "gateway/unregister"},
        {"params", {
            {"subsystem", "abc80"},
            {"web_port", _config.webPort}
        }}
    };
    return sendLine(sockFd, req.dump());
}

std::string AimonGatewayClient::handleToolCall(const std::string &name, const std::string &argumentsJson)
{
    json args = json::object();
    try {
        if (!argumentsJson.empty()) {
            args = json::parse(argumentsJson);
        }
    } catch (...) {}

    if (name == "abc80_read_cpu") {
        TelemetryFrame frame;
        if (!_server.readResidentCpu(frame)) {
            return "{\"error\": \"Resident emulator not initialized\"}";
        }
        const uint8_t f = static_cast<uint8_t>(frame.af & 0xFF);
        json res = {
            {"rom", frame.rom},
            {"cycles", frame.cycles},
            {"paused", frame.paused},
            {"autoPaused", _server.isResidentAutoPaused()},
            {"registers", {
                {"pc", frame.pc},
                {"sp", frame.sp},
                {"af", frame.af},
                {"bc", frame.bc},
                {"de", frame.de},
                {"hl", frame.hl},
                {"ix", frame.ix},
                {"iy", frame.iy}
            }},
            {"flags", {
                {"s", (f & 0x80) != 0},
                {"z", (f & 0x40) != 0},
                {"yf", (f & 0x20) != 0},
                {"h", (f & 0x10) != 0},
                {"xf", (f & 0x08) != 0},
                {"pv", (f & 0x04) != 0},
                {"n", (f & 0x02) != 0},
                {"c", (f & 0x01) != 0}
            }}
        };
        return res.dump(2);
    }

    if (name == "abc80_read_memory") {
        uint16_t addr = static_cast<uint16_t>(args.value("addr", 0));
        uint16_t count = static_cast<uint16_t>(args.value("count", 16));
        if (count > 256) count = 256;
        std::vector<uint8_t> data = _server.readResidentMemory(addr, count);
        std::string hexDump;
        char buf[16];
        for (size_t i = 0; i < data.size(); ++i) {
            if (i % 16 == 0) {
                if (i > 0) {
                    hexDump.push_back('\n');
                }
                std::snprintf(buf, sizeof(buf), "%04X:", static_cast<unsigned>((addr + i) & 0xFFFF));
                hexDump.append(buf);
            }
            std::snprintf(buf, sizeof(buf), " %02X", static_cast<unsigned>(data[i]));
            hexDump.append(buf);
        }
        json res = {
            {"addr", addr},
            {"count", data.size()},
            {"bytes", data},
            {"hex_dump", hexDump}
        };
        return res.dump(2);
    }

    if (name == "abc80_read_display") {
        TelemetryFrame frame;
        if (!_server.readResidentCpu(frame)) {
            return "{\"error\": \"Resident emulator not initialized\"}";
        }
        const std::string digits(frame.displayDigits.begin(), frame.displayDigits.end());
        json masks = json::array();
        for (uint8_t m : frame.displayMasks) {
            masks.push_back(m);
        }
        json res = {
            {"digits", digits},
            {"display_masks", masks},
            {"leds", {
                {"ep", frame.ep},
                {"halt", frame.halt},
                {"speaker", frame.speakerLed}
            }}
        };
        return res.dump(2);
    }

    if (name == "abc80_disassemble") {
        uint16_t addr = static_cast<uint16_t>(args.value("addr", 0));
        uint16_t count = static_cast<uint16_t>(args.value("count", 10));
        if (count > 64) count = 64;
        std::vector<DisasmLine> lines = _server.disassembleResident(addr, count);
        json linesArr = json::array();
        for (const auto &l : lines) {
            linesArr.push_back({
                {"addr", l.addr},
                {"bytes", l.bytes},
                {"mnemonic", l.mnemonic}
            });
        }
        json res = {
            {"addr", addr},
            {"count", lines.size()},
            {"instructions", linesArr}
        };
        return res.dump(2);
    }

    if (name == "abc80_punch_keys") {
        std::vector<std::string> keys;
        if (args.contains("keys") && args["keys"].is_array()) {
            for (const auto &k : args["keys"]) {
                if (k.is_string()) keys.push_back(k.get<std::string>());
            }
        }
        bool sync = args.value("sync", true);
        if (keys.empty()) {
            return "{\"error\": \"keys array cannot be empty\"}";
        }

        // Wake resident CPU clock so keypresses can be sampled
        _server.setResidentClockWakeForAi(true);

        size_t count = 0;
        std::string err;
        if (!_sequencer.enqueue(keys, count, err)) {
            if (!_sequencer.isPunching()) {
                _server.setResidentClockWakeForAi(false);
            }
            return "{\"error\": \"" + err + "\"}";
        }

        const double estDuration = static_cast<double>(count) * 0.5;
        char hintBuf[160];
        std::snprintf(hintBuf, sizeof(hintBuf),
                      "Punching %zu keys will take ~%.1fs (0.5s/key). Keypad is locked to human input during this time.",
                      count, estDuration);
        const std::string hint(hintBuf);

        if (sync) {
            const unsigned timeoutMs = static_cast<unsigned>(count * 600 + 3000);
            const bool finished = _sequencer.waitForCompletion(std::chrono::milliseconds(timeoutMs));
            const std::string status = !finished ? "timeout"
                                                 : (_sequencer.wasCancelled() ? "cancelled" : "completed");
            json res = {
                {"status", status},
                {"enqueued_count", count},
                {"punched_count", count},
                {"estimated_duration_sec", estDuration},
                {"duration_sec", estDuration},
                {"hint", hint}
            };
            return res.dump(2);
        } else {
            json res = {
                {"status", "enqueued"},
                {"enqueued_count", count},
                {"estimated_duration_sec", estDuration},
                {"hint", hint}
            };
            return res.dump(2);
        }
    }

    if (name == "abc80_cancel_punching") {
        size_t cancelled = _sequencer.cancel();
        _server.setResidentClockWakeForAi(false);
        json res = {
            {"status", "cancelled"},
            {"discarded_keys", cancelled}
        };
        return res.dump(2);
    }

    if (name == "abc80_get_punch_status") {
        const size_t depth = _sequencer.queueDepth();
        json res = {
            {"is_punching", _sequencer.isPunching()},
            {"queue_depth", depth},
            {"remaining", depth},
            {"current_key", _sequencer.currentKey()}
        };
        return res.dump(2);
    }

    return "{\"error\": \"unknown tool: " + name + "\"}";
}

void AimonGatewayClient::processMessages(int sockFd)
{
    std::string buffer;
    char chunk[4096];

    while (_running.load()) {
        ssize_t n = ::read(sockFd, chunk, sizeof(chunk));
        if (n <= 0) {
            break;
        }
        buffer.append(chunk, static_cast<size_t>(n));

        size_t pos = 0;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) continue;

            json j;
            try {
                j = json::parse(line);
            } catch (...) {
                continue;
            }

            if (j.contains("method") && j["method"].is_string()) {
                std::string method = j["method"].get<std::string>();
                json id = j.value("id", json(1));

                if (method == "ping") {
                    json resp = {
                        {"jsonrpc", "2.0"},
                        {"id", id},
                        {"result", json::object()}
                    };
                    sendLine(sockFd, resp.dump());
                } else if (method == "tools/call") {
                    json params = j.value("params", json::object());
                    std::string toolName = params.value("name", "");
                    json args = params.value("arguments", json::object());

                    std::string output = handleToolCall(toolName, args.dump());
                    json resp = {
                        {"jsonrpc", "2.0"},
                        {"id", id},
                        {"result", {
                            {"content", json::array({
                                {{"type", "text"}, {"text", output}}
                            })}
                        }}
                    };
                    sendLine(sockFd, resp.dump());
                }
            }
        }
    }
}

void AimonGatewayClient::runLoop()
{
    while (_running.load()) {
        int sockFd = -1;
        if (!connectToGateway(sockFd)) {
            _connected.store(false);
            for (unsigned i = 0; i < _config.reconnectIntervalSec && _running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }

        if (!sendRegistration(sockFd)) {
            ::close(sockFd);
            _currentSock.store(-1);
            _connected.store(false);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        _connected.store(true);
        processMessages(sockFd);

        _connected.store(false);
        int old = _currentSock.exchange(-1);
        if (old >= 0) {
            ::close(old);
        }

        for (unsigned i = 0; i < _config.reconnectIntervalSec && _running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
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
