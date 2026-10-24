/*
 * Abc80QbHost.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_QB_HOST_HXX
#define ABC80_QB_HOST_HXX

#include <abc80/Abc80Board.hxx>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <array>

namespace abc80 {

// CP437 Console Cell
struct ScreenCell {
    char ch = ' ';
    uint8_t attr = 0x07; // Light gray on black
};

// QuickBASIC Host Mode State Machine (matching vintage/abc.bas SUBs)
enum class QbMode : uint8_t {
    Disconnected = 0, // Waiting for F1 / InitialABC
    Neutral      = 1, // InitialABC done; waiting for Home (ADDs), End (Data), PgUp/PgDn, Run, Save, Load
    AddressInput = 2, // SUB NewAddress (Home / ADDs; blue cursors at row 8 cols 14..16, 30..32)
    DataInput    = 3, // SUB DataInput (End / Data; blue cursors at row 8 cols 34..36, 50..52)
    SaveFile     = 4, // SUB SaveProg step 1: line input for filename on row 19
    SaveRange    = 5, // SUB SaveProg step 2: 8 hex digits (Start# 4 digits, Finish# 4 digits) on row 20
    SaveDoneWait = 6, // SUB SaveProg step 3: "Press any key..." on row 20
    LoadFile     = 7  // SUB LoadProg: line input for filename on row 20
};

// PC Speaker Audio Event (SOUND freq, duration / PLAY mml$)
struct QbAudioEvent {
    std::string kind;        // "sound" | "play"
    double freqHz = 0.0;     // For "sound"
    double durationSec = 0.0; // For "sound"
    std::string mml;         // For "play" (e.g. "l32mlabcdfg", "l16mlfgfgfaba>d<")
};

// 80x25 Screen Buffer
class Cp437Screen {
public:
    static constexpr int WIDTH = 80;
    static constexpr int HEIGHT = 25;

    Cp437Screen();
    ~Cp437Screen() = default;

    void cls();
    void locate(int row, int col); // 1-based (row 1..25, col 1..80)
    void color(int fg, int bg = 0);
    void print(const std::string& str);
    void printAt(int row, int col, const std::string& str, int fg = -1, int bg = -1);

    ScreenCell getCell(int row, int col) const; // 1-based
    std::string getRowText(int row) const; // 1-based (ASCII-compatible for legacy tests)
    std::string getRowAttrHex(int row) const; // 1-based, 160 hex chars (80 byte attrs)
    std::string getFullText() const;

private:
    std::array<ScreenCell, WIDTH * HEIGHT> _buffer;
    int _cursorRow; // 0-based
    int _cursorCol; // 0-based
    uint8_t _currentAttr;
};

// QuickBASIC Host Controller
class Abc80QbHost {
public:
    explicit Abc80QbHost(Abc80Board& board);
    ~Abc80QbHost() = default;

    Abc80Board& getBoard() noexcept { return _board; }
    const Abc80Board& getBoard() const noexcept { return _board; }
    Cp437Screen& getScreen() noexcept { return _screen; }
    const Cp437Screen& getScreen() const noexcept { return _screen; }

    // Connection & State Machine Accessors
    bool isConnected() const noexcept { return _connected; }
    bool isLinkServerPolling() const noexcept;
    QbMode getMode() const noexcept { return _mode; }
    std::string getModeName() const;
    uint16_t getCurrentAddress() const noexcept { return _currentAddress; }
    void setCurrentAddress(uint16_t addr) noexcept { _currentAddress = addr; }
    int getHighlightedKey() const noexcept { return _highlightedKey; }
    const std::string& getStatusMessage() const noexcept { return _statusMessage; }
    const std::string& getProgramsDir() const noexcept { return _programsDir; }
    void setProgramsDir(const std::string& dir) { _programsDir = dir; }
    static std::filesystem::path defaultExercisesDir();

    // Disconnect / reset host state (e.g. when board resets or switches ROM)
    void disconnect();

    // Audio event queue (drained when building WebSocket qb_state)
    std::vector<QbAudioEvent> consumeAudioEvents();
    const std::vector<QbAudioEvent>& peekAudioEvents() const noexcept { return _audioEvents; }

    // Screen Layout rendering from vintage abc.bas
    void initVintageScreen(uint16_t currentAddress = 0x1000);
    void updateMemoryTable(uint16_t currentAddress);
    void colourKeyPressed(int whichKey);

    // Interactive key dispatch (matches vintage/abc.bas INKEY$ loops)
    bool pressKey(const std::string& keyName);

    // Z80-side Insert/Delete block shifts (0x1700, 0x1720, 0x1740, 0x1760)
    bool insertByte(uint16_t address, uint64_t maxTStates = 2000000ULL);
    bool deleteByte(uint16_t address, uint64_t maxTStates = 2000000ULL);

    // QuickBASIC .abc File Codec & Persistence (SaveProg / LoadProg)
    static bool sanitizeAbcFilename(const std::string& rawName, std::string& cleanOut);
    static std::string formatAbcFile(uint16_t startAddr, const std::vector<uint8_t>& data);
    static bool parseAbcFile(const std::string& content, uint16_t& startAddrOut, std::vector<uint8_t>& dataOut);

    bool saveProgramAbc(const std::string& filename, uint16_t startAddr, uint16_t endAddr, std::string& errOut);
    bool loadProgramAbc(const std::string& filename, uint16_t& loadedStartOut, size_t& loadedBytesOut, std::string& errOut);
    std::vector<std::string> listPrograms() const;

    // Audio Synthesis (SOUND & PLAY MML)
    static void generateSound(double freqHz, double durationSec, std::vector<int16_t>& pcmOut, uint32_t sampleRate = 44100);
    static void generatePlayMml(const std::string& mml, std::vector<int16_t>& pcmOut, uint32_t sampleRate = 44100);

    // Virtual PC-5523 ISA Lab Card Protocol (0x304 - 0x307)
    // Synchronous handshakes against ABC-80 PC-Link server (0x0B00)
    bool initialAbc();
    bool outAbc(uint16_t address, uint8_t byteOut, uint64_t maxTStatesPerStep = 200000ULL);
    bool inAbc(uint16_t address, uint8_t& byteIn, uint64_t maxTStatesPerStep = 200000ULL);
    bool runAbc(uint16_t address, uint64_t maxTStatesPerStep = 200000ULL);

private:
    Abc80Board& _board;
    Cp437Screen _screen;
    bool _connected = false;
    QbMode _mode = QbMode::Disconnected;
    uint16_t _currentAddress = 0x1000;
    int _highlightedKey = 0;
    std::string _statusMessage = "Standby - Press F1 or Connect to link ABC-80 (0B00h)";
    std::string _programsDir;
    std::string _promptBuffer;
    std::string _pendingSaveFilename;
    int _saveHexDigitsEntered = 0;
    uint16_t _saveStartAddr = 0;
    uint16_t _saveFinishAddr = 0;
    std::vector<QbAudioEvent> _audioEvents;

    bool stepServerUntilSync(uint8_t expectedSync, uint64_t maxTStates);
    bool stepServerUntilPollLoop(uint64_t maxTStates);
    bool uploadHexRoutine(uint16_t destAddr, const char* hexStr);
    void renderModeCursors();
    void renderPromptArea();
    void clearPromptArea();
    void queueSound(double freqHz, double durationSec);
    void queuePlay(const std::string& mml);
};

} // namespace abc80

#endif // ABC80_QB_HOST_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
