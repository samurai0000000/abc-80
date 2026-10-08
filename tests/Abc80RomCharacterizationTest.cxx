/*
 * Abc80RomCharacterizationTest.cxx
 *
 * Locks the measured behavior of both ROMs (plan Section 2.2, rows R1-R13)
 * by running the real ROM on the real CPU and PPI. No mocks.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Board.hxx>
#include <abc80/Abc80Rom.hxx>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <CppUTest/TestHarness.h>

using namespace abc80;

namespace {

// Addresses in both ROMs (monitor code is shared by the two images).
constexpr uint16_t kMain       = 0x009B;  // monitor main loop
constexpr uint16_t kAfterScdK  = 0x009E;  // scd_k returned a key code in A
constexpr uint16_t kMs3k17     = 0x0464;
constexpr uint16_t kMs3k17Ret  = 0x046E;
constexpr uint16_t kScpre      = 0x04B3;  // scd_k debounce start
constexpr uint16_t kScloop     = 0x04C0;  // scd_k waiting for a key
constexpr uint16_t kScdKRet    = 0x04CB;  // scd_k's ret: A holds the key code (any caller)
constexpr uint16_t kScdK1      = 0x04CD;  // one display scan + keypad pass
constexpr uint16_t kLinkEntry  = 0x0B00;  // 1993 ROM: PC-link server entry
constexpr uint16_t kDispFont   = 0x07BF;  // "AbC-80" start display glyphs

// Expected values (plan Section 2.2), in T-states unless noted.
struct Expect {
    RomId id;
    const char *name;
    uint64_t pitch;          // R1
    uint64_t click;          // R5
    uint64_t ready;          // R6 (key ADRS)
    uint64_t warmReady;      // R3 / R3b: reset to the profile's ready fetch
    uint64_t coldReady;      // R4
    size_t chimeBit7Edges;   // R8: reset to ready address
    size_t chimeBit6Edges;
    uint64_t holdMin;        // R7: a hold of one scan pass registers at every phase
    uint64_t ms3kHalf;       // R13
    double digit0Share;      // R11
};

const Expect kMonitor = { RomId::Monitor, "monitor",
                          2871, 89683, 102657, 5465514, 178176141, 1287, 0, 3000, 200, 0.137 };
const Expect kVintage = { RomId::Vintage1993, "1993",
                          17847, 101331, 174263, 9275758, 57868269, 3199, 1, 18000, 226, 0.162 };

const Expect &expectFor(RomId id)
{
    return id == RomId::Monitor ? kMonitor : kVintage;
}

bool within(uint64_t value, uint64_t expected, double fraction)
{
    const double lo = static_cast<double>(expected) * (1.0 - fraction);
    const double hi = static_cast<double>(expected) * (1.0 + fraction);
    return static_cast<double>(value) >= lo && static_cast<double>(value) <= hi;
}

// One board stepped one T-state at a time, observing opcode fetches (M1 cycles)
// and Port C bit transitions. The program counter is never taken from the address bus
// except at an opcode fetch.
struct Rig {
    Abc80Board board;
    uint64_t t{0};
    uint16_t lastFetch{0xFFFF};
    bool fetched{false};  // true on the T-state of an opcode fetch
    uint8_t lastPortC{0xFF};
    std::vector<uint64_t> bit7Edges;
    std::vector<uint64_t> bit6Edges;

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
        const uint8_t changed = static_cast<uint8_t>(c ^ lastPortC);
        if (changed & 0x80) bit7Edges.push_back(t);
        if (changed & 0x40) bit6Edges.push_back(t);
        lastPortC = c;
    }

    // Runs until the next opcode fetch at addr (true), or maxT T-states pass (false).
    bool runUntilFetch(uint16_t addr, uint64_t maxT)
    {
        const uint64_t end = t + maxT;
        while (t < end) {
            tick();
            if (fetched && lastFetch == addr) return true;
        }
        return false;
    }

    void runFor(uint64_t n)
    {
        const uint64_t end = t + n;
        while (t < end) tick();
    }

    uint8_t regA() { return static_cast<uint8_t>(board.getCpu().getAF() >> 8); }
    uint8_t ram(uint16_t a) const { return board.getBus().readByte(a); }
};

// Boots a ROM warm and settles in the keypad monitor's scd_k loop.
// Monitor: runs to main. 1993: runs to the link poll, then enters the monitor at
// 0x009B the way the PC-link host does (Abc80QbHost uses prefetch for 0x0B00).
void bootToMonitor(Rig &r, RomId id)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(r.board.powerOnRom(id, ".")));
    if (id == RomId::Monitor) {
        CHECK(r.runUntilFetch(kMain, 20000000));
    } else {
        CHECK(r.runUntilFetch(romProfile(id).readyAddr, 20000000));
        r.board.getCpu().prefetch(kMain);
    }
    r.runFor(200000);  // several scan passes: scd_k is now waiting in scloop
}

// Presses a key and runs until the ROM accepts it (fetch at 0x009E). Returns true if accepted.
bool pressAndAccept(Rig &r, uint8_t key, uint64_t maxT)
{
    r.board.pressKey(key);
    return r.runUntilFetch(kAfterScdK, maxT);
}

void report(const char *row, const char *rom, const char *what, unsigned long long v)
{
    std::printf("E0 %s %s %s %llu\n", row, rom, what, v);
}

// ---- R1: scan pass pitch ----
void rowR1(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    uint64_t last = 0;
    int measured = 0;
    uint16_t prev = 0xFFFF;
    for (uint64_t g = 0; g < 400000 && measured < 5; ++g) {
        r.tick();
        if (r.fetched && r.lastFetch == kScdK1 && prev != kScdK1) {
            if (last != 0) {
                const uint64_t pitch = r.t - last;
                report("R1", e.name, "pitch_T", pitch);
                CHECK_TEXT(within(pitch, e.pitch, 0.01), e.name);
                ++measured;
            }
            last = r.t;
        }
        if (r.fetched) prev = r.lastFetch;
    }
    LONGS_EQUAL(5, measured);
}

// ---- R2: clean passes before a key is accepted ----
void rowR2(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    CHECK(pressAndAccept(r, KEY_ADRS, 200000));
    r.board.releaseKey(KEY_ADRS);
    CHECK(r.runUntilFetch(kScpre, 3000000));
    int passes = 0;
    uint16_t prev = 0xFFFF;
    const uint64_t start = r.t;
    for (uint64_t g = 0; g < 4000000; ++g) {
        r.tick();
        if (!r.fetched) continue;
        if (r.lastFetch == kScdK1 && prev != kScdK1) ++passes;
        prev = r.lastFetch;
        if (r.lastFetch == kScloop) break;
    }
    report("R2", e.name, "clean_passes", static_cast<unsigned long long>(passes));
    report("R2", e.name, "scpre_to_scloop_T", r.t - start);
    LONGS_EQUAL(4, passes);
}

// ---- R3 / R3b: warm start to the ready address ----
void rowR3(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(r.board.powerOnRom(id, ".")));
    CHECK(r.runUntilFetch(romProfile(id).readyAddr, 40000000));
    report("R3", e.name, "reset_to_ready_T", r.t);
    CHECK_TEXT(within(r.t, e.warmReady, 0.01), e.name);
    // R8: the chime and boot beeps leave Port C bit 7 toggling; bit 6 only once on the 1993 ROM.
    report("R8", e.name, "bit7_edges", r.bit7Edges.size());
    report("R8", e.name, "bit6_edges", r.bit6Edges.size());
    LONGS_EQUAL(static_cast<long>(e.chimeBit7Edges), static_cast<long>(r.bit7Edges.size()));
    LONGS_EQUAL(static_cast<long>(e.chimeBit6Edges), static_cast<long>(r.bit6Edges.size()));
}

// ---- R4: cold start (RAM cleared, pwup != 0x80) ----
void rowR4(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    const std::string path = std::string("./") + romProfile(id).relPath;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(r.board.powerOn(path)));
    const uint16_t target = (id == RomId::Monitor) ? kMain : kLinkEntry;
    CHECK(r.runUntilFetch(target, 400000000ULL));
    report("R4", e.name, "cold_reset_to_ready_T", r.t);
    CHECK_TEXT(within(r.t, e.coldReady, 0.01), e.name);
}

// ---- R5 and R6: keyclick duration and key-accepted-to-ready ----
void rowR5R6(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    CHECK(pressAndAccept(r, KEY_ADRS, 200000));
    const uint64_t accepted = r.t;
    r.board.releaseKey(KEY_ADRS);
    CHECK(r.runUntilFetch(kMs3k17, 100000));
    const uint64_t clickStart = r.t;
    CHECK(r.runUntilFetch(kMs3k17Ret, 3000000));
    const uint64_t clickEnd = r.t;
    CHECK(r.runUntilFetch(kScloop, 3000000));
    const uint64_t ready = r.t - accepted;
    report("R5", e.name, "click_T", clickEnd - clickStart);
    report("R6", e.name, "accept_to_ready_T", ready);
    CHECK_TEXT(within(clickEnd - clickStart, e.click, 0.01), e.name);
    CHECK_TEXT(within(ready, e.ready, 0.01), e.name);
}

// ---- R7: smallest hold that registers: one scan pass registers at every phase ----
void rowR7(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    int shortRegistered = 0;
    for (int phase = 0; phase < 16; ++phase) {
        // Vary the phase of the press against the scan loop; ROM is ready (in scloop) each time.
        r.runFor(static_cast<uint64_t>(e.pitch) * static_cast<uint64_t>(phase) / 16 + 1);
        r.board.pressKey(KEY_ADRS);
        bool accepted = false;
        const uint64_t end = r.t + e.holdMin;
        while (r.t < end) {
            r.tick();
            if (r.fetched && r.lastFetch == kAfterScdK) accepted = true;
        }
        r.board.releaseKey(KEY_ADRS);
        if (!accepted) accepted = r.runUntilFetch(kAfterScdK, e.pitch * 3);
        CHECK_TEXT(accepted, e.name);  // a hold of one full pass always registers
        CHECK(r.runUntilFetch(kScloop, 3000000));

        // A hold of an eighth of a pass is too short at some phases.
        r.runFor(static_cast<uint64_t>(e.pitch) * static_cast<uint64_t>(phase) / 16 + 1);
        r.board.pressKey(KEY_ADRS);
        bool shortAccepted = false;
        const uint64_t shortEnd = r.t + e.pitch / 8;
        while (r.t < shortEnd) {
            r.tick();
            if (r.fetched && r.lastFetch == kAfterScdK) shortAccepted = true;
        }
        r.board.releaseKey(KEY_ADRS);
        if (shortAccepted) ++shortRegistered;
        // Settle: let any accepted key finish its click and return to the scan loop with no key down.
        r.runFor(400000);
    }
    report("R7", e.name, "short_hold_registered_of_16", static_cast<unsigned long long>(shortRegistered));
    CHECK_TEXT(shortRegistered < 16, e.name);
}

// ---- R9 and R10 and R11: idle Port C, Port B polarity, digit duty ----
void rowR9R10R11(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    // R9: while scanning, Port C bit 7 (speaker) stays high.
    // R11 is measured over exactly 10 scan passes (entry to entry), so the window has no partial pass.
    bool bit7AlwaysHigh = true;
    uint64_t low = 0;
    uint64_t total = 0;
    CHECK(r.runUntilFetch(kScdK1, 200000));
    int entries = 0;
    uint16_t prev = kScdK1;
    while (entries < 10) {
        r.tick();
        ++total;
        const uint8_t c = r.board.getPpi().getDigitStrobe();
        if ((c & 0x80) == 0) bit7AlwaysHigh = false;
        if ((c & 0x01) == 0) ++low;
        if (r.fetched) {
            if (r.lastFetch == kScdK1 && prev != kScdK1) ++entries;
            prev = r.lastFetch;
        }
        CHECK(total < 1000000);
    }
    CHECK_TEXT(bit7AlwaysHigh, e.name);
    // R10: Port B is the complement of the glyph byte the ROM holds for each digit.
    for (int i = 0; i < 6; ++i) {
        const uint8_t glyph = r.ram(static_cast<uint16_t>(kDispFont + i));
        report("R10", e.name, "glyph", glyph);
        BYTES_EQUAL(static_cast<uint8_t>(~glyph), r.board.getPpi().getDigitSegment(static_cast<uint8_t>(i)));
    }
    // R11: share of time digit 0 is strobed.
    const double share = static_cast<double>(low) / static_cast<double>(total);
    report("R11", e.name, "digit0_low_T", low);
    report("R11", e.name, "window_T", total);
    report("R11", e.name, "digit0_share_x1000", static_cast<unsigned long long>(share * 1000.0));
    CHECK_TEXT(share > e.digit0Share - 0.005 && share < e.digit0Share + 0.005, e.name);
}

// ---- R12: a key already down when scd_k starts its debounce is ignored until released ----
void rowR12(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    CHECK(pressAndAccept(r, KEY_ADRS, 200000));
    r.board.releaseKey(KEY_ADRS);
    CHECK(r.runUntilFetch(kScpre, 3000000));  // debounce window: key must be absent
    r.board.pressKey(KEY_1);
    CHECK(!r.runUntilFetch(kAfterScdK, e.pitch * 12));  // held, not accepted
    r.board.releaseKey(KEY_1);
    CHECK(r.runUntilFetch(kScloop, 3000000));            // 4 clean passes, now waiting
    r.runFor(e.pitch);
    r.board.pressKey(KEY_1);
    CHECK_TEXT(r.runUntilFetch(kAfterScdK, e.pitch * 4), e.name);  // fresh press accepted
}

// ---- R13: tape tone half-periods, by calling ms1k/ms2k/ms3k from RAM ----
uint64_t toneHalfPeriod(RomId id, uint16_t entry)
{
    Rig r;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(r.board.powerOnRom(id, ".")));
    const uint8_t stub[] = { 0x31, 0xBF, 0x17,                       // ld sp,17BFh
                             0xCD, static_cast<uint8_t>(entry & 0xFF),
                             static_cast<uint8_t>(entry >> 8),         // call entry
                             0x76 };                                   // halt
    for (size_t i = 0; i < sizeof(stub); ++i) {
        r.board.getBus().writeByte(static_cast<uint16_t>(0x2000 + i), stub[i]);
    }
    r.board.getCpu().prefetch(0x2000);
    r.board.getCpu().setHL(0x0020);
    for (uint64_t g = 0; g < 2000000 && !r.board.getCpu().isHalted(); ++g) r.tick();
    CHECK(r.board.getCpu().isHalted());
    CHECK(r.bit7Edges.size() > 8);
    uint64_t minD = ~0ULL;
    uint64_t maxD = 0;
    for (size_t i = 1; i < r.bit7Edges.size(); ++i) {
        const uint64_t d = r.bit7Edges[i] - r.bit7Edges[i - 1];
        if (d < minD) minD = d;
        if (d > maxD) maxD = d;
    }
    CHECK(minD == maxD);  // a steady square wave
    return minD;
}

void rowR13(RomId id)
{
    const Expect &e = expectFor(id);
    const uint64_t h1 = toneHalfPeriod(id, 0x0470);  // ms1k
    const uint64_t h2 = toneHalfPeriod(id, 0x0475);  // ms2k
    const uint64_t h3 = toneHalfPeriod(id, 0x047A);  // ms3k
    report("R13", e.name, "ms1k_half_T", h1);
    report("R13", e.name, "ms2k_half_T", h2);
    report("R13", e.name, "ms3k_half_T", h3);
    LONGS_EQUAL(889, static_cast<long>(h1));  // 1,006.7 Hz at 1.79 MHz
    LONGS_EQUAL(447, static_cast<long>(h2));  // 2,002.2 Hz at 1.79 MHz
    LONGS_EQUAL(static_cast<long>(e.ms3kHalf), static_cast<long>(h3));
}

// ---- Key codes: scd_k returns the monitor key code in A, for every key ----
void keyCodeRow(RomId id, uint8_t first, uint8_t last)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    for (uint8_t k = first; k <= last; ++k) {
        const std::string what = std::string(e.name) + " key " + std::to_string(static_cast<int>(k));
        r.board.pressKey(k);
        // Accept is the return from scd_k itself: some handlers (1993 "+") continue in a loop
        // other than main, so 0x009E is not reached for later keys.
        CHECK_TEXT(r.runUntilFetch(kScdKRet, 300000), what.c_str());
        BYTES_EQUAL(k, r.regA());
        r.board.releaseKey(k);
        CHECK(r.runUntilFetch(kScloop, 4000000));
    }
}

void rowKeyCodesSafe(RomId id)
{
    // 0x00-0x11 (hex digits, +, -), 0x13 (DATA), 0x14 (ADRS) do not leave the monitor loop.
    keyCodeRow(id, KEY_0, KEY_MINUS);
    keyCodeRow(id, KEY_DATA, KEY_ADRS);
}

void rowKeyCodeRun(RomId id)  { keyCodeRow(id, KEY_RUN, KEY_RUN); }
void rowKeyCodeStep(RomId id) { keyCodeRow(id, KEY_STEP, KEY_STEP); }
void rowKeyCodeBp(RomId id)   { keyCodeRow(id, KEY_BP, KEY_BP); }

// ---- Ghosting: two keys down together yield one of them, deterministically ----
void rowGhost(RomId id)
{
    const Expect &e = expectFor(id);
    Rig r;
    bootToMonitor(r, id);
    r.board.pressKey(KEY_0);
    r.board.pressKey(KEY_1);
    CHECK_TEXT(r.runUntilFetch(kAfterScdK, 300000), e.name);
    const uint8_t code = r.regA();
    report("GHOST", e.name, "code_for_0_and_1", code);
    CHECK_TEXT(code == KEY_0 || code == KEY_1, e.name);
}

// ---- Start display: the ROM scans "AbC-80" glyphs, Port B active-low ----
void rowStartDisplay(RomId id)
{
    // Same observable as R10; the glyph table at 0x07BF is what both ROMs scan at start.
    Rig r;
    bootToMonitor(r, id);
    for (int i = 0; i < 6; ++i) {
        const uint8_t glyph = r.ram(static_cast<uint16_t>(kDispFont + i));
        BYTES_EQUAL(static_cast<uint8_t>(~glyph), r.board.getPpi().getDigitSegment(static_cast<uint8_t>(i)));
    }
}

// ---- 1993 link-server boot (A12 at the core level): dark display, dead keypad ----
void link1993()
{
    Rig r;
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(r.board.powerOnRom(RomId::Vintage1993, ".")));
    CHECK(r.runUntilFetch(0x0B04, 20000000));
    // R9 (1993 link poll): the speaker line idles low: Port C is 0x3F.
    BYTES_EQUAL(0x3F, r.board.getPpi().getDigitStrobe());
    int scans = 0;
    r.board.pressKey(KEY_1);
    for (uint64_t g = 0; g < 3000000; ++g) {
        r.tick();
        if (r.fetched && (r.lastFetch == kScdK1 || r.lastFetch == 0x04A6)) ++scans;
    }
    r.board.releaseKey(KEY_1);
    LONGS_EQUAL(0, scans);                 // display never scanned, keypad never read
    for (int i = 0; i < 6; ++i) {
        BYTES_EQUAL(0x00, r.board.getPpi().getDigitSegment(static_cast<uint8_t>(i)));  // all dark
    }
    // Entering the monitor the way the PC host does brings the panel up.
    r.board.getCpu().prefetch(kMain);
    CHECK(r.runUntilFetch(kScdK1, 300000));
}

} // namespace

TEST_GROUP(RomCharacterization)
{
};

#define ROM_ROW(NAME, FN) \
    TEST(RomCharacterization, Monitor_##NAME)  { FN(RomId::Monitor); } \
    TEST(RomCharacterization, Vintage1993_##NAME) { FN(RomId::Vintage1993); }

ROM_ROW(R1_ScanPassPitch, rowR1)
ROM_ROW(R2_CleanPassesBeforeAccept, rowR2)
ROM_ROW(R3_R8_WarmStartAndChimeEdges, rowR3)
ROM_ROW(R4_ColdStart, rowR4)
ROM_ROW(R5_R6_ClickAndReadyWindow, rowR5R6)
ROM_ROW(R7_MinimumHold, rowR7)
ROM_ROW(R9_R10_R11_PortCPortBAndDigitDuty, rowR9R10R11)
ROM_ROW(R12_HeldKeyIgnoredDuringDebounce, rowR12)
ROM_ROW(R13_TapeToneHalfPeriods, rowR13)
ROM_ROW(KeyCodes_SafeKeys, rowKeyCodesSafe)
ROM_ROW(KeyCodes_Run, rowKeyCodeRun)
ROM_ROW(KeyCodes_ToTape, rowKeyCodeStep)
ROM_ROW(KeyCodes_FromTape, rowKeyCodeBp)
ROM_ROW(Ghosting, rowGhost)
ROM_ROW(StartDisplayIsComplementOfGlyphs, rowStartDisplay)

TEST(RomCharacterization, Vintage1993_LinkServerBootIsDarkAndDeaf)
{
    link1993();
}

// ---- Negative control: deliberately wrong expectations; ignored in `make test`,
// run with -ri by the gate, which requires them to fail. ----
TEST_GROUP(RomCharacterizationNegative)
{
};

// One test, one failure (CppUTest's exit code is the failure count; the gate expects 1).
// Both deliberately wrong expectations are evaluated, then asserted together.
IGNORE_TEST(RomCharacterizationNegative, WrongExpectationsMustFail)
{
    uint64_t pitch = 0;
    {
        Rig r;
        bootToMonitor(r, RomId::Monitor);
        uint64_t last = 0;
        uint16_t prev = 0xFFFF;
        for (uint64_t g = 0; g < 400000 && pitch == 0; ++g) {
            r.tick();
            if (r.fetched && r.lastFetch == kScdK1 && prev != kScdK1) {
                if (last != 0) pitch = r.t - last;
                last = r.t;
            }
            if (r.fetched) prev = r.lastFetch;
        }
    }
    size_t edges = 0;
    {
        Rig r;
        LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(r.board.powerOnRom(RomId::Monitor, ".")));
        CHECK(r.runUntilFetch(kMain, 40000000));
        edges = r.bit7Edges.size();
    }
    const bool doubledPitchHolds = within(pitch, 2 * kMonitor.pitch, 0.01);  // false
    const bool offByOneEdgesHold = (edges == 1288);                           // false
    CHECK(doubledPitchHolds && offByOneEdgesHold);
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
