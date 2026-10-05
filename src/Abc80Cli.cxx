/*
 * Abc80Cli.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Cli.hxx>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace abc80 {

Abc80Cli::Abc80Cli(Abc80Board& board)
    : _board(board)
    , _breakpoints()
    , _running(false)
{
}

void Abc80Cli::addBreakpoint(uint16_t addr)
{
    _breakpoints.insert(addr);
}

void Abc80Cli::removeBreakpoint(uint16_t addr)
{
    _breakpoints.erase(addr);
}

void Abc80Cli::clearBreakpoints()
{
    _breakpoints.clear();
}

bool Abc80Cli::hasBreakpoint(uint16_t addr) const
{
    return _breakpoints.find(addr) != _breakpoints.end();
}

uint32_t Abc80Cli::step()
{
    return _board.stepInstruction();
}

uint64_t Abc80Cli::run(uint64_t maxTStates)
{
    _running = true;
    uint64_t executed = 0;

    while (_running && executed < maxTStates) {
        uint32_t stepT = _board.stepInstruction();
        executed += stepT;

        uint16_t pc = _board.getCpu().getPC();
        if (hasBreakpoint(pc)) {
            _running = false;
            break;
        }

        if (_board.getCpu().isHalted()) {
            _running = false;
            break;
        }
    }

    _running = false;
    return executed;
}

void Abc80Cli::reset()
{
    _board.reset();
}

uint8_t Abc80Cli::charToKeyCode(char c)
{
    char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower >= '0' && lower <= '9') {
        return static_cast<uint8_t>(lower - '0');
    }
    if (lower >= 'a' && lower <= 'f') {
        return static_cast<uint8_t>(10 + (lower - 'a'));
    }
    switch (lower) {
        case '+': return KEY_PLUS;
        case '-': return KEY_MINUS;
        case 'r': return KEY_RUN;
        case 'd': return KEY_DATA;
        case 'm': return KEY_ADRS;
        case 's': return KEY_STEP;
        case 'b': return KEY_BP;
        default:  return 0xFF;
    }
}

bool Abc80Cli::injectKeyCode(uint8_t keyCode)
{
    if (keyCode > KEY_BP) {
        return false;
    }

    _board.pressKey(keyCode);
    _board.stepTStates(250000); // Allow scan debounce
    _board.releaseKey(keyCode);
    _board.stepTStates(150000); // Allow release debounce
    return true;
}

bool Abc80Cli::injectKey(char keyChar)
{
    uint8_t code = charToKeyCode(keyChar);
    if (code == 0xFF) {
        return false;
    }
    return injectKeyCode(code);
}

char Abc80Cli::decodeSegmentGlyph(uint8_t mask) noexcept
{
    switch (mask & 0xFF) {
        case 0xBD: return '0';
        case 0x30: return '1';
        case 0x9B: return '2';
        case 0xBA: return '3';
        case 0x36: return '4';
        case 0xAE: return '5';
        case 0xAF: return '6';
        case 0x38: return '7';
        case 0xBF: return '8';
        case 0xBE: return '9';
        case 0x3F: return 'A';
        case 0xA7: return 'B';
        case 0x8D: return 'C';
        case 0xB3: return 'D';
        case 0x8F: return 'E';
        case 0x0F: return 'F';
        case 0x02: return '-';
        case 0x00: return ' ';
        case 0x2F: return 'P';
        case 0x03: return 'r';
        case 0x37: return 'H';
        case 0x85: return 'L';
        default:   return '?';
    }
}

std::string Abc80Cli::formatDisplayString() const
{
    // Check if PPI latched segments are non-zero; otherwise fallback to RAM dispbf at 0x17C6..0x17CB
    uint8_t segs[6];
    bool hasPpiData = false;
    for (int i = 0; i < 6; ++i) {
        segs[i] = _board.getPpi().getDigitSegment(static_cast<uint8_t>(i));
        if (segs[i] != 0x00) {
            hasPpiData = true;
        }
    }

    if (!hasPpiData) {
        // Read directly from RAM display buffer (0x17C6 = digit 0 ... 0x17CB = digit 5)
        for (int i = 0; i < 6; ++i) {
            segs[i] = _board.getBus().readByte(static_cast<uint16_t>(0x17C6 + i));
        }
    }

    char d5 = decodeSegmentGlyph(segs[5]);
    char d4 = decodeSegmentGlyph(segs[4]);
    char d3 = decodeSegmentGlyph(segs[3]);
    char d2 = decodeSegmentGlyph(segs[2]);
    char d1 = decodeSegmentGlyph(segs[1]);
    char d0 = decodeSegmentGlyph(segs[0]);

    std::ostringstream ss;
    ss << "[ " << d5 << " " << d4 << " " << d3 << " " << d2 << "   " << d1 << " " << d0 << " ]";
    return ss.str();
}

std::string Abc80Cli::formatDisplayRawHex() const
{
    uint8_t segs[6];
    for (int i = 0; i < 6; ++i) {
        segs[i] = _board.getPpi().getDigitSegment(static_cast<uint8_t>(i));
        if (segs[i] == 0) {
            segs[i] = _board.getBus().readByte(static_cast<uint16_t>(0x17C6 + i));
        }
    }

    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    ss << "[ "
       << std::setw(2) << static_cast<int>(segs[5]) << " "
       << std::setw(2) << static_cast<int>(segs[4]) << " "
       << std::setw(2) << static_cast<int>(segs[3]) << " "
       << std::setw(2) << static_cast<int>(segs[2]) << "   "
       << std::setw(2) << static_cast<int>(segs[1]) << " "
       << std::setw(2) << static_cast<int>(segs[0]) << " ]";
    return ss.str();
}

std::string Abc80Cli::formatRegisters() const
{
    const auto& cpu = _board.getCpu();
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');

    ss << "PC: " << std::setw(4) << cpu.getPC()
       << "  SP: " << std::setw(4) << cpu.getSP()
       << "  AF: " << std::setw(4) << cpu.getAF()
       << " [S:" << (cpu.getFlagS() ? '1' : '0')
       << " Z:" << (cpu.getFlagZ() ? '1' : '0')
       << " H:" << (cpu.getFlagH() ? '1' : '0')
       << " P:" << (cpu.getFlagPV() ? '1' : '0')
       << " N:" << (cpu.getFlagN() ? '1' : '0')
       << " C:" << (cpu.getFlagC() ? '1' : '0') << "]\n";

    ss << "BC: " << std::setw(4) << cpu.getBC()
       << "  DE: " << std::setw(4) << cpu.getDE()
       << "  HL: " << std::setw(4) << cpu.getHL()
       << "  IX: " << std::setw(4) << cpu.getIX()
       << "  IY: " << std::setw(4) << cpu.getIY() << "\n";

    ss << std::dec;
    ss << "Cycles: " << cpu.getCycles()
       << "  State: 0x" << std::hex << std::setw(2) << static_cast<int>(_board.getBus().readByte(0x17F4))
       << "  Adsave: 0x" << std::setw(4) << _board.getBus().readWord(0x17EE);

    return ss.str();
}

std::string Abc80Cli::disassembleInstruction(uint16_t addr, size_t* outLength) const
{
    const auto& bus = _board.getBus();
    uint8_t op0 = bus.readByte(addr);
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    ss << std::setw(4) << addr << ": ";

    size_t len = 1;

    switch (op0) {
        case 0x00:
            ss << "00          NOP";
            len = 1;
            break;
        case 0x76:
            ss << "76          HALT";
            len = 1;
            break;
        case 0x3E: {
            uint8_t n = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "3E " << std::setw(2) << static_cast<int>(n) << "       LD A, " << std::setw(2) << static_cast<int>(n) << "h";
            len = 2;
            break;
        }
        case 0x06: {
            uint8_t n = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "06 " << std::setw(2) << static_cast<int>(n) << "       LD B, " << std::setw(2) << static_cast<int>(n) << "h";
            len = 2;
            break;
        }
        case 0x0E: {
            uint8_t n = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "0E " << std::setw(2) << static_cast<int>(n) << "       LD C, " << std::setw(2) << static_cast<int>(n) << "h";
            len = 2;
            break;
        }
        case 0x16: {
            uint8_t n = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "16 " << std::setw(2) << static_cast<int>(n) << "       LD D, " << std::setw(2) << static_cast<int>(n) << "h";
            len = 2;
            break;
        }
        case 0x1E: {
            uint8_t n = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "1E " << std::setw(2) << static_cast<int>(n) << "       LD E, " << std::setw(2) << static_cast<int>(n) << "h";
            len = 2;
            break;
        }
        case 0x21: {
            uint16_t nn = bus.readWord(static_cast<uint16_t>(addr + 1));
            ss << "21 " << std::setw(2) << static_cast<int>(nn & 0xFF) << " " << std::setw(2) << static_cast<int>((nn >> 8) & 0xFF)
               << "    LD HL, " << std::setw(4) << nn << "h";
            len = 3;
            break;
        }
        case 0x31: {
            uint16_t nn = bus.readWord(static_cast<uint16_t>(addr + 1));
            ss << "31 " << std::setw(2) << static_cast<int>(nn & 0xFF) << " " << std::setw(2) << static_cast<int>((nn >> 8) & 0xFF)
               << "    LD SP, " << std::setw(4) << nn << "h";
            len = 3;
            break;
        }
        case 0xC3: {
            uint16_t nn = bus.readWord(static_cast<uint16_t>(addr + 1));
            ss << "C3 " << std::setw(2) << static_cast<int>(nn & 0xFF) << " " << std::setw(2) << static_cast<int>((nn >> 8) & 0xFF)
               << "    JP " << std::setw(4) << nn << "h";
            len = 3;
            break;
        }
        case 0xCD: {
            uint16_t nn = bus.readWord(static_cast<uint16_t>(addr + 1));
            ss << "CD " << std::setw(2) << static_cast<int>(nn & 0xFF) << " " << std::setw(2) << static_cast<int>((nn >> 8) & 0xFF)
               << "    CALL " << std::setw(4) << nn << "h";
            len = 3;
            break;
        }
        case 0xC9:
            ss << "C9          RET";
            len = 1;
            break;
        case 0x10: {
            int8_t disp = static_cast<int8_t>(bus.readByte(static_cast<uint16_t>(addr + 1)));
            uint16_t target = static_cast<uint16_t>(addr + 2 + disp);
            ss << "10 " << std::setw(2) << static_cast<int>(static_cast<uint8_t>(disp)) << "       DJNZ " << std::setw(4) << target << "h";
            len = 2;
            break;
        }
        case 0x18: {
            int8_t disp = static_cast<int8_t>(bus.readByte(static_cast<uint16_t>(addr + 1)));
            uint16_t target = static_cast<uint16_t>(addr + 2 + disp);
            ss << "18 " << std::setw(2) << static_cast<int>(static_cast<uint8_t>(disp)) << "       JR " << std::setw(4) << target << "h";
            len = 2;
            break;
        }
        case 0x28: {
            int8_t disp = static_cast<int8_t>(bus.readByte(static_cast<uint16_t>(addr + 1)));
            uint16_t target = static_cast<uint16_t>(addr + 2 + disp);
            ss << "28 " << std::setw(2) << static_cast<int>(static_cast<uint8_t>(disp)) << "       JR Z, " << std::setw(4) << target << "h";
            len = 2;
            break;
        }
        case 0x20: {
            int8_t disp = static_cast<int8_t>(bus.readByte(static_cast<uint16_t>(addr + 1)));
            uint16_t target = static_cast<uint16_t>(addr + 2 + disp);
            ss << "20 " << std::setw(2) << static_cast<int>(static_cast<uint8_t>(disp)) << "       JR NZ, " << std::setw(4) << target << "h";
            len = 2;
            break;
        }
        case 0xD3: {
            uint8_t p = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "D3 " << std::setw(2) << static_cast<int>(p) << "       OUT (" << std::setw(2) << static_cast<int>(p) << "h), A";
            len = 2;
            break;
        }
        case 0xDB: {
            uint8_t p = bus.readByte(static_cast<uint16_t>(addr + 1));
            ss << "DB " << std::setw(2) << static_cast<int>(p) << "       IN A, (" << std::setw(2) << static_cast<int>(p) << "h)";
            len = 2;
            break;
        }
        case 0xAF:
            ss << "AF          XOR A";
            len = 1;
            break;
        case 0x08:
            ss << "08          EX AF, AF'";
            len = 1;
            break;
        case 0xD9:
            ss << "D9          EXX";
            len = 1;
            break;
        case 0xEB:
            ss << "EB          EX DE, HL";
            len = 1;
            break;
        default:
            ss << std::setw(2) << static_cast<int>(op0) << "          DB " << std::setw(2) << static_cast<int>(op0) << "h";
            len = 1;
            break;
    }

    if (outLength) {
        *outLength = len;
    }

    return ss.str();
}

std::string Abc80Cli::disassembleRange(uint16_t startAddr, size_t count) const
{
    std::ostringstream ss;
    uint16_t curr = startAddr;
    for (size_t i = 0; i < count; ++i) {
        size_t len = 1;
        ss << disassembleInstruction(curr, &len) << "\n";
        curr = static_cast<uint16_t>(curr + len);
    }
    return ss.str();
}

std::string Abc80Cli::renderFrontPanelAnsi() const
{
    std::ostringstream ss;
    ss << "\033[1;36m+-------------------------------------------------------------+\033[0m\n";
    ss << "\033[1;36m|                    ABC-80 MICROCOMPUTER                     |\033[0m\n";
    ss << "\033[1;36m|                 Z80 HARDWARE EMULATOR & CLI                 |\033[0m\n";
    ss << "\033[1;36m+-------------------------------------------------------------+\033[0m\n";
    ss << "\033[1;33m|  LED DISPLAY:   " << std::left << std::setw(42) << formatDisplayString() << "|\033[0m\n";
    ss << "\033[1;36m+-------------------------------------------------------------+\033[0m\n";

    // Registers
    std::string regs = formatRegisters();
    std::istringstream regStream(regs);
    std::string line;
    while (std::getline(regStream, line)) {
        ss << "|  " << std::left << std::setw(59) << line << "|\n";
    }

    ss << "\033[1;36m+-------------------------------------------------------------+\033[0m\n";
    ss << "\033[1;32m|  DISASSEMBLY:                                               |\033[0m\n";

    uint16_t pc = _board.getCpu().getPC();
    uint16_t curr = pc;
    for (int i = 0; i < 4; ++i) {
        size_t len = 1;
        std::string dis = disassembleInstruction(curr, &len);
        std::string prefix = (curr == pc) ? "-> " : "   ";
        std::string fullLine = prefix + dis;
        ss << "|  " << std::left << std::setw(59) << fullLine << "|\n";
        curr = static_cast<uint16_t>(curr + len);
    }

    ss << "\033[1;36m+-------------------------------------------------------------+\033[0m\n";
    return ss.str();
}

std::string Abc80Cli::executeCommand(const std::string& cmdLine)
{
    std::istringstream iss(cmdLine);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty()) {
        return "";
    }

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (cmd == "step" || cmd == "s") {
        uint32_t t = step();
        std::ostringstream ss;
        ss << "Stepped " << t << " T-states. " << disassembleInstruction(_board.getCpu().getPC());
        return ss.str();
    } else if (cmd == "run" || cmd == "c" || cmd == "cont") {
        uint64_t maxT = 1000000ULL;
        iss >> maxT;
        uint64_t ran = run(maxT);
        std::ostringstream ss;
        ss << "Executed " << ran << " T-states. PC = 0x" << std::hex << std::uppercase << _board.getCpu().getPC();
        return ss.str();
    } else if (cmd == "break" || cmd == "b") {
        std::string addrStr;
        if (iss >> addrStr) {
            uint16_t addr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
            addBreakpoint(addr);
            return "Breakpoint set at 0x" + addrStr;
        }
        return "Error: missing address";
    } else if (cmd == "delete" || cmd == "d") {
        std::string addrStr;
        if (iss >> addrStr) {
            uint16_t addr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
            removeBreakpoint(addr);
            return "Breakpoint removed at 0x" + addrStr;
        }
        return "Error: missing address";
    } else if (cmd == "clear") {
        clearBreakpoints();
        return "All breakpoints cleared";
    } else if (cmd == "reset") {
        reset();
        return "Board reset";
    } else if (cmd == "reg") {
        return formatRegisters();
    } else if (cmd == "disp") {
        return formatDisplayString() + " " + formatDisplayRawHex();
    } else if (cmd == "peek") {
        std::string addrStr;
        if (iss >> addrStr) {
            uint16_t addr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
            size_t count = 16;
            iss >> count;
            std::ostringstream ss;
            ss << std::hex << std::uppercase << std::setfill('0');
            for (size_t i = 0; i < count; ++i) {
                if (i % 16 == 0) {
                    if (i > 0) ss << "\n";
                    ss << std::setw(4) << static_cast<uint16_t>(addr + i) << ": ";
                }
                ss << std::setw(2) << static_cast<int>(_board.getBus().readByte(static_cast<uint16_t>(addr + i))) << " ";
            }
            return ss.str();
        }
        return "Error: missing address";
    } else if (cmd == "poke") {
        std::string addrStr, valStr;
        if (iss >> addrStr >> valStr) {
            uint16_t addr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
            uint8_t val = static_cast<uint8_t>(std::stoul(valStr, nullptr, 16));
            _board.getBus().writeByte(addr, val);
            return "Wrote 0x" + valStr + " to 0x" + addrStr;
        }
        return "Error: usage poke <addr> <val>";
    } else if (cmd == "key" || cmd == "k") {
        char k;
        if (iss >> k) {
            if (injectKey(k)) {
                return std::string("Injected key '") + k + "'";
            }
            return std::string("Invalid key '") + k + "'";
        }
        return "Error: missing key character";
    } else if (cmd == "dis") {
        uint16_t addr = _board.getCpu().getPC();
        std::string addrStr;
        if (iss >> addrStr) {
            addr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
        }
        size_t count = 8;
        iss >> count;
        return disassembleRange(addr, count);
    } else if (cmd == "help" || cmd == "?") {
        return "Commands: step (s), run (c) [tstates], break (b) <hex>, delete (d) <hex>,\n"
               "          clear, reg, disp, peek <hex> [len], poke <hex> <val>, key (k) <c>,\n"
               "          dis [addr] [count], reset, quit (q)";
    } else if (cmd == "quit" || cmd == "q" || cmd == "exit") {
        return "QUIT";
    }

    return "Unknown command: " + cmd;
}

int Abc80Cli::runInteractive(std::istream& in, std::ostream& out)
{
    out << renderFrontPanelAnsi() << "\n";
    std::string line;

    while (true) {
        out << "\033[1;32mabc80> \033[0m";
        if (!std::getline(in, line)) {
            break;
        }

        std::string res = executeCommand(line);
        if (res == "QUIT") {
            break;
        }
        if (!res.empty()) {
            out << res << "\n";
        }
    }

    return 0;
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
