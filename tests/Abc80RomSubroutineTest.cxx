/*
 * Abc80RomSubroutineTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Cpu.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80RomSubroutine)
{
    Abc80Bus bus;
    Abc80Ppi ppi;
    Abc80Cpu* cpu;

    void setup() override
    {
        bus.reset();
        ppi.reset();
        cpu = new Abc80Cpu(bus, &ppi);
        // Load authentic 1995 rom.abc image into EEPROM 0 & 1 (0x0000 - 0x0FFF)
        Abc80Status st = bus.loadRomFile("vintage/rom.abc");
        LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(st));
    }

    void teardown() override
    {
        delete cpu;
    }

    // Helper: Execute subroutine via CALL at scratch address 0x2500 followed by HALT
    void callSubroutine(uint16_t subroutineAddr)
    {
        bus.writeByte(0x2500, 0xCD); // CALL nn
        bus.writeByte(0x2501, static_cast<uint8_t>(subroutineAddr & 0xFF));
        bus.writeByte(0x2502, static_cast<uint8_t>((subroutineAddr >> 8) & 0xFF));
        bus.writeByte(0x2503, 0x76); // HALT

        cpu->setSP(0x17BF);
        cpu->prefetch(0x2500);

        uint32_t safetyCounter = 0;
        while (!cpu->isHalted() && safetyCounter < 100000) {
            cpu->stepInstruction();
            safetyCounter++;
        }
    }
};

TEST(Abc80RomSubroutine, OnbysegAll16HexDigitsTo7SegmentMasks)
{
    // segtab at 0x07F0:
    // bd 30 9b ba 36 ae af 38 bf be 3f a7 8d b3 8f 0f
    const uint8_t expectedMasks[16] = {
        0xBD, 0x30, 0x9B, 0xBA, 0x36, 0xAE, 0xAF, 0x38,
        0xBF, 0xBE, 0x3F, 0xA7, 0x8D, 0xB3, 0x8F, 0x0F
    };

    for (uint8_t nibble = 0; nibble < 16; ++nibble) {
        cpu->setA(nibble);
        cpu->setHL(0x1234); // HL must be preserved
        callSubroutine(0x0519); // onbyseg

        BYTES_EQUAL(expectedMasks[nibble], cpu->getA());
        LONGS_EQUAL(0x1234, cpu->getHL()); // HL preserved
    }
}

TEST(Abc80RomSubroutine, TobysegEncodesByteIntoConsecutiveDisplayDigits)
{
    // Encode byte 0x35:
    // Low nibble 0x5 -> 0xAE written to (HL)
    // High nibble 0x3 -> 0xBA written to (HL+1)
    // HL incremented by 2
    cpu->setA(0x35);
    cpu->setHL(0x2000);
    callSubroutine(0x0525); // tobyseg

    BYTES_EQUAL(0xAE, bus.readByte(0x2000));
    BYTES_EQUAL(0xBA, bus.readByte(0x2001));
    LONGS_EQUAL(0x2002, cpu->getHL());
}

TEST(Abc80RomSubroutine, AdrsdpFormats16BitAddressInto4DisplayDigits)
{
    // Format DE = 0x1234 into dispbf+2 .. dispbf+5 (0x17C8 - 0x17CB)
    // E = 0x34 -> dispbf+2 = '4' (0x36), dispbf+3 = '3' (0xBA)
    // D = 0x12 -> dispbf+4 = '2' (0x9B), dispbf+5 = '1' (0x30)
    cpu->setDE(0x1234);
    callSubroutine(0x0504); // adrsdp

    BYTES_EQUAL(0x36, bus.readByte(0x17C8));
    BYTES_EQUAL(0xBA, bus.readByte(0x17C9));
    BYTES_EQUAL(0x9B, bus.readByte(0x17CA));
    BYTES_EQUAL(0x30, bus.readByte(0x17CB));
}

TEST(Abc80RomSubroutine, DadpFormats8BitDataInto2DisplayDigits)
{
    // Format A = 0x5B into dispbf .. dispbf+1 (0x17C6 - 0x17C7)
    // Low nibble 0xB -> 0xA7, High nibble 0x5 -> 0xAE
    cpu->setA(0x5B);
    callSubroutine(0x0511); // dadp

    BYTES_EQUAL(0xA7, bus.readByte(0x17C6));
    BYTES_EQUAL(0xAE, bus.readByte(0x17C7));
}

TEST(Abc80RomSubroutine, RamchkDistinguishesRomFromRamAndPreservesContents)
{
    // 1. Point HL to ROM (0x0500): Write should fail, CP fails -> Z=0
    cpu->setHL(0x0500);
    uint8_t originalRomVal = bus.readByte(0x0500);
    callSubroutine(0x049D); // ramchk
    CHECK_FALSE(cpu->getFlagZ()); // Z=0 indicates ROM / read-only
    BYTES_EQUAL(originalRomVal, bus.readByte(0x0500)); // Contents unaltered

    // 2. Point HL to RAM (0x1500): Write succeeds, contents restored -> Z=1
    bus.writeByte(0x1500, 0x42);
    cpu->setHL(0x1500);
    callSubroutine(0x049D); // ramchk
    CHECK_TRUE(cpu->getFlagZ()); // Z=1 indicates writable RAM
    BYTES_EQUAL(0x42, bus.readByte(0x1500)); // Contents restored

    // 3. Point HL to High Expansion RAM (0x3000): Z=1
    bus.writeByte(0x3000, 0x99);
    cpu->setHL(0x3000);
    callSubroutine(0x049D);
    CHECK_TRUE(cpu->getFlagZ());
    BYTES_EQUAL(0x99, bus.readByte(0x3000));
}

TEST(Abc80RomSubroutine, TestmValidatesState1And2AndAbortsOnState0)
{
    // 1. state == 1: returns cleanly to caller
    bus.writeByte(0x17F4, 0x01);
    callSubroutine(0x0537); // testm
    LONGS_EQUAL(0x2503, cpu->getPC()); // Halted at 0x2503 after return

    // 2. state == 2: returns cleanly to caller
    bus.writeByte(0x17F4, 0x02);
    callSubroutine(0x0537);
    LONGS_EQUAL(0x2503, cpu->getPC());

    // 3. state == 0: pops caller return addr, jumps to errdis (0x0375), sets bit 7 of test (0x17F6)
    bus.writeByte(0x17F4, 0x00);
    bus.writeByte(0x17F6, 0x00);
    callSubroutine(0x0537);
    // Returns to outer level with bit 7 of 0x17F6 set
    BYTES_EQUAL(0x80, bus.readByte(0x17F6) & 0x80);
}

TEST(Abc80RomSubroutine, Cl1bytAndCl2bytConditionalMemoryClearing)
{
    // 1. cl1byt (0x037C): if test flag (0x17F6) is 0, does nothing
    bus.writeByte(0x17F6, 0x00);
    bus.writeByte(0x2100, 0x55);
    cpu->setHL(0x2100);
    callSubroutine(0x037C); // cl1byt
    BYTES_EQUAL(0x55, bus.readByte(0x2100)); // Unmodified

    // 2. cl1byt: if test flag is non-zero, clears byte at (HL) and clears test flag
    bus.writeByte(0x17F6, 0x01);
    bus.writeByte(0x2100, 0x55);
    cpu->setHL(0x2100);
    callSubroutine(0x037C);
    BYTES_EQUAL(0x00, bus.readByte(0x2100)); // Cleared
    BYTES_EQUAL(0x00, bus.readByte(0x17F6)); // Test flag cleared

    // 3. cl2byt (0x0389): if test flag is set, clears 2 bytes at (HL) and (HL+1), preserves HL
    bus.writeByte(0x17F6, 0x01);
    bus.writeByte(0x2100, 0xAA);
    bus.writeByte(0x2101, 0xBB);
    cpu->setHL(0x2100);
    callSubroutine(0x0389); // cl2byt
    BYTES_EQUAL(0x00, bus.readByte(0x2100));
    BYTES_EQUAL(0x00, bus.readByte(0x2101));
    LONGS_EQUAL(0x2100, cpu->getHL()); // HL preserved
    BYTES_EQUAL(0x00, bus.readByte(0x17F6));
}

TEST(Abc80RomSubroutine, GetptrCalculatesStartEndAndLength)
{
    // Write start address = 0x2000 at 0x17C1 (low=0x00, high=0x20)
    // Write end address   = 0x2009 at 0x17C3 (low=0x09, high=0x20)
    bus.writeByte(0x17C1, 0x00);
    bus.writeByte(0x17C2, 0x20);
    bus.writeByte(0x17C3, 0x09);
    bus.writeByte(0x17C4, 0x20);

    callSubroutine(0x02BB); // getptr

    LONGS_EQUAL(0x2000, cpu->getHL()); // HL = start
    LONGS_EQUAL(10, cpu->getBC());     // BC = length (end - start + 1 = 10)
    CHECK_FALSE(cpu->getFlagC());      // Carry cleared
}

TEST(Abc80RomSubroutine, SumCalculatesMemoryBlockChecksum)
{
    // Set start = 0x2100, end = 0x2103 (4 bytes)
    bus.writeByte(0x17C1, 0x00);
    bus.writeByte(0x17C2, 0x21);
    bus.writeByte(0x17C3, 0x03);
    bus.writeByte(0x17C4, 0x21);

    // Write known values: 10 + 20 + 30 + 40 = 100 (0x64)
    bus.writeByte(0x2100, 10);
    bus.writeByte(0x2101, 20);
    bus.writeByte(0x2102, 30);
    bus.writeByte(0x2103, 40);

    callSubroutine(0x048F); // sum
    BYTES_EQUAL(100, cpu->getA());
    CHECK_FALSE(cpu->getFlagC());
}

TEST(Abc80RomSubroutine, GtpanaRetrievesParameterLabels)
{
    // state at 0x17F4 = 3 (parameter mode)
    bus.writeByte(0x17F4, 0x03);

    // stmoni = 0 -> parameter label 0x0F
    bus.writeByte(0x17F3, 0x00);
    callSubroutine(0x03E0); // gtpana
    BYTES_EQUAL(0x0F, cpu->getA());

    // stmoni = 1 -> parameter label 0xAE
    bus.writeByte(0x17F3, 0x01);
    callSubroutine(0x03E0);
    BYTES_EQUAL(0xAE, cpu->getA());

    // stmoni = 2 -> parameter label 0x8F
    bus.writeByte(0x17F3, 0x02);
    callSubroutine(0x03E0);
    BYTES_EQUAL(0x8F, cpu->getA());
}

TEST(Abc80RomSubroutine, AdraddAndAdrdecStepCurrentAddress)
{
    // Set adsave at 0x17EE = 0x1234
    bus.writeByte(0x17EE, 0x34);
    bus.writeByte(0x17EF, 0x12);

    // Call adradd (0x013D): increments adsave to 0x1235
    callSubroutine(0x013D);
    BYTES_EQUAL(0x35, bus.readByte(0x17EE));
    BYTES_EQUAL(0x12, bus.readByte(0x17EF));

    // Call adrdec (0x0160): decrements adsave back to 0x1234
    callSubroutine(0x0160);
    BYTES_EQUAL(0x34, bus.readByte(0x17EE));
    BYTES_EQUAL(0x12, bus.readByte(0x17EF));
}

TEST(Abc80RomSubroutine, Hda1ShiftsHexDigitIntoAddressWord)
{
    // Set test flag = 0 (do not clear word on entry)
    bus.writeByte(0x17F6, 0x00);
    // adsave = 0x1234
    bus.writeByte(0x17EE, 0x34);
    bus.writeByte(0x17EF, 0x12);

    // Shift hex digit 0x05 into address word
    cpu->setC(0x05);
    callSubroutine(0x0116); // hda1

    // RLD shifts nibbles left: 0x1234 shifted left by 4 with 5 inserted -> 0x2345
    BYTES_EQUAL(0x45, bus.readByte(0x17EE));
    BYTES_EQUAL(0x23, bus.readByte(0x17EF));
}

TEST(Abc80RomSubroutine, BranchDispatchesViaIndexedJumpTables)
{
    // Branch at 0x036A dispatches via jump table at HL
    // Create a mini jump table in scratch RAM at 0x2200
    // Table format: base address DE, then offset bytes
    bus.writeByte(0x2200, 0x50); // DE = 0x2050 (target base)
    bus.writeByte(0x2201, 0x20);
    bus.writeByte(0x2202, 0x00); // Index 0: offset 0 -> target 0x2050
    bus.writeByte(0x2203, 0x04); // Index 1: offset 4 -> target 0x2054

    // At 0x2050: write NOP; HALT
    bus.writeByte(0x2050, 0x00);
    bus.writeByte(0x2051, 0x76);

    // Write CALL 0x036A; HALT at 0x2500
    bus.writeByte(0x2500, 0xCD);
    bus.writeByte(0x2501, 0x6A);
    bus.writeByte(0x2502, 0x03);
    bus.writeByte(0x2503, 0x76);

    cpu->setSP(0x17BF);
    cpu->setA(0x00);        // Index 0
    cpu->setHL(0x2200);     // Jump table pointer
    cpu->prefetch(0x2500);

    uint32_t safety = 0;
    while (!cpu->isHalted() && safety < 100000) {
        cpu->stepInstruction();
        safety++;
    }

    // Branch dispatched to target at 0x2050 and halted at 0x2051
    LONGS_EQUAL(0x2051, cpu->getPC());
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
