/*
 * Abc80QbHost.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_QB_HOST_HXX
#define ABC80_QB_HOST_HXX

#include <abc80/Abc80Board.hxx>
#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace abc80 {

// CP437 Console Cell
struct ScreenCell {
    char ch = ' ';
    uint8_t attr = 0x07; // Light gray on black
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
    std::string getRowText(int row) const; // 1-based
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

    // Screen Layout rendering from vintage abc.bas
    void initVintageScreen(uint16_t currentAddress = 0x1000);
    void updateMemoryTable(uint16_t currentAddress);

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

    void stepServerUntilSync(uint8_t expectedSync, uint64_t maxTStates);
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
