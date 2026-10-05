/*
 * Abc80Cpu.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_CPU_HXX
#define ABC80_CPU_HXX

#include <abc80/Abc80Types.hxx>
#include <cstdint>
#include <functional>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <chips/z80.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace abc80 {

class Abc80Bus;
class Abc80Ppi;

class Abc80Cpu {
public:
    using IoReadHook = std::function<uint8_t(uint8_t port)>;
    using IoWriteHook = std::function<void(uint8_t port, uint8_t val)>;

    explicit Abc80Cpu(Abc80Bus& bus, Abc80Ppi* ppi = nullptr);
    ~Abc80Cpu() = default;

    // Reset CPU registers, internal step, and bus pins
    void reset();

    // Force program counter to specific address
    void prefetch(uint16_t pc);

    // Step a single clock cycle (T-state). Returns the active bus pins.
    uint64_t tick();

    // Step until the current instruction completes (z80_opdone).
    // Returns the number of T-states consumed.
    uint32_t stepInstruction();

    // Step at least minTStates. Returns actual T-states executed.
    uint64_t stepTStates(uint64_t minTStates);

    // Pin inspection and manipulation
    uint64_t getPins() const noexcept { return _pins; }
    bool isHalted() const noexcept { return (_pins & Z80_HALT) != 0; }
    uint64_t getCycles() const noexcept { return _totalCycles; }

    // Interrupt line controls
    void setInterruptLine(bool active) noexcept;
    bool isInterruptLineActive() const noexcept { return _interruptPending; }
    void requestNmi() noexcept;

    // Single-step hardware trap (/M1 -> /INT flip-flop)
    void setSingleStepTrap(bool enable) noexcept { _singleStepTrap = enable; }
    bool isSingleStepTrapEnabled() const noexcept { return _singleStepTrap; }

    // I/O hooks (optional overrides when PPI is not directly attached)
    void setIoHooks(IoReadHook readHook, IoWriteHook writeHook) {
        _ioReadHook = std::move(readHook);
        _ioWriteHook = std::move(writeHook);
    }

    void setPpi(Abc80Ppi* ppi) noexcept { _ppi = ppi; }
    Abc80Ppi* getPpi() const noexcept { return _ppi; }
    Abc80Bus& getBus() noexcept { return _bus; }

    // Register accessors (Primary bank)
    uint16_t getPC() const noexcept { return Z80_GET_ADDR(_pins); }
    void setPC(uint16_t pc) noexcept;

    uint16_t getSP() const noexcept { return _cpu.sp; }
    void setSP(uint16_t sp) noexcept { _cpu.sp = sp; }

    uint16_t getAF() const noexcept { return _cpu.af; }
    void setAF(uint16_t af) noexcept { _cpu.af = af; }

    uint16_t getBC() const noexcept { return _cpu.bc; }
    void setBC(uint16_t bc) noexcept { _cpu.bc = bc; }

    uint16_t getDE() const noexcept { return _cpu.de; }
    void setDE(uint16_t de) noexcept { _cpu.de = de; }

    uint16_t getHL() const noexcept { return _cpu.hl; }
    void setHL(uint16_t hl) noexcept { _cpu.hl = hl; }

    uint16_t getIX() const noexcept { return _cpu.ix; }
    void setIX(uint16_t ix) noexcept { _cpu.ix = ix; }

    uint16_t getIY() const noexcept { return _cpu.iy; }
    void setIY(uint16_t iy) noexcept { _cpu.iy = iy; }

    uint8_t getA() const noexcept { return _cpu.a; }
    void setA(uint8_t a) noexcept { _cpu.a = a; }

    uint8_t getF() const noexcept { return _cpu.f; }
    void setF(uint8_t f) noexcept { _cpu.f = f; }

    uint8_t getB() const noexcept { return _cpu.b; }
    void setB(uint8_t b) noexcept { _cpu.b = b; }

    uint8_t getC() const noexcept { return _cpu.c; }
    void setC(uint8_t c) noexcept { _cpu.c = c; }

    uint8_t getD() const noexcept { return _cpu.d; }
    void setD(uint8_t d) noexcept { _cpu.d = d; }

    uint8_t getE() const noexcept { return _cpu.e; }
    void setE(uint8_t e) noexcept { _cpu.e = e; }

    uint8_t getH() const noexcept { return _cpu.h; }
    void setH(uint8_t h) noexcept { _cpu.h = h; }

    uint8_t getL() const noexcept { return _cpu.l; }
    void setL(uint8_t l) noexcept { _cpu.l = l; }

    uint8_t getI() const noexcept { return _cpu.i; }
    void setI(uint8_t i) noexcept { _cpu.i = i; }

    uint8_t getR() const noexcept { return _cpu.r; }
    void setR(uint8_t r) noexcept { _cpu.r = r; }

    // Shadow register bank
    uint16_t getAF2() const noexcept { return _cpu.af2; }
    void setAF2(uint16_t af2) noexcept { _cpu.af2 = af2; }

    uint16_t getBC2() const noexcept { return _cpu.bc2; }
    void setBC2(uint16_t bc2) noexcept { _cpu.bc2 = bc2; }

    uint16_t getDE2() const noexcept { return _cpu.de2; }
    void setDE2(uint16_t de2) noexcept { _cpu.de2 = de2; }

    uint16_t getHL2() const noexcept { return _cpu.hl2; }
    void setHL2(uint16_t hl2) noexcept { _cpu.hl2 = hl2; }

    // Interrupt & status state
    uint8_t getIM() const noexcept { return _cpu.im; }
    void setIM(uint8_t im) noexcept { _cpu.im = im; }

    bool getIFF1() const noexcept { return _cpu.iff1; }
    void setIFF1(bool iff1) noexcept { _cpu.iff1 = iff1; }

    bool getIFF2() const noexcept { return _cpu.iff2; }
    void setIFF2(bool iff2) noexcept { _cpu.iff2 = iff2; }

    // All 8 Z80 Flags (S, Z, Y, H, X, P/V, N, C)
    bool getFlagC() const noexcept { return (_cpu.f & Z80_CF) != 0; }
    bool getFlagN() const noexcept { return (_cpu.f & Z80_NF) != 0; }
    bool getFlagPV() const noexcept { return (_cpu.f & Z80_VF) != 0; }
    bool getFlagX() const noexcept { return (_cpu.f & Z80_XF) != 0; }
    bool getFlagH() const noexcept { return (_cpu.f & Z80_HF) != 0; }
    bool getFlagY() const noexcept { return (_cpu.f & Z80_YF) != 0; }
    bool getFlagZ() const noexcept { return (_cpu.f & Z80_ZF) != 0; }
    bool getFlagS() const noexcept { return (_cpu.f & Z80_SF) != 0; }

    void setFlagC(bool v) noexcept { setFlagBit(Z80_CF, v); }
    void setFlagN(bool v) noexcept { setFlagBit(Z80_NF, v); }
    void setFlagPV(bool v) noexcept { setFlagBit(Z80_VF, v); }
    void setFlagX(bool v) noexcept { setFlagBit(Z80_XF, v); }
    void setFlagH(bool v) noexcept { setFlagBit(Z80_HF, v); }
    void setFlagY(bool v) noexcept { setFlagBit(Z80_YF, v); }
    void setFlagZ(bool v) noexcept { setFlagBit(Z80_ZF, v); }
    void setFlagS(bool v) noexcept { setFlagBit(Z80_SF, v); }

    // Direct access to internal z80_t
    z80_t& getRawCpu() noexcept { return _cpu; }
    const z80_t& getRawCpu() const noexcept { return _cpu; }

private:
    void setFlagBit(uint8_t mask, bool val) noexcept {
        if (val) {
            _cpu.f |= mask;
        } else {
            _cpu.f &= static_cast<uint8_t>(~mask);
        }
    }

    Abc80Bus& _bus;
    Abc80Ppi* _ppi;
    z80_t _cpu;
    uint64_t _pins;
    uint64_t _totalCycles;
    bool _interruptPending;
    bool _singleStepTrap;

    IoReadHook _ioReadHook;
    IoWriteHook _ioWriteHook;
};

} // namespace abc80

#endif /* ABC80_CPU_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
