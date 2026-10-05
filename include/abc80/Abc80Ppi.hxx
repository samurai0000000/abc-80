/*
 * Abc80Ppi.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_PPI_HXX
#define ABC80_PPI_HXX

#include <abc80/Abc80Types.hxx>
#include <array>
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
