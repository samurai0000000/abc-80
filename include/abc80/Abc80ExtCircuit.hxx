/*
 * Abc80ExtCircuit.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_EXT_CIRCUIT_HXX
#define ABC80_EXT_CIRCUIT_HXX

#include <abc80/Abc80Types.hxx>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace abc80 {

class Abc80Board;
class Abc80Ppi;

enum class ExtCircuitId : uint8_t {
    None = 0,
    TrafficLight = 24,      // Exercise 24: Crossroad Traffic Light (Port 0x81)
    LedChaser = 25,         // Exercise 25: 8-LED Sequential Chaser (Port 0x81)
    Oscilloscope = 26,      // Exercise 26: Dual 4-Bit DAC X-Y Oscilloscope (Port 0x81)
    SchoolBell = 27,        // Exercise 27: School Bell Relay & Clock (Port 0x82 PC6)
    ApplianceControl = 28,  // Exercise 28: 8-Channel Appliance Relays (Port 0x42 Aux PC)
    RoboticArm = 29,        // Exercise 29: 3-Axis Stepper Robotic Arm (Ports 0x00..0x03 Exp)
};

const char *extCircuitIdName(ExtCircuitId id) noexcept;
inline const char *extCircuitIdToString(ExtCircuitId id) noexcept { return extCircuitIdName(id); }
bool parseExtCircuitId(const std::string &name, ExtCircuitId &out) noexcept;

class Abc80ExtCircuit {
public:
    explicit Abc80ExtCircuit(ExtCircuitId id) noexcept;
    virtual ~Abc80ExtCircuit();

    Abc80ExtCircuit(const Abc80ExtCircuit &) = delete;
    Abc80ExtCircuit &operator=(const Abc80ExtCircuit &) = delete;

    ExtCircuitId id() const noexcept { return _id; }
    const char *name() const noexcept { return extCircuitIdName(_id); }
    int exerciseNumber() const noexcept { return static_cast<int>(_id); }

    bool isAttached() const noexcept { return _board != nullptr; }
    uint64_t currentTStates() const noexcept { return _clock != nullptr ? *_clock : 0; }
    uint32_t frameElapsedTStates() const noexcept { return _frameElapsed; }

    virtual void reset() noexcept;

    void bindBoard(Abc80Board *board, const uint64_t *clock) noexcept;
    void unbindBoard() noexcept;
    void onBoardReset(uint64_t nowTStates) noexcept;

    void beginFrame(uint64_t nowTStates) noexcept;
    void step(uint64_t nowTStates) noexcept;
    void endFrame(uint64_t nowTStates) noexcept;
    void onPortWrite(uint8_t port, uint8_t val, uint64_t nowTStates) noexcept;

    static std::unique_ptr<Abc80ExtCircuit> create(ExtCircuitId id);

protected:
    virtual void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept = 0;
    virtual void onDetach() noexcept {}
    virtual void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept = 0;
    virtual void accumulate(uint64_t elapsedTStates) noexcept = 0;
    virtual void onFrameBegin() noexcept = 0;
    virtual void onFrameEnd(uint32_t frameElapsedTStates) noexcept = 0;
    virtual void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept = 0;

    Abc80Board *board() noexcept { return _board; }
    const Abc80Board *board() const noexcept { return _board; }

private:
    void flushUpTo(uint64_t nowTStates) noexcept;

    ExtCircuitId _id;
    Abc80Board *_board{nullptr};
    const uint64_t *_clock{nullptr};
    uint64_t _frameStartTStates{0};
    uint64_t _lastEventTStates{0};
    uint32_t _frameElapsed{0};
};

// Exercise 24: Crossroad Traffic Light (Port 0x81 PB0..PB5)
class Abc80TrafficLight final : public Abc80ExtCircuit {
public:
    Abc80TrafficLight() noexcept;
    ~Abc80TrafficLight() override = default;

    uint8_t portB() const noexcept { return _portB; }
    bool roadARed() const noexcept { return (_portB & 0x01) != 0; }
    bool roadAYellow() const noexcept { return (_portB & 0x02) != 0; }
    bool roadAGreen() const noexcept { return (_portB & 0x04) != 0; }
    bool roadBRed() const noexcept { return (_portB & 0x08) != 0; }
    bool roadBYellow() const noexcept { return (_portB & 0x10) != 0; }
    bool roadBGreen() const noexcept { return (_portB & 0x20) != 0; }
    const std::array<double, 6> &lampDuty() const noexcept { return _lampDuty; }
    uint64_t transitionCount() const noexcept { return _transitionCount; }

protected:
    void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;
    void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept override;
    void accumulate(uint64_t elapsedTStates) noexcept override;
    void onFrameBegin() noexcept override;
    void onFrameEnd(uint32_t frameElapsedTStates) noexcept override;
    void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;

private:
    void syncFromPpi(const Abc80Ppi *ppi) noexcept;

    uint8_t _portB{0};
    std::array<uint64_t, 6> _lampOnTStates{};
    std::array<double, 6> _lampDuty{};
    uint64_t _transitionCount{0};
};

// Exercise 25: 8-LED Sequential Chaser via 4009/7404 Inverter (Port 0x81 PB0..PB7)
class Abc80LedChaser final : public Abc80ExtCircuit {
public:
    Abc80LedChaser() noexcept;
    ~Abc80LedChaser() override = default;

    uint8_t rawPortB() const noexcept { return _rawPortB; }
    uint8_t activeMask() const noexcept { return _activeMask; }
    bool isLedLit(uint8_t idx) const noexcept { return idx < 8 && ((_activeMask & (1u << idx)) != 0); }
    bool isLedOn(uint8_t idx) const noexcept { return isLedLit(idx); }
    int activeLedIndex() const noexcept;
    const std::array<double, 8> &ledDuty() const noexcept { return _ledDuty; }
    uint64_t stepCount() const noexcept { return _stepCount; }

protected:
    void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;
    void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept override;
    void accumulate(uint64_t elapsedTStates) noexcept override;
    void onFrameBegin() noexcept override;
    void onFrameEnd(uint32_t frameElapsedTStates) noexcept override;
    void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;

private:
    void syncFromPpi(const Abc80Ppi *ppi) noexcept;

    uint8_t _rawPortB{0xFF};
    uint8_t _activeMask{0x00};
    std::array<uint64_t, 8> _ledOnTStates{};
    std::array<double, 8> _ledDuty{};
    uint64_t _stepCount{0};
};

// Exercise 26: Dual 4-Bit R-2R Ladder DAC & 16x16 X-Y Oscilloscope (Port 0x81)
class Abc80Oscilloscope final : public Abc80ExtCircuit {
public:
    static constexpr size_t kMaxTracePoints = 64;

    struct Point {
        uint8_t x{0};
        uint8_t y{0};
        uint32_t dwellTStates{0};
    };

    Abc80Oscilloscope() noexcept;
    ~Abc80Oscilloscope() override = default;

    uint8_t rawPortB() const noexcept { return _rawPortB; }
    uint8_t beamX() const noexcept { return _beamX; }
    uint8_t beamY() const noexcept { return _beamY; }
    bool isBeamValid() const noexcept { return _beamValid; }
    uint32_t gridDwell(uint8_t x, uint8_t y) const noexcept;
    const std::vector<Point> &trace() const noexcept { return _trace; }
    const std::vector<Point> &tracePoints() const noexcept { return _trace; }
    uint64_t sampleCount() const noexcept { return _sampleCount; }

protected:
    void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;
    void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept override;
    void accumulate(uint64_t elapsedTStates) noexcept override;
    void onFrameBegin() noexcept override;
    void onFrameEnd(uint32_t frameElapsedTStates) noexcept override;
    void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;

private:
    void recordCoordinate(uint8_t val) noexcept;

    uint8_t _rawPortB{0x00};
    uint8_t _beamX{0};
    uint8_t _beamY{0};
    bool _beamValid{false};
    std::array<std::array<uint32_t, 16>, 16> _gridDwell{};
    std::vector<Point> _trace;
    uint64_t _sampleCount{0};
};

// Exercise 27: Automated School Bell Clock & Relay (Port 0x82 PC6 + PC7 Speaker)
class Abc80SchoolBell final : public Abc80ExtCircuit {
public:
    Abc80SchoolBell() noexcept;
    ~Abc80SchoolBell() override = default;

    uint8_t portC() const noexcept { return _portC; }
    bool isBellRinging() const noexcept { return _bellRinging; }
    double bellDuty() const noexcept { return _bellDuty; }
    uint64_t totalRingTStates() const noexcept { return _totalRingTStates; }
    uint32_t ringCount() const noexcept { return _ringCount; }
    uint64_t speakerEdges() const noexcept { return _speakerEdges; }

    // BCD Clock helpers (0x1200=SS, 0x1201=MM, 0x1202=HH on attached Abc80Board)
    uint8_t bcdSecond() const noexcept;
    uint8_t bcdMinute() const noexcept;
    uint8_t bcdHour() const noexcept;
    void setBcdTime(uint8_t hh, uint8_t mm, uint8_t ss) noexcept;

protected:
    void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;
    void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept override;
    void accumulate(uint64_t elapsedTStates) noexcept override;
    void onFrameBegin() noexcept override;
    void onFrameEnd(uint32_t frameElapsedTStates) noexcept override;
    void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;

private:
    uint8_t _portC{0xFF};
    bool _bellRinging{false};
    bool _lastSpeakerBit{true};
    uint64_t _frameRingTStates{0};
    double _bellDuty{0.0};
    uint64_t _totalRingTStates{0};
    uint32_t _ringCount{0};
    uint64_t _speakerEdges{0};
};

// Exercise 28: 8-Channel Household Appliance Relay Bank (Aux 8255 Port C 0x42)
class Abc80ApplianceControl final : public Abc80ExtCircuit {
public:
    Abc80ApplianceControl() noexcept;
    ~Abc80ApplianceControl() override = default;

    uint8_t auxControl() const noexcept { return _auxControl; }
    uint8_t relayMask() const noexcept { return _relayMask; }
    bool isRelayOn(uint8_t channel) const noexcept { return channel < 8 && ((_relayMask & (1u << channel)) != 0); }
    const std::array<double, 8> &relayDuty() const noexcept { return _relayDuty; }
    const std::array<uint64_t, 8> &totalOnTStates() const noexcept { return _totalOnTStates; }
    uint64_t updateCount() const noexcept { return _updateCount; }

    // BCD Clock & 24-Hour Schedule helpers (0x1200..0x1202 and 0x1100..0x1123 on attached Abc80Board)
    uint8_t bcdSecond() const noexcept;
    uint8_t bcdMinute() const noexcept;
    uint8_t bcdHour() const noexcept;
    void setBcdTime(uint8_t hh, uint8_t mm, uint8_t ss) noexcept;
    uint8_t activeHourIndex() const noexcept;
    std::array<uint8_t, 24> scheduleMasks() const noexcept;

protected:
    void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;
    void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept override;
    void accumulate(uint64_t elapsedTStates) noexcept override;
    void onFrameBegin() noexcept override;
    void onFrameEnd(uint32_t frameElapsedTStates) noexcept override;
    void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;

private:
    uint8_t _auxControl{0x9B};
    uint8_t _relayMask{0x00};
    std::array<uint64_t, 8> _frameOnTStates{};
    std::array<double, 8> _relayDuty{};
    std::array<uint64_t, 8> _totalOnTStates{};
    uint64_t _updateCount{0};
};

// Exercise 29: 3-Axis 4-Phase Stepper Motor + Electromagnet Gripper (Expansion 8255 0x00..0x03)
class Abc80RoboticArm final : public Abc80ExtCircuit {
public:
    struct MotorState {
        uint8_t phase{0};
        uint8_t coils{0x0C};
        int32_t steps{0};
        uint32_t totalSteps{0};
        bool initialized{false};
    };

    Abc80RoboticArm() noexcept;
    ~Abc80RoboticArm() override = default;

    uint8_t expControl() const noexcept { return _expControl; }
    uint8_t portA() const noexcept { return _portA; }
    uint8_t portB() const noexcept { return _portB; }
    uint8_t portC() const noexcept { return _portC; }
    const MotorState &motor(size_t idx) const noexcept { return _motors[idx < 3 ? idx : 0]; }
    bool isElectromagnetOn() const noexcept { return _electromagnetOn; }
    uint64_t magnetOnTStates() const noexcept { return _magnetOnTStates; }
    uint64_t lastStepTStates() const noexcept { return _lastStepTStates; }

    static uint8_t expandCoils(uint8_t phase2Bit) noexcept
    {
        const uint8_t p = static_cast<uint8_t>(phase2Bit & 0x03);
        return static_cast<uint8_t>(p | (((~p) & 0x03) << 2));
    }

protected:
    void onAttach(const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;
    void onReset(const Abc80Ppi *ppi, uint64_t nowTStates) noexcept override;
    void accumulate(uint64_t elapsedTStates) noexcept override;
    void onFrameBegin() noexcept override;
    void onFrameEnd(uint32_t frameElapsedTStates) noexcept override;
    void handlePortWrite(uint8_t port, uint8_t val, const Abc80Ppi &ppi, uint64_t nowTStates) noexcept override;

private:
    void updateMotorPhase(size_t idx, uint8_t newPhase, uint64_t nowTStates) noexcept;

    uint8_t _expControl{0x9B};
    uint8_t _portA{0x00};
    uint8_t _portB{0x00};
    uint8_t _portC{0x00};
    std::array<MotorState, 3> _motors{};
    bool _electromagnetOn{false};
    uint64_t _magnetOnTStates{0};
    uint64_t _lastStepTStates{0};
};

} // namespace abc80

#endif /* ABC80_EXT_CIRCUIT_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
