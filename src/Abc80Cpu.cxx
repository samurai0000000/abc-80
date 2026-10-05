/*
 * Abc80Cpu.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Ppi.hxx>

#define CHIPS_IMPL
#include <abc80/Abc80Cpu.hxx>

namespace abc80 {

Abc80Cpu::Abc80Cpu(Abc80Bus& bus, Abc80Ppi* ppi)
    : _bus(bus)
    , _ppi(ppi)
    , _pins(0)
    , _totalCycles(0)
    , _interruptPending(false)
    , _singleStepTrap(false)
{
    reset();
}

void Abc80Cpu::reset()
{
    _pins = z80_reset(&_cpu);
    _totalCycles = 0;
    _interruptPending = false;
    tick();
    _totalCycles = 0;
}

void Abc80Cpu::prefetch(uint16_t pc)
{
    _pins = z80_prefetch(&_cpu, pc);
    tick();
}

void Abc80Cpu::setPC(uint16_t pc) noexcept
{
    prefetch(pc);
}

void Abc80Cpu::setInterruptLine(bool active) noexcept
{
    _interruptPending = active;
}

void Abc80Cpu::requestNmi() noexcept
{
    _pins |= Z80_NMI;
}

uint64_t Abc80Cpu::tick()
{
    if (_interruptPending) {
        _pins |= Z80_INT;
    } else {
        _pins &= ~Z80_INT;
    }

    _pins = z80_tick(&_cpu, _pins);
    _totalCycles++;

    if (_pins & Z80_MREQ) {
        uint16_t addr = Z80_GET_ADDR(_pins);
        if (_pins & Z80_RD) {
            uint8_t data = _bus.readByte(addr);
            Z80_SET_DATA(_pins, data);
        } else if (_pins & Z80_WR) {
            uint8_t data = Z80_GET_DATA(_pins);
            _bus.writeByte(addr, data);
        }
    } else if (_pins & Z80_IORQ) {
        uint16_t port = Z80_GET_ADDR(_pins);
        if (_pins & Z80_M1) {
            // Interrupt acknowledge cycle: provide 0xFF (RST 38H / Mode 1 response)
            Z80_SET_DATA(_pins, 0xFF);
            _interruptPending = false;
        } else if (_pins & Z80_RD) {
            uint8_t val = 0xFF;
            if (_ppi != nullptr) {
                val = _ppi->readPort(static_cast<uint8_t>(port & 0xFF));
            } else if (_ioReadHook) {
                val = _ioReadHook(static_cast<uint8_t>(port & 0xFF));
            }
            Z80_SET_DATA(_pins, val);
        } else if (_pins & Z80_WR) {
            uint8_t val = Z80_GET_DATA(_pins);
            if (_ppi != nullptr) {
                _ppi->writePort(static_cast<uint8_t>(port & 0xFF), val);
            } else if (_ioWriteHook) {
                _ioWriteHook(static_cast<uint8_t>(port & 0xFF), val);
            }
        }
    }

    // Single-step hardware trap: if enabled and an instruction just completed, assert INT
    if (_singleStepTrap && z80_opdone(&_cpu)) {
        _interruptPending = true;
    }

    return _pins;
}

uint32_t Abc80Cpu::stepInstruction()
{
    uint32_t tstates = 0;
    do {
        tick();
        tstates++;
        if (isHalted() && tstates >= 4) {
            return tstates;
        }
    } while (!z80_opdone(&_cpu));
    return tstates;
}

uint64_t Abc80Cpu::stepTStates(uint64_t minTStates)
{
    uint64_t count = 0;
    while (count < minTStates) {
        tick();
        count++;
    }
    return count;
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
