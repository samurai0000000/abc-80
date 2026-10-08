/*
 * Abc80Ppi.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_PPI_HXX
#define ABC80_PPI_HXX

#include <abc80/Abc80Types.hxx>
#include <array>
#include <cstddef>
#include <cstdint>

#include <chips/i8255.h>

namespace abc80 {

class Abc80Ppi {
public:
    Abc80Ppi();
    ~Abc80Ppi() = default;

    // Reset all PPI instances to initial power-on state
    void reset();

    // Z80 I/O Port operations (invoked via IN/OUT instructions)
    uint8_t readPort(uint8_t port);
    void writePort(uint8_t port, uint8_t val);

    // Keypad interface: 24-key matrix (codes 0x00 - 0x16)
    void setKeyPressed(uint8_t keyCode, bool pressed);
    void pressKey(uint8_t keyCode) { setKeyPressed(keyCode, true); }
    void releaseKey(uint8_t keyCode) { setKeyPressed(keyCode, false); }
    void clearKeys();
    bool isKeyPressed(uint8_t keyCode) const;

    // Display state: 6-digit 7-segment LED multiplexer
    uint8_t getSegmentOutput() const noexcept { return _segmentOutput; }
    uint8_t getDigitStrobe() const noexcept { return _digitStrobe; }
    uint8_t getDigitSegment(uint8_t digitIndex) const;

    // Audio cassette / speaker bit access
    void setTapeInput(bool level);
    bool getTapeInput() const noexcept { return _tapeInputLevel; }
    bool getSpeakerBit() const noexcept { return _speakerBit; }

    // ---- Per-frame observation (display persistence and speaker edges), in T-states ----
    // Everything below is timestamped with the clock source (the CPU's T-state counter).
    // Without a clock source the accumulators stay empty.
    struct FrameAudio {
        static constexpr size_t kMaxEdges = 1024;
        bool startLevel{false};                       // Port C bit 7 at beginFrame()
        uint16_t edgeCount{0};                        // toggles of Port C bit 7 in the frame (<= kMaxEdges)
        bool overflow{false};                         // more toggles than kMaxEdges happened; extras are dropped
        std::array<uint32_t, kMaxEdges> offsets{};    // T-states since beginFrame() of each logged toggle
        uint32_t lowTStates{0};                       // time Port C bit 7 spent low
    };

    void setClockSource(const uint64_t *tstates) noexcept { _clock = tstates; }
    void beginFrame() noexcept;   // zero the accumulators and mark the frame start
    void endFrame() noexcept;     // account the time up to now; freezes frameElapsedTStates()
    const FrameAudio &frameAudio() const noexcept { return _audio; }
    uint32_t frameElapsedTStates() const noexcept { return _frameElapsed; }

    // T-states in the frame that digit (0 = rightmost .. 5 = leftmost) was strobed while canonical
    // segment (0=a .. 6=g, 7=dp) was driven lit. Port B is active-low, so a low bit is a lit segment.
    uint32_t digitSegmentOnTStates(uint8_t digit, uint8_t canonicalSegment) const noexcept;

    // Latched Port C output of the auxiliary (EPROM programmer) 8255.
    uint8_t auxPortCOutput() const noexcept { return _ppiAux.pc.outp; }

    // Host PC-5523 parallel link interface (ports 0xC0 - 0xC3)
    void hostWriteLinkPortB(uint8_t data);
    uint8_t hostReadLinkPortA() const;
    void hostWriteLinkPortC(uint8_t val);
    uint8_t hostReadLinkPortC() const;

    // Direct access to primary PPI control word
    uint8_t getPrimaryControl() const noexcept;
    uint8_t getLinkControl() const noexcept;

private:
    uint8_t computeKeypadRowInputs() const;
    void updateDigitSegments(uint8_t digitStrobe, uint8_t segmentMask);
    void accumulate() noexcept;  // account time since the last event to the current port state

    // Three 8255 PPI instances:
    // 1. Primary: Keypad, 7-Segment display, Cassette I/O, Speaker (0x80 - 0x83)
    // 2. Aux: 2716/2732 EPROM Programmer accessory (0x40 - 0x43)
    // 3. Parallel Link: PC-Link server communication (0xC0 - 0xC3)
    i8255_t _ppiPrimary;
    i8255_t _ppiAux;
    i8255_t _ppiLink;

    // Display latch state: 6 digits (0 = rightmost/data, 5 = leftmost/address)
    uint8_t _segmentOutput;
    uint8_t _digitStrobe;
    std::array<uint8_t, 6> _digitSegments;

    // Audio & Keypad state
    const uint64_t *_clock{nullptr};
    uint64_t _frameStart{0};
    uint64_t _lastEvent{0};
    uint32_t _frameElapsed{0};
    std::array<std::array<uint32_t, 8>, 6> _segOn{};
    FrameAudio _audio;

    bool _tapeInputLevel;
    bool _speakerBit;
    std::array<bool, 36> _matrixKeys; // 6 columns x 6 rows matrix positions

    // Parallel Link host buffers
    uint8_t _hostLinkPortBIn; // Sent from host to ABC-80 Port B
    uint8_t _hostLinkPortCOut;
};

} // namespace abc80

#endif // ABC80_PPI_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
