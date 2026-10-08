/*
 * Abc80FrameTestSupport.hxx
 *
 * Helpers shared by the Envelope 1 tests: boot a ROM to its steady state,
 * run frames, and drive keys through the KeyQueue.
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_FRAME_TEST_SUPPORT_HXX
#define ABC80_FRAME_TEST_SUPPORT_HXX

#include <abc80/Abc80Board.hxx>
#include <abc80/KeyQueue.hxx>
#include <cstdint>
#include <CppUTest/TestHarness.h>

namespace abc80test {

using namespace abc80;

inline void runFrames(Abc80Board &board, int frames)
{
    for (int i = 0; i < frames; ++i) {
        board.stepFrame();
    }
}

// Boots warm to the keypad monitor and settles it in the scan loop.
// Monitor: runs unpaced to main. 1993: runs unpaced to the link poll, then enters the
// monitor at 0x009B the way the PC-link host does.
inline void bootToMonitor(Abc80Board &board, RomId id)
{
    LONGS_EQUAL(static_cast<int>(Abc80Status::OK), static_cast<int>(board.powerOnRom(id, ".")));
    CHECK(board.bootTurbo(id, 60000000));
    if (id == RomId::Vintage1993) {
        board.getCpu().prefetch(0x009B);
    }
    runFrames(board, 10);
}

inline uint16_t adsave(const Abc80Board &board)
{
    return static_cast<uint16_t>(board.getBus().readByte(ABC80_ADSAVE_ADDR) |
                                 (board.getBus().readByte(ABC80_ADSAVE_ADDR + 1) << 8));
}

// Feeds a KeyQueue into the board one frame at a time.
class KeyDriver {
public:
    KeyDriver(Abc80Board &board, const KeyProfile &profile) : _board(board), _queue(profile) {}

    KeyQueue &queue() { return _queue; }

    void frame()
    {
        KeyQueue::Action actions[4];
        const size_t n = _queue.tick(actions, 4);
        for (size_t i = 0; i < n; ++i) {
            if (actions[i].down) {
                _board.pressKey(actions[i].keyCode);
            } else {
                _board.releaseKey(actions[i].keyCode);
            }
        }
        _board.stepFrame();
    }

    void frames(int n)
    {
        for (int i = 0; i < n; ++i) frame();
    }

    // Types a key like a user: press, release, then wait for the ROM to be ready again.
    void type(uint8_t key)
    {
        CHECK(_queue.press(key));
        _queue.release(key);
        frames(1);
    }

private:
    Abc80Board &_board;
    KeyQueue _queue;
};

} // namespace abc80test

#endif /* ABC80_FRAME_TEST_SUPPORT_HXX */
