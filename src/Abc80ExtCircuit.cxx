/*
 * Abc80ExtCircuit.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80ExtCircuit.hxx>
#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <algorithm>

namespace abc80 {

const char *extCircuitIdName(ExtCircuitId id) noexcept
{
    switch (id) {
    case ExtCircuitId::None:
        return "none";
    case ExtCircuitId::TrafficLight:
        return "traffic_light";
    case ExtCircuitId::LedChaser:
        return "led_chaser";
    case ExtCircuitId::Oscilloscope:
        return "oscilloscope";
    case ExtCircuitId::SchoolBell:
        return "school_bell";
    case ExtCircuitId::ApplianceControl:
        return "appliance_control";
    case ExtCircuitId::RoboticArm:
        return "robotic_arm";
    }
    return "none";
}

bool parseExtCircuitId(const std::string &name, ExtCircuitId &out) noexcept
{
    if (name == "none") {
        out = ExtCircuitId::None;
        return true;
    }
    if (name == "traffic_light") {
        out = ExtCircuitId::TrafficLight;
        return true;
    }
    if (name == "led_chaser") {
        out = ExtCircuitId::LedChaser;
        return true;
    }
    if (name == "oscilloscope") {
        out = ExtCircuitId::Oscilloscope;
        return true;
    }
    if (name == "school_bell") {
        out = ExtCircuitId::SchoolBell;
        return true;
    }
    if (name == "appliance_control") {
        out = ExtCircuitId::ApplianceControl;
        return true;
    }
    if (name == "robotic_arm") {
        out = ExtCircuitId::RoboticArm;
        return true;
    }
    return false;
}

Abc80ExtCircuit::Abc80ExtCircuit(ExtCircuitId id) noexcept
    : _id(id)
{
}

Abc80ExtCircuit::~Abc80ExtCircuit()
{
    if (_board != nullptr) {
        _board->detachCircuit();
    }
}

void Abc80ExtCircuit::reset() noexcept
{
    const uint64_t now = currentTStates();
    _frameStartTStates = now;
    _lastEventTStates = now;
    _frameElapsed = 0;
    onReset(_board != nullptr ? &_board->getPpi() : nullptr, now);
}

void Abc80ExtCircuit::bindBoard(Abc80Board *board, const uint64_t *clock) noexcept
{
    _board = board;
    _clock = clock;
    const uint64_t now = currentTStates();
    _frameStartTStates = now;
    _lastEventTStates = now;
    _frameElapsed = 0;
    if (_board != nullptr) {
        onAttach(_board->getPpi(), now);
    }
}

void Abc80ExtCircuit::unbindBoard() noexcept
{
    if (_board != nullptr) {
        onDetach();
    }
    _board = nullptr;
    _clock = nullptr;
    _frameStartTStates = 0;
    _lastEventTStates = 0;
    _frameElapsed = 0;
}

void Abc80ExtCircuit::onBoardReset(uint64_t nowTStates) noexcept
{
    _frameStartTStates = nowTStates;
    _lastEventTStates = nowTStates;
    _frameElapsed = 0;
    onReset(_board != nullptr ? &_board->getPpi() : nullptr, nowTStates);
}

void Abc80ExtCircuit::beginFrame(uint64_t nowTStates) noexcept
{
    _frameStartTStates = nowTStates;
    _lastEventTStates = nowTStates;
    _frameElapsed = 0;
    onFrameBegin();
}

void Abc80ExtCircuit::step(uint64_t nowTStates) noexcept
{
    flushUpTo(nowTStates);
}

void Abc80ExtCircuit::endFrame(uint64_t nowTStates) noexcept
{
    flushUpTo(nowTStates);
    _frameElapsed = (nowTStates >= _frameStartTStates)
        ? static_cast<uint32_t>(nowTStates - _frameStartTStates)
        : 0;
    onFrameEnd(_frameElapsed);
}

void Abc80ExtCircuit::onPortWrite(uint8_t port, uint8_t val, uint64_t nowTStates) noexcept
{
    flushUpTo(nowTStates);
    if (_board != nullptr) {
        handlePortWrite(port, val, _board->getPpi(), nowTStates);
    }
}

std::unique_ptr<Abc80ExtCircuit> Abc80ExtCircuit::create(ExtCircuitId id)
{
    switch (id) {
    case ExtCircuitId::None:
        return nullptr;
    case ExtCircuitId::TrafficLight:
        return std::make_unique<Abc80TrafficLight>();
    case ExtCircuitId::LedChaser:
        return std::make_unique<Abc80LedChaser>();
    case ExtCircuitId::Oscilloscope:
        return std::make_unique<Abc80Oscilloscope>();
    case ExtCircuitId::SchoolBell:
        return std::make_unique<Abc80SchoolBell>();
    case ExtCircuitId::ApplianceControl:
        return std::make_unique<Abc80ApplianceControl>();
    case ExtCircuitId::RoboticArm:
        return std::make_unique<Abc80RoboticArm>();
    }
    return nullptr;
}

void Abc80ExtCircuit::flushUpTo(uint64_t nowTStates) noexcept
{
    if (nowTStates > _lastEventTStates) {
        accumulate(nowTStates - _lastEventTStates);
        _lastEventTStates = nowTStates;
    } else if (nowTStates < _lastEventTStates) {
        _lastEventTStates = nowTStates;
    }
}

// ---- Exercise 24: Abc80TrafficLight ----

Abc80TrafficLight::Abc80TrafficLight() noexcept
    : Abc80ExtCircuit(ExtCircuitId::TrafficLight)
{
    _lampOnTStates.fill(0);
    _lampDuty.fill(0.0);
}

void Abc80TrafficLight::syncFromPpi(const Abc80Ppi *ppi) noexcept
{
    if (ppi != nullptr && (ppi->getPrimaryControl() & 0x82) == 0x80) {
        _portB = static_cast<uint8_t>(ppi->getSegmentOutput() & 0x3F);
    } else {
        _portB = 0;
    }
}

void Abc80TrafficLight::onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    _lampOnTStates.fill(0);
    _lampDuty.fill(0.0);
    _transitionCount = 0;
    syncFromPpi(&ppi);
}

void Abc80TrafficLight::onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    _lampOnTStates.fill(0);
    _lampDuty.fill(0.0);
    _transitionCount = 0;
    syncFromPpi(ppi);
}

void Abc80TrafficLight::accumulate(uint64_t elapsedTStates) noexcept
{
    for (size_t i = 0; i < 6; ++i) {
        if ((_portB & (1u << i)) != 0) {
            _lampOnTStates[i] += elapsedTStates;
        }
    }
}

void Abc80TrafficLight::onFrameBegin() noexcept
{
    _lampOnTStates.fill(0);
}

void Abc80TrafficLight::onFrameEnd(uint32_t frameElapsedTStates) noexcept
{
    if (frameElapsedTStates == 0) {
        _lampDuty.fill(0.0);
        return;
    }
    const double denom = static_cast<double>(frameElapsedTStates);
    for (size_t i = 0; i < 6; ++i) {
        const double ratio = static_cast<double>(_lampOnTStates[i]) / denom;
        _lampDuty[i] = std::clamp(ratio, 0.0, 1.0);
    }
}

void Abc80TrafficLight::handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    if ((port & 0xFC) != 0x80) {
        return;
    }
    const uint8_t reg = static_cast<uint8_t>(port & 0x03);
    if (reg == 3) {
        syncFromPpi(&ppi);
        return;
    }
    if (reg == 1) {
        if ((ppi.getPrimaryControl() & 0x82) == 0x80) {
            const uint8_t next = static_cast<uint8_t>(val & 0x3F);
            if (next != _portB && next != 0) {
                ++_transitionCount;
            }
            _portB = next;
        } else {
            _portB = 0;
        }
    }
}

// ---- Exercise 25: Abc80LedChaser ----

Abc80LedChaser::Abc80LedChaser() noexcept
    : Abc80ExtCircuit(ExtCircuitId::LedChaser)
{
    _ledOnTStates.fill(0);
    _ledDuty.fill(0.0);
}

int Abc80LedChaser::activeLedIndex() const noexcept
{
    for (int i = 0; i < 8; ++i) {
        if ((_activeMask & (1u << i)) != 0) {
            return i;
        }
    }
    return -1;
}

void Abc80LedChaser::syncFromPpi(const Abc80Ppi *ppi) noexcept
{
    if (ppi != nullptr && (ppi->getPrimaryControl() & 0x82) == 0x80) {
        _rawPortB = ppi->getSegmentOutput();
        _activeMask = static_cast<uint8_t>(~_rawPortB);
    } else {
        _rawPortB = 0xFF;
        _activeMask = 0x00;
    }
}

void Abc80LedChaser::onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    _ledOnTStates.fill(0);
    _ledDuty.fill(0.0);
    _stepCount = 0;
    syncFromPpi(&ppi);
}

void Abc80LedChaser::onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    _ledOnTStates.fill(0);
    _ledDuty.fill(0.0);
    _stepCount = 0;
    syncFromPpi(ppi);
}

void Abc80LedChaser::accumulate(uint64_t elapsedTStates) noexcept
{
    for (size_t i = 0; i < 8; ++i) {
        if ((_activeMask & (1u << i)) != 0) {
            _ledOnTStates[i] += elapsedTStates;
        }
    }
}

void Abc80LedChaser::onFrameBegin() noexcept
{
    _ledOnTStates.fill(0);
}

void Abc80LedChaser::onFrameEnd(uint32_t frameElapsedTStates) noexcept
{
    if (frameElapsedTStates == 0) {
        _ledDuty.fill(0.0);
        return;
    }
    const double denom = static_cast<double>(frameElapsedTStates);
    for (size_t i = 0; i < 8; ++i) {
        const double ratio = static_cast<double>(_ledOnTStates[i]) / denom;
        _ledDuty[i] = std::clamp(ratio, 0.0, 1.0);
    }
}

void Abc80LedChaser::handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    if ((port & 0xFC) != 0x80) {
        return;
    }
    const uint8_t reg = static_cast<uint8_t>(port & 0x03);
    if (reg == 3) {
        _rawPortB = 0xFF;
        _activeMask = 0x00;
        return;
    }
    if (reg == 1) {
        if ((ppi.getPrimaryControl() & 0x82) == 0x80) {
            const uint8_t nextMask = static_cast<uint8_t>(~val);
            if (val != _rawPortB || _stepCount == 0) {
                ++_stepCount;
            }
            _rawPortB = val;
            _activeMask = nextMask;
        } else {
            _rawPortB = 0xFF;
            _activeMask = 0x00;
        }
    }
}

// ---- Exercise 26: Abc80Oscilloscope ----

Abc80Oscilloscope::Abc80Oscilloscope() noexcept
    : Abc80ExtCircuit(ExtCircuitId::Oscilloscope)
{
    for (auto &row : _gridDwell) {
        row.fill(0);
    }
    _trace.reserve(kMaxTracePoints);
}

uint32_t Abc80Oscilloscope::gridDwell(uint8_t x, uint8_t y) const noexcept
{
    if (x >= 16 || y >= 16) {
        return 0;
    }
    return _gridDwell[y][x];
}

void Abc80Oscilloscope::onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    onReset(&ppi, nowTStates);
}

void Abc80Oscilloscope::onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept
{
    (void)ppi;
    (void)nowTStates;
    _rawPortB = 0x00;
    _beamX = 0;
    _beamY = 0;
    _beamValid = false;
    for (auto &row : _gridDwell) {
        row.fill(0);
    }
    _trace.clear();
    _sampleCount = 0;
}

void Abc80Oscilloscope::accumulate(uint64_t elapsedTStates) noexcept
{
    if (!_beamValid) {
        return;
    }
    const uint32_t add = static_cast<uint32_t>(
        std::min<uint64_t>(elapsedTStates, 0x3FFFFFFFULL));
    uint32_t &cell = _gridDwell[_beamY & 0x0F][_beamX & 0x0F];
    cell = static_cast<uint32_t>(std::min<uint64_t>(uint64_t{cell} + add, 0x7FFFFFFFULL));
    if (!_trace.empty() && _trace.back().x == _beamX && _trace.back().y == _beamY) {
        _trace.back().dwellTStates = static_cast<uint32_t>(
            std::min<uint64_t>(uint64_t{_trace.back().dwellTStates} + add, 0x7FFFFFFFULL));
    }
}

void Abc80Oscilloscope::onFrameBegin() noexcept
{
}

void Abc80Oscilloscope::onFrameEnd(uint32_t frameElapsedTStates) noexcept
{
    (void)frameElapsedTStates;
    // Apply phosphor persistence decay (0.85) across frames
    for (auto &row : _gridDwell) {
        for (uint32_t &dwell : row) {
            dwell = static_cast<uint32_t>((uint64_t{dwell} * 85ULL) / 100ULL);
        }
    }
}

void Abc80Oscilloscope::recordCoordinate(uint8_t val) noexcept
{
    _rawPortB = val;
    _beamX = static_cast<uint8_t>(val & 0x0F);
    _beamY = static_cast<uint8_t>((val >> 4) & 0x0F);
    _beamValid = true;
    ++_sampleCount;

    // Update or append in bounded trace vector
    for (auto it = _trace.begin(); it != _trace.end(); ++it) {
        if (it->x == _beamX && it->y == _beamY) {
            Point pt = *it;
            if (pt.dwellTStates == 0) {
                pt.dwellTStates = 1;
            }
            _trace.erase(it);
            _trace.push_back(pt);
            return;
        }
    }

    if (_trace.size() >= kMaxTracePoints) {
        _trace.erase(_trace.begin());
    }
    _trace.push_back(Point{_beamX, _beamY, 1});
}

void Abc80Oscilloscope::handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    if ((port & 0xFC) != 0x80) {
        return;
    }
    const uint8_t reg = static_cast<uint8_t>(port & 0x03);
    if (reg == 1 && (ppi.getPrimaryControl() & 0x82) == 0x80) {
        recordCoordinate(val);
    }
}

// ---- Exercise 27: Abc80SchoolBell ----

Abc80SchoolBell::Abc80SchoolBell() noexcept
    : Abc80ExtCircuit(ExtCircuitId::SchoolBell)
{
}

uint8_t Abc80SchoolBell::bcdSecond() const noexcept
{
    return board() != nullptr ? board()->memory().readByte(0x1200) : 0;
}

uint8_t Abc80SchoolBell::bcdMinute() const noexcept
{
    return board() != nullptr ? board()->memory().readByte(0x1201) : 0;
}

uint8_t Abc80SchoolBell::bcdHour() const noexcept
{
    return board() != nullptr ? board()->memory().readByte(0x1202) : 0;
}

void Abc80SchoolBell::setBcdTime(uint8_t hh, uint8_t mm, uint8_t ss) noexcept
{
    if (board() != nullptr) {
        board()->memory().writeByte(0x1200, ss);
        board()->memory().writeByte(0x1201, mm);
        board()->memory().writeByte(0x1202, hh);
    }
}

void Abc80SchoolBell::onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    onReset(&ppi, nowTStates);
}

void Abc80SchoolBell::onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    _portC = ppi != nullptr ? ppi->getDigitStrobe() : 0xFF;
    const bool configured = ppi != nullptr && ((ppi->getPrimaryControl() & 0x88) == 0x80);
    _bellRinging = configured && ((_portC & 0x40) == 0);
    _lastSpeakerBit = (_portC & 0x80) != 0;
    _frameRingTStates = 0;
    _bellDuty = 0.0;
    _totalRingTStates = 0;
    _ringCount = _bellRinging ? 1u : 0u;
    _speakerEdges = 0;
}

void Abc80SchoolBell::accumulate(uint64_t elapsedTStates) noexcept
{
    if (_bellRinging) {
        _frameRingTStates += elapsedTStates;
        _totalRingTStates += elapsedTStates;
    }
}

void Abc80SchoolBell::onFrameBegin() noexcept
{
    _frameRingTStates = 0;
}

void Abc80SchoolBell::onFrameEnd(uint32_t frameElapsedTStates) noexcept
{
    if (frameElapsedTStates == 0) {
        _bellDuty = 0.0;
        return;
    }
    const double ratio = static_cast<double>(_frameRingTStates) / static_cast<double>(frameElapsedTStates);
    _bellDuty = std::clamp(ratio, 0.0, 1.0);
}

void Abc80SchoolBell::handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    if ((port & 0xFC) != 0x80) {
        return;
    }
    const uint8_t reg = static_cast<uint8_t>(port & 0x03);
    if (reg == 3) {
        if ((ppi.getPrimaryControl() & 0x88) != 0x80) {
            _bellRinging = false;
            _portC = 0xFF;
        }
        return;
    }
    if (reg == 2) {
        const bool configured = (ppi.getPrimaryControl() & 0x88) == 0x80;
        _portC = val;
        const bool spk = (val & 0x80) != 0;
        if (spk != _lastSpeakerBit) {
            ++_speakerEdges;
            _lastSpeakerBit = spk;
        }
        if (!configured) {
            _bellRinging = false;
            return;
        }
        if ((val & 0x40) == 0) {
            if (!_bellRinging) {
                _bellRinging = true;
                ++_ringCount;
            }
        } else if (val == 0xFF || (val & 0x3F) != 0x3F) {
            // Note: Monitor ROM SOUND (047CH) reloads A=FFH and toggles bit 7 (0x7F/0xFF)
            // while playing the 8 chime notes inside BALL (107BH..10CDH), before BALL
            // explicitly outputs 0xFF at 10CCH. Only clear _bellRinging when not inside
            // the BALL chime routine (PC in 0x047C..0x048E or 0x107F..0x10C9).
            bool inBallChime = false;
            if (board() != nullptr) {
                const uint16_t pc = board()->lastFetchAddress();
                inBallChime = (pc >= 0x047C && pc <= 0x048E) || (pc >= 0x107F && pc <= 0x10C9);
            }
            if (!inBallChime) {
                _bellRinging = false;
            }
        }
    }
}

// ---- Exercise 28: Abc80ApplianceControl ----

Abc80ApplianceControl::Abc80ApplianceControl() noexcept
    : Abc80ExtCircuit(ExtCircuitId::ApplianceControl)
{
    _frameOnTStates.fill(0);
    _relayDuty.fill(0.0);
    _totalOnTStates.fill(0);
}

uint8_t Abc80ApplianceControl::bcdSecond() const noexcept
{
    return board() != nullptr ? board()->memory().readByte(0x1200) : 0;
}

uint8_t Abc80ApplianceControl::bcdMinute() const noexcept
{
    return board() != nullptr ? board()->memory().readByte(0x1201) : 0;
}

uint8_t Abc80ApplianceControl::bcdHour() const noexcept
{
    return board() != nullptr ? board()->memory().readByte(0x1202) : 0;
}

void Abc80ApplianceControl::setBcdTime(uint8_t hh, uint8_t mm, uint8_t ss) noexcept
{
    if (board() != nullptr) {
        board()->memory().writeByte(0x1200, ss);
        board()->memory().writeByte(0x1201, mm);
        board()->memory().writeByte(0x1202, hh);
    }
}

uint8_t Abc80ApplianceControl::activeHourIndex() const noexcept
{
    const uint8_t hh = bcdHour();
    const uint8_t dec = static_cast<uint8_t>(((hh >> 4) & 0x0F) * 10u + (hh & 0x0F));
    return dec > 23 ? 23 : dec;
}

std::array<uint8_t, 24> Abc80ApplianceControl::scheduleMasks() const noexcept
{
    std::array<uint8_t, 24> out{};
    if (board() == nullptr) {
        return out;
    }
    for (uint8_t h = 0; h < 24; ++h) {
        const uint8_t bcdH = static_cast<uint8_t>(((h / 10u) << 4) | (h % 10u));
        out[h] = board()->memory().readByte(static_cast<uint16_t>(0x1100u + bcdH));
    }
    return out;
}

void Abc80ApplianceControl::onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    onReset(&ppi, nowTStates);
}

void Abc80ApplianceControl::onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept
{
    (void)nowTStates;
    _frameOnTStates.fill(0);
    _relayDuty.fill(0.0);
    _totalOnTStates.fill(0);
    _updateCount = 0;
    if (ppi != nullptr) {
        _auxControl = ppi->getAuxControl();
        if ((_auxControl & 0x89) == 0x80) {
            _relayMask = ppi->auxPortCOutput();
        } else {
            _relayMask = 0x00;
        }
    } else {
        _auxControl = 0x9B;
        _relayMask = 0x00;
    }
}

void Abc80ApplianceControl::accumulate(uint64_t elapsedTStates) noexcept
{
    for (size_t ch = 0; ch < 8; ++ch) {
        if ((_relayMask & (1u << ch)) != 0) {
            _frameOnTStates[ch] += elapsedTStates;
            _totalOnTStates[ch] += elapsedTStates;
        }
    }
}

void Abc80ApplianceControl::onFrameBegin() noexcept
{
    _frameOnTStates.fill(0);
}

void Abc80ApplianceControl::onFrameEnd(uint32_t frameElapsedTStates) noexcept
{
    if (frameElapsedTStates == 0) {
        _relayDuty.fill(0.0);
        return;
    }
    const double denom = static_cast<double>(frameElapsedTStates);
    for (size_t ch = 0; ch < 8; ++ch) {
        const double ratio = static_cast<double>(_frameOnTStates[ch]) / denom;
        _relayDuty[ch] = std::clamp(ratio, 0.0, 1.0);
    }
}

void Abc80ApplianceControl::handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    (void)val;
    (void)nowTStates;
    if ((port & 0xFC) != 0x40) {
        return;
    }
    _auxControl = ppi.getAuxControl();
    const uint8_t reg = static_cast<uint8_t>(port & 0x03);
    if (reg == 3) {
        if ((_auxControl & 0x89) == 0x80) {
            _relayMask = ppi.auxPortCOutput();
        } else {
            _relayMask = 0x00;
        }
    } else if (reg == 2) {
        if ((_auxControl & 0x89) == 0x80) {
            _relayMask = ppi.auxPortCOutput();
            ++_updateCount;
        } else {
            _relayMask = 0x00;
        }
    }
}

// ---- Exercise 29: Abc80RoboticArm ----

Abc80RoboticArm::Abc80RoboticArm() noexcept
    : Abc80ExtCircuit(ExtCircuitId::RoboticArm)
{
}

void Abc80RoboticArm::onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    onReset(&ppi, nowTStates);
}

void Abc80RoboticArm::onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept
{
    _motors = {};
    _magnetOnTStates = 0;
    _lastStepTStates = nowTStates;
    if (ppi != nullptr) {
        _expControl = ppi->getExpControl();
        _portA = ppi->expPortAOutput();
        _portB = ppi->expPortBOutput();
        _portC = ppi->expPortCOutput();
        if ((_expControl & 0x80) != 0) {
            _motors[0].phase = static_cast<uint8_t>(_portA & 0x03);
            _motors[0].coils = expandCoils(_motors[0].phase);
            _motors[0].initialized = true;

            _motors[1].phase = static_cast<uint8_t>(_portB & 0x03);
            _motors[1].coils = expandCoils(_motors[1].phase);
            _motors[1].initialized = true;

            _motors[2].phase = static_cast<uint8_t>((_portB >> 4) & 0x03);
            _motors[2].coils = expandCoils(_motors[2].phase);
            _motors[2].initialized = true;

            _electromagnetOn = (_portC & 0x01) != 0;
        } else {
            _electromagnetOn = false;
        }
    } else {
        _expControl = 0x9B;
        _portA = 0x00;
        _portB = 0x00;
        _portC = 0x00;
        _electromagnetOn = false;
    }
}

void Abc80RoboticArm::accumulate(uint64_t elapsedTStates) noexcept
{
    if (_electromagnetOn) {
        _magnetOnTStates += elapsedTStates;
    }
}

void Abc80RoboticArm::onFrameBegin() noexcept
{
}

void Abc80RoboticArm::onFrameEnd(uint32_t frameElapsedTStates) noexcept
{
    (void)frameElapsedTStates;
}

namespace {

int grayQuadratureDelta(uint8_t prevPhase, uint8_t nextPhase) noexcept
{
    constexpr int kPhaseToRank[4] = { 0, 1, 3, 2 }; // 00->0, 01->1, 11(03)->2, 10(02)->3
    const int r0 = kPhaseToRank[prevPhase & 0x03];
    const int r1 = kPhaseToRank[nextPhase & 0x03];
    const int diff = (r1 - r0 + 4) & 3;
    if (diff == 1) {
        return +1;
    }
    if (diff == 3) {
        return -1;
    }
    return 0;
}

} // namespace

void Abc80RoboticArm::updateMotorPhase(size_t idx, uint8_t newPhase, uint64_t nowTStates) noexcept
{
    if (idx >= _motors.size()) {
        return;
    }
    MotorState &m = _motors[idx];
    const uint8_t p = static_cast<uint8_t>(newPhase & 0x03);
    if (!m.initialized) {
        m.phase = p;
        m.coils = expandCoils(p);
        m.initialized = true;
        return;
    }
    if (p == m.phase) {
        return;
    }
    const int delta = grayQuadratureDelta(m.phase, p);
    m.phase = p;
    m.coils = expandCoils(p);
    if (delta != 0) {
        m.steps += delta;
        m.totalSteps += 1;
        _lastStepTStates = nowTStates;
    }
}

void Abc80RoboticArm::handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept
{
    if ((port & 0xFC) != 0x00) {
        return;
    }
    _expControl = ppi.getExpControl();
    const uint8_t reg = static_cast<uint8_t>(port & 0x03);
    if (reg == 3) {
        if ((_expControl & 0x80) == 0) {
            _electromagnetOn = false;
        }
        return;
    }
    if ((_expControl & 0x80) == 0) {
        return;
    }
    if (reg == 0) {
        _portA = val;
        updateMotorPhase(0, static_cast<uint8_t>(val & 0x03), nowTStates);
    } else if (reg == 1) {
        _portB = val;
        updateMotorPhase(1, static_cast<uint8_t>(val & 0x03), nowTStates);
        updateMotorPhase(2, static_cast<uint8_t>((val >> 4) & 0x03), nowTStates);
    } else if (reg == 2) {
        _portC = val;
        const bool nextMag = (val & 0x01) != 0;
        if (nextMag != _electromagnetOn) {
            _electromagnetOn = nextMag;
            _lastStepTStates = nowTStates;
        }
    }
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
