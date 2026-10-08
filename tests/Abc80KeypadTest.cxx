/*
 * Abc80KeypadTest.cxx
 *
 * Keypad behavior of the real ROMs driven through the KeyQueue: scenarios A2, A3, A5
 * at the core level, and sweeps proving each ROM profile is at or above the measured minimum.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "Abc80FrameTestSupport.hxx"
#include <cstdint>
#include <vector>
#include <CppUTest/TestHarness.h>

using namespace abc80;
using namespace abc80test;

TEST_GROUP(Abc80Keypad)
{
    Abc80Board board;
};

TEST(Abc80Keypad, A2_AddressEntryOnTheMonitor)
{
    bootToMonitor(board, RomId::Monitor);
    KeyDriver keys(board, romProfile(RomId::Monitor).keys);
    keys.type(KEY_ADRS);
    keys.type(KEY_1);
    keys.type(KEY_2);
    keys.type(KEY_3);
    keys.type(KEY_4);
    keys.frames(30);
    LONGS_EQUAL(0x1234, adsave(board));
}

TEST(Abc80Keypad, A3_DataEntryAndStepping)
{
    bootToMonitor(board, RomId::Monitor);
    KeyDriver keys(board, romProfile(RomId::Monitor).keys);
    keys.type(KEY_ADRS);
    keys.type(KEY_2);
    keys.type(KEY_5);
    keys.type(KEY_0);
    keys.type(KEY_0);
    keys.type(KEY_DATA);
    keys.type(KEY_C);
    keys.type(KEY_3);
    keys.frames(30);
    LONGS_EQUAL(0x2500, adsave(board));
    keys.type(KEY_PLUS);
    keys.frames(30);
    LONGS_EQUAL(0xC3, board.getBus().readByte(0x2500));
    LONGS_EQUAL(0x2501, adsave(board));
    keys.type(KEY_MINUS);
    keys.frames(30);
    LONGS_EQUAL(0x2500, adsave(board));
}

// Ten keys typed 20 ms apart (about one frame): all register through the queue, on both ROMs.
TEST(Abc80Keypad, A5_FastTypingLosesNoKeys)
{
    for (RomId id : { RomId::Monitor, RomId::Vintage1993 }) {
        Abc80Board b;
        bootToMonitor(b, id);
        KeyDriver keys(b, romProfile(id).keys);
        keys.type(KEY_ADRS);
        keys.frames(15);  // let the first function key finish; then ten digits back to back
        const uint8_t digits[] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_A };
        for (uint8_t d : digits) {
            keys.type(d);  // type() advances one frame per key
        }
        keys.frames(200);  // drain the queue
        LONGS_EQUAL(0x789A, adsave(b));  // the last four of the ten digits
    }
}

// --- Sweeps: the smallest spacing/gap that always works, against the profile ---

namespace {

// Presses A for holdFrames, then B at (holdFrames + gapFrames). Returns true if both registered,
// judged by the address-entry shift register: B lost leaves only A shifted in.
bool bothRegister(Abc80Board &b, uint8_t keyA, uint8_t keyB, int holdFrames, int gapFrames)
{
    const uint16_t before = adsave(b);
    b.pressKey(keyA);
    for (int f = 0; f < holdFrames; ++f) b.stepFrame();
    b.releaseKey(keyA);
    for (int f = 0; f < gapFrames; ++f) b.stepFrame();
    b.pressKey(keyB);
    b.stepFrame();
    b.stepFrame();
    b.releaseKey(keyB);
    for (int f = 0; f < 40; ++f) b.stepFrame();  // let everything finish
    const uint16_t both = static_cast<uint16_t>(((before << 4) | (keyA & 0xF)) << 4 | (keyB & 0xF));
    const uint16_t onlyA = static_cast<uint16_t>((before << 4) | (keyA & 0xF));
    const uint16_t got = adsave(b);
    CHECK(got == both || got == onlyA);
    return got == both;
}

void enterAddressMode(Abc80Board &b)
{
    b.pressKey(KEY_ADRS);
    runFrames(b, 3);
    b.releaseKey(KEY_ADRS);
    runFrames(b, 40);
    b.pressKey(KEY_0);  // priming digit: the first digit after ADRS replaces the address
    runFrames(b, 3);
    b.releaseKey(KEY_0);
    runFrames(b, 40);
}

// Smallest value v in [1, maxV] such that the trial succeeds for v and every larger value, over
// several scan-loop phases (the settle count shifts the phase).
template <typename Trial>
int smallestAlwaysWorking(Abc80Board &b, int maxV, Trial trial)
{
    int smallest = maxV + 1;
    for (int v = maxV; v >= 1; --v) {
        bool all = true;
        for (int phase = 0; phase < 4; ++phase) {
            runFrames(b, phase);
            all = all && trial(v);
        }
        if (!all) break;
        smallest = v;
    }
    return smallest;
}

} // namespace

TEST(Abc80Keypad, PressSpacingProfileIsAtOrAboveTheMeasuredMinimum)
{
    for (RomId id : { RomId::Monitor, RomId::Vintage1993 }) {
        Abc80Board b;
        bootToMonitor(b, id);
        enterAddressMode(b);
        // Press B `spacing` frames after A started: hold A one frame, so gap = spacing - 1.
        const int minSpacing = smallestAlwaysWorking(b, 12, [&](int spacing) {
            return bothRegister(b, KEY_1, KEY_2, 1, spacing - 1);
        });
        std::printf("E1 keys %s min_press_spacing_frames %d profile %d\n",
                    romProfile(id).name, minSpacing, romProfile(id).keys.pressSpacingFrames);
        CHECK(minSpacing <= 12);
        CHECK(romProfile(id).keys.pressSpacingFrames >= minSpacing);
        // And not wastefully slow: within three frames of the minimum.
        CHECK(romProfile(id).keys.pressSpacingFrames <= minSpacing + 3);
    }
}

TEST(Abc80Keypad, ReleaseGapProfileIsAtOrAboveTheMeasuredMinimum)
{
    for (RomId id : { RomId::Monitor, RomId::Vintage1993 }) {
        Abc80Board b;
        bootToMonitor(b, id);
        enterAddressMode(b);
        // Hold A long (ten frames, past the deaf window); B follows `gap` frames after the release.
        const int minGap = smallestAlwaysWorking(b, 8, [&](int gap) {
            return bothRegister(b, KEY_1, KEY_2, 10, gap);
        });
        std::printf("E1 keys %s min_release_gap_frames %d profile %d\n",
                    romProfile(id).name, minGap, romProfile(id).keys.releaseGapFrames);
        CHECK(minGap <= 8);
        CHECK(romProfile(id).keys.releaseGapFrames >= minGap);
        CHECK(romProfile(id).keys.releaseGapFrames <= minGap + 2);
    }
}

TEST(Abc80Keypad, OneFrameHoldRegistersAtEveryPhase)
{
    for (RomId id : { RomId::Monitor, RomId::Vintage1993 }) {
        Abc80Board b;
        bootToMonitor(b, id);
        enterAddressMode(b);
        for (int phase = 0; phase < 12; ++phase) {
            runFrames(b, phase);
            const uint16_t before = adsave(b);
            b.pressKey(KEY_3);
            b.stepFrame();  // exactly one frame (the profile's hold)
            b.releaseKey(KEY_3);
            runFrames(b, 40);
            LONGS_EQUAL(static_cast<uint16_t>((before << 4) | 3), adsave(b));
        }
    }
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
