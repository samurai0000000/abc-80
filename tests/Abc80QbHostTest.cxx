/*
 * Abc80QbHostTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80QbHost.hxx>
#include <CppUTest/TestHarness.h>
#include <rapidcheck.h>
#include <filesystem>
#include <fstream>
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

TEST(Abc80QbHost, HandshakeInAbcReading0x55DoesNotDesyncNextCommandAndStrobesDisplay)
{
    // 0x55 is the firmware's own sync byte! At 0x0B7A (FE 55 / CC 90 0B),
    // the 1993 ROM clears Port C0h to 0x00 if the read byte was 0x55, and
    // then calls adrsdp + scd_k1 to strobe the 6-digit 7-segment display.
    board.getBus().writeByte(0x1500, 0x55);
    board.getBus().writeByte(0x1501, 0xA3);

    board.getCpu().setSP(0x17BF);
    board.getCpu().prefetch(0x0B00);

    uint8_t val0 = 0;
    CHECK_TRUE(qb->inAbc(0x1500, val0));
    BYTES_EQUAL(0x55, val0);

    // Port C0h must have been cleared by 0x0B90 so the next command doesn't
    // immediately see a stale 0x55 sync byte.
    BYTES_EQUAL(0x00, board.getPpi().hostReadLinkPortA());

    // And the 7-segment display buffer (0x17C6..0x17CB) must have been updated by dadp/adrsdp
    bool displayUpdated = false;
    for (uint16_t addr = 0x17C6; addr <= 0x17CB; ++addr) {
        if (board.getBus().readByte(addr) != 0x00) {
            displayUpdated = true;
            break;
        }
    }
    CHECK_TRUE(displayUpdated);

    // Next InABC must succeed without desynchronizing
    uint8_t val1 = 0;
    CHECK_TRUE(qb->inAbc(0x1501, val1));
    BYTES_EQUAL(0xA3, val1);
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

TEST(Abc80QbHost, InitialAbcUploadsZ80InsertAndDeleteHelpersToRam)
{
    bool ok = qb->initialAbc();
    CHECK_TRUE(ok);
    CHECK_TRUE(qb->isConnected());

    // Bytes at 0x1000 and 0x3000 should both be 0xFF
    BYTES_EQUAL(0xFF, board.getBus().readByte(0x1000));
    BYTES_EQUAL(0xFF, board.getBus().readByte(0x3000));

    // Verify InsertProgOut1$ at 0x1700 (LD BC, 16FEh ... JP 0B00h)
    BYTES_EQUAL(0x01, board.getBus().readByte(0x1700));
    BYTES_EQUAL(0xFE, board.getBus().readByte(0x1701));
    BYTES_EQUAL(0x16, board.getBus().readByte(0x1702));
    BYTES_EQUAL(0xC3, board.getBus().readByte(0x1717));
    BYTES_EQUAL(0x00, board.getBus().readByte(0x1718));
    BYTES_EQUAL(0x0B, board.getBus().readByte(0x1719));

    // Verify InsertProgOut2$ at 0x1720 (LD BC, 37FEh ... JP 0B00h)
    BYTES_EQUAL(0x01, board.getBus().readByte(0x1720));
    BYTES_EQUAL(0xFE, board.getBus().readByte(0x1721));
    BYTES_EQUAL(0x37, board.getBus().readByte(0x1722));

    // Verify DeleteProgOut1$ at 0x1740 (LD BC, (178Ah) ... JP 0B00h)
    BYTES_EQUAL(0xED, board.getBus().readByte(0x1740));
    BYTES_EQUAL(0x4B, board.getBus().readByte(0x1741));
    BYTES_EQUAL(0x8A, board.getBus().readByte(0x1742));
    BYTES_EQUAL(0x17, board.getBus().readByte(0x1743));

    // Verify DeleteProgOut2$ at 0x1760 (LD BC, (178Ah) ... LD HL, 37FFh)
    BYTES_EQUAL(0xED, board.getBus().readByte(0x1760));
    BYTES_EQUAL(0x21, board.getBus().readByte(0x1769));
    BYTES_EQUAL(0xFF, board.getBus().readByte(0x176A));
    BYTES_EQUAL(0x37, board.getBus().readByte(0x176B));
}

TEST(Abc80QbHost, InsertAndDeleteShiftRamBlocksViaZ80HelperRoutines)
{
    CHECK_TRUE(qb->initialAbc());

    // Populate 0x1000..0x1002 with 0x11, 0x22, 0x33
    CHECK_TRUE(qb->outAbc(0x1000, 0x11));
    CHECK_TRUE(qb->outAbc(0x1001, 0x22));
    CHECK_TRUE(qb->outAbc(0x1002, 0x33));

    // Insert at 0x1001 -> 0x1000 stays 0x11, 0x1001 becomes 0x00, 0x1002 becomes 0x22, 0x1003 becomes 0x33
    CHECK_TRUE(qb->insertByte(0x1001));
    LONGS_EQUAL(17, qb->getHighlightedKey());
    BYTES_EQUAL(0x11, board.getBus().readByte(0x1000));
    BYTES_EQUAL(0x00, board.getBus().readByte(0x1001));
    BYTES_EQUAL(0x22, board.getBus().readByte(0x1002));
    BYTES_EQUAL(0x33, board.getBus().readByte(0x1003));

    // Delete at 0x1001 -> shifts 0x1002..0x16FF down by 1 byte
    CHECK_TRUE(qb->deleteByte(0x1001));
    LONGS_EQUAL(20, qb->getHighlightedKey());
    BYTES_EQUAL(0x11, board.getBus().readByte(0x1000));
    BYTES_EQUAL(0x22, board.getBus().readByte(0x1001));
    BYTES_EQUAL(0x33, board.getBus().readByte(0x1002));

    // Also test upper bank 0x3000..0x37FF
    CHECK_TRUE(qb->outAbc(0x3000, 0xAA));
    CHECK_TRUE(qb->outAbc(0x3001, 0xBB));
    CHECK_TRUE(qb->insertByte(0x3000));
    BYTES_EQUAL(0x00, board.getBus().readByte(0x3000));
    BYTES_EQUAL(0xAA, board.getBus().readByte(0x3001));
    BYTES_EQUAL(0xBB, board.getBus().readByte(0x3002));

    CHECK_TRUE(qb->deleteByte(0x3000));
    BYTES_EQUAL(0xAA, board.getBus().readByte(0x3000));
    BYTES_EQUAL(0xBB, board.getBus().readByte(0x3001));

    // Out-of-range addresses (e.g. 0x0800 ROM or 0x1700 helper area or 0x2000) must fail with ERROR key 23
    CHECK_FALSE(qb->insertByte(0x0800));
    LONGS_EQUAL(23, qb->getHighlightedKey());
    CHECK_FALSE(qb->deleteByte(0x1700));
    LONGS_EQUAL(23, qb->getHighlightedKey());
}

TEST(Abc80QbHost, InteractivePressKeyAddressDataAndRomWriteError)
{
    // F1 connects and initializes screen at 0x1000
    CHECK_TRUE(qb->pressKey("F1"));
    CHECK_TRUE(qb->isConnected());
    LONGS_EQUAL(static_cast<int>(QbMode::Neutral), static_cast<int>(qb->getMode()));
    LONGS_EQUAL(0x1000, qb->getCurrentAddress());

    // Enter NewAddress mode via HOME ("ADDS")
    CHECK_TRUE(qb->pressKey("HOME"));
    LONGS_EQUAL(static_cast<int>(QbMode::AddressInput), static_cast<int>(qb->getMode()));
    LONGS_EQUAL(18, qb->getHighlightedKey());

    // Type 1, 2, 3, 4 -> address shifts to 0x1234
    CHECK_TRUE(qb->pressKey("1"));
    CHECK_TRUE(qb->pressKey("2"));
    CHECK_TRUE(qb->pressKey("3"));
    CHECK_TRUE(qb->pressKey("4"));
    LONGS_EQUAL(0x1234, qb->getCurrentAddress());

    // Enter DataInput mode via END ("DATA")
    CHECK_TRUE(qb->pressKey("END"));
    LONGS_EQUAL(static_cast<int>(QbMode::DataInput), static_cast<int>(qb->getMode()));
    LONGS_EQUAL(21, qb->getHighlightedKey());

    // Type A, 5 -> byte at 0x1234 becomes 0xA5 via OutABC
    CHECK_TRUE(qb->pressKey("A"));
    CHECK_TRUE(qb->pressKey("5"));
    BYTES_EQUAL(0xA5, board.getBus().readByte(0x1234));

    // PGUP / PGDN step address
    CHECK_TRUE(qb->pressKey("PGDN"));
    LONGS_EQUAL(0x1235, qb->getCurrentAddress());
    LONGS_EQUAL(22, qb->getHighlightedKey());
    CHECK_TRUE(qb->pressKey("PGUP"));
    LONGS_EQUAL(0x1234, qb->getCurrentAddress());
    LONGS_EQUAL(19, qb->getHighlightedKey());

    // Navigate to ROM address 0x0100 and attempt to write a nibble -> must flag ERROR (key 23)
    CHECK_TRUE(qb->pressKey("HOME"));
    CHECK_TRUE(qb->pressKey("0"));
    CHECK_TRUE(qb->pressKey("1"));
    CHECK_TRUE(qb->pressKey("0"));
    CHECK_TRUE(qb->pressKey("0"));
    LONGS_EQUAL(0x0100, qb->getCurrentAddress());
    CHECK_TRUE(qb->pressKey("END"));
    uint8_t romByte = board.getBus().readByte(0x0100);
    uint8_t diffNibble = ((romByte & 0x0F) == 0x0F) ? 0x00 : 0x0F;
    CHECK_TRUE(qb->pressKey(diffNibble == 0 ? "0" : "F"));
    LONGS_EQUAL(23, qb->getHighlightedKey());
    BYTES_EQUAL(romByte, board.getBus().readByte(0x0100));
}

TEST(Abc80QbHost, SaveAndLoadAbcProgramFilesAndPropertyTests)
{
    auto tmpDir = std::filesystem::temp_directory_path() / "abc80_qb_test_progs";
    std::filesystem::create_directories(tmpDir);
    qb->setProgramsDir(tmpDir.string());

    CHECK_TRUE(qb->initialAbc());
    CHECK_TRUE(qb->outAbc(0x1100, 0x3E));
    CHECK_TRUE(qb->outAbc(0x1101, 0x42));
    CHECK_TRUE(qb->outAbc(0x1102, 0x76));

    std::string err;
    CHECK_TRUE(qb->saveProgramAbc("test_prog", 0x1100, 0x1102, err));
    CHECK_TRUE(err.empty());

    // Overwrite memory so we can verify loadProgramAbc restores it via OutABC
    CHECK_TRUE(qb->outAbc(0x1100, 0x00));
    CHECK_TRUE(qb->outAbc(0x1101, 0x00));
    CHECK_TRUE(qb->outAbc(0x1102, 0x00));

    uint16_t loadedStart = 0;
    size_t loadedBytes = 0;
    CHECK_TRUE(qb->loadProgramAbc("test_prog.abc", loadedStart, loadedBytes, err));
    LONGS_EQUAL(0x1100, loadedStart);
    LONGS_EQUAL(3, loadedBytes);
    BYTES_EQUAL(0x3E, board.getBus().readByte(0x1100));
    BYTES_EQUAL(0x42, board.getBus().readByte(0x1101));
    BYTES_EQUAL(0x76, board.getBus().readByte(0x1102));

    // Reject path traversal
    std::string clean;
    CHECK_FALSE(Abc80QbHost::sanitizeAbcFilename("../evil.abc", clean));
    CHECK_FALSE(Abc80QbHost::sanitizeAbcFilename("/etc/passwd", clean));
    CHECK_FALSE(Abc80QbHost::sanitizeAbcFilename("", clean));

    // RapidCheck property: arbitrary address + byte array round-trips through formatAbcFile / parseAbcFile
    bool propOk = rc::check("AbcFile codec round-trips arbitrary binary payloads", []() {
        uint16_t startAddr = *rc::gen::inRange<uint16_t>(0x1000, 0x3700);
        size_t len = *rc::gen::inRange<size_t>(1, 128);
        auto payload = *rc::gen::container<std::vector<uint8_t>>(len, rc::gen::arbitrary<uint8_t>());
        std::string encoded = Abc80QbHost::formatAbcFile(startAddr, payload);
        uint16_t parsedStart = 0;
        std::vector<uint8_t> parsedData;
        RC_ASSERT(Abc80QbHost::parseAbcFile(encoded, parsedStart, parsedData));
        RC_ASSERT(parsedStart == startAddr);
        RC_ASSERT(parsedData == payload);
    });
    CHECK_TRUE(propOk);

    // RapidCheck property: malformed / truncated / empty strings never crash parseAbcFile
    bool fuzzOk = rc::check("parseAbcFile handles arbitrary malformed strings safely", []() {
        auto garbage = *rc::gen::arbitrary<std::string>();
        uint16_t parsedStart = 0;
        std::vector<uint8_t> parsedData;
        (void)Abc80QbHost::parseAbcFile(garbage, parsedStart, parsedData);
    });
    CHECK_TRUE(fuzzOk);

    std::filesystem::remove_all(tmpDir);
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
