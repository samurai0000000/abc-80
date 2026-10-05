/*
 * Abc80QbHost.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80QbHost.hxx>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace abc80 {

static constexpr double TWO_PI = 6.28318530717958647692;

Cp437Screen::Cp437Screen()
    : _cursorRow(0)
    , _cursorCol(0)
    , _currentAttr(0x07)
{
    cls();
}

void Cp437Screen::cls()
{
    ScreenCell blank;
    blank.ch = ' ';
    blank.attr = _currentAttr;
    _buffer.fill(blank);
    _cursorRow = 0;
    _cursorCol = 0;
}

void Cp437Screen::locate(int row, int col)
{
    _cursorRow = std::clamp(row - 1, 0, HEIGHT - 1);
    _cursorCol = std::clamp(col - 1, 0, WIDTH - 1);
}

void Cp437Screen::color(int fg, int bg)
{
    _currentAttr = static_cast<uint8_t>(((bg & 0x07) << 4) | (fg & 0x0F));
}

void Cp437Screen::print(const std::string& str)
{
    for (char c : str) {
        if (c == '\r') {
            _cursorCol = 0;
        } else if (c == '\n') {
            _cursorCol = 0;
            if (_cursorRow < HEIGHT - 1) {
                _cursorRow++;
            }
        } else {
            if (_cursorRow < HEIGHT && _cursorCol < WIDTH) {
                int idx = _cursorRow * WIDTH + _cursorCol;
                _buffer[idx].ch = c;
                _buffer[idx].attr = _currentAttr;
            }
            _cursorCol++;
            if (_cursorCol >= WIDTH) {
                _cursorCol = 0;
                if (_cursorRow < HEIGHT - 1) {
                    _cursorRow++;
                }
            }
        }
    }
}

void Cp437Screen::printAt(int row, int col, const std::string& str, int fg, int bg)
{
    int savedRow = _cursorRow;
    int savedCol = _cursorCol;
    uint8_t savedAttr = _currentAttr;

    if (fg >= 0) {
        color(fg, (bg >= 0) ? bg : (_currentAttr >> 4));
    }

    locate(row, col);
    print(str);

    _cursorRow = savedRow;
    _cursorCol = savedCol;
    _currentAttr = savedAttr;
}

ScreenCell Cp437Screen::getCell(int row, int col) const
{
    int r = std::clamp(row - 1, 0, HEIGHT - 1);
    int c = std::clamp(col - 1, 0, WIDTH - 1);
    return _buffer[r * WIDTH + c];
}

std::string Cp437Screen::getRowText(int row) const
{
    int r = std::clamp(row - 1, 0, HEIGHT - 1);
    std::string line;
    line.reserve(WIDTH);
    for (int c = 0; c < WIDTH; ++c) {
        line.push_back(_buffer[r * WIDTH + c].ch);
    }
    return line;
}

std::string Cp437Screen::getFullText() const
{
    std::string full;
    full.reserve(WIDTH * HEIGHT + HEIGHT);
    for (int r = 1; r <= HEIGHT; ++r) {
        full += getRowText(r);
        if (r < HEIGHT) {
            full += "\n";
        }
    }
    return full;
}

// ============================================================================
// Abc80QbHost
// ============================================================================

Abc80QbHost::Abc80QbHost(Abc80Board& board)
    : _board(board)
    , _screen()
{
}

void Abc80QbHost::initVintageScreen(uint16_t currentAddress)
{
    _screen.cls();

    // Top Title (Rows 1-5, Color 13)
    _screen.printAt(1, 10, "  *   *    *          *     * ", 13, 0);
    _screen.printAt(2, 10, "*  *  *   *  *    *        *    *   *    *", 13, 0);
    _screen.printAt(3, 10, "*  *   *        *    *    *    *", 13, 0);
    _screen.printAt(4, 10, "*  *  *   *  *    *        *    *   *    *", 13, 0);
    _screen.printAt(5, 10, "*  *  *    *          *     * ", 13, 0);

    // Memory Table Frame (Rows 7-17, Color 7)
    _screen.printAt(7, 12,  "+---------------------+---------------------+", 7, 0);
    _screen.printAt(8, 12,  "|     Address         |        Byte         |", 7, 0);
    _screen.printAt(9, 12,  "+---------------------+---------------------+", 7, 0);
    for (int r = 10; r <= 16; ++r) {
        if (r == 13) {
            _screen.printAt(r, 12, "|*                   *|*                   *|", 14, 0);
        } else {
            _screen.printAt(r, 12, "|                     |                     |", 7, 0);
        }
    }
    _screen.printAt(17, 12, "+---------------------+---------------------+", 7, 0);

    // Right-side Function Key Box (Rows 11-18, Col 57, Color 7)
    _screen.printAt(11, 57, "+-------------------+", 7, 0);
    _screen.printAt(12, 57, "| INS  | ADDs |  -  |", 7, 0);
    _screen.printAt(13, 57, "+------+------+-----+", 7, 0);
    _screen.printAt(14, 57, "| DEL  | Data |  +  |", 7, 0);
    _screen.printAt(15, 57, "+-------------------+", 7, 0);
    _screen.printAt(16, 57, "+-------------------+", 7, 0);
    _screen.printAt(17, 57, "|   ERROR  |   RUN  |", 7, 0);
    _screen.printAt(18, 57, "+-------------------+", 7, 0);

    // Hex Digit Keys (Rows 21-23, Col 8, Color 7)
    _screen.printAt(21, 8, "+---------------------------------------------------------------+", 7, 0);
    _screen.printAt(22, 8, "| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |", 7, 0);
    _screen.printAt(23, 8, "+---------------------------------------------------------------+", 7, 0);

    // Function Key Shortcuts (Row 24, Col 8, Color 15)
    _screen.printAt(24, 8, "Shift+F5 = Run, Shift+F9 = Save, Shift+F10 = Load, Shift+Esc = Quit", 15, 0);

    // Footer Attribution (Row 25, Col 10, Color 12)
    _screen.printAt(25, 10, "PC/AT <-> ABC-80  LINK via 8255... Designed By  Charles Chiou", 12, 0);

    updateMemoryTable(currentAddress);
}

void Abc80QbHost::updateMemoryTable(uint16_t currentAddress)
{
    for (int a = 1; a <= 7; ++a) {
        uint16_t addr = static_cast<uint16_t>(currentAddress - 4 + a);
        uint8_t val = _board.getBus().readByte(addr);

        int row = 9 + a;
        int colorFg = (a == 4) ? 14 : 7; // Yellow for cursor line 13

        std::ostringstream addrSs;
        addrSs << std::hex << std::uppercase << std::setfill('0');
        addrSs << std::setw(1) << ((addr >> 12) & 0xF) << " "
               << std::setw(1) << ((addr >> 8) & 0xF) << " "
               << std::setw(1) << ((addr >> 4) & 0xF) << " "
               << std::setw(1) << (addr & 0xF);

        std::ostringstream valSs;
        valSs << std::hex << std::uppercase << std::setfill('0');
        valSs << std::setw(2) << static_cast<int>(val);

        _screen.printAt(row, 18, addrSs.str(), colorFg, 0);
        _screen.printAt(row, 40, valSs.str(), colorFg, 0);
    }
}

void Abc80QbHost::generateSound(double freqHz, double durationSec, std::vector<int16_t>& pcmOut, uint32_t sampleRate)
{
    const uint32_t numSamples = static_cast<uint32_t>(std::round(durationSec * static_cast<double>(sampleRate)));
    const double phaseInc = TWO_PI * freqHz / static_cast<double>(sampleRate);
    double phase = 0.0;

    pcmOut.reserve(pcmOut.size() + numSamples);
    for (uint32_t i = 0; i < numSamples; ++i) {
        double val = std::sin(phase) * 24000.0;
        pcmOut.push_back(static_cast<int16_t>(std::round(val)));
        phase += phaseInc;
        if (phase >= TWO_PI) {
            phase -= TWO_PI;
        }
    }
}

void Abc80QbHost::generatePlayMml(const std::string& mml, std::vector<int16_t>& pcmOut, uint32_t sampleRate)
{
    int octave = 4;
    int defaultLength = 4; // quarter note
    int tempo = 120; // BPM
    char musicStyle = 'N'; // Normal (7/8 sound, 1/8 silence), 'L' Legato (full sound)

    size_t i = 0;
    while (i < mml.size()) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(mml[i++])));

        if (c == ' ' || c == '\t') {
            continue;
        } else if (c == 'O' && i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
            octave = mml[i++] - '0';
        } else if (c == '>') {
            if (octave < 7) octave++;
        } else if (c == '<') {
            if (octave > 1) octave--;
        } else if (c == 'L') {
            int len = 0;
            while (i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
                len = len * 10 + (mml[i++] - '0');
            }
            if (len > 0) defaultLength = len;
        } else if (c == 'T') {
            int t = 0;
            while (i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
                t = t * 10 + (mml[i++] - '0');
            }
            if (t > 0) tempo = t;
        } else if (c == 'M' && i < mml.size()) {
            musicStyle = static_cast<char>(std::toupper(static_cast<unsigned char>(mml[i++])));
        } else if (c == 'P') {
            // Pause
            int len = defaultLength;
            if (i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
                len = 0;
                while (i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
                    len = len * 10 + (mml[i++] - '0');
                }
            }
            double dur = (240.0 / tempo) / len;
            size_t numZeros = static_cast<size_t>(std::round(dur * sampleRate));
            pcmOut.insert(pcmOut.end(), numZeros, 0);
        } else if (c >= 'A' && c <= 'G') {
            // Note
            int semitone = 0;
            switch (c) {
                case 'C': semitone = 0; break;
                case 'D': semitone = 2; break;
                case 'E': semitone = 4; break;
                case 'F': semitone = 5; break;
                case 'G': semitone = 7; break;
                case 'A': semitone = 9; break;
                case 'B': semitone = 11; break;
            }

            if (i < mml.size()) {
                if (mml[i] == '+' || mml[i] == '#') {
                    semitone++;
                    i++;
                } else if (mml[i] == '-') {
                    semitone--;
                    i++;
                }
            }

            int noteLen = defaultLength;
            if (i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
                noteLen = 0;
                while (i < mml.size() && std::isdigit(static_cast<unsigned char>(mml[i]))) {
                    noteLen = noteLen * 10 + (mml[i++] - '0');
                }
            }

            int midi = (octave + 1) * 12 + semitone;
            double freq = 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
            double totalDur = (240.0 / static_cast<double>(tempo)) / static_cast<double>(noteLen);

            double soundDur = totalDur;
            if (musicStyle == 'N') {
                soundDur = totalDur * 0.875;
            } else if (musicStyle == 'S') {
                soundDur = totalDur * 0.75;
            }

            generateSound(freq, soundDur, pcmOut, sampleRate);
            if (totalDur > soundDur) {
                size_t pauseSamples = static_cast<size_t>(std::round((totalDur - soundDur) * sampleRate));
                pcmOut.insert(pcmOut.end(), pauseSamples, 0);
            }
        }
    }
}

void Abc80QbHost::stepServerUntilSync(uint8_t expectedSync, uint64_t maxTStates)
{
    uint64_t executed = 0;
    while (executed < maxTStates) {
        if (_board.getPpi().hostReadLinkPortA() == expectedSync) {
            return;
        }
        executed += _board.stepInstruction();
    }
}

bool Abc80QbHost::initialAbc()
{
    // Start PC-Link server at 0x0B00 if not already running there
    _board.getCpu().setSP(0x17BF);
    _board.getCpu().prefetch(0x0B00);

    // Step until server writes Link PPI control word (82h) and is polling Port C1
    uint64_t executed = 0;
    while (executed < 100000) {
        uint16_t pc = _board.getCpu().getPC();
        if (pc >= 0x0B04 && pc <= 0x0B08) {
            break;
        }
        executed += _board.stepInstruction();
    }

    // Checking read/write OK at address 0x1000 and 0x3000 as in authentic abc.bas
    if (!outAbc(0x1000, 0x00)) return false;
    if (!outAbc(0x1000, 0xFF)) return false;
    uint8_t r1000 = 0;
    if (!inAbc(0x1000, r1000)) return false;
    if (r1000 != 0xFF) return false;

    if (!outAbc(0x3000, 0x00)) return false;
    if (!outAbc(0x3000, 0xFF)) return false;
    uint8_t r3000 = 0;
    if (!inAbc(0x3000, r3000)) return false;
    if (r3000 != 0xFF) return false;

    return true;
}

bool Abc80QbHost::outAbc(uint16_t address, uint8_t byteOut, uint64_t maxTStatesPerStep)
{
    uint8_t low = static_cast<uint8_t>(address & 0xFF);
    uint8_t high = static_cast<uint8_t>((address >> 8) & 0xFF);

    // 1. Send Cmd 1 (OutABC)
    _board.getPpi().hostWriteLinkPortB(0x01);
    stepServerUntilSync(0x55, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0x55) return false;

    // 2. Send AddrLow
    _board.getPpi().hostWriteLinkPortB(low);
    stepServerUntilSync(0xAA, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0xAA) return false;

    // 3. Send AddrHigh
    _board.getPpi().hostWriteLinkPortB(high);
    stepServerUntilSync(0x55, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0x55) return false;

    // 4. Send ByteOut
    _board.getPpi().hostWriteLinkPortB(byteOut);
    stepServerUntilSync(0xAA, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0xAA) return false;

    // 5. Release line
    _board.getPpi().hostWriteLinkPortB(0x00);
    return true;
}

bool Abc80QbHost::inAbc(uint16_t address, uint8_t& byteIn, uint64_t maxTStatesPerStep)
{
    uint8_t low = static_cast<uint8_t>(address & 0xFF);
    uint8_t high = static_cast<uint8_t>((address >> 8) & 0xFF);

    // 1. Send Cmd 2 (InABC)
    _board.getPpi().hostWriteLinkPortB(0x02);
    stepServerUntilSync(0x55, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0x55) return false;

    // 2. Send AddrLow
    _board.getPpi().hostWriteLinkPortB(low);
    stepServerUntilSync(0xAA, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0xAA) return false;

    // 3. Send AddrHigh
    _board.getPpi().hostWriteLinkPortB(high);

    // Step CPU until InABC reads (HL) and writes it out on Port C0h (0B6Fh: OUT (C0h), A)
    uint64_t stepCount = 0;
    while (stepCount < maxTStatesPerStep) {
        uint16_t pc = _board.getCpu().getPC();
        stepCount += _board.stepInstruction();
        // After 0B71h, Port C0h holds the data byte read from memory
        if (pc == 0x0B71) {
            break;
        }
    }

    byteIn = _board.getPpi().hostReadLinkPortA();

    // 4. Release line
    _board.getPpi().hostWriteLinkPortB(0x00);
    return true;
}

bool Abc80QbHost::runAbc(uint16_t address, uint64_t maxTStatesPerStep)
{
    uint8_t low = static_cast<uint8_t>(address & 0xFF);
    uint8_t high = static_cast<uint8_t>((address >> 8) & 0xFF);

    // 1. Send Cmd 3 (RunABC)
    _board.getPpi().hostWriteLinkPortB(0x03);
    stepServerUntilSync(0x55, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0x55) return false;

    // 2. Send AddrLow
    _board.getPpi().hostWriteLinkPortB(low);
    stepServerUntilSync(0xAA, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0xAA) return false;

    // 3. Send AddrHigh
    _board.getPpi().hostWriteLinkPortB(high);
    stepServerUntilSync(0x55, maxTStatesPerStep);
    if (_board.getPpi().hostReadLinkPortA() != 0x55) return false;

    // 4. Release line and step into user program
    _board.getPpi().hostWriteLinkPortB(0x00);

    // Step until JP (HL) at 0x0BA7 branches to address
    uint64_t stepCount = 0;
    while (stepCount < maxTStatesPerStep) {
        if (_board.getCpu().getPC() == address) {
            return true;
        }
        stepCount += _board.stepInstruction();
    }

    return (_board.getCpu().getPC() == address);
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
