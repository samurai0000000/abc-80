/*
 * Abc80CpuTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Cpu.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80Cpu)
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

TEST(Abc80Cpu, ResetSetsRegistersToHardwareDefaults)
{
    cpu->reset();
    LONGS_EQUAL(0x0000, cpu->getPC());
    LONGS_EQUAL(0xFFFF, cpu->getAF());
    LONGS_EQUAL(0xFFFF, cpu->getBC());
    LONGS_EQUAL(0xFFFF, cpu->getDE());
    LONGS_EQUAL(0xFFFF, cpu->getHL());
    LONGS_EQUAL(0xFFFF, cpu->getSP());
    LONGS_EQUAL(0xFFFF, cpu->getIX());
    LONGS_EQUAL(0xFFFF, cpu->getIY());
    LONGS_EQUAL(0, cpu->getCycles());
    CHECK_FALSE(cpu->isHalted());
}

TEST(Abc80Cpu, RegisterGettersAndSetters)
{
    cpu->setA(0x12);
    cpu->setF(0x34);
    BYTES_EQUAL(0x12, cpu->getA());
    BYTES_EQUAL(0x34, cpu->getF());
    LONGS_EQUAL(0x1234, cpu->getAF());

    cpu->setBC(0xABCD);
    BYTES_EQUAL(0xAB, cpu->getB());
    BYTES_EQUAL(0xCD, cpu->getC());
    LONGS_EQUAL(0xABCD, cpu->getBC());

    cpu->setDE(0x5678);
    BYTES_EQUAL(0x56, cpu->getD());
    BYTES_EQUAL(0x78, cpu->getE());
    LONGS_EQUAL(0x5678, cpu->getDE());

    cpu->setHL(0x9ABC);
    BYTES_EQUAL(0x9A, cpu->getH());
    BYTES_EQUAL(0xBC, cpu->getL());
    LONGS_EQUAL(0x9ABC, cpu->getHL());

    cpu->setIX(0x1122);
    LONGS_EQUAL(0x1122, cpu->getIX());

    cpu->setIY(0x3344);
    LONGS_EQUAL(0x3344, cpu->getIY());

    cpu->setSP(0x17BF);
    LONGS_EQUAL(0x17BF, cpu->getSP());

    cpu->setPC(0x0070);
    LONGS_EQUAL(0x0070, cpu->getPC());

    cpu->setAF2(0xCAFE);
    LONGS_EQUAL(0xCAFE, cpu->getAF2());

    cpu->setBC2(0xBEEF);
    LONGS_EQUAL(0xBEEF, cpu->getBC2());

    cpu->setDE2(0xDEAD);
    LONGS_EQUAL(0xDEAD, cpu->getDE2());

    cpu->setHL2(0xF00D);
    LONGS_EQUAL(0xF00D, cpu->getHL2());

    cpu->setI(0x20);
    BYTES_EQUAL(0x20, cpu->getI());

    cpu->setR(0x5A);
    BYTES_EQUAL(0x5A, cpu->getR());
}

TEST(Abc80Cpu, FlagsManipulation)
{
    cpu->setF(0x00);
    CHECK_FALSE(cpu->getFlagC());
    CHECK_FALSE(cpu->getFlagN());
    CHECK_FALSE(cpu->getFlagPV());
    CHECK_FALSE(cpu->getFlagX());
    CHECK_FALSE(cpu->getFlagH());
    CHECK_FALSE(cpu->getFlagY());
    CHECK_FALSE(cpu->getFlagZ());
    CHECK_FALSE(cpu->getFlagS());

    cpu->setFlagC(true);
    CHECK_TRUE(cpu->getFlagC());
    cpu->setFlagZ(true);
    CHECK_TRUE(cpu->getFlagZ());
    cpu->setFlagS(true);
    CHECK_TRUE(cpu->getFlagS());
    cpu->setFlagH(true);
    CHECK_TRUE(cpu->getFlagH());
    cpu->setFlagPV(true);
    CHECK_TRUE(cpu->getFlagPV());
    cpu->setFlagN(true);
    CHECK_TRUE(cpu->getFlagN());

    cpu->setFlagC(false);
    CHECK_FALSE(cpu->getFlagC());
    CHECK_TRUE(cpu->getFlagZ());
}

TEST(Abc80Cpu, NopExecutionAdvancesPc)
{
    // Write NOP (0x00) at 0x1000
    bus.writeByte(0x1000, 0x00);
    cpu->prefetch(0x1000);

    uint32_t tstates = cpu->stepInstruction();
    LONGS_EQUAL(4, tstates);
    LONGS_EQUAL(0x1001, cpu->getPC());
}

TEST(Abc80Cpu, LdImmediateInstruction)
{
    // Write LD A, 42h (0x3E, 0x42) at 0x1000
    bus.writeByte(0x1000, 0x3E);
    bus.writeByte(0x1001, 0x42);
    cpu->prefetch(0x1000);

    uint32_t tstates = cpu->stepInstruction();
    LONGS_EQUAL(7, tstates);
    BYTES_EQUAL(0x42, cpu->getA());
    LONGS_EQUAL(0x1002, cpu->getPC());
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
