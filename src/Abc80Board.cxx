/*
 * Abc80Board.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define CHIPS_UTIL_IMPL
#include <util/z80dasm.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace abc80 {

namespace {

struct DasmContext {
    const Abc80Bus &bus;
    uint16_t currentAddr{0};
    std::vector<uint8_t> collectedBytes;
    std::string mnemonic;
};

uint8_t dasmInput(void *userData)
{
    auto *ctx = static_cast<DasmContext*>(userData);
    const uint8_t b = ctx->bus.readByte(ctx->currentAddr);
    ctx->collectedBytes.push_back(b);
    ctx->currentAddr = static_cast<uint16_t>(ctx->currentAddr + 1);
    return b;
}

void dasmOutput(char c, void *userData)
{
    auto *ctx = static_cast<DasmContext*>(userData);
    ctx->mnemonic.push_back(c);
}

} // namespace

Abc80Board::Abc80Board()
    : _bus()
    , _ppi()
    , _cpu(_bus, &_ppi)
    , _cassetteDeck(&_ppi)
{
    _ppi.setClockSource(_cpu.cyclesPtr());
    _ppi.setCassetteDeck(&_cassetteDeck);
    _ppi.beginFrame();
}

Abc80Board::~Abc80Board()
{
    detachCircuit();
}

void Abc80Board::attachCircuit(Abc80ExtCircuit *circuit) noexcept
{
    if (circuit == nullptr) {
        detachCircuit();
        return;
    }
    if (_extCircuit == circuit) {
        return;
    }
    detachCircuit();
    _extCircuit = circuit;
    _ppi.setExtCircuit(_extCircuit);
    _extCircuit->bindBoard(this, _cpu.cyclesPtr());
}

Abc80ExtCircuit *Abc80Board::attachCircuit(std::unique_ptr<Abc80ExtCircuit> circuit) noexcept
{
    if (!circuit) {
        detachCircuit();
        return nullptr;
    }
    detachCircuit();
    _ownedCircuit = std::move(circuit);
    _extCircuit = _ownedCircuit.get();
    _ppi.setExtCircuit(_extCircuit);
    _extCircuit->bindBoard(this, _cpu.cyclesPtr());
    return _extCircuit;
}

Abc80ExtCircuit *Abc80Board::attachCircuit(ExtCircuitId id)
{
    if (id == ExtCircuitId::None) {
        detachCircuit();
        return nullptr;
    }
    return attachCircuit(Abc80ExtCircuit::create(id));
}

void Abc80Board::detachCircuit() noexcept
{
    if (_extCircuit != nullptr) {
        Abc80ExtCircuit *prev = _extCircuit;
        _extCircuit = nullptr;
        _ppi.setExtCircuit(nullptr);
        prev->unbindBoard();
    }
    _ownedCircuit.reset();
}

ExtCircuitId Abc80Board::attachedCircuitId() const noexcept
{
    return _extCircuit != nullptr ? _extCircuit->id() : ExtCircuitId::None;
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
    if (_extCircuit != nullptr) {
        _extCircuit->onBoardReset(*_cpu.cyclesPtr());
    }
    _readySeen[0] = false;
    _readySeen[1] = false;
    return Abc80Status::OK;
}

Abc80Status Abc80Board::powerOnRom(RomId id, const std::string& root)
{
    const RomProfile& profile = romProfile(id);
    const std::string romPath = (root.empty() || root == ".")
        ? std::string(profile.relPath)
        : (root + "/" + profile.relPath);
    Abc80Status st = powerOn(romPath);
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
    if (_extCircuit != nullptr) {
        _extCircuit->onBoardReset(*_cpu.cyclesPtr());
    }
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
    if (_cassetteDeck.isActive()) {
        _cassetteDeck.step(*_cpu.cyclesPtr());
    }
    const uint64_t pins = _cpu.tick();
    if (_extCircuit != nullptr) {
        _extCircuit->step(*_cpu.cyclesPtr());
    }
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
    if (_extCircuit != nullptr) {
        _extCircuit->beginFrame(*_cpu.cyclesPtr());
    }
    for (uint32_t t = 0; t < ABC80_FRAME_TSTATES; ++t) {
        tickTracked();
    }
    _ppi.endFrame();
    if (_extCircuit != nullptr) {
        _extCircuit->endFrame(*_cpu.cyclesPtr());
    }
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
    if (!_cassetteDeck.isActive() && _extCircuit == nullptr) {
        return _cpu.stepTStates(tstates);
    }
    for (uint64_t i = 0; i < tstates; ++i) {
        tickTracked();
    }
    return tstates;
}

uint32_t Abc80Board::stepInstruction()
{
    uint32_t tstates = 0;
    do {
        tickTracked();
        tstates++;
        if (_cpu.isHalted() && tstates >= 4) {
            return tstates;
        }
    } while (!_cpu.isOpDone());
    return tstates;
}

uint32_t Abc80Board::finishInstruction()
{
    uint32_t tstates = 0;
    while (!_cpu.isOpDone() && !_cpu.isHalted()) {
        tickTracked();
        tstates++;
    }
    return tstates;
}

bool Abc80Board::stepUntil(const std::function<bool(const Abc80Board&)>& predicate,
                           uint64_t maxTStates)
{
    uint64_t executed = 0;
    while (executed < maxTStates) {
        if (predicate(*this)) {
            return true;
        }
        uint32_t step = stepInstruction();
        executed += step;
    }
    return predicate(*this);
}

std::vector<DisasmLine> Abc80Board::disassemble(uint16_t startAddr, size_t count) const
{
    std::vector<DisasmLine> lines;
    lines.reserve(count);
    uint16_t addr = startAddr;
    while (lines.size() < count) {
        DasmContext ctx{_bus, addr, {}, ""};
        const uint16_t nextAddr = z80dasm_op(addr, dasmInput, dasmOutput, &ctx);
        DisasmLine line;
        line.addr = addr;
        line.bytes = std::move(ctx.collectedBytes);
        line.mnemonic = std::move(ctx.mnemonic);
        lines.push_back(std::move(line));
        addr = nextAddr;
    }
    return lines;
}

std::vector<uint8_t> Abc80Board::readMemoryBlock(uint16_t startAddr, size_t count) const
{
    std::vector<uint8_t> block;
    block.reserve(count);
    uint16_t addr = startAddr;
    for (size_t i = 0; i < count; ++i) {
        block.push_back(_bus.readByte(addr));
        addr = static_cast<uint16_t>(addr + 1);
    }
    return block;
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
