/*
 * Abc80PpiTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <CppUTest/TestHarness.h>
#include <abc80/Abc80Ppi.hxx>

TEST_GROUP(Abc80Ppi)
{
    abc80::Abc80Ppi ppi;

    void setup() override
    {
        ppi.reset();
    }

    void teardown() override
    {
    }
};

TEST(Abc80Ppi, ControlWordConfiguresMode0)
{
    // Write 0x90 to primary PPI control register (Port 0x83)
    // Mode 0: Port A = Input (0x10), Port B = Output (0x00), Port C = Output (0x00)
    ppi.writePort(0x83, 0x90);
    BYTES_EQUAL(0x90, ppi.getPrimaryControl());

    // Write 0x82 to Parallel Link PPI control register (Port 0xC3)
    // Mode 0: Port A = Output (0x00), Port B = Input (0x02), Port C = Output (0x00)
    ppi.writePort(0xC3, 0x82);
    BYTES_EQUAL(0x82, ppi.getLinkControl());
}

TEST(Abc80Ppi, KeypadMatrixScanReturnsExactRowBits)
{
    // Configure primary PPI in Mode 0 (PA in, PB out, PC out)
    ppi.writePort(0x83, 0x90);

    // 1. Strobe Column 0 (digit strobe bit 0 low -> Port C = 0xFE)
    ppi.writePort(0x82, 0xFE);

    // No key pressed -> rows 0-3 should be all 1s (0x0F)
    BYTES_EQUAL(0x0F, ppi.readPort(0x80) & 0x0F);

    // Press 'ADRS' key (code 0x14, Col 0, Row 0): Row 0 goes low (0)
    ppi.pressKey(0x14);
    CHECK_TRUE(ppi.isKeyPressed(0x14));
    BYTES_EQUAL(0x0E, ppi.readPort(0x80) & 0x0F); // bit 0 is 0

    // Release 'ADRS', press 'DATA' (code 0x13, Col 0, Row 1): Row 1 goes low (0)
    ppi.releaseKey(0x14);
    ppi.pressKey(0x13);
    BYTES_EQUAL(0x0D, ppi.readPort(0x80) & 0x0F); // bit 1 is 0

    // Press '-' key (code 0x11, Col 0, Row 2): Row 2 goes low (0)
    ppi.releaseKey(0x13);
    ppi.pressKey(0x11);
    BYTES_EQUAL(0x0B, ppi.readPort(0x80) & 0x0F); // bit 2 is 0

    // Press '+' key (code 0x10, Col 0, Row 3): Row 3 goes low (0)
    ppi.releaseKey(0x11);
    ppi.pressKey(0x10);
    BYTES_EQUAL(0x07, ppi.readPort(0x80) & 0x0F); // bit 3 is 0

    // 2. Strobe Column 4 (Port C = 0xEF): Press '0' (code 0x00, Col 4, Row 3)
    ppi.clearKeys();
    ppi.writePort(0x82, 0xEF);
    ppi.pressKey(0x00);
    BYTES_EQUAL(0x07, ppi.readPort(0x80) & 0x0F); // Row 3 is 0 in Col 4

    // If we switch strobe to Column 3 (0xF7) while key '0' (in Col 4) is still pressed,
    // Column 3 should NOT see key '0'
    ppi.writePort(0x82, 0xF7);
    BYTES_EQUAL(0x0F, ppi.readPort(0x80) & 0x0F); // Key not in column 3
}

TEST(Abc80Ppi, SevenSegmentCathodeDecode)
{
    // Configure Mode 0
    ppi.writePort(0x83, 0x90);

    // Output segment bitmask 0x3F (digit '0') to Port B
    ppi.writePort(0x81, 0x3F);
    // Strobe digit 0 (Port C bit 0 low -> 0xFE)
    ppi.writePort(0x82, 0xFE);
    BYTES_EQUAL(0x3F, ppi.getDigitSegment(0));

    // Blank display between multiplexed digit strobes (authentic hardware multiplexing)
    ppi.writePort(0x82, 0xFF);

    // Output segment bitmask 0x06 (digit '1') to Port B
    ppi.writePort(0x81, 0x06);
    // Strobe digit 1 (Port C bit 1 low -> 0xFD)
    ppi.writePort(0x82, 0xFD);
    BYTES_EQUAL(0x06, ppi.getDigitSegment(1));

    // Assert that digit 0 retained its previously latched segments
    BYTES_EQUAL(0x3F, ppi.getDigitSegment(0));
}

TEST(Abc80Ppi, ParallelPortHandshakeBidirectional)
{
    // Configure Parallel Link PPI (0xC3 = 0x82: PA output, PB input, PC output)
    ppi.writePort(0xC3, 0x82);

    // 1. ABC-80 writes 0x55 to Port 0xC0
    ppi.writePort(0xC0, 0x55);
    // Virtual host reads Port A output
    BYTES_EQUAL(0x55, ppi.hostReadLinkPortA());

    // ABC-80 writes 0xAA to Port 0xC0
    ppi.writePort(0xC0, 0xAA);
    BYTES_EQUAL(0xAA, ppi.hostReadLinkPortA());

    // 2. Virtual host writes 0x55 to Port B
    ppi.hostWriteLinkPortB(0x55);
    // ABC-80 reads Port 0xC1
    BYTES_EQUAL(0x55, ppi.readPort(0xC1));

    // Virtual host writes 0xAA to Port B
    ppi.hostWriteLinkPortB(0xAA);
    BYTES_EQUAL(0xAA, ppi.readPort(0xC1));
}

TEST(Abc80Ppi, CassetteTapeAudioBit)
{
    ppi.writePort(0x83, 0x90);

    // Audio tape input high (1) -> bit 7 of Port A is 1
    ppi.setTapeInput(true);
    CHECK_TRUE(ppi.getTapeInput());
    BYTES_EQUAL(0x80, ppi.readPort(0x80) & 0x80);

    // Audio tape input low (0) -> bit 7 of Port A is 0
    ppi.setTapeInput(false);
    CHECK_FALSE(ppi.getTapeInput());
    BYTES_EQUAL(0x00, ppi.readPort(0x80) & 0x80);
}

TEST(Abc80Ppi, UnmappedPortReturnsFloatingBus)
{
    // Any read from an unmapped port (e.g. 0x99) must return 0xFF
    BYTES_EQUAL(0xFF, ppi.readPort(0x99));
    BYTES_EQUAL(0xFF, ppi.readPort(0x00));
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
