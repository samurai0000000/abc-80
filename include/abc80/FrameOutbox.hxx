/*
 * FrameOutbox.hxx
 *
 * One-slot, latest-frame-wins mailbox between the 60 Hz worker (producer) and a connection's
 * sender thread (consumer). The producer never blocks and never queues: a slow client only
 * misses frames.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_FRAME_OUTBOX_HXX
#define ABC80_FRAME_OUTBOX_HXX

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace abc80 {

class FrameOutbox {
public:
    static constexpr size_t kMaxQueuedEvents = 64;

    // Replaces any unsent telemetry frame. Ignored after close().
    void post(const std::string &frame)
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (_closed) {
                return;
            }
            _frame = frame;
            _has = true;
        }
        _cv.notify_one();
    }

    // Queues a control/error/reply event so back-to-back responses are not overwritten by telemetry.
    void postEvent(const std::string &event)
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (_closed) {
                return;
            }
            if (_events.size() >= kMaxQueuedEvents) {
                _events.pop_front();
            }
            _events.push_back(event);
        }
        _cv.notify_one();
    }

    // Waits up to timeoutMs for a frame. Returns true and moves the frame into out;
    // false on timeout or after close() (check closed()).
    bool wait(std::string &out, unsigned timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                     [this] { return !_events.empty() || _has || _closed; });
        if (_closed) {
            return false;
        }
        if (!_events.empty()) {
            out = std::move(_events.front());
            _events.pop_front();
            return true;
        }
        if (!_has) {
            return false;
        }
        out = std::move(_frame);
        _frame.clear();
        _has = false;
        return true;
    }

    void close()
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _closed = true;
        }
        _cv.notify_all();
    }

    bool closed() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _closed;
    }

private:
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::deque<std::string> _events;
    std::string _frame;
    bool _has{false};
    bool _closed{false};
};

} // namespace abc80

#endif /* ABC80_FRAME_OUTBOX_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
