/*
 * Abc80AiKeySequencerTest.cxx
 *
 * Unit tests for AiKeySequencer (plan Section 2.3.3).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/AiKeySequencer.hxx>
#include <abc80/WebProtocol.hxx>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <rapidcheck.h>
#include <CppUTest/TestHarness.h>

using namespace abc80;

TEST_GROUP(Abc80AiKeySequencer)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(Abc80AiKeySequencer, RejectsEmptyOrInvalidKeys)
{
    AiKeySequencer seq([](const std::string &, bool, size_t) {});

    size_t count = 0;
    std::string err;
    CHECK(!seq.enqueue({}, count, err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!seq.enqueue({"ADRS", "NOT_A_KEY", "1"}, count, err));
    CHECK(err.find("unknown key name") != std::string::npos);
}

TEST(Abc80AiKeySequencer, DispatchesKeysSequentiallyWithDownAndUp)
{
    struct PunchEvent {
        std::string key;
        bool down;
        size_t remaining;
    };

    std::vector<PunchEvent> events;
    std::mutex mtx;
    std::atomic<bool> doneFired{false};

    AiKeySequencer seq(
        [&](const std::string &k, bool down, size_t remaining) {
            std::lock_guard<std::mutex> lock(mtx);
            events.push_back({k, down, remaining});
        },
        [&]() {
            doneFired.store(true);
        },
        50, // 50ms interval
        20  // 20ms hold
    );

    size_t count = 0;
    std::string err;
    CHECK(seq.enqueue({"ADRS", "1", "2"}, count, err));
    LONGS_EQUAL(3, static_cast<long>(count));

    CHECK(seq.waitForCompletion(std::chrono::milliseconds(2000)));
    CHECK(doneFired.load());

    std::lock_guard<std::mutex> lock(mtx);
    // 3 keys * 2 events each (down + up) = 6 events
    LONGS_EQUAL(6, static_cast<long>(events.size()));

    // Key 1: ADRS
    STRCMP_EQUAL("ADRS", events[0].key.c_str());
    CHECK(events[0].down);
    STRCMP_EQUAL("ADRS", events[1].key.c_str());
    CHECK(!events[1].down);

    // Key 2: 1
    STRCMP_EQUAL("1", events[2].key.c_str());
    CHECK(events[2].down);
    STRCMP_EQUAL("1", events[3].key.c_str());
    CHECK(!events[3].down);

    // Key 3: 2
    STRCMP_EQUAL("2", events[4].key.c_str());
    CHECK(events[4].down);
    STRCMP_EQUAL("2", events[5].key.c_str());
    CHECK(!events[5].down);
}

TEST(Abc80AiKeySequencer, CancelImmediatelyAbortsQueueAndReleasesKey)
{
    std::vector<bool> downStates;
    std::mutex mtx;
    std::atomic<bool> doneFired{false};

    AiKeySequencer seq(
        [&](const std::string &, bool down, size_t) {
            std::lock_guard<std::mutex> lock(mtx);
            downStates.push_back(down);
        },
        [&]() {
            doneFired.store(true);
        },
        200, // 200ms per key
        80
    );

    size_t count = 0;
    std::string err;
    CHECK(seq.enqueue({"ADRS", "1", "2", "3", "4", "5", "6", "7", "8", "9"}, count, err));

    // Wait until punching begins
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(seq.isPunching());

    // Cancel while active
    size_t discarded = seq.cancel();
    CHECK(discarded > 0);
    CHECK(!seq.isPunching());
    LONGS_EQUAL(0, static_cast<long>(seq.queueDepth()));
    CHECK(doneFired.load());

    // Verify last event released the key if down
    std::lock_guard<std::mutex> lock(mtx);
    if (!downStates.empty()) {
        CHECK(!downStates.back()); // last state must be released (down == false)
    }
}

TEST(Abc80AiKeySequencer, CancelDuringReleaseWindowBroadcastsRemainingZero)
{
    struct PunchEvent {
        std::string key;
        bool down;
        size_t remaining;
    };

    std::vector<PunchEvent> events;
    std::mutex mtx;
    std::atomic<bool> doneFired{false};

    AiKeySequencer seq(
        [&](const std::string &k, bool down, size_t remaining) {
            std::lock_guard<std::mutex> lock(mtx);
            events.push_back({k, down, remaining});
        },
        [&]() {
            doneFired.store(true);
        },
        300, // 300ms total interval per key
        40   // 40ms hold -> 260ms release window
    );

    size_t count = 0;
    std::string err;
    CHECK(seq.enqueue({"ADRS", "1", "2"}, count, err));

    // Wait 100ms so key 1 ("ADRS") finishes its 40ms hold and enters the 260ms release sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(seq.isPunching());

    // Cancel during the release window (when _holdingKey is already false)
    const size_t discarded = seq.cancel();
    LONGS_EQUAL(2, static_cast<long>(discarded));
    CHECK(!seq.isPunching());
    CHECK(doneFired.load());

    std::lock_guard<std::mutex> lock(mtx);
    CHECK(!events.empty());
    CHECK(!events.back().down);
    LONGS_EQUAL(0, static_cast<long>(events.back().remaining));
}

TEST(Abc80AiKeySequencer, PropertyArbitraryKeySequencesValidateAllOrNothing)
{
    const bool ok = rc::check("arbitrary key sequences validate all-or-nothing",
                              [](const std::vector<std::string> &keys) {
        AiKeySequencer seq([](const std::string &, bool, size_t) {}, nullptr, 20, 5);
        bool allValid = !keys.empty();
        for (const auto &k : keys) {
            uint8_t code = 0;
            if (k != "RST" && !keyCodeFromName(k, code)) {
                allValid = false;
                break;
            }
        }
        size_t enqueued = 0;
        std::string err;
        const bool accepted = seq.enqueue(keys, enqueued, err);
        if (allValid) {
            RC_ASSERT(accepted);
            RC_ASSERT(enqueued == keys.size());
            seq.cancel();
        } else {
            RC_ASSERT(!accepted);
            RC_ASSERT(enqueued == 0);
            RC_ASSERT(!err.empty());
            RC_ASSERT(!seq.isPunching());
        }
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

