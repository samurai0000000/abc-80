/*
 * Abc80ResetTest.cxx
 *
 * Reset, ROM power-cycle, and program-counter behavior of the real ROMs, plus the Envelope 1
 * negative control (one deliberately wrong test, ignored in `make test`, required to fail by the gate).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <cstdint>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

TEST_GROUP(Abc80Reset)
{
    Abc80Board board;
};

TEST(Abc80Reset, A9_ResetKeepsRamReplaysTheChimeAndReturnsToTheStartDisplay)
{
    bootToMonitor(board, RomId::Monitor);
    board.getBus().writeByte(0x2000, 0x55);  // a marker in RAM
    KeyDriver keys(board, romProfile(RomId::Monitor).keys);
    keys.type(KEY_ADRS);
    keys.type(KEY_1);
    keys.type(KEY_2);
    keys.frames(30);
    LONGS_EQUAL(0x0012, adsave(board));

    board.reset();
    board.stepFrame();
    LONGS_EQUAL(0x80, board.getBus().readByte(ABC80_PWUP_ADDR));  // still the warm flag
    LONGS_EQUAL(0x55, board.getBus().readByte(0x2000));            // RAM persists
    for (uint8_t m : board.displayMasks(0.05)) BYTES_EQUAL(0, m);   // dark while the chime plays

    size_t edges = board.frameAudio().edgeCount;
    CHECK(board.bootTurbo(RomId::Monitor, 60000000));  // the chime ends and the monitor is back
    runFrames(board, 10);
    CHECK(edges > 0);                                                // the chime started again
    LONGS_EQUAL(0x1000, adsave(board));                              // the monitor reinitialized the address
    bool anyLit = false;
    for (uint8_t m : board.displayMasks(0.05)) anyLit = anyLit || (m != 0);
    CHECK(anyLit);
}

TEST(Abc80Reset, A9_ResetReleasesAnyHeldKey)
{
    bootToMonitor(board, RomId::Monitor);
    board.pressKey(KEY_ADRS);
    board.reset();
    CHECK(!board.getPpi().isKeyPressed(KEY_ADRS));
}

TEST(Abc80Reset, A11_PowerCyclingToAnotherRomClearsRamAndBootsIt)
{
    bootToMonitor(board, RomId::Monitor);
    board.getBus().writeByte(0x2000, 0x55);
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Vintage1993, ".")));
    LONGS_EQUAL(0x00, board.getBus().readByte(0x2000));            // RAM cleared
    LONGS_EQUAL(0x80, board.getBus().readByte(ABC80_PWUP_ADDR));   // warm start flag set
    CHECK(board.bootTurbo(RomId::Vintage1993, 60000000));
    CHECK(board.reachedReady(RomId::Vintage1993));
    runFrames(board, 5);
    for (uint8_t m : board.displayMasks(0.05)) BYTES_EQUAL(0, m);  // A12: dark link-server state
    board.pressKey(KEY_1);
    runFrames(board, 30);
    for (uint8_t m : board.displayMasks(0.05)) BYTES_EQUAL(0, m);  // keys do nothing there
    board.releaseKey(KEY_1);
}

TEST(Abc80Reset, ReachedReadyIsTheSteadyStateFetchOfEachRom)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    CHECK(!board.reachedReady(RomId::Monitor));
    CHECK(board.bootTurbo(RomId::Monitor, 60000000));
    CHECK(board.reachedReady(RomId::Monitor));
}

// The address of the last opcode fetch is exact, at any T-state, even while the address bus holds a data address.
TEST(Abc80Reset, LastFetchAddressIsTheOpcodeAddressNotTheDataBusAddress)
{
    bootToMonitor(board, RomId::Monitor);
    // 2000: ld a,(2100h)   2003: jp 2000   -- the data read puts 0x2100 on the address bus.
    const uint8_t prog[] = { 0x3A, 0x00, 0x21, 0xC3, 0x00, 0x20 };
    for (size_t i = 0; i < sizeof(prog); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), prog[i]);
    }
    board.getCpu().prefetch(0x2000);
    for (int f = 0; f < 3; ++f) {
        board.stepFrame();
        const uint16_t a = board.lastFetchAddress();
        CHECK(a == 0x2000 || a == 0x2003);
    }
}

// Why the raw core register is not used: it differs from the next opcode address by an amount
// that depends on the instruction.
TEST(Abc80Reset, TheRawCorePcIsNotTheNextOpcodeAddress)
{
    bootToMonitor(board, RomId::Monitor);
    int different = 0;
    int checked = 0;
    for (int i = 0; i < 500; ++i) {
        board.getCpu().stepInstruction();
        const uint16_t pc = board.getCpu().getRegPC();
        for (int t = 0; t < 40; ++t) {
            const uint64_t pins = board.getCpu().tick();
            if ((pins & Z80_M1) && (pins & Z80_MREQ) && (pins & Z80_RD)) {
                if (Z80_GET_ADDR(pins) != pc) ++different;
                ++checked;
                break;
            }
        }
    }
    CHECK(checked > 400);
    CHECK(different > 50);
}

// ---- Negative control (gate): one test, one failure; ignored in `make test`, run with -ri ----

TEST_GROUP(Abc80Env1Negative)
{
};

IGNORE_TEST(Abc80Env1Negative, WrongExpectationsMustFail)
{
    Abc80Board b;
    bootToMonitor(b, RomId::Monitor);
    KeyDriver keys(b, romProfile(RomId::Monitor).keys);
    keys.type(KEY_ADRS);
    keys.type(KEY_1);
    keys.type(KEY_2);
    keys.frames(30);
    const bool wrongAddress = (adsave(b) == 0x0021);          // digits swapped: false
    const bool wrongLed = b.haltLed();                        // the monitor is running: false
    const bool wrongMask = (b.displayMasks(0.05)[0] == 0xFF);  // not all segments lit: false
    CHECK(wrongAddress && wrongLed && wrongMask);
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
