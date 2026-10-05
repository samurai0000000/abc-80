/*
 * Abc80FaultInjectionTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Cpu.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80FaultInjection)
{
    Abc80Bus bus;
    Abc80Ppi ppi;
    Abc80Cpu* cpu;

    void setup() override
    {
        bus.reset();
        ppi.reset();
        cpu = new Abc80Cpu(bus, &ppi);
    }

    void teardown() override
    {
        delete cpu;
    }
};

TEST(Abc80FaultInjection, ProgramCounterWrapAround)
{
    // Write NOP (0x00) at 0xFFFF, and NOP at 0x0000
    bus.writeByte(0xFFFF, 0x00);
    bus.writeByte(0x0000, 0x00);

    cpu->prefetch(0xFFFF);
    LONGS_EQUAL(0xFFFF, cpu->getPC());

    // Step instruction: PC wraps from 0xFFFF to 0x0000
    cpu->stepInstruction();
    LONGS_EQUAL(0x0000, cpu->getPC());
}

TEST(Abc80FaultInjection, StackPointerWrapAround)
{
    // SP = 0x0000, executing PUSH BC should write high byte to 0xFFFF, low byte to 0xFFFE, SP = 0xFFFE
    // C5: PUSH BC
    bus.writeByte(0x2000, 0xC5);
    cpu->prefetch(0x2000);
    cpu->setSP(0x0000);
    cpu->setBC(0x1234);

    cpu->stepInstruction();
    LONGS_EQUAL(0xFFFE, cpu->getSP());
    BYTES_EQUAL(0x34, bus.readByte(0xFFFE));
    BYTES_EQUAL(0x12, bus.readByte(0xFFFF));
}

TEST(Abc80FaultInjection, RelativeJumpBoundaryLimits)
{
    // 1. JR +127 (maximum positive offset: 0x7F)
    // 18 7F: JR +127 at 0x2000.
    // In Z80, offset is relative to PC after the 2-byte JR instruction (0x2002).
    // Target = 0x2002 + 127 = 0x2081.
    bus.writeByte(0x2000, 0x18);
    bus.writeByte(0x2001, 0x7F);
    cpu->prefetch(0x2000);
    cpu->stepInstruction();
    LONGS_EQUAL(0x2081, cpu->getPC());

    // 2. JR -128 (maximum negative offset: 0x80)
    // 18 80: JR -128 at 0x2081.
    // Offset is relative to 0x2083.
    // Target = 0x2083 - 128 = 0x2003.
    bus.writeByte(0x2081, 0x18);
    bus.writeByte(0x2082, 0x80);
    cpu->prefetch(0x2081);
    cpu->stepInstruction();
    LONGS_EQUAL(0x2003, cpu->getPC());
}

TEST(Abc80FaultInjection, IndexedAddressingOffsetBounds)
{
    // 1. LD A, (IX + 127) [max positive displacement 0x7F]
    // DD 7E 7F: LD A, (IX+127)
    bus.writeByte(0x2000, 0xDD);
    bus.writeByte(0x2001, 0x7E);
    bus.writeByte(0x2002, 0x7F);
    bus.writeByte(0x307F, 0x5A);

    cpu->prefetch(0x2000);
    cpu->setIX(0x3000);
    cpu->stepInstruction();
    BYTES_EQUAL(0x5A, cpu->getA());

    // 2. LD A, (IX - 128) [max negative displacement 0x80]
    // DD 7E 80: LD A, (IX-128)
    bus.writeByte(0x2003, 0xDD);
    bus.writeByte(0x2004, 0x7E);
    bus.writeByte(0x2005, 0x80);
    bus.writeByte(0x2F80, 0xA5); // 0x3000 - 128 = 0x2F80

    cpu->prefetch(0x2003);
    cpu->setIX(0x3000);
    cpu->stepInstruction();
    BYTES_EQUAL(0xA5, cpu->getA());
}

TEST(Abc80FaultInjection, UnmappedMemoryFloatingBusRead)
{
    // All RAM is cleared to 0x00 or initialized, but reads from uninitialized ROM return 0xFF
    bus.reset();
    BYTES_EQUAL(0xFF, bus.readByte(0x0100)); // Unloaded ROM location returns 0xFF
}

TEST(Abc80FaultInjection, RomWriteProtectionViolation)
{
    bus.reset();
    // Attempt write to Monitor ROM EEPROM 0 (0x0200)
    bus.writeByte(0x0200, 0xAA);
    BYTES_EQUAL(0xFF, bus.readByte(0x0200)); // Write silently rejected

    // Attempt write to Expansion ROM EEPROM 1 (0x0A00)
    bus.writeByte(0x0A00, 0x55);
    BYTES_EQUAL(0xFF, bus.readByte(0x0A00)); // Write silently rejected
}

TEST(Abc80FaultInjection, UnmappedIoPortRead)
{
    // DB 99: IN A, (99h) - port 0x99 is not mapped
    bus.writeByte(0x2000, 0xDB);
    bus.writeByte(0x2001, 0x99);
    cpu->prefetch(0x2000);
    cpu->stepInstruction();
    BYTES_EQUAL(0xFF, cpu->getA()); // Floating I/O bus returns 0xFF
}

TEST(Abc80FaultInjection, HaltInstructionIdling)
{
    // 76: HALT
    bus.writeByte(0x2000, 0x76);
    cpu->prefetch(0x2000);
    cpu->stepInstruction();
    CHECK_TRUE(cpu->isHalted());

    // Verify stepping while halted advances T-states and cycles without freezing
    uint64_t initialCycles = cpu->getCycles();
    cpu->stepInstruction();
    CHECK_TRUE(cpu->isHalted());
    CHECK_TRUE(cpu->getCycles() > initialCycles);
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
