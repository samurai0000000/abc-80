/*
 * Abc80SpeakerTest.cxx
 *
 * Speaker behavior (Port C bit 7) of the real ROMs: every toggle is logged with its
 * T-state offset inside the frame, and the green LED is the share of the frame the
 * line was low.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <cstdint>
#include <vector>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

TEST_GROUP(Abc80Speaker)
{
    Abc80Board board;
};

// One frame run tick by tick, with an independent log of Port C bit 7 for comparison.
struct ObservedFrame {
    bool startLevel;
    std::vector<uint32_t> offsets;
    uint32_t lowT;
};

static ObservedFrame observeFrame(Abc80Board &board)
{
    Abc80Ppi &ppi = board.getPpi();
    ObservedFrame o{};
    ppi.beginFrame();
    o.startLevel = (ppi.getDigitStrobe() & 0x80) != 0;
    bool level = o.startLevel;
    for (uint32_t t = 1; t <= ABC80_FRAME_TSTATES; ++t) {
        if (!level) ++o.lowT;  // the level before this tick applies during it
        board.getCpu().tick();
        const bool now = (ppi.getDigitStrobe() & 0x80) != 0;
        if (now != level) {
            o.offsets.push_back(t);
            level = now;
        }
    }
    ppi.endFrame();
    return o;
}

TEST(Abc80Speaker, ChimeEdgesMatchAnIndependentLogFrameByFrame)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    size_t total = 0;
    for (int f = 0; f < 90; ++f) {  // the reset chime lasts about 3 s = 183 frames; sample the first 90
        const ObservedFrame o = observeFrame(board);
        const Abc80Ppi::FrameAudio &a = board.getPpi().frameAudio();
        CHECK_EQUAL(o.startLevel, a.startLevel);
        LONGS_EQUAL(static_cast<long>(o.offsets.size()), static_cast<long>(a.edgeCount));
        CHECK(!a.overflow);
        for (size_t i = 0; i < o.offsets.size(); ++i) {
            LONGS_EQUAL(o.offsets[i], a.offsets[i]);
        }
        LONGS_EQUAL(o.lowT, a.lowTStates);
        total += o.offsets.size();
    }
    CHECK(total > 100);  // the chime really toggles the speaker
}

TEST(Abc80Speaker, IdleMonitorHasNoEdgesAndTheLedIsDark)
{
    bootToMonitor(board, RomId::Monitor);
    board.stepFrame();
    LONGS_EQUAL(0, board.frameAudio().edgeCount);
    LONGS_EQUAL(0, board.frameAudio().lowTStates);
    DOUBLES_EQUAL(0.0, board.speakerLedFraction(), 1e-9);
}

TEST(Abc80Speaker, LedFractionIsTheLowTimeShareOfTheFrame)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    bool sawMiddle = false;
    for (int f = 0; f < 60; ++f) {
        const ObservedFrame o = observeFrame(board);
        const double expected = static_cast<double>(o.lowT) / static_cast<double>(ABC80_FRAME_TSTATES);
        const double got = static_cast<double>(board.getPpi().frameAudio().lowTStates) /
                           static_cast<double>(board.getPpi().frameElapsedTStates());
        DOUBLES_EQUAL(expected, got, 1e-9);
        if (expected > 0.2 && expected < 0.8) sawMiddle = true;
    }
    CHECK(sawMiddle);  // during a tone the LED is neither fully on nor off
}

TEST(Abc80Speaker, KeyclickToggles448TimesOnBothRoms)
{
    for (RomId id : { RomId::Monitor, RomId::Vintage1993 }) {
        Abc80Board b;
        bootToMonitor(b, id);
        b.pressKey(KEY_ADRS);
        size_t edges = 0;
        bool started = false;
        for (int f = 0; f < 40; ++f) {  // accept, click (about 3 frames), then quiet again
            b.stepFrame();
            const size_t e = b.frameAudio().edgeCount;
            if (e > 0) started = true;
            edges += e;
            if (f == 3) b.releaseKey(KEY_ADRS);
        }
        CHECK(started);
        // ms3k17 toggles hl*2 = 0xE0*2 = 448 times; the ROM then drives the line high again once.
        CHECK(edges >= 448 && edges <= 450);
    }
}

TEST(Abc80Speaker, Rom1993LinkPollHoldsTheLineLowSoTheLedReadsOne)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Vintage1993, ".")));
    CHECK(board.bootTurbo(RomId::Vintage1993, 60000000));
    runFrames(board, 3);
    LONGS_EQUAL(0, board.frameAudio().edgeCount);
    DOUBLES_EQUAL(1.0, board.speakerLedFraction(), 1e-9);  // plan Section 2.2 R9: Port C is 0x3F here
}

// More toggles than a frame can log: real CPU executing real RAM code at the fastest toggle rate.
TEST(Abc80Speaker, MoreThanTheCapOfEdgesSetsOverflowAndKeepsTheFirstOnes)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    // loop: out (82h),a ; xor 80h ; jp loop
    const uint8_t prog[] = { 0x3E, 0xFF, 0xD3, 0x82, 0xEE, 0x80, 0xC3, 0x02, 0x20 };
    for (size_t i = 0; i < sizeof(prog); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), prog[i]);
    }
    board.getCpu().prefetch(0x2000);
    board.stepFrame();
    CHECK(board.frameAudio().overflow);
    LONGS_EQUAL(static_cast<long>(Abc80Ppi::FrameAudio::kMaxEdges), static_cast<long>(board.frameAudio().edgeCount));
    for (size_t i = 1; i < board.frameAudio().edgeCount; ++i) {
        CHECK(board.frameAudio().offsets[i] > board.frameAudio().offsets[i - 1]);
    }
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
