/*
 * WebProtocol.hxx
 *
 * Wire protocol between the browser page and the server (plan Section 2.4.6): parsing of
 * untrusted client messages and construction of telemetry and error frames. No sockets here.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_WEB_PROTOCOL_HXX
#define ABC80_WEB_PROTOCOL_HXX

#include <abc80/Abc80Rom.hxx>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace abc80 {

constexpr size_t kMaxClientMessageBytes = 4096;

struct ClientMessage {
    enum class Type { Invalid, Hello, SetRom, Key, Reset };

    Type type{Type::Invalid};
    std::string token;      // Hello
    bool hasRom{false};     // Hello, SetRom
    RomId rom{RomId::Monitor};
    uint8_t keyCode{0};     // Key
    bool down{false};       // Key
    uint64_t seq{0};        // Key (optional)
    std::string error;      // Invalid: why
};

// Parses one client message. Never throws; anything unacceptable (including a message over
// kMaxClientMessageBytes) yields Type::Invalid with a reason.
ClientMessage parseClientMessage(const std::string &text);

// "0".."9", "A".."F" (either case), "+", "-", "GO", "DATA", "ADRS", "TO TAPE", "FROM TAPE".
// Returns false and leaves code untouched for anything else (including "RST").
bool keyCodeFromName(const std::string &name, uint8_t &code) noexcept;

// 8 to 64 characters of [A-Za-z0-9_-].
bool validToken(const std::string &token) noexcept;

struct TelemetryFrame {
    uint64_t seq{0};
    std::string rom;
    uint64_t cycles{0};
    std::array<uint8_t, 6> displayMasks{};
    std::array<char, 6> displayDigits{};
    bool ep{false};
    bool halt{false};
    double speakerLed{0.0};
    bool speakerLevel{false};
    std::vector<uint32_t> speakerEdges;
    bool speakerOverflow{false};
    uint16_t pc{0}, sp{0}, af{0}, bc{0}, de{0}, hl{0};
};

std::string buildTelemetryJson(const TelemetryFrame &frame);
std::string buildErrorJson(const std::string &reason);

} // namespace abc80

#endif /* ABC80_WEB_PROTOCOL_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
