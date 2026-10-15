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
#include <abc80/Abc80Rom.hxx>
#include <abc80/Abc80Tape.hxx>
#include <abc80/Abc80ExtCircuit.hxx>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace abc80 {

struct DisasmLine {
    uint16_t addr{0};
    std::vector<uint8_t> bytes;
    std::string mnemonic;
};

class Abc80Board {
public:
    Abc80Board();
    ~Abc80Board();

    // Power on the ABC-80 board, initializing bus, loading ROM, and resetting CPU/PPI
    Abc80Status powerOn(const std::string& romPath = "vintage/rom.abc");

    // Power on with one of the two ROM profiles. The ROM is read from
    // root + "/" + profile.relPath, and the warm-start flag (pwup, 0x17F5)
    // is set to 0x80 after RAM is cleared.
    Abc80Status powerOnRom(RomId id, const std::string& root = ".");

    // Perform hardware reset
    void reset();

    // ---- Front-panel observation, frame by frame (1 frame = ABC80_FRAME_TSTATES of emulated time) ----

    // Runs exactly one frame T-state by T-state and finalizes the PPI's per-frame accounting.
    // Returns the T-states executed (ABC80_FRAME_TSTATES).
    uint64_t stepFrame();

    // Runs unpaced until the ROM's steady-state opcode fetch (romProfile(id).readyAddr) is seen,
    // or maxTStates pass. Returns true when it was seen.
    bool bootTurbo(RomId id, uint64_t maxTStates);

    // Address of the most recent opcode fetch (M1 cycle): the instruction currently executing or just
    // executed. Exact at any T-state, unlike the address bus or the core's raw PC register.
    uint16_t lastFetchAddress() const noexcept { return _lastFetch; }

    // True once the ROM's ready address has been fetched since the last power-on or reset.
    bool reachedReady(RomId id) const noexcept;

    // Display, left to right (index 0 = leftmost digit): canonical segment masks (bit 0=a .. 6=g, 7=dp)
    // of the segments lit for at least litDutyMin of the last frame. Port B's active-low polarity
    // is handled here and nowhere else.
    std::array<uint8_t, 6> displayMasks(double litDutyMin) const;
    std::array<char, 6> displayDigits(double litDutyMin) const;

    // Character for a canonical mask (decimal point ignored), or '?' when it is not a glyph.
    static char glyphForMask(uint8_t canonicalMask) noexcept;

    const Abc80Ppi::FrameAudio& frameAudio() const noexcept { return _ppi.frameAudio(); }

    // Share (0-1) of the last frame that the speaker line (Port C bit 7) was low: the green LED.
    double speakerLedFraction() const noexcept;
    bool haltLed() const noexcept { return _cpu.isHalted(); }
    bool epLed() const noexcept { return (_ppi.auxPortCOutput() & 0x60) == 0x60; }

    // Step clock for a given number of T-states (returns actual executed)
    uint64_t stepTStates(uint64_t tstates);

    // Step a single instruction
    uint32_t stepInstruction();

    // Finish in-flight instruction if mid-cycle (returns T-states ticked, 0 if already at boundary)
    uint32_t finishInstruction();

    // Step until predicate returns true or maxTStates exceeded
    bool stepUntil(const std::function<bool(const Abc80Board&)>& predicate,
                   uint64_t maxTStates = 1000000);

    // Disassemble up to count instructions starting at startAddr
    std::vector<DisasmLine> disassemble(uint16_t startAddr, size_t count) const;

    // Read a contiguous block of bytes from the memory bus (safe, side-effect free)
    std::vector<uint8_t> readMemoryBlock(uint16_t startAddr, size_t count) const;

    // Accessors
    Abc80Cpu& getCpu() noexcept { return _cpu; }
    const Abc80Cpu& getCpu() const noexcept { return _cpu; }

    Abc80Bus& getBus() noexcept { return _bus; }
    const Abc80Bus& getBus() const noexcept { return _bus; }
    Abc80Bus& memory() noexcept { return _bus; }
    const Abc80Bus& memory() const noexcept { return _bus; }

    Abc80Ppi& getPpi() noexcept { return _ppi; }
    const Abc80Ppi& getPpi() const noexcept { return _ppi; }

    CassetteDeck& getCassetteDeck() noexcept { return _cassetteDeck; }
    const CassetteDeck& getCassetteDeck() const noexcept { return _cassetteDeck; }

    // Mutually-exclusive external I/O circuit attachment (Exercises 24-29)
    void attachCircuit(Abc80ExtCircuit *circuit) noexcept;
    Abc80ExtCircuit *attachCircuit(std::unique_ptr<Abc80ExtCircuit> circuit) noexcept;
    Abc80ExtCircuit *attachCircuit(ExtCircuitId id);
    void detachCircuit() noexcept;

    Abc80ExtCircuit *attachedCircuit() noexcept { return _extCircuit; }
    const Abc80ExtCircuit *attachedCircuit() const noexcept { return _extCircuit; }
    ExtCircuitId attachedCircuitId() const noexcept;

    template <typename T>
    T *attachedCircuitAs() noexcept
    {
        return dynamic_cast<T *>(_extCircuit);
    }

    template <typename T>
    const T *attachedCircuitAs() const noexcept
    {
        return dynamic_cast<const T *>(_extCircuit);
    }

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
    CassetteDeck _cassetteDeck;
    std::unique_ptr<Abc80ExtCircuit> _ownedCircuit;
    Abc80ExtCircuit *_extCircuit{nullptr};
    bool _readySeen[2]{ false, false };  // indexed by RomId
    uint16_t _lastFetch{0};
    void tickTracked();
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
