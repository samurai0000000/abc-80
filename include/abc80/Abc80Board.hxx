/*
 * Abc80Board.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_BOARD_HXX
#define ABC80_BOARD_HXX

#include <abc80/Abc80Types.hxx>
#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <abc80/Abc80Cpu.hxx>
#include <functional>
#include <string>

namespace abc80 {

class Abc80Board {
public:
    Abc80Board();
    ~Abc80Board() = default;

    // Power on the ABC-80 board, initializing bus, loading ROM, and resetting CPU/PPI
    Abc80Status powerOn(const std::string& romPath = "vintage/rom.abc");

    // Perform hardware reset
    void reset();

    // Step clock for a given number of T-states (returns actual executed)
    uint64_t stepTStates(uint64_t tstates);

    // Step a single instruction
    uint32_t stepInstruction();

    // Step until predicate returns true or maxTStates exceeded
    bool stepUntil(const std::function<bool(const Abc80Board&)>& predicate,
                   uint64_t maxTStates = 1000000);

    // Accessors
    Abc80Cpu& getCpu() noexcept { return _cpu; }
    const Abc80Cpu& getCpu() const noexcept { return _cpu; }

    Abc80Bus& getBus() noexcept { return _bus; }
    const Abc80Bus& getBus() const noexcept { return _bus; }

    Abc80Ppi& getPpi() noexcept { return _ppi; }
    const Abc80Ppi& getPpi() const noexcept { return _ppi; }

    // Single-step hardware trap
    void setSingleStep(bool enable) noexcept { _cpu.setSingleStepTrap(enable); }
    bool isSingleStepEnabled() const noexcept { return _cpu.isSingleStepTrapEnabled(); }

    // Keypad interface
    void pressKey(uint8_t keyCode) { _ppi.pressKey(keyCode); }
    void releaseKey(uint8_t keyCode) { _ppi.releaseKey(keyCode); }
    void clearKeys() { _ppi.clearKeys(); }

private:
    Abc80Bus _bus;
    Abc80Ppi _ppi;
    Abc80Cpu _cpu;
};

} // namespace abc80

#endif /* ABC80_BOARD_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
