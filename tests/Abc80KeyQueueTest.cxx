/*
 * Abc80KeyQueueTest.cxx
 *
 * The KeyQueue turns client presses into emulated-time key actions that respect the ROM's
 * key timing. Plain cases plus RapidCheck properties over arbitrary event sequences.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Types.hxx>
#include <abc80/KeyQueue.hxx>
#include <cstdint>
#include <vector>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

using namespace abc80;

namespace {

const KeyProfile kProfile = { 1, 5, 1 };  // hold, press spacing, release gap (frames)

struct Seen {
    int frame;
    uint8_t key;
    bool down;
};

// Runs `frames` ticks, collecting actions.
std::vector<Seen> drive(KeyQueue &q, int &frame, int frames)
{
    std::vector<Seen> out;
    for (int i = 0; i < frames; ++i, ++frame) {
        KeyQueue::Action a[4];
        const size_t n = q.tick(a, 4);
        for (size_t k = 0; k < n; ++k) out.push_back({ frame, a[k].keyCode, a[k].down });
    }
    return out;
}

// Checks the timing invariants on an action stream.
bool timingHolds(const std::vector<Seen> &s, const KeyProfile &p)
{
    int downKey = -1;
    int lastPressStart = -1000000;
    int lastRelease = -1000000;
    for (const Seen &e : s) {
        if (e.down) {
            if (downKey != -1) return false;                                   // two keys down
            if (e.frame - lastPressStart < p.pressSpacingFrames) return false;  // too soon after a press
            if (e.frame - lastRelease < p.releaseGapFrames) return false;       // too soon after a release
            downKey = e.key;
            lastPressStart = e.frame;
        } else {
            if (downKey != e.key) return false;                                  // release of a key not down
            downKey = -1;
            lastRelease = e.frame;
        }
    }
    return true;
}

} // namespace

TEST_GROUP(Abc80KeyQueue)
{
};

TEST(Abc80KeyQueue, SinglePressAndReleaseGivesDownThenUpAfterTheHold)
{
    KeyQueue q(kProfile);
    int frame = 0;
    CHECK(q.press(KEY_ADRS));
    q.release(KEY_ADRS);
    const std::vector<Seen> s = drive(q, frame, 10);
    LONGS_EQUAL(2, static_cast<long>(s.size()));
    CHECK(s[0].down && s[0].key == KEY_ADRS && s[0].frame == 0);
    CHECK(!s[1].down && s[1].key == KEY_ADRS && s[1].frame == 1);  // held holdFrames = 1
}

TEST(Abc80KeyQueue, SecondPressWaitsForTheSpacing)
{
    KeyQueue q(kProfile);
    int frame = 0;
    q.press(KEY_1);
    q.release(KEY_1);
    q.press(KEY_2);
    q.release(KEY_2);
    const std::vector<Seen> s = drive(q, frame, 20);
    LONGS_EQUAL(4, static_cast<long>(s.size()));
    LONGS_EQUAL(0, s[0].frame);
    LONGS_EQUAL(kProfile.pressSpacingFrames, s[2].frame);  // second press starts 5 frames after the first
    CHECK(timingHolds(s, kProfile));
}

TEST(Abc80KeyQueue, AKeyHeldByTheClientStaysDownUntilTheClientReleases)
{
    KeyQueue q(kProfile);
    int frame = 0;
    q.press(KEY_3);
    std::vector<Seen> s = drive(q, frame, 12);
    LONGS_EQUAL(1, static_cast<long>(s.size()));  // down only; no release yet
    q.release(KEY_3);
    s = drive(q, frame, 3);
    LONGS_EQUAL(1, static_cast<long>(s.size()));
    CHECK(!s[0].down);
}

TEST(Abc80KeyQueue, ReleaseOfAQueuedPressDoesNotCancelIt)
{
    KeyQueue q(kProfile);
    int frame = 0;
    q.press(KEY_1);
    q.release(KEY_1);
    q.press(KEY_2);
    q.release(KEY_2);  // arrives while key 2's press is still queued
    const std::vector<Seen> s = drive(q, frame, 20);
    int downs = 0;
    for (const Seen &e : s) if (e.down) ++downs;
    LONGS_EQUAL(2, downs);  // both taps survive
}

TEST(Abc80KeyQueue, SeventeenthQueuedPressIsRefusedAndReleasesAreNeverRefused)
{
    KeyQueue q(kProfile);
    for (int i = 0; i < 16; ++i) CHECK(q.press(KEY_1));
    CHECK(!q.press(KEY_1));
    q.release(KEY_1);  // must not crash or be refused silently into a stuck key
    int frame = 0;
    const std::vector<Seen> s = drive(q, frame, 200);
    CHECK(timingHolds(s, kProfile));
}

TEST(Abc80KeyQueue, ClearReleasesAHeldKeyAndDropsTheRest)
{
    KeyQueue q(kProfile);
    int frame = 0;
    q.press(KEY_1);
    q.press(KEY_2);
    std::vector<Seen> s = drive(q, frame, 2);
    LONGS_EQUAL(1, static_cast<long>(s.size()));  // key 1 down
    q.clear();
    s = drive(q, frame, 3);
    LONGS_EQUAL(1, static_cast<long>(s.size()));
    CHECK(!s[0].down && s[0].key == KEY_1);
    s = drive(q, frame, 20);
    LONGS_EQUAL(0, static_cast<long>(s.size()));  // key 2 was dropped
}

TEST(Abc80KeyQueue, ARepeatedKeyTapsAreEachDelivered)
{
    KeyQueue q(kProfile);
    int frame = 0;
    for (int i = 0; i < 5; ++i) {
        q.press(KEY_7);
        q.release(KEY_7);
    }
    const std::vector<Seen> s = drive(q, frame, 100);
    int downs = 0;
    for (const Seen &e : s) if (e.down) ++downs;
    LONGS_EQUAL(5, downs);
    CHECK(timingHolds(s, kProfile));
}

TEST(Abc80KeyQueue, TickNeedsRoomForTwoActionsAndAdvancesNothingWithout)
{
    KeyQueue q(kProfile);
    q.press(KEY_1);
    KeyQueue::Action a[2];
    LONGS_EQUAL(0, static_cast<long>(q.tick(nullptr, 4)));
    LONGS_EQUAL(0, static_cast<long>(q.tick(a, 0)));
    LONGS_EQUAL(0, static_cast<long>(q.tick(a, 1)));
    LONGS_EQUAL(1, static_cast<long>(q.tick(a, 2)));  // exactly two slots is enough
    CHECK(a[0].down && a[0].keyCode == KEY_1);
}

TEST(Abc80KeyQueue, AKeyIsHeldForTheProfilesHoldEvenIfTheClientReleasesAtOnce)
{
    const KeyProfile slow = { 3, 5, 1 };  // hold three frames
    KeyQueue q(slow);
    int frame = 0;
    q.press(KEY_4);
    q.release(KEY_4);
    const std::vector<Seen> s = drive(q, frame, 10);
    LONGS_EQUAL(2, static_cast<long>(s.size()));
    LONGS_EQUAL(0, s[0].frame);
    LONGS_EQUAL(3, s[1].frame);  // not before the third frame after the press
}

TEST(Abc80KeyQueue, TheHoldIsCountedFromTheFrameThePressStarted)
{
    const KeyProfile slow = { 3, 5, 1 };
    KeyQueue q(slow);
    int frame = 0;
    drive(q, frame, 7);  // idle frames first: the press does not start at frame 0
    q.press(KEY_4);
    q.release(KEY_4);
    const std::vector<Seen> s = drive(q, frame, 10);
    LONGS_EQUAL(2, static_cast<long>(s.size()));
    LONGS_EQUAL(7, s[0].frame);
    LONGS_EQUAL(10, s[1].frame);  // exactly three frames after it started
}

TEST(Abc80KeyQueue, ANextPressStartsExactlyOneReleaseGapAfterALateRelease)
{
    const KeyProfile profile = { 1, 3, 4 };  // spacing 3, gap 4
    KeyQueue q(profile);
    int frame = 0;
    q.press(KEY_1);
    q.press(KEY_2);
    std::vector<Seen> s = drive(q, frame, 10);      // key 1 held by the client for the whole time
    LONGS_EQUAL(1, static_cast<long>(s.size()));    // key 2 cannot start while key 1 is down
    q.release(KEY_1);
    s = drive(q, frame, 12);                         // frames 10.. : release happens at frame 10
    LONGS_EQUAL(2, static_cast<long>(s.size()));
    CHECK(!s[0].down);
    LONGS_EQUAL(10, s[0].frame);
    CHECK(s[1].down && s[1].key == KEY_2);
    LONGS_EQUAL(14, s[1].frame);                     // exactly release + gap
}

TEST(Abc80KeyQueue, APressStartsExactlyOneSpacingAfterThePreviousPress)
{
    const KeyProfile profile = { 1, 6, 1 };
    KeyQueue q(profile);
    int frame = 0;
    q.press(KEY_1);
    q.release(KEY_1);
    q.press(KEY_2);
    q.release(KEY_2);
    const std::vector<Seen> s = drive(q, frame, 20);
    LONGS_EQUAL(4, static_cast<long>(s.size()));
    LONGS_EQUAL(0, s[0].frame);
    LONGS_EQUAL(6, s[2].frame);
}

// ---- Properties over arbitrary event sequences ----

namespace {

// Exclusivity only: never two keys down, releases only of the key that is down, nothing left down.
bool exclusiveAndDrained(const std::vector<Seen> &s)
{
    int downKey = -1;
    for (const Seen &e : s) {
        if (e.down) {
            if (downKey != -1) return false;
            downKey = e.key;
        } else {
            if (downKey != e.key) return false;
            downKey = -1;
        }
    }
    return downKey == -1;
}

} // namespace

// Without resets, every timing rule holds for any mix of presses, releases (even stray ones) and idle frames.
TEST(Abc80KeyQueue, PropertyTimingRulesHoldWithoutResets)
{
    const bool ok = rc::check("timing rules", [](const std::vector<uint8_t> &events) {
        KeyQueue q(kProfile);
        int frame = 0;
        std::vector<Seen> all;
        for (uint8_t e : events) {
            const uint8_t key = static_cast<uint8_t>((e >> 2) % 23);  // keys 0x00-0x16
            if ((e & 3) == 0) q.press(key);
            else if ((e & 3) == 1) q.release(key);
            const std::vector<Seen> s = drive(q, frame, 1 + (e >> 6));  // 1-4 frames per event
            all.insert(all.end(), s.begin(), s.end());
        }
        // The client lets go of everything it could still be holding (one release per outstanding press), then drain.
        for (int pass = 0; pass < 20; ++pass) {  // each release frees the oldest unreleased press of that key
            for (uint8_t key = 0; key < 23; ++key) q.release(key);
        }
        const std::vector<Seen> rest = drive(q, frame, 600);
        all.insert(all.end(), rest.begin(), rest.end());
        RC_ASSERT(timingHolds(all, kProfile));
        RC_ASSERT(exclusiveAndDrained(all));
    });
    CHECK(ok);
}

// With resets in the mix the timing rules restart, but no two keys are ever down together
// and nothing is left stuck down.
TEST(Abc80KeyQueue, PropertyResetsNeverLeaveAKeyStuckOrTwoKeysDown)
{
    const bool ok = rc::check("exclusivity across resets", [](const std::vector<uint8_t> &events) {
        KeyQueue q(kProfile);
        int frame = 0;
        std::vector<Seen> all;
        for (uint8_t e : events) {
            const uint8_t key = static_cast<uint8_t>((e >> 2) % 23);
            switch (e & 3) {
            case 0: q.press(key); break;
            case 1: q.release(key); break;
            case 2: q.clear(); break;
            default: break;
            }
            const std::vector<Seen> s = drive(q, frame, 1 + (e >> 6));
            all.insert(all.end(), s.begin(), s.end());
        }
        for (int pass = 0; pass < 20; ++pass) {  // each release frees the oldest unreleased press of that key
            for (uint8_t key = 0; key < 23; ++key) q.release(key);
        }
        const std::vector<Seen> rest = drive(q, frame, 600);
        all.insert(all.end(), rest.begin(), rest.end());
        RC_ASSERT(exclusiveAndDrained(all));
    });
    CHECK(ok);
}

// Taps (press + release) up to the queue depth are all delivered, in the order they were typed.
TEST(Abc80KeyQueue, PropertyTapsAreDeliveredInOrderWithNoneLost)
{
    const bool ok = rc::check("fifo no loss", [](const std::vector<uint8_t> &keys) {
        std::vector<uint8_t> taps;
        for (uint8_t k : keys) {
            if (taps.size() < 16) taps.push_back(static_cast<uint8_t>(k % 23));
        }
        KeyQueue q(kProfile);
        int frame = 0;
        for (uint8_t k : taps) {
            RC_ASSERT(q.press(k));
            q.release(k);
        }
        const std::vector<Seen> s = drive(q, frame, 1200);
        std::vector<uint8_t> delivered;
        for (const Seen &e : s) if (e.down) delivered.push_back(e.key);
        RC_ASSERT(delivered == taps);
        RC_ASSERT(timingHolds(s, kProfile));
        RC_ASSERT(exclusiveAndDrained(s));
    });
    CHECK(ok);
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
