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
    _ppi.setClockSource(_cpu.cyclesPtr());
    _ppi.beginFrame();
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
    _ppi.beginFrame();
    _readySeen[0] = false;
    _readySeen[1] = false;
    return Abc80Status::OK;
}

Abc80Status Abc80Board::powerOnRom(RomId id, const std::string& root)
{
    const RomProfile& profile = romProfile(id);
    Abc80Status st = powerOn(root + "/" + profile.relPath);
    if (st != Abc80Status::OK) {
        return st;
    }
    _bus.writeByte(ABC80_PWUP_ADDR, ABC80_PWUP_CODE);
    return Abc80Status::OK;
}

void Abc80Board::reset()
{
    _ppi.reset();
    _cpu.reset();
    _ppi.beginFrame();
    _readySeen[0] = false;
    _readySeen[1] = false;
}

namespace {

// Port B hardware bit order is handled in the PPI; here only canonical masks exist.
struct Glyph {
    uint8_t mask;
    char ch;
};

constexpr Glyph kGlyphs[] = {
    { 0x3F, '0' }, { 0x06, '1' }, { 0x5B, '2' }, { 0x4F, '3' }, { 0x66, '4' }, { 0x6D, '5' },
    { 0x7D, '6' }, { 0x07, '7' }, { 0x7F, '8' }, { 0x6F, '9' }, { 0x77, 'A' }, { 0x7C, 'b' },
    { 0x39, 'C' }, { 0x5E, 'd' }, { 0x79, 'E' }, { 0x71, 'F' }, { 0x40, '-' }, { 0x00, ' ' },
};

} // namespace

void Abc80Board::tickTracked()
{
    const uint64_t pins = _cpu.tick();
    if ((pins & Z80_M1) && (pins & Z80_MREQ) && (pins & Z80_RD)) {
        const uint16_t addr = Z80_GET_ADDR(pins);
        _lastFetch = addr;
        if (addr == romProfile(RomId::Monitor).readyAddr) {
            _readySeen[static_cast<int>(RomId::Monitor)] = true;
        }
        if (addr == romProfile(RomId::Vintage1993).readyAddr) {
            _readySeen[static_cast<int>(RomId::Vintage1993)] = true;
        }
    }
}

uint64_t Abc80Board::stepFrame()
{
    _ppi.beginFrame();
    for (uint32_t t = 0; t < ABC80_FRAME_TSTATES; ++t) {
        tickTracked();
    }
    _ppi.endFrame();
    return ABC80_FRAME_TSTATES;
}

bool Abc80Board::bootTurbo(RomId id, uint64_t maxTStates)
{
    for (uint64_t t = 0; t < maxTStates; ++t) {
        tickTracked();
        if (_readySeen[static_cast<int>(id)]) {
            return true;
        }
    }
    return false;
}

bool Abc80Board::reachedReady(RomId id) const noexcept
{
    return _readySeen[static_cast<int>(id)];
}

std::array<uint8_t, 6> Abc80Board::displayMasks(double litDutyMin) const
{
    std::array<uint8_t, 6> masks{};
    const double elapsed = static_cast<double>(_ppi.frameElapsedTStates());
    if (elapsed <= 0.0) {
        return masks;
    }
    const double threshold = litDutyMin * elapsed;
    for (int i = 0; i < 6; ++i) {
        const uint8_t ppiDigit = static_cast<uint8_t>(5 - i);  // PPI digit 0 is the rightmost
        uint8_t mask = 0;
        for (uint8_t seg = 0; seg < 8; ++seg) {
            const uint32_t on = _ppi.digitSegmentOnTStates(ppiDigit, seg);
            if (on > 0 && static_cast<double>(on) >= threshold) {
                mask = static_cast<uint8_t>(mask | (1u << seg));
            }
        }
        masks[static_cast<size_t>(i)] = mask;
    }
    return masks;
}

char Abc80Board::glyphForMask(uint8_t canonicalMask) noexcept
{
    const uint8_t base = static_cast<uint8_t>(canonicalMask & 0x7F);  // the decimal point is not part of the glyph
    for (const Glyph &g : kGlyphs) {
        if (g.mask == base) {
            return g.ch;
        }
    }
    return '?';
}

std::array<char, 6> Abc80Board::displayDigits(double litDutyMin) const
{
    std::array<char, 6> out{};
    const std::array<uint8_t, 6> masks = displayMasks(litDutyMin);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = glyphForMask(masks[i]);
    }
    return out;
}

double Abc80Board::speakerLedFraction() const noexcept
{
    const uint32_t elapsed = _ppi.frameElapsedTStates();
    if (elapsed == 0) {
        return 0.0;
    }
    return static_cast<double>(_ppi.frameAudio().lowTStates) / static_cast<double>(elapsed);
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
