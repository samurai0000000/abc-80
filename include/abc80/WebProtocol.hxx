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

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80QbHost.hxx>
#include <abc80/Abc80Rom.hxx>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace abc80 {

constexpr size_t kMaxClientMessageBytes = 4096;

struct ClientMessage {
    enum class Type {
        Invalid,
        Hello,
        SetRom,
        Key,
        Reset,
        Run,
        Pause,
        Step,
        ReadMem,
        ReadDisasm,
        CancelAiPunch,
        TapeList,
        TapeSelect,
        TapeAnnotate,
        TapeTransport,
        QbConnect,
        QbKey,
        QbSave,
        QbLoad,
        QbStatus
    };

    Type type{Type::Invalid};
    std::string token;      // Hello
    bool hasRom{false};     // Hello, SetRom
    RomId rom{RomId::Monitor};
    uint8_t keyCode{0};     // Key
    bool down{false};       // Key
    uint64_t seq{0};        // Key (optional)
    uint16_t memAddr{0};    // ReadMem, ReadDisasm
    uint16_t memCount{0};   // ReadMem, ReadDisasm
    uint16_t tapeFileId{0}; // TapeSelect, TapeAnnotate, TapeTransport
    bool hasTapeFileId{false};
    std::string tapeAnnotation; // TapeAnnotate, TapeTransport
    bool hasTapeAnnotation{false};
    std::string tapeAction; // TapeTransport: "play" | "record" | "rewind" | "stop"
    std::string qbKey;      // QbKey
    std::string qbFilename; // QbSave, QbLoad
    uint16_t qbStartAddr{0}; // QbSave
    uint16_t qbEndAddr{0};   // QbSave
    std::string error;      // Invalid: why
};

// Parses one client message. Never throws; anything unacceptable (including a message over
// kMaxClientMessageBytes) yields Type::Invalid with a reason.
ClientMessage parseClientMessage(const std::string &text);

// "0".."9", "A".."F" (either case), "+", "-", "GO", "DATA", "ADRS", "TO TAPE", "FROM TAPE",
// plus aliases "RUN", "ADDR", "STEP", "BP".
// Returns false and leaves code untouched for anything else (including "RST").
bool keyCodeFromName(const std::string &name, uint8_t &code) noexcept;

// 8 to 64 characters of [A-Za-z0-9_-].
bool validToken(const std::string &token) noexcept;

struct TelemetryFrame {
    uint64_t seq{0};
    std::string rom;
    uint64_t cycles{0};
    bool paused{false};
    std::vector<uint8_t> stackBytes;     // 32 bytes starting at SP (paused/stepping only)
    uint16_t stackBaseAddr{0};           // start address of stackBytes (== sp)
    std::array<uint8_t, 6> displayMasks{};
    std::array<char, 6> displayDigits{};
    bool ep{false};
    bool halt{false};
    double speakerLed{0.0};
    bool speakerLevel{false};
    std::vector<uint32_t> speakerEdges;
    bool speakerOverflow{false};
    uint16_t pc{0}, sp{0}, af{0}, bc{0}, de{0}, hl{0}, ix{0}, iy{0};
    std::string tapeState{"stopped"};
    std::string tapeFileId{"0001"};
    std::string tapeAnnotation;
    uint32_t tapeCounter{0};
    double tapeProgress{0.0};
    uint32_t tapeToneHz{0};
};

std::string buildTelemetryJson(const TelemetryFrame &frame);
std::string buildErrorJson(const std::string &reason);
std::string buildMemDataJson(uint16_t addr, const std::vector<uint8_t> &data);
std::string buildDisasmJson(uint16_t addr, const std::vector<DisasmLine> &lines);
std::string buildAiPunchJson(const std::string &key, bool down, size_t remaining);
std::string buildTapeListJson(const std::vector<TapeFileInfo> &tapes);
std::string buildQbStateJson(Abc80QbHost &qbHost);

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
