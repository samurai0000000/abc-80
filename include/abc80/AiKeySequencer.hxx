/*
 * AiKeySequencer.hxx
 *
 * Paced keypad sequencer for AI agents (plan Section 2.3.3).
 * Buffers keys and dispatches them at 0.5s cadence (120ms hold, 380ms release),
 * with cancellation and synchronous completion waiting.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef AI_KEY_SEQUENCER_HXX
#define AI_KEY_SEQUENCER_HXX

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace abc80 {

class AiKeySequencer {
public:
    using PunchCallback = std::function<void(const std::string &key, bool down, size_t remaining)>;
    using DoneCallback = std::function<void()>;

    explicit AiKeySequencer(PunchCallback onPunch, DoneCallback onDone = nullptr,
                            unsigned intervalMs = 500, unsigned holdMs = 120);
    ~AiKeySequencer();
    AiKeySequencer(const AiKeySequencer &) = delete;
    AiKeySequencer &operator=(const AiKeySequencer &) = delete;

    bool enqueue(const std::vector<std::string> &keys, size_t &enqueuedCount, std::string &error);
    size_t cancel();
    bool isPunching() const;
    bool wasCancelled() const;
    size_t queueDepth() const;
    std::string currentKey() const;
    bool waitForCompletion(std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    void stop();

private:
    void workerLoop();

    PunchCallback _onPunch;
    DoneCallback _onDone;
    unsigned _intervalMs{500};
    unsigned _holdMs{120};

    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::condition_variable _cvDone;

    std::deque<std::string> _queue;
    std::string _currentKey;
    std::string _lastKey;
    bool _isPunching{false};
    bool _isHolding{false};
    bool _lastCancelled{false};
    uint64_t _generation{0};
    std::atomic<bool> _running{true};
    std::thread _worker;
};

} // namespace abc80

#endif /* AI_KEY_SEQUENCER_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
