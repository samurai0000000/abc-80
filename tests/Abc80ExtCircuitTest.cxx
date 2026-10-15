/*
 * Abc80ExtCircuitTest.cxx
 *
 * Qualification suite for Abc80ExtCircuit and the 6 concrete external I/O
 * peripheral circuit classes (Exercises 24-29):
 *   - Abc80TrafficLight     (Ex 24)
 *   - Abc80LedChaser        (Ex 25)
 *   - Abc80Oscilloscope     (Ex 26)
 *   - Abc80SchoolBell       (Ex 27)
 *   - Abc80ApplianceControl (Ex 28)
 *   - Abc80RoboticArm       (Ex 29)
 *
 * Covers instantiation/destruction, strict mutual exclusivity, T-state clock
 * synchronization, end-to-end execution of tapes/0024.tape..0029.tape, and
 * RapidCheck property-based tests.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80ExtCircuit.hxx>
#include <abc80/Abc80Tape.hxx>
#include <CppUTest/TestHarness.h>
#include <rapidcheck.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace abc80;

namespace {

void initBoardAndLoadTape(Abc80Board &board, const char *tapePath)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    board.getPpi().writePort(0x83, 0x90);
    board.getPpi().writePort(0x82, 0xFF);
    board.finishInstruction();

    uint16_t fileId = 0;
    uint16_t startAddr = 0;
    uint16_t endAddr = 0;
    std::string annot;
    std::vector<TapePulseRun> runs;
    std::vector<uint8_t> code;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::loadTapeFile(tapePath, fileId, annot, runs)));
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(Abc80Tape::decodeProgramFromRuns(runs, fileId, startAddr, endAddr, code)));

    for (size_t i = 0; i < code.size(); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(startAddr + i), code[i]);
    }
    board.getCpu().setSP(0x17BD);
    board.getCpu().setIX(0x13C6);
    board.getCpu().setPC(startAddr);
    board.stepInstruction();
}

void stepUntilFetch(Abc80Board &board, uint16_t targetPc, uint64_t maxSteps = 500000)
{
    uint64_t steps = 0;
    while (board.lastFetchAddress() != targetPc) {
        if (++steps > maxSteps) {
            char msg[128];
            std::snprintf(msg, sizeof(msg),
                          "Timeout waiting for fetch 0x%04X (stuck at 0x%04X after %llu steps)",
                          targetPc, board.lastFetchAddress(),
                          static_cast<unsigned long long>(maxSteps));
            FAIL(msg);
        }
        board.stepInstruction();
    }
}

} // namespace

TEST_GROUP(Abc80ExtCircuit)
{
};

TEST(Abc80ExtCircuit, InstantiationDestructionAndMutualExclusivityAcrossAll6Classes)
{
    Abc80Board board;
    CHECK_TRUE(board.attachedCircuit() == nullptr);
    LONGS_EQUAL(static_cast<int>(ExtCircuitId::None),
                static_cast<int>(board.attachedCircuitId()));

    // 1. Stack-allocated circuit destroyed before board auto-detaches cleanly
    {
        Abc80TrafficLight traffic;
        CHECK_FALSE(traffic.isAttached());
        LONGS_EQUAL(24, traffic.exerciseNumber());
        STRCMP_EQUAL("traffic_light", traffic.name());

        board.attachCircuit(&traffic);
        CHECK_TRUE(traffic.isAttached());
        POINTERS_EQUAL(&traffic, board.attachedCircuit());
        POINTERS_EQUAL(&traffic, board.attachedCircuitAs<Abc80TrafficLight>());
        CHECK_TRUE(board.attachedCircuitAs<Abc80LedChaser>() == nullptr);
        LONGS_EQUAL(static_cast<int>(ExtCircuitId::TrafficLight),
                    static_cast<int>(board.attachedCircuitId()));
    }
    CHECK_TRUE(board.attachedCircuit() == nullptr);
    LONGS_EQUAL(static_cast<int>(ExtCircuitId::None),
                static_cast<int>(board.attachedCircuitId()));

    // 2. Board destroyed before stack-allocated circuit unbinds circuit cleanly
    Abc80LedChaser chaser;
    {
        Abc80Board tempBoard;
        tempBoard.attachCircuit(&chaser);
        CHECK_TRUE(chaser.isAttached());
        POINTERS_EQUAL(&chaser, tempBoard.attachedCircuit());
    }
    CHECK_FALSE(chaser.isAttached());
    LONGS_EQUAL(0, chaser.currentTStates());

    // 3. Switching between two caller-owned circuits detaches the first immediately
    Abc80Oscilloscope scope;
    board.attachCircuit(&chaser);
    CHECK_TRUE(chaser.isAttached());
    board.attachCircuit(&scope);
    CHECK_FALSE(chaser.isAttached());
    CHECK_TRUE(scope.isAttached());
    POINTERS_EQUAL(&scope, board.attachedCircuit());
    board.detachCircuit();
    CHECK_FALSE(scope.isAttached());
    CHECK_TRUE(board.attachedCircuit() == nullptr);

    // 4. std::unique_ptr ownership transfer and replacement across all 6 circuit IDs
    auto bellPtr = std::make_unique<Abc80SchoolBell>();
    Abc80SchoolBell *rawBell = bellPtr.get();
    POINTERS_EQUAL(rawBell, board.attachCircuit(std::move(bellPtr)));
    CHECK_TRUE(rawBell->isAttached());
    LONGS_EQUAL(static_cast<int>(ExtCircuitId::SchoolBell),
                static_cast<int>(board.attachedCircuitId()));

    const std::array<ExtCircuitId, 6> allIds = {
        ExtCircuitId::TrafficLight,
        ExtCircuitId::LedChaser,
        ExtCircuitId::Oscilloscope,
        ExtCircuitId::SchoolBell,
        ExtCircuitId::ApplianceControl,
        ExtCircuitId::RoboticArm,
    };
    for (ExtCircuitId id : allIds) {
        Abc80ExtCircuit *c = board.attachCircuit(id);
        CHECK_TRUE(c != nullptr);
        CHECK_TRUE(c->isAttached());
        LONGS_EQUAL(static_cast<int>(id), static_cast<int>(c->id()));
        LONGS_EQUAL(static_cast<int>(id), static_cast<int>(board.attachedCircuitId()));
    }

    // Attaching ExtCircuitId::None detaches and destroys the active circuit
    CHECK_TRUE(board.attachCircuit(ExtCircuitId::None) == nullptr);
    CHECK_TRUE(board.attachedCircuit() == nullptr);
    LONGS_EQUAL(static_cast<int>(ExtCircuitId::None),
                static_cast<int>(board.attachedCircuitId()));
}

TEST(Abc80ExtCircuit, ClockSynchronizationAndDutyRatiosFollowCpuTStatesAndReset)
{
    Abc80Board board;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));

    auto *traffic = dynamic_cast<Abc80TrafficLight *>(board.attachCircuit(ExtCircuitId::TrafficLight));
    CHECK_TRUE(traffic != nullptr);
    LONGS_EQUAL(board.getCpu().getCycles(), traffic->currentTStates());

    // Configure Primary 8255 Mode 0 (Port B output)
    board.getPpi().writePort(0x83, 0x90);
    board.getPpi().writePort(0x81, 0x00);

    // Run a full frame with Road A Green + Road B Red (0x0C) for the first half and Road A Red + Road B Green (0x21) for the second half
    const uint64_t startCycles = board.getCpu().getCycles();
    traffic->beginFrame(startCycles);
    board.getPpi().writePort(0x81, 0x0C);
    board.stepTStates(10000);
    LONGS_EQUAL(startCycles + 10000, traffic->currentTStates());
    board.getPpi().writePort(0x81, 0x21);
    board.stepTStates(10000);
    traffic->endFrame(board.getCpu().getCycles());

    LONGS_EQUAL(20000, traffic->frameElapsedTStates());
    DOUBLES_EQUAL(0.5, traffic->lampDuty()[2], 0.01); // PB2 Road A Green on for half
    DOUBLES_EQUAL(0.5, traffic->lampDuty()[3], 0.01); // PB3 Road B Red on for half
    DOUBLES_EQUAL(0.5, traffic->lampDuty()[0], 0.01); // PB0 Road A Red on for second half
    DOUBLES_EQUAL(0.5, traffic->lampDuty()[5], 0.01); // PB5 Road B Green on for second half
    DOUBLES_EQUAL(0.0, traffic->lampDuty()[1], 0.001);
    DOUBLES_EQUAL(0.0, traffic->lampDuty()[4], 0.001);

    // stepFrame() drives beginFrame/endFrame automatically on the attached circuit
    board.getPpi().writePort(0x81, 0x0C);
    board.stepFrame();
    LONGS_EQUAL(ABC80_FRAME_TSTATES, traffic->frameElapsedTStates());
    DOUBLES_EQUAL(1.0, traffic->lampDuty()[2], 0.01);
    DOUBLES_EQUAL(1.0, traffic->lampDuty()[3], 0.01);

    // Board reset keeps circuit attached and re-anchors T-state watermarks to 0 without underflow
    board.reset();
    CHECK_TRUE(traffic->isAttached());
    POINTERS_EQUAL(traffic, board.attachedCircuit());
    LONGS_EQUAL(0, traffic->currentTStates());
    board.stepFrame();
    LONGS_EQUAL(ABC80_FRAME_TSTATES, traffic->frameElapsedTStates());
}

TEST(Abc80ExtCircuit, Exercise24TrafficLightEndToEndWithTape0024)
{
    Abc80Board board;
    Abc80TrafficLight traffic;
    board.attachCircuit(&traffic);
    initBoardAndLoadTape(board, "tapes/0024.tape");

    // Patch DELAY at 1049H (LD DE, 8000H -> LD DE, 0001H)
    board.getBus().writeByte(0x104A, 0x01);
    board.getBus().writeByte(0x104B, 0x00);

    bool seenAGreenBRed = false;
    bool seenAYellowBRed = false;
    bool seenARedBGreen = false;
    bool seenARedBYellow = false;

    for (int i = 0; i < 40000; ++i) {
        board.stepInstruction();
        if (traffic.roadAGreen() && traffic.roadBRed() && !traffic.roadAYellow()) {
            seenAGreenBRed = true;
        }
        if (traffic.roadAYellow() && traffic.roadBRed() && !traffic.roadAGreen()) {
            seenAYellowBRed = true;
        }
        if (traffic.roadARed() && traffic.roadBGreen() && !traffic.roadBYellow()) {
            seenARedBGreen = true;
        }
        if (traffic.roadARed() && traffic.roadBYellow() && !traffic.roadBGreen()) {
            seenARedBYellow = true;
        }
        if (seenAGreenBRed && seenAYellowBRed && seenARedBGreen && seenARedBYellow &&
            traffic.transitionCount() >= 12) {
            break;
        }
    }

    CHECK_TRUE(seenAGreenBRed);
    CHECK_TRUE(seenAYellowBRed);
    CHECK_TRUE(seenARedBGreen);
    CHECK_TRUE(seenARedBYellow);
    CHECK_TRUE(traffic.transitionCount() >= 12);
}

TEST(Abc80ExtCircuit, Exercise25LedChaserEndToEndWithTape0025)
{
    Abc80Board board;
    Abc80LedChaser chaser;
    board.attachCircuit(&chaser);
    initBoardAndLoadTape(board, "tapes/0025.tape");

    // Clamp HL delay seed at 1009H (LD HL, FFFEH)
    board.getBus().writeByte(0x100A, 0xFE);
    board.getBus().writeByte(0x100B, 0xFF);

    for (uint8_t idx = 0; idx < 8; ++idx) {
        stepUntilFetch(board, 0x1008);
        BYTES_EQUAL(static_cast<uint8_t>(~(1u << idx)), chaser.rawPortB());
        BYTES_EQUAL(static_cast<uint8_t>(1u << idx), chaser.activeMask());
        LONGS_EQUAL(static_cast<int>(idx), chaser.activeLedIndex());
        CHECK_TRUE(chaser.isLedLit(idx));
        CHECK_FALSE(chaser.isLedLit(static_cast<uint8_t>((idx + 1) % 8)));
        board.stepInstruction();
    }
    CHECK_TRUE(chaser.stepCount() >= 8);
}

TEST(Abc80ExtCircuit, Exercise26OscilloscopeEndToEndWithTape0026)
{
    Abc80Board board;
    Abc80Oscilloscope scope;
    board.attachCircuit(&scope);
    initBoardAndLoadTape(board, "tapes/0026.tape");

    // Clamp DELAY C counter at 1016H (LD C, 02H)
    board.getBus().writeByte(0x1017, 0x02);

    uint64_t steps = 0;
    while (scope.sampleCount() < 37) {
        if (++steps > 100000) {
            FAIL("Timeout waiting for 37 oscilloscope samples in Lab 26");
        }
        board.stepInstruction();
    }
    board.stepInstruction();

    CHECK_TRUE(scope.isBeamValid());
    CHECK_TRUE(scope.sampleCount() >= 37);
    LONGS_EQUAL(37, scope.trace().size());
    // First point in xytab is 0x92 -> X=2, Y=9
    BYTES_EQUAL(2, scope.trace().front().x);
    BYTES_EQUAL(9, scope.trace().front().y);
    CHECK_TRUE(scope.gridDwell(2, 9) > 0);
    // Last point in xytab is 0x6D -> X=13, Y=6
    BYTES_EQUAL(13, scope.trace().back().x);
    BYTES_EQUAL(6, scope.trace().back().y);
    CHECK_TRUE(scope.gridDwell(13, 6) > 0);
}

TEST(Abc80ExtCircuit, Exercise27SchoolBellEndToEndWithTape0027AndScanImmunity)
{
    Abc80Board board;
    Abc80SchoolBell bell;
    board.attachCircuit(&bell);
    initBoardAndLoadTape(board, "tapes/0027.tape");

    // Seed 07:59:59 via helper accessor
    bell.setBcdTime(0x07, 0x59, 0x59);
    BYTES_EQUAL(0x07, bell.bcdHour());
    BYTES_EQUAL(0x59, bell.bcdMinute());
    BYTES_EQUAL(0x59, bell.bcdSecond());

    // Step through one pass of SCD&K1 (04CDH) and verify normal display scan does NOT ring the bell
    stepUntilFetch(board, 0x1007);
    board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
    stepUntilFetch(board, 0x100C);
    CHECK_FALSE(bell.isBellRinging());
    LONGS_EQUAL(0, bell.ringCount());

    // Advance past TMUPD/BFUPD/LOOP2 into TIMET -> BALL (0x107B)
    stepUntilFetch(board, 0x1012);
    board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
    stepUntilFetch(board, 0x107F);

    BYTES_EQUAL(0x08, bell.bcdHour());
    BYTES_EQUAL(0x00, bell.bcdMinute());
    BYTES_EQUAL(0x00, bell.bcdSecond());
    BYTES_EQUAL(0x3F, bell.portC());
    CHECK_TRUE(bell.isBellRinging());
    LONGS_EQUAL(1, bell.ringCount());

    // Step one frame while inside BALL chime and verify bellDuty > 0 and speakerEdges > 0
    board.stepFrame();
    CHECK_TRUE(bell.bellDuty() > 0.9);
    CHECK_TRUE(bell.totalRingTStates() >= ABC80_FRAME_TSTATES);
    CHECK_TRUE(bell.speakerEdges() > 0);
}

TEST(Abc80ExtCircuit, Exercise28ApplianceControlEndToEndWithTape0028)
{
    Abc80Board board;
    Abc80ApplianceControl app;
    board.attachCircuit(&app);
    initBoardAndLoadTape(board, "tapes/0028.tape");

    // Seed 05:59:59 so next tick rolls over to 06:00:00 (schedule table 1106H = 0xE1)
    app.setBcdTime(0x05, 0x59, 0x59);
    BYTES_EQUAL(5, app.activeHourIndex());

    const auto masks = app.scheduleMasks();
    BYTES_EQUAL(0x60, masks[0]);
    BYTES_EQUAL(0x68, masks[5]);
    BYTES_EQUAL(0xE1, masks[6]);
    BYTES_EQUAL(0xD0, masks[10]);
    BYTES_EQUAL(0xB2, masks[20]);

    stepUntilFetch(board, 0x100E);
    board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
    stepUntilFetch(board, 0x1019);
    board.getCpu().setBC(static_cast<uint16_t>((0x01 << 8) | (board.getCpu().getBC() & 0xFF)));
    stepUntilFetch(board, 0x101F);

    BYTES_EQUAL(0x06, app.bcdHour());
    BYTES_EQUAL(6, app.activeHourIndex());
    BYTES_EQUAL(0x80, app.auxControl());
    BYTES_EQUAL(0xE1, app.relayMask());
    CHECK_TRUE(app.isRelayOn(0));
    CHECK_FALSE(app.isRelayOn(1));
    CHECK_TRUE(app.isRelayOn(5));
    CHECK_TRUE(app.isRelayOn(6));
    CHECK_TRUE(app.isRelayOn(7));
    CHECK_TRUE(app.updateCount() >= 2);
}

TEST(Abc80ExtCircuit, Exercise29RoboticArmEndToEndWithTape0029)
{
    Abc80Board board;
    Abc80RoboticArm arm;
    board.attachCircuit(&arm);
    initBoardAndLoadTape(board, "tapes/0029.tape");

    // Patch DELAY at 115EH with RET (0xC9)
    board.getBus().writeByte(0x115E, 0xC9);

    bool seenMagnetOn = false;
    uint64_t steps = 0;
    while (steps < 100000 && board.lastFetchAddress() != 0x1047) {
        board.stepInstruction();
        steps++;
        if (arm.isElectromagnetOn()) {
            seenMagnetOn = true;
        }
    }

    LONGS_EQUAL(0x1047, board.lastFetchAddress());
    BYTES_EQUAL(0x80, arm.expControl());
    CHECK_TRUE(seenMagnetOn);
    CHECK_FALSE(arm.isElectromagnetOn()); // Released at end (cmd 07h)
    CHECK_TRUE(arm.magnetOnTStates() > 0);

    // Trajectory table 1167h..1173h:
    //   Motor 1: +10 steps (00h, 0Ah), then -14 steps (01h, 0Eh) => net -4, total 24
    //   Motor 2: +16 steps (02h, 10h)                            => net +16, total 16
    //   Motor 3: -8 steps (05h, 08h), then +11 steps (04h, 0Bh)  => net +3, total 19
    LONGS_EQUAL(-4, arm.motor(0).steps);
    LONGS_EQUAL(24, arm.motor(0).totalSteps);
    LONGS_EQUAL(16, arm.motor(1).steps);
    LONGS_EQUAL(16, arm.motor(1).totalSteps);
    LONGS_EQUAL(3, arm.motor(2).steps);
    LONGS_EQUAL(19, arm.motor(2).totalSteps);
}

TEST(Abc80ExtCircuit, RapidCheckPropertiesForRoboticArmOscilloscopeAndIdCodec)
{
    // Property 1: ExtCircuitId name <-> parse round-trip and rejection of arbitrary invalid strings
    rc::check("ExtCircuitId round-trip and invalid rejection", [](const std::string &arbitrary) {
        const std::array<ExtCircuitId, 7> ids = {
            ExtCircuitId::None,
            ExtCircuitId::TrafficLight,
            ExtCircuitId::LedChaser,
            ExtCircuitId::Oscilloscope,
            ExtCircuitId::SchoolBell,
            ExtCircuitId::ApplianceControl,
            ExtCircuitId::RoboticArm,
        };
        for (ExtCircuitId id : ids) {
            ExtCircuitId parsed = ExtCircuitId::TrafficLight;
            RC_ASSERT(parseExtCircuitId(extCircuitIdToString(id), parsed));
            RC_ASSERT(parsed == id);
        }

        ExtCircuitId out = ExtCircuitId::None;
        const bool ok = parseExtCircuitId(arbitrary, out);
        if (ok) {
            RC_ASSERT(std::string(extCircuitIdToString(out)) == arbitrary);
        }
    });

    // Property 2: Abc80RoboticArm Gray-code quadrature step counting is exact and reversible
    rc::check("Abc80RoboticArm reversible Gray-code quadrature steps",
              [](const std::vector<int8_t> &dirSeq) {
        Abc80Board board;
        Abc80RoboticArm arm;
        board.attachCircuit(&arm);
        board.getPpi().writePort(0x03, 0x80);
        board.getPpi().writePort(0x00, 0x00);

        constexpr uint8_t kGrayOrder[4] = { 0x00, 0x01, 0x03, 0x02 };
        int idx = 0;
        int32_t expectedNet = 0;
        uint32_t expectedTotal = 0;

        for (int8_t d : dirSeq) {
            const int step = (d >= 0) ? 1 : -1;
            idx = (idx + step + 4) & 3;
            expectedNet += step;
            expectedTotal += 1;
            const uint8_t phase = kGrayOrder[idx];
            board.getPpi().writePort(0x00, phase);
            board.stepTStates(10);

            RC_ASSERT(arm.motor(0).phase == phase);
            RC_ASSERT(arm.motor(0).coils == Abc80RoboticArm::expandCoils(phase));
        }

        RC_ASSERT(arm.motor(0).steps == expectedNet);
        RC_ASSERT(arm.motor(0).totalSteps == expectedTotal);
    });

    // Property 3: Abc80Oscilloscope coordinate decoding, dwell accumulation, and trace cap
    rc::check("Abc80Oscilloscope dwell accumulation and bounded trace",
              [](const std::vector<uint8_t> &coords) {
        Abc80Board board;
        Abc80Oscilloscope scope;
        board.attachCircuit(&scope);
        board.getPpi().writePort(0x83, 0x90);

        for (uint8_t b : coords) {
            board.getPpi().writePort(0x81, b);
            board.stepTStates(25);
        }

        RC_ASSERT(scope.sampleCount() == coords.size());
        RC_ASSERT(scope.trace().size() <= Abc80Oscilloscope::kMaxTracePoints);
        if (!coords.empty()) {
            const uint8_t last = coords.back();
            RC_ASSERT(scope.isBeamValid());
            RC_ASSERT(scope.beamX() == (last & 0x0F));
            RC_ASSERT(scope.beamY() == ((last >> 4) & 0x0F));
            RC_ASSERT(scope.gridDwell(last & 0x0F, (last >> 4) & 0x0F) >= 25u);
        }
    });
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
