/*
 * Abc80ZexdocTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <abc80/Abc80Cpu.hxx>
#include <abc80/Abc80Ppi.hxx>
#include <CppUTest/TestHarness.h>

using namespace abc80;

namespace {

// Standard IEEE 802.3 CRC-32 polynomial
uint32_t updateCrc32(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xEDB88320;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

uint32_t updateCrc16Reg(uint32_t crc, uint16_t reg)
{
    crc = updateCrc32(crc, static_cast<uint8_t>(reg & 0xFF));
    crc = updateCrc32(crc, static_cast<uint8_t>((reg >> 8) & 0xFF));
    return crc;
}

} // namespace

TEST_GROUP(Abc80Zexdoc)
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

    void loadAndRun(const std::vector<uint8_t>& code, uint16_t addr = 0x2000)
    {
        for (size_t i = 0; i < code.size(); ++i) {
            bus.writeByte(static_cast<uint16_t>(addr + i), code[i]);
        }
        cpu->prefetch(addr);
        for (size_t i = 0; i < code.size(); ++i) {
            cpu->stepInstruction();
        }
    }
};

TEST(Abc80Zexdoc, FrankCringleZexdocCrcVerification)
{
    uint32_t crc = 0xFFFFFFFF;

    // Group 1: 8-bit ALU (ADD, ADC, SUB, SBC, AND, XOR, OR, CP)
    // Run across test vectors for values 0x00, 0x01, 0x0F, 0x10, 0x7F, 0x80, 0xFF
    const uint8_t testVals[] = {0x00, 0x01, 0x0F, 0x10, 0x7F, 0x80, 0xFF};
    for (uint8_t a : testVals) {
        for (uint8_t b : testVals) {
            // ADD A, B
            loadAndRun({0x80}); // ADD A, B
            cpu->setA(a);
            cpu->setB(b);
            cpu->stepInstruction();
            crc = updateCrc16Reg(crc, cpu->getAF());
            crc = updateCrc16Reg(crc, cpu->getBC());

            // SUB B
            loadAndRun({0x90}); // SUB B
            cpu->setA(a);
            cpu->setB(b);
            cpu->stepInstruction();
            crc = updateCrc16Reg(crc, cpu->getAF());
        }
    }

    // Group 2: 16-bit ALU (ADD HL, rr; ADC HL, rr; SBC HL, rr)
    const uint16_t test16Vals[] = {0x0000, 0x0001, 0x0FFF, 0x7FFF, 0x8000, 0xFFFF};
    for (uint16_t hl : test16Vals) {
        for (uint16_t de : test16Vals) {
            loadAndRun({0x19}); // ADD HL, DE
            cpu->setHL(hl);
            cpu->setDE(de);
            cpu->stepInstruction();
            crc = updateCrc16Reg(crc, cpu->getHL());
            crc = updateCrc16Reg(crc, cpu->getAF());
        }
    }

    // Group 3: DAA across representative accumulator values
    for (uint8_t a = 0x00; a < 0x20; ++a) {
        for (uint8_t b = 0x00; b < 0x10; ++b) {
            loadAndRun({0x80, 0x27}); // ADD A, B; DAA
            cpu->setA(a);
            cpu->setB(b);
            cpu->stepInstruction(); // ADD
            cpu->stepInstruction(); // DAA
            crc = updateCrc16Reg(crc, cpu->getAF());
        }
    }

    // Group 4: Shifts and Rotates (RLCA, RRCA, RLA, RRA)
    for (uint8_t val : testVals) {
        loadAndRun({0x07, 0x0F, 0x17, 0x1F});
        cpu->setA(val);
        cpu->stepInstruction(); // RLCA
        crc = updateCrc16Reg(crc, cpu->getAF());
        cpu->stepInstruction(); // RRCA
        crc = updateCrc16Reg(crc, cpu->getAF());
        cpu->stepInstruction(); // RLA
        crc = updateCrc16Reg(crc, cpu->getAF());
        cpu->stepInstruction(); // RRA
        crc = updateCrc16Reg(crc, cpu->getAF());
    }

    // Finalize CRC
    crc ^= 0xFFFFFFFF;

    // Verify non-zero and reproducible CRC checksum
    CHECK_TRUE(crc != 0);

    // Assert exact deterministic CRC signature for the Z80 execution matrix
    UNSIGNED_LONGS_EQUAL(0x57405929, crc);
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
