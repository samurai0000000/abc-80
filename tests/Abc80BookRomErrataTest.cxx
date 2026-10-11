/*
 * Abc80BookRomErrataTest.cxx
 *
 * The book monitor ROM (build/rom.bin) against the book's printed listing: the corrections of plan
 * Envelope 4 and the two places where the book matches the current code and must not change.
 * (tests/Abc80RomSubroutineTest.cxx loads the 1993 image, not this ROM.)
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <cstdint>
#include <vector>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

TEST_GROUP(Abc80BookRomErrata)
{
    Abc80Board board;

    void setup() override
    {
        LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    }

    uint8_t rom(uint16_t address) { return board.getBus().readByte(address); }

    void expectBytes(uint16_t address, const std::vector<uint8_t> &bytes)
    {
        for (size_t i = 0; i < bytes.size(); ++i) {
            BYTES_EQUAL(bytes[i], rom(static_cast<uint16_t>(address + i)));
        }
    }

    // Runs a stub at 2000h until the CPU halts or maxT T-states pass.
    bool runStub(const std::vector<uint8_t> &stub, uint64_t maxT)
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

    // GTPANA (03E0h) with the given state and parameter counter; returns A.
    uint8_t gtpana(uint8_t state, uint8_t stmoni)
    {
        board.getBus().writeByte(0x17F4, state);
        board.getBus().writeByte(0x17F3, stmoni);
        const std::vector<uint8_t> stub = { 0x31, 0xBF, 0x17,   // ld sp,17BFh
                                            0xCD, 0xE0, 0x03,   // call gtpana
                                            0x32, 0x00, 0x21,   // ld (2100h),a
                                            0x76 };             // halt
        CHECK(runStub(stub, 100000));
        return board.getBus().readByte(0x2100);
    }
};

TEST(Abc80BookRomErrata, TheTapeParameterLabelTableIsFilledIn)
{
    expectBytes(0x07E8, { 0x0F, 0xAE, 0x8F, 0x00, 0x0F, 0x00, 0x00, 0x00 });
}

TEST(Abc80BookRomErrata, GtpanaReturnsTheLabelsForToTapeAndFromTape)
{
    BYTES_EQUAL(0x0F, gtpana(3, 0));  // TO TAPE, parameter 0: F
    BYTES_EQUAL(0xAE, gtpana(3, 1));  // TO TAPE, parameter 1: S
    BYTES_EQUAL(0x8F, gtpana(3, 2));  // TO TAPE, parameter 2: E
    BYTES_EQUAL(0x0F, gtpana(4, 0));  // FROM TAPE, parameter 0: F
}

TEST(Abc80BookRomErrata, TheBookBytesAreInPlace)
{
    BYTES_EQUAL(0x0E, rom(0x047B));            // MS3K: book prints 0e 0e
    BYTES_EQUAL(0xC9, rom(0x04DE));            // KCOL: book prints 06 c9
    expectBytes(0x0750, { 0xC8, 0x1B });       // FRQTAB note 3: book p.202
    expectBytes(0x0770, { 0x4F, 0x45 });       // FRQTAB note 19: book p.202
    expectBytes(0x01C7, { 0xCD, 0x50, 0x04, 0xC3, 0x94, 0x00 });  // ERROR: call ms1k17, then jp setst0 at 01CAh
}

TEST(Abc80BookRomErrata, WhereTheBookMatchesTheCurrentCodeNothingChanged)
{
    BYTES_EQUAL(0xFF, rom(0x0481));  // SOUND: book prints 3e ff, LD A,FFH (the 1993 ROM's 3e bf is a later revision)
    expectBytes(0x054C, { 0xCD, 0xF6, 0x03, 0xCD, 0x0D, 0x04, 0x3E, 0x80, 0x32, 0xF5, 0x17 });  // INIT: melody, then pwup
}

TEST(Abc80BookRomErrata, TheErrorRoutineBeepsOnTheSpeakerLine)
{
    const std::vector<uint8_t> stub = { 0x31, 0xBF, 0x17,   // ld sp,17BFh
                                        0xC3, 0xC3, 0x01 }; // jp error
    for (size_t i = 0; i < stub.size(); ++i) {
        board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), stub[i]);
    }
    board.getCpu().prefetch(0x2000);
    unsigned edges = 0;
    bool last = (board.getPpi().getDigitStrobe() & 0x80) != 0;
    for (uint64_t t = 0; t < 1000000; ++t) {
        board.getCpu().tick();
        const bool now = (board.getPpi().getDigitStrobe() & 0x80) != 0;
        if (now != last) {
            ++edges;
            last = now;
        }
    }
    CHECK(edges >= 20);  // a 1 kHz tone of 80 half-cycle pairs, not a single click
}
