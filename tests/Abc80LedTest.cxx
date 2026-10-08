/*
 * Abc80LedTest.cxx
 *
 * HALT and EP LEDs of the real ROMs: HALT follows the CPU /HALT pin; EP is driven by the
 * monitor's EPROM burn routine through the auxiliary 8255.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <cstdint>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

TEST_GROUP(Abc80Led)
{
    Abc80Board board;
};

TEST(Abc80Led, A13_HaltLedLightsOnAHaltInstructionAndResetClearsIt)
{
    bootToMonitor(board, RomId::Monitor);
    CHECK(!board.haltLed());
    board.getBus().writeByte(0x2000, 0x76);  // halt
    board.getCpu().prefetch(0x2000);
    runFrames(board, 2);
    CHECK(board.haltLed());
    runFrames(board, 5);
    CHECK(board.haltLed());  // stays lit until reset
    board.reset();
    runFrames(board, 1);
    CHECK(!board.haltLed());
}

// Calls the monitor's burn routine (0x0290) from RAM with real hardware behavior: the EP LED
// rises when it asserts Port C bits 5 and 6 and falls when it releases them.
TEST(Abc80Led, EpLedFollowsTheBurnRoutine)
{
    bootToMonitor(board, RomId::Monitor);
    board.getBus().writeByte(0x2100, 0xA5);  // the byte to "burn", read from (hl)
    const uint8_t stub[] = { 0x31, 0xBF, 0x17,        // ld sp,17BFh
                             0x21, 0x00, 0x21,        // ld hl,2100h
                             0x11, 0x10, 0x00,        // ld de,0010h
                             0xCD, 0x90, 0x02,        // call burn
                             0x76 };                  // halt
    for (size_t i = 0; i < sizeof(stub); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), stub[i]);
    }
    board.getCpu().prefetch(0x2000);
    CHECK(!board.epLed());
    uint64_t litT = 0;
    bool wasLit = false;
    for (uint64_t t = 0; t < 4000000 && !board.getCpu().isHalted(); ++t) {
        board.getCpu().tick();
        if (board.epLed()) {
            ++litT;
            wasLit = true;
        }
    }
    CHECK(wasLit);
    CHECK(board.getCpu().isHalted());
    CHECK(!board.epLed());  // released again
    // The pulse covers 5 display scan passes (b = 5 in burn) of 2,871 T each, plus a little overhead.
    CHECK(litT > 5 * 2871 && litT < 6 * 2871 + 2000);
}

TEST(Abc80Led, BothLedsAreDarkAtTheMonitorStart)
{
    bootToMonitor(board, RomId::Monitor);
    CHECK(!board.haltLed());
    CHECK(!board.epLed());
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
