/*
 * Abc80CpuOpcodeTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Cpu.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80CpuOpcode)
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

    // Helper: load instruction sequence into RAM at 0x2000, prefetch 0x2000
    void loadCode(const std::vector<uint8_t>& code, uint16_t startAddr = 0x2000)
    {
        for (size_t i = 0; i < code.size(); ++i) {
            bus.writeByte(static_cast<uint16_t>(startAddr + i), code[i]);
        }
        cpu->prefetch(startAddr);
    }
};

TEST(Abc80CpuOpcode, Alu8BitNominalAndFlags)
{
    // 1. ADD A, B: 0x0F + 0x01 = 0x10 (Half-carry, no carry, not zero)
    // 80: ADD A, B
    loadCode({0x80});
    cpu->setA(0x0F);
    cpu->setB(0x01);
    cpu->stepInstruction();
    BYTES_EQUAL(0x10, cpu->getA());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_FALSE(cpu->getFlagC());
    CHECK_FALSE(cpu->getFlagZ());
    CHECK_FALSE(cpu->getFlagN());

    // 2. ADD A, B: 0x7F + 0x01 = 0x80 (Overflow V=1, Sign S=1, Half-carry H=1)
    loadCode({0x80});
    cpu->setA(0x7F);
    cpu->setB(0x01);
    cpu->stepInstruction();
    BYTES_EQUAL(0x80, cpu->getA());
    CHECK_TRUE(cpu->getFlagPV()); // Overflow
    CHECK_TRUE(cpu->getFlagS());  // Sign negative
    CHECK_TRUE(cpu->getFlagH());

    // 3. ADD A, B: 0xFF + 0x01 = 0x00 (Carry C=1, Zero Z=1, Half-carry H=1)
    loadCode({0x80});
    cpu->setA(0xFF);
    cpu->setB(0x01);
    cpu->stepInstruction();
    BYTES_EQUAL(0x00, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());
    CHECK_TRUE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagH());

    // 4. ADC A, B with Carry in: 0x10 + 0x20 + 1 = 0x31
    // 88: ADC A, B
    loadCode({0x88});
    cpu->setA(0x10);
    cpu->setB(0x20);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    BYTES_EQUAL(0x31, cpu->getA());
    CHECK_FALSE(cpu->getFlagC());

    // 5. SUB B: 0x10 - 0x01 = 0x0F (H=1, N=1, C=0)
    // 90: SUB B
    loadCode({0x90});
    cpu->setA(0x10);
    cpu->setB(0x01);
    cpu->stepInstruction();
    BYTES_EQUAL(0x0F, cpu->getA());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_TRUE(cpu->getFlagN());
    CHECK_FALSE(cpu->getFlagC());

    // 6. SUB B: 0x00 - 0x01 = 0xFF (C=1, S=1, H=1, N=1)
    loadCode({0x90});
    cpu->setA(0x00);
    cpu->setB(0x01);
    cpu->stepInstruction();
    BYTES_EQUAL(0xFF, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());
    CHECK_TRUE(cpu->getFlagS());
    CHECK_TRUE(cpu->getFlagN());

    // 7. SBC A, B with Carry in: 0x20 - 0x10 - 1 = 0x0F
    // 98: SBC A, B
    loadCode({0x98});
    cpu->setA(0x20);
    cpu->setB(0x10);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    BYTES_EQUAL(0x0F, cpu->getA());
    CHECK_FALSE(cpu->getFlagC());

    // 8. AND B: 0xF0 & 0x0F = 0x00 (Z=1, H=1, N=0, C=0, P/V=1 for even parity)
    // A0: AND B
    loadCode({0xA0});
    cpu->setA(0xF0);
    cpu->setB(0x0F);
    cpu->stepInstruction();
    BYTES_EQUAL(0x00, cpu->getA());
    CHECK_TRUE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_TRUE(cpu->getFlagPV());
    CHECK_FALSE(cpu->getFlagN());
    CHECK_FALSE(cpu->getFlagC());

    // 9. OR B: 0xF0 | 0x0F = 0xFF (S=1, Z=0, H=0, N=0, C=0, P/V=1 for 8 bits set)
    // B0: OR B
    loadCode({0xB0});
    cpu->setA(0xF0);
    cpu->setB(0x0F);
    cpu->stepInstruction();
    BYTES_EQUAL(0xFF, cpu->getA());
    CHECK_TRUE(cpu->getFlagS());
    CHECK_TRUE(cpu->getFlagPV());
    CHECK_FALSE(cpu->getFlagZ());

    // 10. XOR B: 0xAA ^ 0xAA = 0x00 (Z=1, H=0, N=0, C=0, P/V=1)
    // A8: XOR B
    loadCode({0xA8});
    cpu->setA(0xAA);
    cpu->setB(0xAA);
    cpu->stepInstruction();
    BYTES_EQUAL(0x00, cpu->getA());
    CHECK_TRUE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagPV());

    // 11. CP B: Compare 0x40 with 0x40 (A unchanged, Z=1, N=1, C=0)
    // B8: CP B
    loadCode({0xB8});
    cpu->setA(0x40);
    cpu->setB(0x40);
    cpu->stepInstruction();
    BYTES_EQUAL(0x40, cpu->getA());
    CHECK_TRUE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagN());
    CHECK_FALSE(cpu->getFlagC());

    // 12. INC B: 0x0F -> 0x10 (H=1, Z=0, preserves Carry)
    // 04: INC B
    loadCode({0x04});
    cpu->setB(0x0F);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    BYTES_EQUAL(0x10, cpu->getB());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_TRUE(cpu->getFlagC()); // Preserved

    // 13. DEC B: 0x00 -> 0xFF (H=1, S=1, N=1, preserves Carry)
    // 05: DEC B
    loadCode({0x05});
    cpu->setB(0x00);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    BYTES_EQUAL(0xFF, cpu->getB());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_TRUE(cpu->getFlagS());
    CHECK_TRUE(cpu->getFlagN());
    CHECK_TRUE(cpu->getFlagC()); // Preserved
}

TEST(Abc80CpuOpcode, Alu16BitOperations)
{
    // 1. ADD HL, BC: 0x1000 + 0x2000 = 0x3000
    // 09: ADD HL, BC
    loadCode({0x09});
    cpu->setHL(0x1000);
    cpu->setBC(0x2000);
    cpu->stepInstruction();
    LONGS_EQUAL(0x3000, cpu->getHL());
    CHECK_FALSE(cpu->getFlagC());
    CHECK_FALSE(cpu->getFlagN());

    // 2. ADD HL, BC with Carry: 0xFFFF + 0x0001 = 0x0000, C=1
    loadCode({0x09});
    cpu->setHL(0xFFFF);
    cpu->setBC(0x0001);
    cpu->stepInstruction();
    LONGS_EQUAL(0x0000, cpu->getHL());
    CHECK_TRUE(cpu->getFlagC());

    // 3. ADC HL, DE: 0x1000 + 0x2000 + 1 = 0x3001
    // ED 5A: ADC HL, DE
    loadCode({0xED, 0x5A});
    cpu->setHL(0x1000);
    cpu->setDE(0x2000);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    LONGS_EQUAL(0x3001, cpu->getHL());
    CHECK_FALSE(cpu->getFlagC());

    // 4. SBC HL, DE: 0x3000 - 0x1000 - 1 = 0x1FFF
    // ED 52: SBC HL, DE
    loadCode({0xED, 0x52});
    cpu->setHL(0x3000);
    cpu->setDE(0x1000);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    LONGS_EQUAL(0x1FFF, cpu->getHL());
    CHECK_TRUE(cpu->getFlagN());
    CHECK_FALSE(cpu->getFlagC());

    // 5. ADD IX, DE: 0x1234 + 0x0111 = 0x1345
    // DD 19: ADD IX, DE
    loadCode({0xDD, 0x19});
    cpu->setIX(0x1234);
    cpu->setDE(0x0111);
    cpu->stepInstruction();
    LONGS_EQUAL(0x1345, cpu->getIX());

    // 6. ADD IY, BC: 0x2000 + 0x0500 = 0x2500
    // FD 09: ADD IY, BC
    loadCode({0xFD, 0x09});
    cpu->setIY(0x2000);
    cpu->setBC(0x0500);
    cpu->stepInstruction();
    LONGS_EQUAL(0x2500, cpu->getIY());
}

TEST(Abc80CpuOpcode, DaaDecimalAdjustExhaustiveTruthTable)
{
    // Validate BCD addition adjust: 0x15 + 0x27 = 0x3C -> DAA -> 0x42
    // 80: ADD A, B
    // 27: DAA
    loadCode({0x80, 0x27});
    cpu->setA(0x15);
    cpu->setB(0x27);
    cpu->stepInstruction(); // ADD A, B
    BYTES_EQUAL(0x3C, cpu->getA());
    cpu->stepInstruction(); // DAA
    BYTES_EQUAL(0x42, cpu->getA());
    CHECK_FALSE(cpu->getFlagC());

    // Validate BCD carry out: 0x85 + 0x25 = 0xAA -> DAA -> 0x10 (C=1)
    loadCode({0x80, 0x27});
    cpu->setA(0x85);
    cpu->setB(0x25);
    cpu->stepInstruction();
    cpu->stepInstruction();
    BYTES_EQUAL(0x10, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());

    // Validate BCD subtraction adjust: 0x42 - 0x15 = 0x2D -> DAA -> 0x27
    // 90: SUB B
    // 27: DAA
    loadCode({0x90, 0x27});
    cpu->setA(0x42);
    cpu->setB(0x15);
    cpu->stepInstruction(); // SUB B
    cpu->stepInstruction(); // DAA
    BYTES_EQUAL(0x27, cpu->getA());
    CHECK_FALSE(cpu->getFlagC());
}

TEST(Abc80CpuOpcode, ShiftsAndRotates)
{
    // 1. RLCA: Rotate Left A (0x85 -> 0x0B, Carry = 1)
    // 07: RLCA
    loadCode({0x07});
    cpu->setA(0x85);
    cpu->stepInstruction();
    BYTES_EQUAL(0x0B, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());

    // 2. RRCA: Rotate Right A (0x85 -> 0xC2, Carry = 1)
    // 0F: RRCA
    loadCode({0x0F});
    cpu->setA(0x85);
    cpu->stepInstruction();
    BYTES_EQUAL(0xC2, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());

    // 3. RLA: Rotate Left through Carry (0x80, Carry=0 -> 0x00, Carry=1, Zero unaffected)
    // 17: RLA
    loadCode({0x17});
    cpu->setA(0x80);
    cpu->setFlagC(false);
    cpu->stepInstruction();
    BYTES_EQUAL(0x00, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());

    // 4. RRA: Rotate Right through Carry (0x01, Carry=1 -> 0x80, Carry=1)
    // 1F: RRA
    loadCode({0x1F});
    cpu->setA(0x01);
    cpu->setFlagC(true);
    cpu->stepInstruction();
    BYTES_EQUAL(0x80, cpu->getA());
    CHECK_TRUE(cpu->getFlagC());

    // 5. SLA B: Shift Left Arithmetic (0x41 -> 0x82, C=0)
    // CB 20: SLA B
    loadCode({0xCB, 0x20});
    cpu->setB(0x41);
    cpu->stepInstruction();
    BYTES_EQUAL(0x82, cpu->getB());
    CHECK_FALSE(cpu->getFlagC());
    CHECK_TRUE(cpu->getFlagS());

    // 6. SRA B: Shift Right Arithmetic preserves sign (0x84 -> 0xC2, C=0)
    // CB 28: SRA B
    loadCode({0xCB, 0x28});
    cpu->setB(0x84);
    cpu->stepInstruction();
    BYTES_EQUAL(0xC2, cpu->getB());
    CHECK_FALSE(cpu->getFlagC());
    CHECK_TRUE(cpu->getFlagS());

    // 7. SRL B: Shift Right Logical (0x84 -> 0x42, C=0)
    // CB 38: SRL B
    loadCode({0xCB, 0x38});
    cpu->setB(0x84);
    cpu->stepInstruction();
    BYTES_EQUAL(0x42, cpu->getB());
    CHECK_FALSE(cpu->getFlagC());
    CHECK_FALSE(cpu->getFlagS());

    // 8. RLD: Rotate Left Decimal between A and (HL)
    // ED 6F: RLD
    // A = 0x7A, (HL) = 0x34 -> A = 0x73, (HL) = 0x4A
    bus.writeByte(0x3000, 0x34);
    loadCode({0xED, 0x6F});
    cpu->setHL(0x3000);
    cpu->setA(0x7A);
    cpu->stepInstruction();
    BYTES_EQUAL(0x73, cpu->getA());
    BYTES_EQUAL(0x4A, bus.readByte(0x3000));

    // 9. RRD: Rotate Right Decimal between A and (HL)
    // ED 67: RRD
    // A = 0x7A, (HL) = 0x34 -> A = 0x74, (HL) = 0xA3
    bus.writeByte(0x3000, 0x34);
    loadCode({0xED, 0x67});
    cpu->setHL(0x3000);
    cpu->setA(0x7A);
    cpu->stepInstruction();
    BYTES_EQUAL(0x74, cpu->getA());
    BYTES_EQUAL(0xA3, bus.readByte(0x3000));
}

TEST(Abc80CpuOpcode, BitManipulationSetReset)
{
    // 1. BIT 3, B: where bit 3 is 0 -> Z=1, H=1, N=0
    // CB 58: BIT 3, B
    loadCode({0xCB, 0x58});
    cpu->setB(0xF7); // bit 3 is 0
    cpu->stepInstruction();
    CHECK_TRUE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_FALSE(cpu->getFlagN());

    // 2. BIT 3, B: where bit 3 is 1 -> Z=0, H=1, N=0
    loadCode({0xCB, 0x58});
    cpu->setB(0x08); // bit 3 is 1
    cpu->stepInstruction();
    CHECK_FALSE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagH());
    CHECK_FALSE(cpu->getFlagN());

    // 3. SET 5, C: sets bit 5 of C
    // CB E9: SET 5, C
    loadCode({0xCB, 0xE9});
    cpu->setC(0x00);
    cpu->stepInstruction();
    BYTES_EQUAL(0x20, cpu->getC());

    // 4. RES 5, C: clears bit 5 of C
    // CB A9: RES 5, C
    loadCode({0xCB, 0xA9});
    cpu->setC(0xFF);
    cpu->stepInstruction();
    BYTES_EQUAL(0xDF, cpu->getC());

    // 5. BIT 7, (HL)
    // CB 7E: BIT 7, (HL)
    bus.writeByte(0x2500, 0x80);
    loadCode({0xCB, 0x7E});
    cpu->setHL(0x2500);
    cpu->stepInstruction();
    CHECK_FALSE(cpu->getFlagZ());
    CHECK_TRUE(cpu->getFlagH());
}

TEST(Abc80CpuOpcode, BlockTransferAndSearch)
{
    // 1. LDIR: Copy 4 bytes from 0x2100 to 0x2200
    // ED B0: LDIR
    bus.writeByte(0x2100, 0xAA);
    bus.writeByte(0x2101, 0xBB);
    bus.writeByte(0x2102, 0xCC);
    bus.writeByte(0x2103, 0xDD);
    loadCode({0xED, 0xB0});
    cpu->setHL(0x2100);
    cpu->setDE(0x2200);
    cpu->setBC(4);

    // Run until LDIR finishes (BC reaches 0 and PC advances past ED B0)
    while (cpu->getPC() == 0x2000) {
        cpu->stepInstruction();
    }

    LONGS_EQUAL(0, cpu->getBC());
    LONGS_EQUAL(0x2104, cpu->getHL());
    LONGS_EQUAL(0x2204, cpu->getDE());
    BYTES_EQUAL(0xAA, bus.readByte(0x2200));
    BYTES_EQUAL(0xBB, bus.readByte(0x2201));
    BYTES_EQUAL(0xCC, bus.readByte(0x2202));
    BYTES_EQUAL(0xDD, bus.readByte(0x2203));
    CHECK_FALSE(cpu->getFlagPV()); // P/V = 0 on completion

    // 2. CPIR: Search for byte 0xCC in 0x2100..0x2103
    // ED B1: CPIR
    loadCode({0xED, 0xB1});
    cpu->setHL(0x2100);
    cpu->setBC(4);
    cpu->setA(0xCC);

    while (cpu->getPC() == 0x2000) {
        cpu->stepInstruction();
    }

    CHECK_TRUE(cpu->getFlagZ()); // Found match!
    LONGS_EQUAL(0x2103, cpu->getHL()); // HL points to byte after match (0x2102 + 1)
    LONGS_EQUAL(1, cpu->getBC());      // 1 byte remaining in search block
}

TEST(Abc80CpuOpcode, StackOperationsAndExchanges)
{
    // 1. PUSH BC / POP DE
    // C5: PUSH BC
    // D1: POP DE
    loadCode({0xC5, 0xD1});
    cpu->setSP(0x3000);
    cpu->setBC(0x1234);
    cpu->stepInstruction(); // PUSH BC
    LONGS_EQUAL(0x2FFE, cpu->getSP());
    BYTES_EQUAL(0x34, bus.readByte(0x2FFE));
    BYTES_EQUAL(0x12, bus.readByte(0x2FFF));

    cpu->stepInstruction(); // POP DE
    LONGS_EQUAL(0x3000, cpu->getSP());
    LONGS_EQUAL(0x1234, cpu->getDE());

    // 2. EX DE, HL
    // EB: EX DE, HL
    loadCode({0xEB});
    cpu->setDE(0xAAAA);
    cpu->setHL(0x5555);
    cpu->stepInstruction();
    LONGS_EQUAL(0x5555, cpu->getDE());
    LONGS_EQUAL(0xAAAA, cpu->getHL());

    // 3. EX AF, AF'
    // 08: EX AF, AF'
    loadCode({0x08});
    cpu->setAF(0x1122);
    cpu->setAF2(0x3344);
    cpu->stepInstruction();
    LONGS_EQUAL(0x3344, cpu->getAF());
    LONGS_EQUAL(0x1122, cpu->getAF2());

    // 4. EXX
    // D9: EXX
    loadCode({0xD9});
    cpu->setBC(0x1111);
    cpu->setDE(0x2222);
    cpu->setHL(0x3333);
    cpu->setBC2(0x4444);
    cpu->setDE2(0x5555);
    cpu->setHL2(0x6666);
    cpu->stepInstruction();
    LONGS_EQUAL(0x4444, cpu->getBC());
    LONGS_EQUAL(0x5555, cpu->getDE());
    LONGS_EQUAL(0x6666, cpu->getHL());
    LONGS_EQUAL(0x1111, cpu->getBC2());

    // 5. EX (SP), HL
    // E3: EX (SP), HL
    bus.writeByte(0x2FFE, 0x78);
    bus.writeByte(0x2FFF, 0x56);
    loadCode({0xE3});
    cpu->setSP(0x2FFE);
    cpu->setHL(0x1234);
    cpu->stepInstruction();
    LONGS_EQUAL(0x5678, cpu->getHL());
    BYTES_EQUAL(0x34, bus.readByte(0x2FFE));
    BYTES_EQUAL(0x12, bus.readByte(0x2FFF));
}

TEST(Abc80CpuOpcode, FlowControlCallsReturnsRestarts)
{
    // 1. JP nn: Unconditional jump
    // C3 00 25: JP 2500h
    loadCode({0xC3, 0x00, 0x25});
    cpu->stepInstruction();
    LONGS_EQUAL(0x2500, cpu->getPC());

    // 2. JP Z, nn: Conditional jump taken when Z=1
    // CA 50 25: JP Z, 2550h
    loadCode({0xCA, 0x50, 0x25});
    cpu->setFlagZ(true);
    cpu->stepInstruction();
    LONGS_EQUAL(0x2550, cpu->getPC());

    // 3. JP Z, nn: Conditional jump not taken when Z=0
    loadCode({0xCA, 0x50, 0x25});
    cpu->setFlagZ(false);
    cpu->stepInstruction();
    LONGS_EQUAL(0x2003, cpu->getPC());

    // 4. JR e: Relative jump forward +10 bytes (offset 0x0A)
    // 18 0A: JR +10 (PC after fetch is 2002, target = 2002 + 10 = 200C)
    loadCode({0x18, 0x0A});
    cpu->stepInstruction();
    LONGS_EQUAL(0x200C, cpu->getPC());

    // 5. CALL nn / RET
    // CD 50 20: CALL 2050h
    // at 2050h: C9 (RET)
    bus.writeByte(0x2050, 0xC9);
    loadCode({0xCD, 0x50, 0x20});
    cpu->setSP(0x3000);
    cpu->stepInstruction(); // Executes CALL
    LONGS_EQUAL(0x2050, cpu->getPC());
    LONGS_EQUAL(0x2FFE, cpu->getSP());
    BYTES_EQUAL(0x03, bus.readByte(0x2FFE)); // Return addr low (0x2003)
    BYTES_EQUAL(0x20, bus.readByte(0x2FFF)); // Return addr high

    cpu->stepInstruction(); // Executes RET
    LONGS_EQUAL(0x2003, cpu->getPC());
    LONGS_EQUAL(0x3000, cpu->getSP());

    // 6. RST 38H: Calls 0x0038
    // FF: RST 38H
    loadCode({0xFF});
    cpu->setSP(0x3000);
    cpu->stepInstruction();
    LONGS_EQUAL(0x0038, cpu->getPC());
    LONGS_EQUAL(0x2FFE, cpu->getSP());
}

TEST(Abc80CpuOpcode, InterruptFlipFlopsAndEiDelayWindow)
{
    // 1. DI: Disables interrupts
    // F3: DI
    loadCode({0xF3});
    cpu->setIFF1(true);
    cpu->setIFF2(true);
    cpu->stepInstruction();
    CHECK_FALSE(cpu->getIFF1());
    CHECK_FALSE(cpu->getIFF2());

    // 2. EI: Enables interrupts
    // FB: EI
    loadCode({0xFB});
    cpu->stepInstruction();
    CHECK_TRUE(cpu->getIFF1());
    CHECK_TRUE(cpu->getIFF2());

    // 3. LD A, I: Copies IFF2 into P/V flag
    // ED 57: LD A, I
    loadCode({0xED, 0x57});
    cpu->setI(0x42);
    cpu->setIFF2(true);
    cpu->stepInstruction();
    BYTES_EQUAL(0x42, cpu->getA());
    CHECK_TRUE(cpu->getFlagPV()); // P/V reflects IFF2 == 1

    loadCode({0xED, 0x57});
    cpu->setIFF2(false);
    cpu->stepInstruction();
    CHECK_FALSE(cpu->getFlagPV()); // P/V reflects IFF2 == 0
}

TEST(Abc80CpuOpcode, InterruptModes012)
{
    // 1. IM 0 (ED 46)
    loadCode({0xED, 0x46});
    cpu->stepInstruction();
    LONGS_EQUAL(0, cpu->getIM());

    // 2. IM 1 (ED 56)
    loadCode({0xED, 0x56});
    cpu->stepInstruction();
    LONGS_EQUAL(1, cpu->getIM());

    // 3. IM 2 (ED 5E)
    loadCode({0xED, 0x5E});
    cpu->stepInstruction();
    LONGS_EQUAL(2, cpu->getIM());

    // In Mode 1: assert interrupt line, ensure CPU vectors to 0x0038
    cpu->setIM(1);
    loadCode({0x00, 0x00}); // NOPs
    cpu->setSP(0x3000);
    cpu->setIFF1(true);
    cpu->setInterruptLine(true);

    // Step instruction: interrupt acknowledge cycle vectors to 0x0038
    cpu->stepInstruction();
    LONGS_EQUAL(0x0038, cpu->getPC());
    CHECK_FALSE(cpu->getIFF1()); // Interrupts disabled upon entry
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
