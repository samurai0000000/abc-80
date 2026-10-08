/*
 * Abc80DisplayTest.cxx
 *
 * Display behavior of the real ROMs on the real CPU and PPI: Port B polarity,
 * digit order, persistence over a frame, and the lit threshold.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <array>
#include <string>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

namespace {

// Independent of the production table: Port B hardware bit -> segment letter (rom.asm segtab comment),
// then segment letter -> canonical bit by its position in "abcdefgp".
uint8_t canonicalFromHardware(uint8_t hw)
{
    const char hwLetter[8] = { 'e', 'g', 'f', 'a', 'b', 'c', 'p', 'd' };  // hw bit 0..7
    const std::string order = "abcdefgp";
    uint8_t mask = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if (hw & (1 << bit)) {
            mask = static_cast<uint8_t>(mask | (1 << order.find(hwLetter[bit])));
        }
    }
    return mask;
}

constexpr double kLit = 0.05;

} // namespace

TEST_GROUP(Abc80Display)
{
    Abc80Board board;
};

TEST(Abc80Display, MonitorStartMasksFollowRomGlyphsLeftToRight)
{
    bootToMonitor(board, RomId::Monitor);
    const std::array<uint8_t, 6> masks = board.displayMasks(kLit);
    for (int i = 0; i < 6; ++i) {
        // Leftmost display digit i is ROM digit 5 - i; the ROM scans glyph bytes at 0x07BF + digit.
        const uint8_t glyph = board.getBus().readByte(static_cast<uint16_t>(0x07BF + (5 - i)));
        BYTES_EQUAL(canonicalFromHardware(glyph), masks[i]);
    }
}

TEST(Abc80Display, Rom1993MonitorMasksFollowRomGlyphsLeftToRight)
{
    bootToMonitor(board, RomId::Vintage1993);
    const std::array<uint8_t, 6> masks = board.displayMasks(kLit);
    for (int i = 0; i < 6; ++i) {
        const uint8_t glyph = board.getBus().readByte(static_cast<uint16_t>(0x07BF + (5 - i)));
        BYTES_EQUAL(canonicalFromHardware(glyph), masks[i]);
    }
}

TEST(Abc80Display, DarkWhileTheChimePlaysAndLitOnceScanning)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    runFrames(board, 20);  // the reset chime: the ROM is not scanning the display
    for (uint8_t m : board.displayMasks(kLit)) BYTES_EQUAL(0, m);
    CHECK(board.bootTurbo(RomId::Monitor, 60000000));
    runFrames(board, 5);
    bool anyLit = false;
    for (uint8_t m : board.displayMasks(kLit)) anyLit = anyLit || (m != 0);
    CHECK(anyLit);
}

TEST(Abc80Display, LitThresholdIsAShareOfTheFrame)
{
    bootToMonitor(board, RomId::Monitor);  // a scanned digit is on about 13.7% of the time
    bool lit5 = false, lit30 = false;
    for (uint8_t m : board.displayMasks(0.05)) lit5 = lit5 || (m != 0);
    for (uint8_t m : board.displayMasks(0.30)) lit30 = lit30 || (m != 0);
    CHECK(lit5);
    CHECK(!lit30);
}

TEST(Abc80Display, Rom1993LinkPollIsDark)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Vintage1993, ".")));
    CHECK(board.bootTurbo(RomId::Vintage1993, 60000000));
    runFrames(board, 5);
    for (uint8_t m : board.displayMasks(kLit)) BYTES_EQUAL(0, m);
}

// The accumulator must equal an independent recount from per-T-state samples of the ports.
TEST(Abc80Display, OnTimeAccumulatorMatchesAnIndependentRecount)
{
    bootToMonitor(board, RomId::Monitor);
    Abc80Ppi &ppi = board.getPpi();
    uint32_t expected[6][8] = {};
    ppi.beginFrame();
    const uint32_t frame = ABC80_FRAME_TSTATES;
    for (uint32_t t = 0; t < frame; ++t) {
        // The state before a tick applies during that tick.
        const uint8_t strobe = ppi.getDigitStrobe();
        const uint8_t portB = ppi.getSegmentOutput();
        for (int d = 0; d < 6; ++d) {
            if (strobe & (1 << d)) continue;  // digit not selected
            for (int bit = 0; bit < 8; ++bit) {
                if (portB & (1 << bit)) continue;  // Port B is active-low: low = lit
                const uint8_t canon = canonicalFromHardware(static_cast<uint8_t>(1 << bit));
                for (int c = 0; c < 8; ++c) {
                    if (canon & (1 << c)) ++expected[d][c];
                }
            }
        }
        board.getCpu().tick();
    }
    ppi.endFrame();
    for (int d = 0; d < 6; ++d) {
        for (int c = 0; c < 8; ++c) {
            LONGS_EQUAL(expected[d][c], ppi.digitSegmentOnTStates(static_cast<uint8_t>(d), static_cast<uint8_t>(c)));
        }
    }
}

TEST(Abc80Display, QueriesOutsideTheSixDigitsAndEightSegmentsReadZero)
{
    bootToMonitor(board, RomId::Monitor);  // plenty of on-time recorded in range
    const Abc80Ppi &ppi = board.getPpi();
    CHECK(ppi.digitSegmentOnTStates(0, 0) + ppi.digitSegmentOnTStates(5, 6) > 0);
    LONGS_EQUAL(0, ppi.digitSegmentOnTStates(6, 0));
    LONGS_EQUAL(0, ppi.digitSegmentOnTStates(255, 0));
    LONGS_EQUAL(0, ppi.digitSegmentOnTStates(0, 8));
    LONGS_EQUAL(0, ppi.digitSegmentOnTStates(0, 255));
    LONGS_EQUAL(0, ppi.digitSegmentOnTStates(6, 8));
}

TEST(Abc80Display, GlyphDecodeCoversHexDashAndBlankAndIgnoresTheDecimalPoint)
{
    // Canonical masks, bit 0=a .. 6=g, 7=dp.
    const struct { uint8_t mask; char ch; } table[] = {
        { 0x3F, '0' }, { 0x06, '1' }, { 0x5B, '2' }, { 0x4F, '3' }, { 0x66, '4' }, { 0x6D, '5' },
        { 0x7D, '6' }, { 0x07, '7' }, { 0x7F, '8' }, { 0x6F, '9' }, { 0x77, 'A' }, { 0x7C, 'b' },
        { 0x39, 'C' }, { 0x5E, 'd' }, { 0x79, 'E' }, { 0x71, 'F' }, { 0x40, '-' }, { 0x00, ' ' },
    };
    for (const auto &e : table) {
        CHECK_EQUAL(e.ch, Abc80Board::glyphForMask(e.mask));
        CHECK_EQUAL(e.ch, Abc80Board::glyphForMask(static_cast<uint8_t>(e.mask | 0x80)));  // dp set
    }
    CHECK_EQUAL('?', Abc80Board::glyphForMask(0x01));  // a lone segment is not a glyph
}

// The book monitor's start screen is "AbC-80" and its hex font has the same bytes as the author's 1993 ROM
// (the book listing had three transcription slips at 0x07C4, 0x07FA and 0x07FF).
TEST(Abc80Display, MonitorStartDisplayReadsAbC80)
{
    bootToMonitor(board, RomId::Monitor);
    const std::array<char, 6> d = board.displayDigits(kLit);
    STRCMP_EQUAL("AbC-80", std::string(d.begin(), d.end()).c_str());
}

TEST(Abc80Display, MonitorFontAndMessageTablesMatchThe1993Rom)
{
    Abc80Board other;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(RomId::Monitor, ".")));
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(other.powerOnRom(RomId::Vintage1993, ".")));
    for (uint16_t a = 0x07BF; a <= 0x07C4; ++a) {  // "AbC-80" glyphs
        BYTES_EQUAL(other.getBus().readByte(a), board.getBus().readByte(a));
    }
    for (uint16_t a = 0x07D9; a <= 0x07DE; ++a) {  // "-Error" glyphs
        BYTES_EQUAL(other.getBus().readByte(a), board.getBus().readByte(a));
    }
    for (uint16_t a = 0x07F0; a <= 0x07FF; ++a) {  // hex digits 0-F
        BYTES_EQUAL(other.getBus().readByte(a), board.getBus().readByte(a));
    }
}

TEST(Abc80Display, AddressEntryShowsTheTypedDigitsOnTheLeft)
{
    bootToMonitor(board, RomId::Monitor);
    KeyDriver keys(board, romProfile(RomId::Monitor).keys);
    keys.type(KEY_ADRS);
    keys.type(KEY_1);
    keys.type(KEY_2);
    keys.type(KEY_3);
    keys.type(KEY_4);
    keys.frames(20);
    const std::array<char, 6> d = board.displayDigits(kLit);
    CHECK_EQUAL('1', d[0]);
    CHECK_EQUAL('2', d[1]);
    CHECK_EQUAL('3', d[2]);
    CHECK_EQUAL('4', d[3]);
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
