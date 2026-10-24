/*
 * Abc80QbHost.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80QbHost.hxx>
#include <abc80/Abc80Config.hxx>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace abc80 {

static constexpr double TWO_PI = 6.28318530717958647692;

// Authentic Z80 machine code strings from vintage/abc.bas InitialABC:
// InsertProgOut1$ (0x1700, 26 bytes): shifts [addr..0x16FE] -> [addr+1..0x16FF], writes 0x00 at (addr), outputs 0xCC to 0xC0, jumps to 0x0B00
static constexpr const char* kInsertProgOut1Hex = "01FE160A03020B0B2A8A17ED4220F4033E00023ECCD3C0C3000B";
// InsertProgOut2$ (0x1720, 26 bytes): shifts [addr..0x37FE] -> [addr+1..0x37FF], writes 0x00 at (addr), outputs 0xCC to 0xC0, jumps to 0x0B00
static constexpr const char* kInsertProgOut2Hex = "01FE370A03020B0B2A8A17ED4220F4033E00023ECCD3C0C3000B";
// DeleteProgOut1$ (0x1740, 23 bytes): shifts [addr+1..0x16FF] -> [addr..0x16FE], outputs 0xCC to 0xC0, jumps to 0x0B00
static constexpr const char* kDeleteProgOut1Hex = "ED4B8A17030A0B020321FF16ED4220F43ECCD3C0C3000B";
// DeleteProgOut2$ (0x1760, 23 bytes): shifts [addr+1..0x37FF] -> [addr..0x37FE], outputs 0xCC to 0xC0, jumps to 0x0B00
static constexpr const char* kDeleteProgOut2Hex = "ED4B8A17030A0B020321FF37ED4220F43ECCD3C0C3000B";

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

std::string Cp437Screen::getRowAttrHex(int row) const
{
    static constexpr const char* kHex = "0123456789ABCDEF";
    int r = std::clamp(row - 1, 0, HEIGHT - 1);
    std::string hex;
    hex.reserve(WIDTH * 2);
    for (int c = 0; c < WIDTH; ++c) {
        uint8_t a = _buffer[r * WIDTH + c].attr;
        hex.push_back(kHex[(a >> 4) & 0x0F]);
        hex.push_back(kHex[a & 0x0F]);
    }
    return hex;
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
    initVintageScreen(_currentAddress);
}

std::filesystem::path Abc80QbHost::defaultExercisesDir()
{
    return Abc80Config::resolveResourcePath(std::filesystem::path("exercises") / "qb");
}

bool Abc80QbHost::isLinkServerPolling() const noexcept
{
    uint16_t pc = _board.getCpu().getPC();
    return (pc >= 0x0B00 && pc <= 0x0BA8);
}

std::string Abc80QbHost::getModeName() const
{
    switch (_mode) {
        case QbMode::Disconnected: return "disconnected";
        case QbMode::Neutral:      return "neutral";
        case QbMode::AddressInput: return "address";
        case QbMode::DataInput:    return "data";
        case QbMode::SaveFile:     return "save_file";
        case QbMode::SaveRange:    return "save_range";
        case QbMode::SaveDoneWait: return "save_done";
        case QbMode::LoadFile:     return "load_file";
    }
    return "disconnected";
}

void Abc80QbHost::disconnect()
{
    _connected = false;
    _mode = QbMode::Disconnected;
    _highlightedKey = 0;
    _promptBuffer.clear();
    _pendingSaveFilename.clear();
    _saveHexDigitsEntered = 0;
    _statusMessage = "Standby - Press F1 or Connect to link ABC-80 (0B00h)";
    initVintageScreen(_currentAddress);
}

std::vector<QbAudioEvent> Abc80QbHost::consumeAudioEvents()
{
    std::vector<QbAudioEvent> out;
    out.swap(_audioEvents);
    return out;
}

void Abc80QbHost::queueSound(double freqHz, double durationSec)
{
    QbAudioEvent ev;
    ev.kind = "sound";
    ev.freqHz = freqHz;
    ev.durationSec = durationSec;
    _audioEvents.push_back(std::move(ev));
}

void Abc80QbHost::queuePlay(const std::string& mml)
{
    QbAudioEvent ev;
    ev.kind = "play";
    ev.mml = mml;
    _audioEvents.push_back(std::move(ev));
}

void Abc80QbHost::initVintageScreen(uint16_t currentAddress)
{
    _currentAddress = currentAddress;
    _screen.color(7, 0);
    _screen.cls();

    // Top Title (Rows 1-5, Col 10, Color 13 Light Magenta - '#' represents CP437 219 full block in ASCII rows)
    _screen.printAt(1, 10, " ######   #####.    .####.          .####.     .####. ", 13, 0);
    _screen.printAt(2, 10, "###  ###  ##   ##  ##    ##        ##    ##   ##    ##", 13, 0);
    _screen.printAt(3, 10, "########  ######   ##        ###    [####]    ##    ##", 13, 0);
    _screen.printAt(4, 10, "###  ###  ##   ##  ##    ##        ##    ##   ##    ##", 13, 0);
    _screen.printAt(5, 10, "###  ###  #####^    ^####^          ^####^     ^####^ ", 13, 0);

    // Memory Table Frame (Rows 6-18, Col 12, Color 7 Light Gray)
    _screen.printAt(6, 12,  "                                           ", 7, 0);
    _screen.printAt(7, 12,  " +-------------------+-------------------+ ", 7, 0);
    _screen.printAt(8, 12,  " |     Address       |        Byte       | ", 7, 0);
    _screen.printAt(9, 12,  " +-------------------+-------------------+ ", 7, 0);
    for (int r = 10; r <= 16; ++r) {
        if (r == 13) {
            _screen.printAt(r, 12, " +-*               *-+-*               *-+ ", 7, 0);
        } else {
            _screen.printAt(r, 12, " |                   |                   | ", 7, 0);
        }
    }
    _screen.printAt(17, 12, " +-------------------+-------------------+ ", 7, 0);
    _screen.printAt(18, 12, "                                           ", 7, 0);

    // Right-side Function Key Box (Rows 11-18, Col 57, Color 7)
    _screen.printAt(11, 57, "+----+----+----+", 7, 0);
    _screen.printAt(12, 57, "|INS |ADDs| -  |", 7, 0);
    _screen.printAt(13, 57, "+----+----+----+", 7, 0);
    _screen.printAt(14, 57, "|DEL |Data| +  |", 7, 0);
    _screen.printAt(15, 57, "+----+----+----+", 7, 0);
    _screen.printAt(16, 57, "+-------+------+", 7, 0);
    _screen.printAt(17, 57, "| ERROR |  RUN |", 7, 0);
    _screen.printAt(18, 57, "+-------+------+", 7, 0);

    // Hex Digit Keys (Rows 21-23, Col 8, Color 7)
    _screen.printAt(21, 8, "+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+", 7, 0);
    _screen.printAt(22, 8, "| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |", 7, 0);
    _screen.printAt(23, 8, "+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+", 7, 0);

    // Function Key Shortcuts (Row 24, Col 8, Color 15 Bright White)
    _screen.printAt(24, 8, "Shift+F5 = Run, Shift+F9 = Save, Shift+F10 = Load, Shift+Esc = Quit", 15, 0);

    // Footer Attribution (Row 25, Col 10, Color 12 Light Red)
    _screen.printAt(25, 10, "PC/AT <-> ABC-80  LINK via 8255... Designed By  Charles Chiou", 12, 0);

    renderModeCursors();
    updateMemoryTable(currentAddress);
    renderPromptArea();
}

void Abc80QbHost::renderModeCursors()
{
    if (_mode == QbMode::AddressInput) {
        // vintage/abc.bas SUB NewAddress lines 617-620:
        // COLOR 9: LOCATE 8, 34: PRINT "   ": LOCATE 8, 50: PRINT "   "
        // LOCATE 8, 14: PRINT "███": LOCATE 8, 30: PRINT "███"
        _screen.printAt(8, 34, "   ", 9, 0);
        _screen.printAt(8, 50, "   ", 9, 0);
        _screen.printAt(8, 14, "###", 9, 0);
        _screen.printAt(8, 30, "###", 9, 0);
    } else if (_mode == QbMode::DataInput) {
        // vintage/abc.bas SUB DataInput lines 196-199:
        // COLOR 9: LOCATE 8, 14: PRINT "   ": LOCATE 8, 30: PRINT "   "
        // LOCATE 8, 34: PRINT "███": LOCATE 8, 50: PRINT "███"
        _screen.printAt(8, 14, "   ", 9, 0);
        _screen.printAt(8, 30, "   ", 9, 0);
        _screen.printAt(8, 34, "###", 9, 0);
        _screen.printAt(8, 50, "###", 9, 0);
    } else {
        _screen.printAt(8, 14, "   ", 7, 0);
        _screen.printAt(8, 30, "   ", 7, 0);
        _screen.printAt(8, 34, "   ", 7, 0);
        _screen.printAt(8, 50, "   ", 7, 0);
    }
}

void Abc80QbHost::clearPromptArea()
{
    _screen.printAt(19, 1, std::string(78, ' '), 7, 0);
    _screen.printAt(20, 1, std::string(78, ' '), 7, 0);
}

void Abc80QbHost::renderPromptArea()
{
    clearPromptArea();
    if (_mode == QbMode::SaveFile) {
        _screen.printAt(19, 6, "* Save *  file name and path :  " + _promptBuffer, 7, 0);
    } else if (_mode == QbMode::SaveRange) {
        _screen.printAt(19, 6, "* Save *  file name and path :  " + _pendingSaveFilename, 7, 0);
        _screen.printAt(20, 6, "Starting Address :         Finishing Address:        ", 7, 0);
        std::ostringstream startSs;
        startSs << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << _saveStartAddr;
        std::string startHex = startSs.str();
        for (int i = 0; i < std::min(_saveHexDigitsEntered, 4); ++i) {
            _screen.printAt(20, 26 + i, std::string(1, startHex[static_cast<size_t>(i)]), 14, 0);
        }
        std::ostringstream finSs;
        finSs << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << _saveFinishAddr;
        std::string finHex = finSs.str();
        for (int i = 0; i < std::max(0, _saveHexDigitsEntered - 4); ++i) {
            _screen.printAt(20, 53 + i, std::string(1, finHex[static_cast<size_t>(i)]), 14, 0);
        }
    } else if (_mode == QbMode::SaveDoneWait) {
        _screen.printAt(19, 6, "Reading from ABC-80...", 7, 0);
        _screen.printAt(20, 6, "Saving to " + _pendingSaveFilename, 7, 0);
        _screen.printAt(20, 40, " Press any key... ", 14, 0);
    } else if (_mode == QbMode::LoadFile) {
        _screen.printAt(20, 6, "* Load *  file name and path :  " + _promptBuffer, 7, 0);
    } else if (!_statusMessage.empty()) {
        _screen.printAt(19, 6, _statusMessage.substr(0, 72), _connected ? 10 : 7, 0);
    }
}

void Abc80QbHost::colourKeyPressed(int whichKey)
{
    _highlightedKey = whichKey;

    // Reset all key highlights to default Color 7 first
    _screen.printAt(12, 57, "|INS |ADDs| -  |", 7, 0);
    _screen.printAt(14, 57, "|DEL |Data| +  |", 7, 0);
    _screen.printAt(17, 57, "| ERROR |  RUN |", 7, 0);
    _screen.printAt(22, 8, "| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |", 7, 0);

    // Queue authentic PC speaker sound effects (QuickBASIC clock ticks: 18.2 ticks/sec)
    if (whichKey > 0 && whichKey <= 22) {
        queueSound(2000.0, 0.1 / 18.2);
    } else if (whichKey == 23) {
        queueSound(800.0, 2.0 / 18.2);
        queueSound(200.0, 5.0 / 18.2);
    } else if (whichKey == 24) {
        queuePlay("l16mlfgfgfaba>d<");
    }

    // Highlight active key on the 80x25 screen with Color 10 (Light Green) or Color 12 (Light Red for ERROR)
    if (whichKey >= 1 && whichKey <= 16) {
        static constexpr const char* kHex = "0123456789ABCDEF";
        int col = 9 + 4 * (whichKey - 1);
        std::string cellStr = std::string("#") + kHex[whichKey - 1] + "#";
        _screen.printAt(22, col, cellStr, 10, 0);
    } else if (whichKey == 17) {
        _screen.printAt(12, 58, "INS#", 10, 0);
    } else if (whichKey == 18) {
        _screen.printAt(12, 63, "ADDs", 10, 0);
    } else if (whichKey == 19) {
        _screen.printAt(12, 68, "#-##", 10, 0);
    } else if (whichKey == 20) {
        _screen.printAt(14, 58, "DEL#", 10, 0);
    } else if (whichKey == 21) {
        _screen.printAt(14, 63, "DATA", 10, 0);
    } else if (whichKey == 22) {
        _screen.printAt(14, 68, "#+##", 10, 0);
    } else if (whichKey == 23) {
        _screen.printAt(17, 58, "#ERROR#", 12, 0);
    } else if (whichKey == 24) {
        _screen.printAt(17, 66, "##RUN#", 10, 0);
    }
}

void Abc80QbHost::updateMemoryTable(uint16_t currentAddress)
{
    _currentAddress = currentAddress;
    const bool useLink = _connected && isLinkServerPolling();

    uint8_t byteDisplay[7] = {0};
    for (int a = 1; a <= 7; ++a) {
        uint16_t addr = static_cast<uint16_t>(currentAddress - 4 + a);
        uint8_t val = 0;
        if (useLink) {
            if (!inAbc(addr, val)) {
                val = _board.getBus().readByte(addr);
            }
        } else {
            val = _board.getBus().readByte(addr);
        }
        byteDisplay[a - 1] = val;
    }

    // In vintage/abc.bas SUB DisplayTable, the loop ends on a=7 (currentAddress+3),
    // but when the user is inspecting currentAddress, ensure the ABC-80's 6-digit
    // 7-segment display ends up showing currentAddress and its byte!
    if (useLink) {
        uint8_t centerVal = 0;
        (void)inAbc(currentAddress, centerVal);
        byteDisplay[3] = centerVal;
    }

    for (int a = 1; a <= 7; ++a) {
        uint16_t addr = static_cast<uint16_t>(currentAddress - 4 + a);
        uint8_t val = byteDisplay[a - 1];

        int row = 9 + a;
        int colorFg = (a == 4) ? 14 : 7; // Color 14 (Yellow) for center cursor line 13

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

bool Abc80QbHost::stepServerUntilSync(uint8_t expectedSync, uint64_t maxTStates)
{
    uint64_t executed = 0;
    while (executed < maxTStates) {
        if (_board.getPpi().hostReadLinkPortA() == expectedSync) {
            return true;
        }
        if (_board.getCpu().isHalted()) {
            return false;
        }
        executed += _board.stepInstruction();
    }
    return (_board.getPpi().hostReadLinkPortA() == expectedSync);
}

bool Abc80QbHost::stepServerUntilPollLoop(uint64_t maxTStates)
{
    uint64_t executed = 0;
    while (executed < maxTStates) {
        uint16_t pc = _board.getCpu().getPC();
        if (pc >= 0x0B04 && pc <= 0x0B0A) {
            return true;
        }
        if (_board.getCpu().isHalted()) {
            return false;
        }
        executed += _board.stepInstruction();
    }
    uint16_t pc = _board.getCpu().getPC();
    return (pc >= 0x0B04 && pc <= 0x0B0A);
}

bool Abc80QbHost::uploadHexRoutine(uint16_t destAddr, const char* hexStr)
{
    if (!hexStr) return false;
    uint16_t addr = destAddr;
    for (size_t i = 0; hexStr[i] != '\0' && hexStr[i + 1] != '\0'; i += 2) {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        int hi = hexVal(hexStr[i]);
        int lo = hexVal(hexStr[i + 1]);
        if (hi < 0 || lo < 0) return false;
        uint8_t byteVal = static_cast<uint8_t>((hi << 4) | lo);
        if (!outAbc(addr, byteVal)) {
            return false;
        }
        addr++;
    }
    return true;
}

bool Abc80QbHost::initialAbc()
{
    // Ensure PC-Link server at 0x0B00 is active and polling Port C1h
    uint16_t pc = _board.getCpu().getPC();
    if (pc < 0x0B00 || pc > 0x0BA8) {
        _board.getCpu().setSP(0x17BF);
        _board.getCpu().prefetch(0x0B00);
    }
    _board.getPpi().hostWriteLinkPortB(0x00);

    if (!stepServerUntilPollLoop(250000ULL)) {
        _connected = false;
        _statusMessage = "Link timeout waiting for 0B00h poll loop";
        return false;
    }

    // Checking read/write OK at address 0x1000 and 0x3000 as in authentic abc.bas lines 404-412
    if (!outAbc(0x1000, 0x00)) return false;
    if (!outAbc(0x1000, 0xFF)) return false;
    uint8_t r1000 = 0;
    if (!inAbc(0x1000, r1000) || r1000 != 0xFF) {
        _connected = false;
        _statusMessage = "RAM fail at 1000h...";
        return false;
    }

    if (!outAbc(0x3000, 0x00)) return false;
    if (!outAbc(0x3000, 0xFF)) return false;
    uint8_t r3000 = 0;
    if (!inAbc(0x3000, r3000) || r3000 != 0xFF) {
        _connected = false;
        _statusMessage = "RAM fail at 3000h...";
        return false;
    }

    // PLAY "l32mlabcdfg" & PRINT " ABC-80 Connected successfully..."
    queuePlay("l32mlabcdfg");
    _statusMessage = "ABC-80 Connected successfully...";

    // Upload the 4 Z80 Insert/Delete helper routines to 0x1700, 0x1720, 0x1740, 0x1760
    if (!uploadHexRoutine(0x1700, kInsertProgOut1Hex)) return false;
    if (!uploadHexRoutine(0x1720, kInsertProgOut2Hex)) return false;
    if (!uploadHexRoutine(0x1740, kDeleteProgOut1Hex)) return false;
    if (!uploadHexRoutine(0x1760, kDeleteProgOut2Hex)) return false;

    _connected = true;
    _mode = QbMode::Neutral;
    _currentAddress = 0x1000;
    _highlightedKey = 0;
    initVintageScreen(_currentAddress);
    return true;
}

bool Abc80QbHost::outAbc(uint16_t address, uint8_t byteOut, uint64_t maxTStatesPerStep)
{
    if (!stepServerUntilPollLoop(maxTStatesPerStep)) {
        return false;
    }

    uint8_t low = static_cast<uint8_t>(address & 0xFF);
    uint8_t high = static_cast<uint8_t>((address >> 8) & 0xFF);

    // 1. Send Cmd 1 (OutABC)
    _board.getPpi().hostWriteLinkPortB(0x01);
    if (!stepServerUntilSync(0x55, maxTStatesPerStep)) return false;

    // 2. Send AddrLow
    _board.getPpi().hostWriteLinkPortB(low);
    if (!stepServerUntilSync(0xAA, maxTStatesPerStep)) return false;

    // 3. Send AddrHigh
    _board.getPpi().hostWriteLinkPortB(high);
    if (!stepServerUntilSync(0x55, maxTStatesPerStep)) return false;

    // 4. Send ByteOut (firmware writes (HL), calls adrsdp, dadp, scd_k1, then outputs 0xAA at 0x0B61)
    _board.getPpi().hostWriteLinkPortB(byteOut);
    if (!stepServerUntilSync(0xAA, maxTStatesPerStep)) return false;

    // 5. Release line and step back to 0x0B04 poll loop
    _board.getPpi().hostWriteLinkPortB(0x00);
    return stepServerUntilPollLoop(maxTStatesPerStep);
}

bool Abc80QbHost::inAbc(uint16_t address, uint8_t& byteIn, uint64_t maxTStatesPerStep)
{
    if (!stepServerUntilPollLoop(maxTStatesPerStep)) {
        return false;
    }

    uint8_t low = static_cast<uint8_t>(address & 0xFF);
    uint8_t high = static_cast<uint8_t>((address >> 8) & 0xFF);

    // 1. Send Cmd 2 (InABC)
    _board.getPpi().hostWriteLinkPortB(0x02);
    if (!stepServerUntilSync(0x55, maxTStatesPerStep)) return false;

    // 2. Send AddrLow
    _board.getPpi().hostWriteLinkPortB(low);
    if (!stepServerUntilSync(0xAA, maxTStatesPerStep)) return false;

    // 3. Send AddrHigh
    _board.getPpi().hostWriteLinkPortB(high);

    // Step CPU until InABC reads (HL) and writes it out on Port C0h (0B6Fh: OUT (C0h), A -> PC == 0x0B71)
    uint64_t stepCount = 0;
    bool latched = false;
    while (stepCount < maxTStatesPerStep) {
        if (_board.getCpu().isHalted()) return false;
        stepCount += _board.stepInstruction();
        if (_board.getCpu().getPC() == 0x0B71) {
            latched = true;
            break;
        }
    }
    if (!latched) return false;

    byteIn = _board.getPpi().hostReadLinkPortA();

    // 4. Release line and allow firmware to finish dadp (0x0B71), the 0x55 guard (0x0B7A -> 0x0B90),
    // adrsdp (0x0B85), scd_k1 (0x0B8B), and return to the 0x0B04 poll loop!
    _board.getPpi().hostWriteLinkPortB(0x00);
    return stepServerUntilPollLoop(maxTStatesPerStep);
}

bool Abc80QbHost::runAbc(uint16_t address, uint64_t maxTStatesPerStep)
{
    if (!stepServerUntilPollLoop(maxTStatesPerStep)) {
        return false;
    }

    uint8_t low = static_cast<uint8_t>(address & 0xFF);
    uint8_t high = static_cast<uint8_t>((address >> 8) & 0xFF);

    // 1. Send Cmd 3 (RunABC)
    _board.getPpi().hostWriteLinkPortB(0x03);
    if (!stepServerUntilSync(0x55, maxTStatesPerStep)) return false;

    // 2. Send AddrLow
    _board.getPpi().hostWriteLinkPortB(low);
    if (!stepServerUntilSync(0xAA, maxTStatesPerStep)) return false;

    // 3. Send AddrHigh
    _board.getPpi().hostWriteLinkPortB(high);
    if (!stepServerUntilSync(0x55, maxTStatesPerStep)) return false;

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

bool Abc80QbHost::insertByte(uint16_t address, uint64_t maxTStates)
{
    const bool inBank1 = (address >= 0x1000 && address < 0x1700);
    const bool inBank2 = (address >= 0x3000 && address < 0x3800);
    if (!inBank1 && !inBank2) {
        colourKeyPressed(23);
        _statusMessage = "Insert error: Address must be in 1000h-16FFh or 3000h-37FFh";
        renderPromptArea();
        return false;
    }

    uint8_t record1 = static_cast<uint8_t>((address >> 8) & 0xFF);
    uint8_t record2 = static_cast<uint8_t>(address & 0xFF);
    if (!outAbc(0x178A, record2)) return false;
    if (!outAbc(0x178B, record1)) return false;

    uint16_t helperAddr = inBank1 ? 0x1700 : 0x1720;
    if (!runAbc(helperAddr)) return false;

    // Step Z80 while the helper shifts RAM, outputs 0xCC to Port C0h, and jumps back to 0x0B00
    if (!stepServerUntilSync(0xCC, maxTStates)) return false;
    if (!stepServerUntilPollLoop(200000ULL)) return false;

    colourKeyPressed(17);
    updateMemoryTable(_currentAddress);
    return true;
}

bool Abc80QbHost::deleteByte(uint16_t address, uint64_t maxTStates)
{
    const bool inBank1 = (address >= 0x1000 && address < 0x1700);
    const bool inBank2 = (address >= 0x3000 && address < 0x3800);
    if (!inBank1 && !inBank2) {
        colourKeyPressed(23);
        _statusMessage = "Delete error: Address must be in 1000h-16FFh or 3000h-37FFh";
        renderPromptArea();
        return false;
    }

    uint8_t record1 = static_cast<uint8_t>((address >> 8) & 0xFF);
    uint8_t record2 = static_cast<uint8_t>(address & 0xFF);
    if (!outAbc(0x178A, record2)) return false;
    if (!outAbc(0x178B, record1)) return false;

    uint16_t helperAddr = inBank1 ? 0x1740 : 0x1760;
    if (!runAbc(helperAddr)) return false;

    if (!stepServerUntilSync(0xCC, maxTStates)) return false;
    if (!stepServerUntilPollLoop(200000ULL)) return false;

    colourKeyPressed(20);
    updateMemoryTable(_currentAddress);
    return true;
}

bool Abc80QbHost::sanitizeAbcFilename(const std::string& rawName, std::string& cleanOut)
{
    cleanOut.clear();
    if (rawName.empty() || rawName.size() > 64) {
        return false;
    }
    if (rawName.find("..") != std::string::npos ||
        rawName.find('/') != std::string::npos ||
        rawName.find('\\') != std::string::npos) {
        return false;
    }
    for (char c : rawName) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!(std::isalnum(uc) || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    if (rawName.front() == '.') {
        return false;
    }
    cleanOut = rawName;
    bool hasAbcExt = false;
    if (cleanOut.size() >= 4) {
        std::string ext = cleanOut.substr(cleanOut.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        hasAbcExt = (ext == ".abc");
    }
    if (!hasAbcExt) {
        cleanOut += ".abc";
    }
    return cleanOut.size() <= 64;
}

std::string Abc80QbHost::formatAbcFile(uint16_t startAddr, const std::vector<uint8_t>& data)
{
    std::ostringstream oss;
    oss << static_cast<unsigned int>(startAddr) << "\r\n\"";
    oss << std::hex << std::uppercase << std::setfill('0');
    for (uint8_t b : data) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    oss << "\"\r\n";
    return oss.str();
}

bool Abc80QbHost::parseAbcFile(const std::string& content, uint16_t& startAddrOut, std::vector<uint8_t>& dataOut)
{
    dataOut.clear();
    startAddrOut = 0;

    std::istringstream iss(content);
    std::string line1;
    if (!std::getline(iss, line1)) {
        return false;
    }
    // Trim whitespace
    while (!line1.empty() && std::isspace(static_cast<unsigned char>(line1.front()))) {
        line1.erase(line1.begin());
    }
    while (!line1.empty() && std::isspace(static_cast<unsigned char>(line1.back()))) {
        line1.pop_back();
    }
    if (line1.empty()) {
        return false;
    }
    unsigned long addrVal = 0;
    try {
        size_t idx = 0;
        addrVal = std::stoul(line1, &idx, 10);
        if (idx != line1.size() || addrVal > 0xFFFFUL) {
            return false;
        }
    } catch (...) {
        return false;
    }

    std::string line2;
    if (!std::getline(iss, line2)) {
        return false;
    }
    while (!line2.empty() && std::isspace(static_cast<unsigned char>(line2.front()))) {
        line2.erase(line2.begin());
    }
    while (!line2.empty() && std::isspace(static_cast<unsigned char>(line2.back()))) {
        line2.pop_back();
    }
    if (line2.size() >= 2 && line2.front() == '"' && line2.back() == '"') {
        line2 = line2.substr(1, line2.size() - 2);
    }
    if (line2.empty() || (line2.size() % 2) != 0) {
        return false;
    }
    const size_t numBytes = line2.size() / 2;
    if (numBytes > 65536 || (addrVal + numBytes) > 0x10000UL) {
        return false;
    }

    dataOut.reserve(numBytes);
    for (size_t i = 0; i < line2.size(); i += 2) {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        int hi = hexVal(line2[i]);
        int lo = hexVal(line2[i + 1]);
        if (hi < 0 || lo < 0) {
            dataOut.clear();
            return false;
        }
        dataOut.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }

    startAddrOut = static_cast<uint16_t>(addrVal);
    return true;
}

bool Abc80QbHost::saveProgramAbc(const std::string& filename, uint16_t startAddr, uint16_t endAddr, std::string& errOut)
{
    errOut.clear();
    if (startAddr > endAddr) {
        colourKeyPressed(23);
        errOut = "Starting address must be <= finishing address";
        return false;
    }

    std::string cleanName;
    if (!sanitizeAbcFilename(filename, cleanName)) {
        colourKeyPressed(23);
        errOut = "Invalid .abc filename";
        return false;
    }

    if (!_connected && !initialAbc()) {
        errOut = "ABC-80 link is not connected";
        return false;
    }

    std::vector<uint8_t> data;
    data.reserve(static_cast<size_t>(endAddr - startAddr) + 1);
    for (uint32_t addr = startAddr; addr <= endAddr; ++addr) {
        uint8_t b = 0;
        if (!inAbc(static_cast<uint16_t>(addr), b)) {
            errOut = "Link read failed at address";
            return false;
        }
        data.push_back(b);
    }

    std::filesystem::path dirPath = _programsDir.empty()
        ? (std::filesystem::temp_directory_path() / "abc80_programs")
        : std::filesystem::path(_programsDir);
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    std::filesystem::path fullPath = dirPath / cleanName;

    std::ofstream ofs(fullPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        errOut = "Failed to open file for writing";
        return false;
    }
    std::string encoded = formatAbcFile(startAddr, data);
    ofs.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    ofs.close();

    _statusMessage = "Saved " + std::to_string(data.size()) + " bytes to " + cleanName;
    updateMemoryTable(_currentAddress);
    renderPromptArea();
    return true;
}

bool Abc80QbHost::loadProgramAbc(const std::string& filename, uint16_t& loadedStartOut, size_t& loadedBytesOut, std::string& errOut)
{
    errOut.clear();
    loadedStartOut = 0;
    loadedBytesOut = 0;

    std::string cleanName;
    if (!sanitizeAbcFilename(filename, cleanName)) {
        colourKeyPressed(23);
        errOut = "Invalid .abc filename";
        return false;
    }

    std::filesystem::path dirPath = _programsDir.empty()
        ? (Abc80Config::defaultDir() / "programs")
        : std::filesystem::path(_programsDir);
    std::filesystem::path fullPath = dirPath / cleanName;
    if (!std::filesystem::exists(fullPath)) {
        std::filesystem::path vintagePath =
            Abc80Config::resolveResourcePath(std::filesystem::path("vintage") / cleanName);
        if (std::filesystem::exists(vintagePath)) {
            fullPath = vintagePath;
        } else {
            std::filesystem::path exPath = defaultExercisesDir() / cleanName;
            if (std::filesystem::exists(exPath)) {
                fullPath = exPath;
            } else {
                colourKeyPressed(23);
                errOut = "Program file not found: " + cleanName;
                return false;
            }
        }
    }

    std::ifstream ifs(fullPath, std::ios::binary);
    if (!ifs.is_open()) {
        errOut = "Failed to open program file";
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    uint16_t startAddr = 0;
    std::vector<uint8_t> data;
    if (!parseAbcFile(content, startAddr, data)) {
        colourKeyPressed(23);
        errOut = "Invalid .abc file format";
        return false;
    }

    if (!_connected && !initialAbc()) {
        errOut = "ABC-80 link is not connected";
        return false;
    }

    for (size_t i = 0; i < data.size(); ++i) {
        uint16_t addr = static_cast<uint16_t>(startAddr + i);
        if (!outAbc(addr, data[i])) {
            errOut = "Link write failed during LoadProg";
            return false;
        }
    }

    loadedStartOut = startAddr;
    loadedBytesOut = data.size();
    _currentAddress = startAddr;
    _statusMessage = "Loaded " + std::to_string(data.size()) + " bytes from " + cleanName;
    updateMemoryTable(_currentAddress);
    renderPromptArea();
    return true;
}

std::vector<std::string> Abc80QbHost::listPrograms() const
{
    std::vector<std::string> names;
    auto scanDir = [&names](const std::filesystem::path& p) {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) return;
        for (const auto& entry : std::filesystem::directory_iterator(p, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (ext == ".abc") {
                std::string fname = entry.path().filename().string();
                if (fname != "rom.abc" && std::find(names.begin(), names.end(), fname) == names.end()) {
                    names.push_back(fname);
                }
            }
        }
    };
    if (!_programsDir.empty()) {
        scanDir(_programsDir);
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool Abc80QbHost::pressKey(const std::string& keyName)
{
    std::string key = keyName;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    // F1 / CONNECT always re-runs Start: InitialABC -> SCREEN 12 -> DisplayTable &H1000
    if (key == "F1" || key == "CONNECT") {
        return initialAbc();
    }

    // Quit (Shift+Esc / Esc / QUIT)
    if (key == "QUIT" || key == "SHIFT+ESC" || key == "SHIFT+ESCAPE") {
        _connected = false;
        _mode = QbMode::Disconnected;
        _screen.color(7, 0);
        _screen.cls();
        _screen.printAt(1, 1, " Programmed by Charles Chiou", 7, 0);
        _statusMessage = "Programmed by Charles Chiou (Disconnected)";
        return true;
    }

    // If not connected yet, auto-connect on first operational key if the 1993 ROM is polling 0x0B00
    if (!_connected) {
        if (!initialAbc()) {
            return false;
        }
    }

    // Handle SaveDoneWait ("Press any key...")
    if (_mode == QbMode::SaveDoneWait) {
        _mode = QbMode::AddressInput;
        renderModeCursors();
        renderPromptArea();
        return true;
    }

    // Handle line-input modes (SaveFile / LoadFile)
    if (_mode == QbMode::SaveFile || _mode == QbMode::LoadFile) {
        if (key == "ESC" || key == "ESCAPE") {
            _promptBuffer.clear();
            _mode = QbMode::AddressInput;
            renderModeCursors();
            renderPromptArea();
            return true;
        }
        if (key == "BACKSPACE") {
            if (!_promptBuffer.empty()) {
                _promptBuffer.pop_back();
            }
            renderPromptArea();
            return true;
        }
        if (key == "ENTER" || key == "RETURN") {
            if (_promptBuffer.empty()) {
                _mode = QbMode::AddressInput;
                renderModeCursors();
                renderPromptArea();
                return true;
            }
            if (_mode == QbMode::SaveFile) {
                std::string clean;
                if (!sanitizeAbcFilename(_promptBuffer, clean)) {
                    colourKeyPressed(23);
                    _statusMessage = "Invalid filename";
                    _mode = QbMode::AddressInput;
                    renderModeCursors();
                    renderPromptArea();
                    return false;
                }
                _pendingSaveFilename = clean;
                _saveHexDigitsEntered = 0;
                _saveStartAddr = 0;
                _saveFinishAddr = 0;
                _mode = QbMode::SaveRange;
                renderPromptArea();
                return true;
            } else {
                uint16_t loadedStart = 0;
                size_t loadedBytes = 0;
                std::string err;
                bool ok = loadProgramAbc(_promptBuffer, loadedStart, loadedBytes, err);
                _promptBuffer.clear();
                _mode = QbMode::AddressInput;
                renderModeCursors();
                if (!ok) {
                    _statusMessage = err;
                }
                renderPromptArea();
                return ok;
            }
        }
        if (keyName.size() == 1) {
            char c = keyName[0];
            if ((std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.') &&
                _promptBuffer.size() < 40) {
                _promptBuffer.push_back(c);
                renderPromptArea();
                return true;
            }
        }
        return false;
    }

    // Handle SaveRange (8 hex digits: 4 for Start#, 4 for Finish#)
    if (_mode == QbMode::SaveRange) {
        if (key == "ESC" || key == "ESCAPE") {
            _mode = QbMode::AddressInput;
            renderModeCursors();
            renderPromptArea();
            return true;
        }
        int hexDigit = -1;
        if (key.size() == 1) {
            char c = key[0];
            if (c >= '0' && c <= '9') hexDigit = c - '0';
            else if (c >= 'A' && c <= 'F') hexDigit = c - 'A' + 10;
        }
        if (hexDigit < 0) {
            return false;
        }
        queueSound(1000.0, 1.0 / 18.2);
        if (_saveHexDigitsEntered < 4) {
            _saveStartAddr = static_cast<uint16_t>((_saveStartAddr << 4) | hexDigit);
        } else {
            _saveFinishAddr = static_cast<uint16_t>((_saveFinishAddr << 4) | hexDigit);
        }
        _saveHexDigitsEntered++;
        renderPromptArea();

        if (_saveHexDigitsEntered >= 8) {
            if (_saveStartAddr >= _saveFinishAddr) {
                colourKeyPressed(23);
                _saveHexDigitsEntered = 0;
                _saveStartAddr = 0;
                _saveFinishAddr = 0;
                renderPromptArea();
                return false;
            }
            std::string err;
            bool ok = saveProgramAbc(_pendingSaveFilename, _saveStartAddr, _saveFinishAddr, err);
            if (ok) {
                _mode = QbMode::SaveDoneWait;
            } else {
                _mode = QbMode::AddressInput;
                _statusMessage = err;
            }
            renderPromptArea();
            return ok;
        }
        return true;
    }

    // Mode switching & global keys in Neutral / AddressInput / DataInput
    if (key == "HOME" || key == "ADDS") {
        colourKeyPressed(18);
        _mode = QbMode::AddressInput;
        _statusMessage = "Address Input Mode (Type 0-F to enter 16-bit address)";
        renderModeCursors();
        renderPromptArea();
        return true;
    }
    if (key == "END" || key == "DATA") {
        colourKeyPressed(21);
        _mode = QbMode::DataInput;
        _statusMessage = "Data Input Mode (Type 0-F to write byte, INS/DEL to shift)";
        renderModeCursors();
        renderPromptArea();
        return true;
    }
    if (key == "PGUP" || key == "PAGEUP" || key == "-") {
        _currentAddress = static_cast<uint16_t>(_currentAddress - 1);
        updateMemoryTable(_currentAddress);
        colourKeyPressed(19);
        if (_mode == QbMode::AddressInput) {
            // Per vintage/abc.bas line 651: PGUP in NewAddress transfers to DataInput
            _mode = QbMode::DataInput;
            renderModeCursors();
        }
        return true;
    }
    if (key == "PGDN" || key == "PAGEDOWN" || key == "+") {
        _currentAddress = static_cast<uint16_t>(_currentAddress + 1);
        updateMemoryTable(_currentAddress);
        colourKeyPressed(22);
        if (_mode == QbMode::AddressInput) {
            // Per vintage/abc.bas line 657: PGDN in NewAddress transfers to DataInput
            _mode = QbMode::DataInput;
            renderModeCursors();
        }
        return true;
    }
    if (key == "RUN" || key == "SHIFT+F5") {
        colourKeyPressed(24);
        bool ok = runAbc(_currentAddress);
        if (ok) {
            std::ostringstream oss;
            oss << "Executing on ABC-80 at " << std::hex << std::uppercase
                << std::setw(4) << std::setfill('0') << _currentAddress << "h";
            _statusMessage = oss.str();
        } else {
            colourKeyPressed(23);
            _statusMessage = "RunABC failed: link server not responding";
        }
        renderPromptArea();
        return ok;
    }
    if (key == "SAVE" || key == "SHIFT+F9") {
        _promptBuffer.clear();
        _mode = QbMode::SaveFile;
        renderPromptArea();
        return true;
    }
    if (key == "LOAD" || key == "SHIFT+F10") {
        _promptBuffer.clear();
        _mode = QbMode::LoadFile;
        renderPromptArea();
        return true;
    }
    if (key == "INS" || key == "INSERT") {
        if (_mode != QbMode::DataInput) {
            _mode = QbMode::DataInput;
            renderModeCursors();
        }
        return insertByte(_currentAddress);
    }
    if (key == "DEL" || key == "DELETE") {
        if (_mode != QbMode::DataInput) {
            _mode = QbMode::DataInput;
            renderModeCursors();
        }
        return deleteByte(_currentAddress);
    }

    // Hex Digit Entry (0..9, A..F)
    int adval = -1;
    if (key.size() == 1) {
        char c = key[0];
        if (c >= '0' && c <= '9') adval = c - '0';
        else if (c >= 'A' && c <= 'F') adval = c - 'A' + 10;
    }

    if (adval >= 0) {
        if (_mode == QbMode::Neutral) {
            // Default to AddressInput if user starts typing hex digits from Neutral
            _mode = QbMode::AddressInput;
            renderModeCursors();
        }
        if (_mode == QbMode::AddressInput) {
            colourKeyPressed(adval + 1);
            // vintage/abc.bas line 668: Address# = (Address# - (INT(Address# / &H1000) * &H1000)) * &H10 + Adval%
            _currentAddress = static_cast<uint16_t>(((_currentAddress & 0x0FFFU) << 4) | static_cast<uint16_t>(adval));
            updateMemoryTable(_currentAddress);
            return true;
        } else if (_mode == QbMode::DataInput) {
            colourKeyPressed(adval + 1);
            // vintage/abc.bas lines 248-254:
            uint8_t byteInfo = 0;
            if (!inAbc(_currentAddress, byteInfo)) {
                colourKeyPressed(23);
                return false;
            }
            uint8_t outInfo = static_cast<uint8_t>(((byteInfo & 0x0FU) << 4) | static_cast<uint8_t>(adval));
            if (!outAbc(_currentAddress, outInfo)) {
                colourKeyPressed(23);
                return false;
            }
            updateMemoryTable(_currentAddress);
            uint8_t verifyByte = 0;
            if (!inAbc(_currentAddress, verifyByte) || verifyByte != outInfo) {
                colourKeyPressed(23);
                _statusMessage = "Write verify error (ROM / unmapped address)";
                renderPromptArea();
            }
            return true;
        }
    }

    // Any unrecognized key in AddressInput / DataInput triggers ERROR (key 23) per lines 240 / 660
    colourKeyPressed(23);
    return false;
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
