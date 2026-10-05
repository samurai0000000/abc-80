/*
 * Abc80Board.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>

namespace abc80 {

Abc80Board::Abc80Board()
    : _bus()
    , _ppi()
    , _cpu(_bus, &_ppi)
{
}

Abc80Status Abc80Board::powerOn(const std::string& romPath)
{
    _bus.reset();
    _ppi.reset();
    Abc80Status st = _bus.loadRomFile(romPath.c_str());
    if (st != Abc80Status::OK) {
        return st;
    }
    _cpu.reset();
    return Abc80Status::OK;
}

void Abc80Board::reset()
{
    _ppi.reset();
    _cpu.reset();
}

uint64_t Abc80Board::stepTStates(uint64_t tstates)
{
    return _cpu.stepTStates(tstates);
}

uint32_t Abc80Board::stepInstruction()
{
    return _cpu.stepInstruction();
}

bool Abc80Board::stepUntil(const std::function<bool(const Abc80Board&)>& predicate,
                           uint64_t maxTStates)
{
    uint64_t executed = 0;
    while (executed < maxTStates) {
        if (predicate(*this)) {
            return true;
        }
        uint32_t step = _cpu.stepInstruction();
        executed += step;
    }
    return predicate(*this);
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
