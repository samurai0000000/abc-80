/*
 * Abc80EpromProgrammerTest.cxx
 *
 * The EPROM in the programmer socket (U7), behind the auxiliary 8255 (ports 40h to 43h), exercised
 * through the monitor's own routines: DATAIN (027Eh), BURN (0290h) and the VERIFY loop (026Eh).
 * Book wiring (monitor listing pp.178-179): PA = address A0-A7, PC bits 0-2 = A8-A10 (the routines
 * mask with 7), PC bit 7 = chip enable for a read, PC bits 5 and 6 high = programming, PB = data.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <cstdint>
#include <vector>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

TEST_GROUP(Abc80EpromProgrammer)
{
    Abc80Board board;

    void setup() override
    {
        bootToMonitor(board, RomId::Monitor);
    }

    // Puts the stub at 2000h and runs until the CPU halts (true) or maxT T-states pass (false).
    bool runStub(const std::vector<uint8_t> &stub, uint64_t maxT = 4000000)
    {
        for (size_t i = 0; i < stub.size(); ++i) {
            board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), stub[i]);
        }
        board.getCpu().prefetch(0x2000);
        for (uint64_t t = 0; t < maxT; ++t) {
            board.getCpu().tick();
            if (board.getCpu().isHalted()) {
                return true;
            }
        }
        return false;
    }

    // DATAIN at the given address; the byte read is left in RAM at 2101h.
    uint8_t read(uint16_t address)
    {
        const std::vector<uint8_t> stub = { 0x31, 0xBF, 0x17,                                          // ld sp,17BFh
                                            0x11, static_cast<uint8_t>(address & 0xFF), static_cast<uint8_t>(address >> 8),  // ld de,address
                                            0xCD, 0x7E, 0x02,                                          // call datain
                                            0x32, 0x01, 0x21,                                          // ld (2101h),a
                                            0x76 };                                                    // halt
        CHECK(runStub(stub));
        return board.getBus().readByte(0x2101);
    }

    // BURN of one byte at the given address; the byte is taken from RAM at 2100h.
    void burn(uint16_t address, uint8_t value)
    {
        board.getBus().writeByte(0x2100, value);
        const std::vector<uint8_t> stub = { 0x31, 0xBF, 0x17,                                          // ld sp,17BFh
                                            0x21, 0x00, 0x21,                                          // ld hl,2100h
                                            0x11, static_cast<uint8_t>(address & 0xFF), static_cast<uint8_t>(address >> 8),  // ld de,address
                                            0xCD, 0x90, 0x02,                                          // call burn
                                            0x76 };                                                    // halt
        CHECK(runStub(stub));
    }

    // The monitor's VERIFY loop (026Eh) over one byte. Returns true when it accepts (RST 0: an opcode fetch at
    // 0000h) and false when it stops on HALT at 0275h.
    bool verifyOneByte(uint16_t address, uint8_t expected)
    {
        board.getBus().writeByte(0x2100, expected);
        const uint8_t stub[] = { 0x31, 0xBF, 0x17,                                                     // ld sp,17BFh
                                 0x21, 0x00, 0x21,                                                     // ld hl,2100h
                                 0x11, static_cast<uint8_t>(address & 0xFF), static_cast<uint8_t>(address >> 8),  // ld de,address
                                 0x01, 0x01, 0x00,                                                     // ld bc,0001h
                                 0xC3, 0x6E, 0x02 };                                                   // jp verify
        for (size_t i = 0; i < sizeof(stub); ++i) {
            board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), stub[i]);
        }
        board.getCpu().prefetch(0x2000);
        uint16_t lastFetch = 0xFFFF;
        for (uint64_t t = 0; t < 4000000; ++t) {
            const uint64_t pins = board.getCpu().tick();
            if ((pins & Z80_M1) && (pins & Z80_MREQ) && (pins & Z80_RD)) {
                lastFetch = Z80_GET_ADDR(pins);  // an opcode fetch (M1 cycle)
            }
            if (board.getCpu().isHalted()) {
                LONGS_EQUAL(0x0275, lastFetch);
                return false;
            }
            if (lastFetch == 0x0000) {
                return true;
            }
        }
        FAIL("the verify loop neither accepted nor halted");
        return false;
    }
};

TEST(Abc80EpromProgrammer, ABlankSocketReadsFFEverywhere)
{
    LONGS_EQUAL(0xFF, read(0x0000));
    LONGS_EQUAL(0xFF, read(0x0010));
    LONGS_EQUAL(0xFF, read(0x07FF));
}

TEST(Abc80EpromProgrammer, ABurnedByteReadsBackAndItsNeighboursStayBlank)
{
    burn(0x0010, 0xA5);
    LONGS_EQUAL(0xA5, board.getPpi().targetEprom(0x0010));
    LONGS_EQUAL(0xFF, board.getPpi().targetEprom(0x000F));
    LONGS_EQUAL(0xFF, board.getPpi().targetEprom(0x0011));
    LONGS_EQUAL(0xA5, read(0x0010));
    LONGS_EQUAL(0xFF, read(0x000F));
    LONGS_EQUAL(0xFF, read(0x0011));
}

TEST(Abc80EpromProgrammer, ProgrammingOnlyClearsBitsLikeARealEprom)
{
    burn(0x0010, 0xA5);
    burn(0x0010, 0x0F);
    LONGS_EQUAL(0x05, read(0x0010));
}

TEST(Abc80EpromProgrammer, TheMonitorRoutinesDriveOnlyElevenAddressBits)
{
    burn(0x0010, 0xA5);
    LONGS_EQUAL(0xA5, read(0x0810));  // the routines mask the high byte with 7 (A11 and up are not driven)
    burn(0x0720, 0x3C);
    LONGS_EQUAL(0x3C, board.getPpi().targetEprom(0x0720));
    LONGS_EQUAL(0x3C, read(0x0720));
}

TEST(Abc80EpromProgrammer, TheContentsSurviveABoardReset)
{
    burn(0x0010, 0xA5);
    board.reset();
    LONGS_EQUAL(0xA5, board.getPpi().targetEprom(0x0010));
}

TEST(Abc80EpromProgrammer, TheMonitorVerifyLoopAcceptsAGoodByteAndRejectsABadOne)
{
    burn(0x0010, 0xA5);
    CHECK(verifyOneByte(0x0010, 0xA5));
    bootToMonitor(board, RomId::Monitor);  // fresh monitor state; the socket contents stay
    board.getPpi().setTargetEprom(0x0010, 0xA4);
    CHECK(!verifyOneByte(0x0010, 0xA5));
}
