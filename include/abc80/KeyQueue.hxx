/*
 * KeyQueue.hxx
 *
 * Turns client key presses into emulated-time key actions that respect the ROM's key
 * timing (hold, press spacing, release gap; plan Section 2.4.4). Pure logic, no board dependency.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_KEY_QUEUE_HXX
#define ABC80_KEY_QUEUE_HXX

#include <abc80/Abc80Rom.hxx>
#include <array>
#include <cstddef>
#include <cstdint>

namespace abc80 {

class KeyQueue {
public:
    static constexpr size_t kMaxQueued = 16;

    struct Action {
        uint8_t keyCode;
        bool down;
    };

    explicit KeyQueue(const KeyProfile &profile) noexcept;

    // Queues a press. Returns false (newest dropped) when kMaxQueued presses are already waiting.
    bool press(uint8_t keyCode) noexcept;

    // The client let go of the key. Never refused; ignored if the key has no unreleased press.
    void release(uint8_t keyCode) noexcept;

    // Reset or ROM switch: drops queued presses and releases a held key on the next tick.
    void clear() noexcept;

    // Call once per frame, before the frame is stepped. Writes the actions to apply this
    // frame (at most two) into out and returns how many. cap must be at least 2.
    size_t tick(Action *out, size_t cap) noexcept;

private:
    struct Entry {
        uint8_t key;
        bool released;
    };

    KeyProfile _profile;
    std::array<Entry, kMaxQueued> _queue;
    size_t _head;
    size_t _count;
    bool _down;
    Entry _current;
    bool _forceRelease;
    int64_t _frame;
    int64_t _downSince;
    int64_t _lastPressStart;
    int64_t _lastRelease;
};

} // namespace abc80

#endif /* ABC80_KEY_QUEUE_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
