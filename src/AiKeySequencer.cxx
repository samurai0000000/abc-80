/*
 * AiKeySequencer.cxx
 *
 * Paced keypad sequencer for AI agents (plan Section 2.3.3).
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/AiKeySequencer.hxx>
#include <abc80/WebProtocol.hxx>

namespace abc80 {

AiKeySequencer::AiKeySequencer(PunchCallback onPunch, DoneCallback onDone,
                               unsigned intervalMs, unsigned holdMs)
    : _onPunch(std::move(onPunch)),
      _onDone(std::move(onDone)),
      _intervalMs(intervalMs),
      _holdMs(holdMs)
{
    if (_holdMs >= _intervalMs) {
        _holdMs = _intervalMs / 2;
    }
    _worker = std::thread(&AiKeySequencer::workerLoop, this);
}

AiKeySequencer::~AiKeySequencer()
{
    stop();
}

void AiKeySequencer::stop()
{
    if (!_running.exchange(false)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(_mtx);
        ++_generation;
        _queue.clear();
        _isHolding = false;
        _isPunching = false;
        _cv.notify_all();
        _cvDone.notify_all();
    }
    if (_worker.joinable()) {
        _worker.join();
    }
}

bool AiKeySequencer::enqueue(const std::vector<std::string> &keys, size_t &enqueuedCount, std::string &error)
{
    enqueuedCount = 0;
    if (keys.empty()) {
        error = "key list cannot be empty";
        return false;
    }

    // Validate all keys first
    for (const auto &k : keys) {
        uint8_t code = 0;
        if (k != "RST" && !keyCodeFromName(k, code)) {
            error = "unknown key name: " + k;
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(_mtx);
        for (const auto &k : keys) {
            _queue.push_back(k);
        }
        enqueuedCount = keys.size();
        _isPunching = true;
        _lastCancelled = false;
        _cv.notify_all();
    }
    return true;
}

size_t AiKeySequencer::cancel()
{
    size_t discarded = 0;
    std::string keyToRelease;
    bool wasPunching = false;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        discarded = _queue.size();
        _queue.clear();
        ++_generation;
        wasPunching = _isPunching;
        keyToRelease = !_currentKey.empty() ? _currentKey : _lastKey;
        _isHolding = false;
        _isPunching = false;
        _lastCancelled = true;
        _currentKey.clear();
        _cv.notify_all();
        _cvDone.notify_all();
    }

    if (wasPunching && _onPunch && !keyToRelease.empty()) {
        _onPunch(keyToRelease, false, 0);
    }

    if (_onDone) {
        _onDone();
    }

    return discarded;
}

bool AiKeySequencer::isPunching() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _isPunching || !_queue.empty();
}

bool AiKeySequencer::wasCancelled() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _lastCancelled;
}

size_t AiKeySequencer::queueDepth() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _queue.size();
}

std::string AiKeySequencer::currentKey() const
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _currentKey;
}

bool AiKeySequencer::waitForCompletion(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(_mtx);
    return _cvDone.wait_for(lock, timeout, [this] {
        return !_isPunching && _queue.empty();
    });
}

void AiKeySequencer::workerLoop()
{
    while (_running.load()) {
        std::string key;
        size_t remaining = 0;
        uint64_t gen = 0;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cv.wait(lock, [this] {
                return !_running.load() || !_queue.empty();
            });

            if (!_running.load()) {
                break;
            }

            if (_queue.empty()) {
                continue;
            }

            gen = _generation;
            key = _queue.front();
            _queue.pop_front();
            _currentKey = key;
            _lastKey = key;
            _isHolding = true;
            _isPunching = true;
            remaining = _queue.size();
        }

        // 1. Dispatch Key Down
        if (_onPunch) {
            _onPunch(key, true, remaining);
        }

        // 2. Hold Duration Sleep (interruptible on cancel)
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cv.wait_for(lock, std::chrono::milliseconds(_holdMs), [this, gen] {
                return !_running.load() || _generation != gen;
            });
            if (!_running.load() || _generation != gen) {
                continue;
            }
            _isHolding = false;
        }

        // 3. Dispatch Key Up
        if (_onPunch) {
            _onPunch(key, false, remaining);
        }

        // 4. Release Delay Sleep (interruptible on cancel)
        const unsigned releaseWaitMs = (_intervalMs > _holdMs) ? (_intervalMs - _holdMs) : 50;
        bool done = false;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cv.wait_for(lock, std::chrono::milliseconds(releaseWaitMs), [this, gen] {
                return !_running.load() || _generation != gen;
            });
            if (!_running.load() || _generation != gen) {
                continue;
            }

            if (_queue.empty()) {
                _isPunching = false;
                _currentKey.clear();
                _cvDone.notify_all();
                done = true;
            }
        }

        // 5. If queue became empty, invoke onDone
        if (done && _onDone) {
            _onDone();
        }
    }
}

} // namespace abc80

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
