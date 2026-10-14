/*
 * Abc80BookRomGapTest.cxx
 *
 * Qualification suite for the restored 296-byte built-in demo region (05FDh-0724h)
 * in the book monitor ROM (build/rom.bin):
 *   - Byte-for-byte parity with vintage/rom.abc across 05FDh-0724h and zero drift outside
 *   - COUNTR (05FDh): 4-digit BCD counter, nibble/byte rollovers, 7-seg output, key pause
 *   - ORGAN & ORGTAB (0625h, 0648h): 16-key scale and Port C bit 7 square-wave synthesis
 *   - CLOCK, CLKINC, CLKDP, CLKTAB (0658h-06A1h): 12-hour BCD clock & RapidCheck properties
 *   - CLKRST, CLKSET, CLKCLR (06A2h-06DAh): preset display buffer formatting and clearing
 *   - TIGERS & SCORE (06DCh-0724h): "Two Tigers" melody playback and 40h repeat marker
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Rom.hxx>
#include <cstdint>
#include <vector>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

using namespace abc80;

namespace {

uint8_t toBcd(unsigned v)
{
    return static_cast<uint8_t>(((v / 10U) << 4) | (v % 10U));
}

struct GapRig {
    Abc80Board board;
    uint64_t t{0};
    uint16_t lastFetch{0xFFFF};
    bool fetched{false};
    uint8_t lastPortC{0xFF};
    unsigned bit7Edges{0};

    void init(RomId id = RomId::Monitor)
    {
        LONGS_EQUAL(static_cast<int>(Abc80Status::OK),
                    static_cast<int>(board.powerOnRom(id, ".")));
        t = 0;
        lastFetch = 0xFFFF;
        fetched = false;
        lastPortC = board.getPpi().getDigitStrobe();
        bit7Edges = 0;
    }

    void tick()
    {
        const uint64_t pins = board.getCpu().tick();
        ++t;
        fetched = false;
        if ((pins & Z80_M1) && (pins & Z80_MREQ) && (pins & Z80_RD)) {
            lastFetch = Z80_GET_ADDR(pins);
            fetched = true;
        }
        const uint8_t c = board.getPpi().getDigitStrobe();
        if ((c ^ lastPortC) & 0x80) {
            ++bit7Edges;
        }
        lastPortC = c;
    }

    bool runUntilFetch(uint16_t addr, uint64_t maxT)
    {
        const uint64_t end = t + maxT;
        while (t < end) {
            tick();
            if (fetched && lastFetch == addr) {
                return true;
            }
        }
        return false;
    }

    void runFor(uint64_t deltaT)
    {
        const uint64_t end = t + deltaT;
        while (t < end) {
            tick();
        }
    }

    bool runStub(const std::vector<uint8_t> &stub, uint64_t maxT)
    {
        for (size_t i = 0; i < stub.size(); ++i) {
            board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), stub[i]);
        }
        board.getCpu().prefetch(0x2000);
        const uint64_t end = t + maxT;
        while (t < end) {
            tick();
            if (board.getCpu().isHalted()) {
                return true;
            }
        }
        return false;
    }

    uint8_t byteAt(uint16_t addr) const
    {
        return board.getBus().readByte(addr);
    }

    void writeByte(uint16_t addr, uint8_t val)
    {
        board.getBus().writeByte(addr, val);
    }
};

} // namespace

TEST_GROUP(Abc80BookRomGap)
{
    GapRig rig;

    void setup() override
    {
        rig.init(RomId::Monitor);
    }
};

TEST(Abc80BookRomGap, GapBytesMatchVintage1993RomByteForByte)
{
    GapRig vintage;
    vintage.init(RomId::Vintage1993);

    // Boundary anchors immediately before and after 05FDh..0724h must remain unshifted.
    BYTES_EQUAL(0x80, rig.byteAt(0x05FC));  // end of rmusic
    BYTES_EQUAL(0xDD, rig.byteAt(0x05FD));  // start of countr (ld ix,1100h)
    BYTES_EQUAL(0x40, rig.byteAt(0x0724));  // end of score (repeat marker)
    BYTES_EQUAL(0xDE, rig.byteAt(0x0725));  // start of subfun table
    BYTES_EQUAL(0xBD, rig.byteAt(0x07F0));  // start of segtab ('0' glyph)

    for (uint16_t addr = 0x05FD; addr <= 0x0724; ++addr) {
        BYTES_EQUAL(vintage.byteAt(addr), rig.byteAt(addr));
    }
}

TEST(Abc80BookRomGap, CountrIncrementsBcdAndPausesWhileKeyHeld)
{
    rig.board.getCpu().setSP(0x17BF);
    rig.board.getCpu().prefetch(0x05FD);

    // Reach first scan pass at cnt_lp (0604h): IX = 1100h, DE = 0000h.
    CHECK(rig.runUntilFetch(0x0604, 10000));
    LONGS_EQUAL(0x1100, rig.board.getCpu().getIX());
    LONGS_EQUAL(0x0000, rig.board.getCpu().getDE());

    // One full pass with no key held reaches 0623h (jr cnt_lp) with DE = 0001h.
    CHECK(rig.runUntilFetch(0x0623, 50000));
    LONGS_EQUAL(0x0001, rig.board.getCpu().getDE());
    BYTES_EQUAL(rig.byteAt(0x07F1), rig.byteAt(0x1100));  // digit '1'
    BYTES_EQUAL(rig.byteAt(0x07F0), rig.byteAt(0x1101));  // digit '0'
    BYTES_EQUAL(0x02, rig.byteAt(0x1102));                // separator segment
    BYTES_EQUAL(rig.byteAt(0x07F0), rig.byteAt(0x1103));  // digit '0'
    BYTES_EQUAL(rig.byteAt(0x07F0), rig.byteAt(0x1104));  // digit '0'
    BYTES_EQUAL(0x00, rig.byteAt(0x1105));                // blank leftmost digit

    // BCD low-nibble carry: 0009h -> 0010h.
    CHECK(rig.runUntilFetch(0x0604, 10000));
    rig.board.getCpu().setDE(0x0009);
    CHECK(rig.runUntilFetch(0x0623, 50000));
    LONGS_EQUAL(0x0010, rig.board.getCpu().getDE());
    BYTES_EQUAL(rig.byteAt(0x07F0), rig.byteAt(0x1100));  // '0'
    BYTES_EQUAL(rig.byteAt(0x07F1), rig.byteAt(0x1101));  // '1'

    // BCD byte carry: 0099h -> 0100h.
    CHECK(rig.runUntilFetch(0x0604, 10000));
    rig.board.getCpu().setDE(0x0099);
    CHECK(rig.runUntilFetch(0x0623, 50000));
    LONGS_EQUAL(0x0100, rig.board.getCpu().getDE());
    BYTES_EQUAL(rig.byteAt(0x07F1), rig.byteAt(0x1103));  // '1'
    BYTES_EQUAL(rig.byteAt(0x07F0), rig.byteAt(0x1104));  // '0'

    // BCD 4-digit rollover: 9999h -> 0000h.
    CHECK(rig.runUntilFetch(0x0604, 10000));
    rig.board.getCpu().setDE(0x9999);
    CHECK(rig.runUntilFetch(0x0623, 50000));
    LONGS_EQUAL(0x0000, rig.board.getCpu().getDE());

    // Holding a key pauses counting (scd_k1 returns C = 0 -> jr nc,cnt_lp at 0607h).
    CHECK(rig.runUntilFetch(0x0604, 10000));
    rig.board.getCpu().setDE(0x0042);
    rig.board.pressKey(KEY_0);
    CHECK(!rig.runUntilFetch(0x0609, 60000));  // never reaches increment at 0609h while held
    CHECK(rig.runUntilFetch(0x0604, 25000));   // return from scd_k1 (after EXX restores DE)
    LONGS_EQUAL(0x0042, rig.board.getCpu().getDE());
    rig.board.releaseKey(KEY_0);
    CHECK(rig.runUntilFetch(0x0623, 50000));
    LONGS_EQUAL(0x0043, rig.board.getCpu().getDE());
}

TEST(Abc80BookRomGap, OrganPitchTableAndSpeakerSquareWave)
{
    const uint8_t expectedOrgtab[16] = {
        0xB2, 0xA8, 0x96, 0x85, 0x7E, 0x70, 0x64, 0x59,
        0x54, 0x4A, 0x42, 0x3E, 0x37, 0x31, 0x2C, 0x29
    };
    for (size_t i = 0; i < 16; ++i) {
        BYTES_EQUAL(expectedOrgtab[i], rig.byteAt(static_cast<uint16_t>(0x0648 + i)));
        if (i > 0) {
            CHECK(rig.byteAt(static_cast<uint16_t>(0x0648 + i - 1)) >
                  rig.byteAt(static_cast<uint16_t>(0x0648 + i)));
        }
    }

    // Boot ORGAN (0625h), let scd_k settle into scloop (04C0h), then hold KEY_5.
    rig.board.getCpu().setSP(0x17BF);
    rig.board.getCpu().prefetch(0x0625);
    CHECK(rig.runUntilFetch(0x04C0, 300000));
    LONGS_EQUAL(0x07E0, rig.board.getCpu().getIX());

    rig.board.pressKey(KEY_5);
    CHECK(rig.runUntilFetch(0x062C, 100000));  // scd_k returned key code 5 in A
    BYTES_EQUAL(0x05, static_cast<uint8_t>(rig.board.getCpu().getAF() >> 8));

    rig.bit7Edges = 0;
    rig.runFor(60000);
    CHECK(rig.bit7Edges >= 12);  // continuous square wave on Port C bit 7 while key held

    // Releasing the key exits the tone loop back to org_wt (0629h: call scd_k).
    rig.board.releaseKey(KEY_5);
    CHECK(rig.runUntilFetch(0x0629, 20000));
}

TEST(Abc80BookRomGap, ClkincRolloverBoundariesAndProperty)
{
    auto stepClock = [&](uint8_t ss, uint8_t mm, uint8_t hh) {
        rig.writeByte(0x1200, ss);
        rig.writeByte(0x1201, mm);
        rig.writeByte(0x1202, hh);
        const std::vector<uint8_t> stub = {
            0x31, 0xBF, 0x17,  // ld sp,17BFh
            0xCD, 0x6F, 0x06,  // call clkinc (066Fh)
            0x76               // halt
        };
        CHECK(rig.runStub(stub, 20000));
    };

    // Nominal 00:00:00 -> 00:00:01
    stepClock(0x00, 0x00, 0x00);
    BYTES_EQUAL(0x01, rig.byteAt(0x1200));
    BYTES_EQUAL(0x00, rig.byteAt(0x1201));
    BYTES_EQUAL(0x00, rig.byteAt(0x1202));

    // BCD low-nibble boundary 00:00:09 -> 00:00:10
    stepClock(0x09, 0x00, 0x00);
    BYTES_EQUAL(0x10, rig.byteAt(0x1200));
    BYTES_EQUAL(0x00, rig.byteAt(0x1201));
    BYTES_EQUAL(0x00, rig.byteAt(0x1202));

    // Seconds modulo-60 rollover 00:00:59 -> 00:01:00
    stepClock(0x59, 0x00, 0x00);
    BYTES_EQUAL(0x00, rig.byteAt(0x1200));
    BYTES_EQUAL(0x01, rig.byteAt(0x1201));
    BYTES_EQUAL(0x00, rig.byteAt(0x1202));

    // Minutes modulo-60 rollover 00:59:59 -> 01:00:00
    stepClock(0x59, 0x59, 0x00);
    BYTES_EQUAL(0x00, rig.byteAt(0x1200));
    BYTES_EQUAL(0x00, rig.byteAt(0x1201));
    BYTES_EQUAL(0x01, rig.byteAt(0x1202));

    // Hours modulo-12 wrap 11:59:59 -> 00:00:00
    stepClock(0x59, 0x59, 0x11);
    BYTES_EQUAL(0x00, rig.byteAt(0x1200));
    BYTES_EQUAL(0x00, rig.byteAt(0x1201));
    BYTES_EQUAL(0x00, rig.byteAt(0x1202));

    // RapidCheck property: clkinc implements (total_seconds + 1) mod (12 * 3600) in BCD.
    const bool ok = rc::check("clkinc BCD modulo-12:60:60 arithmetic",
                              [this](uint8_t rawS, uint8_t rawM, uint8_t rawH) {
        const unsigned s = rawS % 60U;
        const unsigned m = rawM % 60U;
        const unsigned h = rawH % 12U;
        rig.writeByte(0x1200, toBcd(s));
        rig.writeByte(0x1201, toBcd(m));
        rig.writeByte(0x1202, toBcd(h));
        const std::vector<uint8_t> stub = {
            0x31, 0xBF, 0x17,
            0xCD, 0x6F, 0x06,
            0x76
        };
        RC_ASSERT(rig.runStub(stub, 20000));
        const unsigned next = (h * 3600U + m * 60U + s + 1U) % (12U * 3600U);
        const unsigned expS = next % 60U;
        const unsigned expM = (next / 60U) % 60U;
        const unsigned expH = (next / 3600U) % 12U;
        RC_ASSERT(rig.byteAt(0x1200) == toBcd(expS));
        RC_ASSERT(rig.byteAt(0x1201) == toBcd(expM));
        RC_ASSERT(rig.byteAt(0x1202) == toBcd(expH));
    });
    CHECK(ok);
}

TEST(Abc80BookRomGap, ClkdpAndClockLoopFormatSevenSegmentWithDecimalPoints)
{
    // Format 11:34:56 via clkdp (0687h).
    rig.writeByte(0x1200, 0x56);
    rig.writeByte(0x1201, 0x34);
    rig.writeByte(0x1202, 0x11);
    const std::vector<uint8_t> stub = {
        0x31, 0xBF, 0x17,  // ld sp,17BFh
        0xCD, 0x87, 0x06,  // call clkdp (0687h)
        0x76               // halt
    };
    CHECK(rig.runStub(stub, 30000));

    BYTES_EQUAL(rig.byteAt(0x07F6), rig.byteAt(0x1203));                 // '6'
    BYTES_EQUAL(rig.byteAt(0x07F5), rig.byteAt(0x1204));                 // '5'
    BYTES_EQUAL(static_cast<uint8_t>(rig.byteAt(0x07F4) | 0x40),
                rig.byteAt(0x1205));                                     // '4.' (DP bit 6 set)
    BYTES_EQUAL(rig.byteAt(0x07F3), rig.byteAt(0x1206));                 // '3'
    BYTES_EQUAL(static_cast<uint8_t>(rig.byteAt(0x07F1) | 0x40),
                rig.byteAt(0x1207));                                     // '1.' (DP bit 6 set)
    BYTES_EQUAL(rig.byteAt(0x07F1), rig.byteAt(0x1208));                 // '1'

    // Also run the top-level CLOCK entry (0658h) through one full 100-scan-pass second tick.
    rig.writeByte(0x1200, 0x59);
    rig.writeByte(0x1201, 0x59);
    rig.writeByte(0x1202, 0x11);
    rig.board.getCpu().setSP(0x17BF);
    rig.board.getCpu().prefetch(0x0658);
    CHECK(rig.runUntilFetch(0x066A, 3000000));  // reaches clk_fn after 100 scans + clkinc + clkdp
    LONGS_EQUAL(0x1203, rig.board.getCpu().getIX());
    BYTES_EQUAL(0x00, rig.byteAt(0x1200));
    BYTES_EQUAL(0x00, rig.byteAt(0x1201));
    BYTES_EQUAL(0x00, rig.byteAt(0x1202));
    BYTES_EQUAL(static_cast<uint8_t>(rig.byteAt(0x07F0) | 0x40), rig.byteAt(0x1205));
    BYTES_EQUAL(static_cast<uint8_t>(rig.byteAt(0x07F0) | 0x40), rig.byteAt(0x1207));
}

TEST(Abc80BookRomGap, ClkrstClksetAndClkclrFormatAndClearDisplayBuffer)
{
    rig.writeByte(0x1050, 0x12);
    rig.writeByte(0x1051, 0x34);
    rig.writeByte(0x1052, 0x56);

    // Entry 06A2h (clkrst): formats 1 byte (2 digits) into dispbf (17C6h..17C7h), zeroes 17C8h..17CBh.
    rig.board.getCpu().setSP(0x17BF);
    rig.board.getCpu().prefetch(0x06A2);
    CHECK(rig.runUntilFetch(0x06BF, 30000));
    BYTES_EQUAL(rig.byteAt(0x07F2), rig.byteAt(0x17C6));  // '2'
    BYTES_EQUAL(rig.byteAt(0x07F1), rig.byteAt(0x17C7));  // '1'
    BYTES_EQUAL(0x00, rig.byteAt(0x17C8));
    BYTES_EQUAL(0x00, rig.byteAt(0x17C9));
    BYTES_EQUAL(0x00, rig.byteAt(0x17CA));
    BYTES_EQUAL(0x00, rig.byteAt(0x17CB));

    // Entry 06AEh (clkrs2): formats 2 bytes (4 digits) into 17C6h..17C9h, zeroes 17CAh..17CBh.
    rig.board.getCpu().prefetch(0x06AE);
    CHECK(rig.runUntilFetch(0x06BF, 30000));
    BYTES_EQUAL(rig.byteAt(0x07F2), rig.byteAt(0x17C6));  // '2'
    BYTES_EQUAL(rig.byteAt(0x07F1), rig.byteAt(0x17C7));  // '1'
    BYTES_EQUAL(rig.byteAt(0x07F4), rig.byteAt(0x17C8));  // '4'
    BYTES_EQUAL(rig.byteAt(0x07F3), rig.byteAt(0x17C9));  // '3'
    BYTES_EQUAL(0x00, rig.byteAt(0x17CA));
    BYTES_EQUAL(0x00, rig.byteAt(0x17CB));

    // Entry 06BAh (clkrs3): formats all 3 bytes (6 digits) into 17C6h..17CBh.
    rig.board.getCpu().prefetch(0x06BA);
    CHECK(rig.runUntilFetch(0x06BF, 30000));
    BYTES_EQUAL(rig.byteAt(0x07F6), rig.byteAt(0x17CA));  // '6'
    BYTES_EQUAL(rig.byteAt(0x07F5), rig.byteAt(0x17CB));  // '5'
}

TEST(Abc80BookRomGap, TigersPlaysScoreAndRepeatsOn40hMarker)
{
    // Verify TIGERS (06DCh) loads IY = 06E4h and calls MUSIC (0411h), toggling speaker PC7.
    rig.board.getCpu().setSP(0x17BF);
    rig.board.getCpu().prefetch(0x06DC);
    CHECK(rig.runUntilFetch(0x0415, 10000));
    LONGS_EQUAL(0x06E4, rig.board.getCpu().getIY());
    LONGS_EQUAL(0x06E4, rig.board.getCpu().getIX());

    rig.bit7Edges = 0;
    rig.runFor(100000);
    CHECK(rig.bit7Edges >= 20);

    // Position IX at the final note pair (0722h: 11h, 08h) before the 40h repeat byte at 0724h.
    rig.board.getCpu().setIY(0x06E4);
    rig.board.getCpu().setIX(0x0722);
    rig.board.getCpu().prefetch(0x0415);
    // After playing the note at 0722h, IX advances to 0724h (40h), which triggers jp m,music (0411h).
    CHECK(rig.runUntilFetch(0x0411, 2000000));
    CHECK(rig.runUntilFetch(0x0415, 1000));
    LONGS_EQUAL(0x06E4, rig.board.getCpu().getIX());  // reloaded from IY to repeat melody
}

// Negative control for gemini_quality_gate.py (-ri -g Abc80BookRomGapNegative must fail).
TEST_GROUP(Abc80BookRomGapNegative)
{
};

IGNORE_TEST(Abc80BookRomGapNegative, WrongExpectationsMustFail)
{
    GapRig rig;
    rig.init(RomId::Monitor);
    // Deliberately assert that the gap is still zero-filled (false once src/rom.asm is populated).
    BYTES_EQUAL(0x00, rig.byteAt(0x05FD));
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
