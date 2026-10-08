/*
 * KeyQueue.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/KeyQueue.hxx>

namespace abc80 {

namespace {
constexpr int64_t kLongAgo = -(static_cast<int64_t>(1) << 40);
}

KeyQueue::KeyQueue(const KeyProfile &profile) noexcept
    : _profile(profile)
    , _queue()
    , _head(0)
    , _count(0)
    , _down(false)
    , _current{ 0, false }
    , _forceRelease(false)
    , _frame(0)
    , _downSince(kLongAgo)
    , _lastPressStart(kLongAgo)
    , _lastRelease(kLongAgo)
{
}

bool KeyQueue::press(uint8_t keyCode) noexcept
{
    if (_count >= kMaxQueued) {
        return false;
    }
    _queue[(_head + _count) % kMaxQueued] = Entry{ keyCode, false };
    ++_count;
    return true;
}

void KeyQueue::release(uint8_t keyCode) noexcept
{
    // The oldest unreleased press of this key: the one being held, then the queued ones in order.
    if (_down && _current.key == keyCode && !_current.released) {
        _current.released = true;
        return;
    }
    for (size_t i = 0; i < _count; ++i) {
        Entry &e = _queue[(_head + i) % kMaxQueued];
        if (e.key == keyCode && !e.released) {
            e.released = true;
            return;
        }
    }
}

void KeyQueue::clear() noexcept
{
    _head = 0;
    _count = 0;
    if (_down) {
        _forceRelease = true;
    }
    _lastPressStart = kLongAgo;
    _lastRelease = kLongAgo;
}

size_t KeyQueue::tick(Action *out, size_t cap) noexcept
{
    if (out == nullptr || cap < 2) {
        return 0;
    }
    size_t n = 0;

    if (_down) {
        const bool heldLongEnough = (_frame - _downSince) >= static_cast<int64_t>(_profile.holdFrames);
        if (_forceRelease || (_current.released && heldLongEnough)) {
            out[n++] = Action{ _current.key, false };
            _down = false;
            _forceRelease = false;
            _lastRelease = _frame;
        }
    }

    if (!_down && _count > 0) {
        const bool spacingOk = (_frame - _lastPressStart) >= static_cast<int64_t>(_profile.pressSpacingFrames);
        const bool gapOk = (_frame - _lastRelease) >= static_cast<int64_t>(_profile.releaseGapFrames);
        if (spacingOk && gapOk) {
            _current = _queue[_head];
            _head = (_head + 1) % kMaxQueued;
            --_count;
            _down = true;
            _downSince = _frame;
            _lastPressStart = _frame;
            out[n++] = Action{ _current.key, true };
        }
    }

    ++_frame;
    return n;
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
