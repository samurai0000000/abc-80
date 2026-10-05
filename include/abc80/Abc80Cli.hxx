/*
 * Abc80Cli.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_CLI_HXX
#define ABC80_CLI_HXX

#include <abc80/Abc80Board.hxx>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <iostream>

namespace abc80 {

class Abc80Cli {
public:
    explicit Abc80Cli(Abc80Board& board);
    ~Abc80Cli() = default;

    Abc80Board& getBoard() noexcept { return _board; }
    const Abc80Board& getBoard() const noexcept { return _board; }

    // Breakpoint Management
    void addBreakpoint(uint16_t addr);
    void removeBreakpoint(uint16_t addr);
    void clearBreakpoints();
    bool hasBreakpoint(uint16_t addr) const;
    const std::unordered_set<uint16_t>& getBreakpoints() const noexcept { return _breakpoints; }

    // Stepping & Execution Control
    uint32_t step();
    uint64_t run(uint64_t maxTStates = 5000000ULL);
    void reset();

    // Keypad Interaction
    bool injectKey(char keyChar);
    bool injectKeyCode(uint8_t keyCode);

    // 7-Segment Display Formatting
    static char decodeSegmentGlyph(uint8_t mask) noexcept;
    std::string formatDisplayString() const;
    std::string formatDisplayRawHex() const;

    // Terminal Inspection & Disassembly
    std::string formatRegisters() const;
    std::string disassembleInstruction(uint16_t addr, size_t* outLength = nullptr) const;
    std::string disassembleRange(uint16_t startAddr, size_t count) const;
    std::string renderFrontPanelAnsi() const;

    // Command Interpreter
    std::string executeCommand(const std::string& cmdLine);
    int runInteractive(std::istream& in = std::cin, std::ostream& out = std::cout);

private:
    Abc80Board& _board;
    std::unordered_set<uint16_t> _breakpoints;
    bool _running;

    static uint8_t charToKeyCode(char c);
};

} // namespace abc80

#endif // ABC80_CLI_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
