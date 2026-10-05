/*
 * Abc80QbHostTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80QbHost.hxx>
#include <CppUTest/TestHarness.h>
#include <vector>

using namespace abc80;

TEST_GROUP(Abc80QbHost)
{
    Abc80Board board;
    Abc80QbHost* qb;

    void setup() override
    {
        board.powerOn("vintage/rom.abc");
        qb = new Abc80QbHost(board);
    }

    void teardown() override
    {
        delete qb;
    }
};

TEST(Abc80QbHost, ScreenMatrixMatchesVintageQuickBasicCoordinates)
{
    qb->initVintageScreen(0x1000);
    const auto& screen = qb->getScreen();

    // Line 8: Address and Byte headers
    std::string line8 = screen.getRowText(8);
    CHECK_TRUE(line8.find("Address") != std::string::npos);
    CHECK_TRUE(line8.find("Byte") != std::string::npos);

    // Line 13: Yellow highlighted center address (0x1000)
    std::string line13 = screen.getRowText(13);
    CHECK_TRUE(line13.find("1 0 0 0") != std::string::npos);
    ScreenCell cell13 = screen.getCell(13, 18);
    LONGS_EQUAL(14, cell13.attr & 0x0F); // Color 14 = Yellow

    // Right-side Function Key Box (Lines 12, 14, 17)
    std::string line12 = screen.getRowText(12);
    CHECK_TRUE(line12.find("INS") != std::string::npos);
    CHECK_TRUE(line12.find("ADDs") != std::string::npos);

    std::string line14 = screen.getRowText(14);
    CHECK_TRUE(line14.find("DEL") != std::string::npos);
    CHECK_TRUE(line14.find("Data") != std::string::npos);

    std::string line17 = screen.getRowText(17);
    CHECK_TRUE(line17.find("RUN") != std::string::npos);

    // Line 22: Hex keypad buttons
    std::string line22 = screen.getRowText(22);
    CHECK_TRUE(line22.find("0 | 1 | 2 | 3") != std::string::npos);
    CHECK_TRUE(line22.find("C | D | E | F") != std::string::npos);

    // Line 24: Function Key Shortcuts
    std::string line24 = screen.getRowText(24);
    CHECK_TRUE(line24.find("Shift+F5 = Run") != std::string::npos);

    // Line 25: Footer Attribution
    std::string line25 = screen.getRowText(25);
    CHECK_TRUE(line25.find("Charles Chiou") != std::string::npos);
}

TEST(Abc80QbHost, AudioSynthesisGeneratesToneFrequenciesAndMml)
{
    std::vector<int16_t> pcmSound;
    Abc80QbHost::generateSound(2000.0, 0.05, pcmSound);
    CHECK_TRUE(pcmSound.size() > 0);

    // Connection chime from abc.bas: "l32mlabcdfg"
    std::vector<int16_t> pcmChime;
    Abc80QbHost::generatePlayMml("l32mlabcdfg", pcmChime);
    CHECK_TRUE(pcmChime.size() > 1000);

    // Run fanfare from abc.bas: "l16mlfgfgfaba>d<"
    std::vector<int16_t> pcmFanfare;
    Abc80QbHost::generatePlayMml("l16mlfgfgfaba>d<", pcmFanfare);
    CHECK_TRUE(pcmFanfare.size() > 1000);
}

TEST(Abc80QbHost, HandshakeOutAbcSyncWithFirmware0B00)
{
    // Start PC-Link server at 0x0B00
    board.getCpu().setSP(0x17BF);
    board.getCpu().prefetch(0x0B00);

    // Execute OutABC: write byte 0x42 to address 0x1234
    bool ok = qb->outAbc(0x1234, 0x42);
    CHECK_TRUE(ok);

    // Assert byte 0x42 written to RAM address 0x1234
    BYTES_EQUAL(0x42, board.getBus().readByte(0x1234));

    // Assert 7-segment display buffer in RAM is updated with formatted digits
    bool displayActive = false;
    for (uint16_t addr = 0x17C6; addr <= 0x17CB; ++addr) {
        if (board.getBus().readByte(addr) != 0x00) {
            displayActive = true;
            break;
        }
    }
    CHECK_TRUE(displayActive);
}

TEST(Abc80QbHost, HandshakeInAbcReadsMemoryFromFirmware)
{
    // Write test byte 0x5A to RAM address 0x1500 directly
    board.getBus().writeByte(0x1500, 0x5A);

    // Start PC-Link server at 0x0B00
    board.getCpu().setSP(0x17BF);
    board.getCpu().prefetch(0x0B00);

    // Execute InABC: read byte from address 0x1500
    uint8_t readVal = 0;
    bool ok = qb->inAbc(0x1500, readVal);
    CHECK_TRUE(ok);
    BYTES_EQUAL(0x5A, readVal);
}

TEST(Abc80QbHost, HandshakeRunAbcJumpsToUserAddress)
{
    // Place a NOP followed by HALT at user target address 0x1800
    board.getBus().writeByte(0x1800, 0x00); // NOP
    board.getBus().writeByte(0x1801, 0x76); // HALT

    // Start PC-Link server at 0x0B00
    board.getCpu().setSP(0x17BF);
    board.getCpu().prefetch(0x0B00);

    // Execute RunABC to address 0x1800
    bool ok = qb->runAbc(0x1800);
    CHECK_TRUE(ok);

    // Assert CPU branched to address 0x1800
    LONGS_EQUAL(0x1800, board.getCpu().getPC());
}

TEST(Abc80QbHost, InitialAbcExecutesRamVerificationCheck)
{
    // Execute InitialABC: verifies R/W at 0x1000 and 0x3000
    bool ok = qb->initialAbc();
    CHECK_TRUE(ok);

    // Bytes at 0x1000 and 0x3000 should both be 0xFF
    BYTES_EQUAL(0xFF, board.getBus().readByte(0x1000));
    BYTES_EQUAL(0xFF, board.getBus().readByte(0x3000));
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
