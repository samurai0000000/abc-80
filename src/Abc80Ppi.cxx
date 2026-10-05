/*
 * Abc80Ppi.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#define CHIPS_IMPL
#include <chips/i8255.h>
#include <abc80/Abc80Ppi.hxx>
#include <algorithm>

namespace abc80 {

// Lookup table mapping internal key code (0x00 - 0x16) to 6x6 matrix position (0 - 35)
// Derived from vintage/rom.abc keytab (0x078C)
static constexpr uint8_t KEYCODE_TO_MATRIX_POS[] = {
    27, // 0x00: '0'       (Col 4, Row 3)
    21, // 0x01: '1'       (Col 3, Row 3)
    15, // 0x02: '2'       (Col 2, Row 3)
     9, // 0x03: '3'       (Col 1, Row 3)
    26, // 0x04: '4'       (Col 4, Row 2)
    20, // 0x05: '5'       (Col 3, Row 2)
    14, // 0x06: '6'       (Col 2, Row 2)
     8, // 0x07: '7'       (Col 1, Row 2)
    25, // 0x08: '8'       (Col 4, Row 1)
    19, // 0x09: '9'       (Col 3, Row 1)
    13, // 0x0A: 'a'       (Col 2, Row 1)
     7, // 0x0B: 'b'       (Col 1, Row 1)
    24, // 0x0C: 'c'       (Col 4, Row 0)
    18, // 0x0D: 'd'       (Col 3, Row 0)
    12, // 0x0E: 'e'       (Col 2, Row 0)
     6, // 0x0F: 'f'       (Col 1, Row 0)
     3, // 0x10: 'inc'     (Col 0, Row 3) '+'
     2, // 0x11: 'dec'     (Col 0, Row 2) '-'
    33, // 0x12: 'exec'    (Col 5, Row 3)
     1, // 0x13: 'data'    (Col 0, Row 1)
     0, // 0x14: 'adrs'    (Col 0, Row 0)
    31, // 0x15: 'to tape' (Col 5, Row 1)
    32, // 0x16: 'from tape' (Col 5, Row 2)
};

Abc80Ppi::Abc80Ppi()
    : _segmentOutput(0x00)
    , _digitStrobe(0xFF)
    , _tapeInputLevel(false)
    , _speakerBit(false)
    , _hostLinkPortBIn(0x00)
    , _hostLinkPortCOut(0x00)
{
    _digitSegments.fill(0x00);
    _matrixKeys.fill(false);

    reset();
}

void Abc80Ppi::reset()
{
    i8255_init(&_ppiPrimary);
    i8255_init(&_ppiAux);
    i8255_init(&_ppiLink);

    _segmentOutput = 0x00;
    _digitStrobe = 0xFF;
    _digitSegments.fill(0x00);
    _tapeInputLevel = false;
    _speakerBit = false;
    _matrixKeys.fill(false);
    _hostLinkPortBIn = 0x00;
    _hostLinkPortCOut = 0x00;
}

uint8_t Abc80Ppi::computeKeypadRowInputs() const
{
    // Key rows are active-low: 1 = unpressed, 0 = pressed
    uint8_t rowBits = 0x7F; // Bits 0-5 active, bit 6 pulled high (1), bit 7 = tape

    for (uint8_t col = 0; col < 6; ++col) {
        // If column strobe bit is 0 (active-low strobe for this column)
        if ((_digitStrobe & (1 << col)) == 0) {
            for (uint8_t row = 0; row < 6; ++row) {
                uint8_t pos = static_cast<uint8_t>(col * 6 + row);
                if (pos < _matrixKeys.size() && _matrixKeys[pos]) {
                    rowBits &= static_cast<uint8_t>(~(1 << row));
                }
            }
        }
    }

    if (_tapeInputLevel) {
        rowBits |= 0x80;
    } else {
        rowBits &= 0x7F;
    }

    return rowBits;
}

void Abc80Ppi::updateDigitSegments(uint8_t digitStrobe, uint8_t segmentMask)
{
    _digitStrobe = digitStrobe;
    _speakerBit = ((digitStrobe & 0x80) != 0) || ((digitStrobe & 0x20) != 0);

    for (uint8_t digit = 0; digit < 6; ++digit) {
        if ((digitStrobe & (1 << digit)) == 0) {
            _digitSegments[digit] = segmentMask;
        }
    }
}

uint8_t Abc80Ppi::readPort(uint8_t port)
{
    if ((port & 0xFC) == 0x80) { // Primary PPI: 0x80 - 0x83
        uint8_t reg = port & 0x03;
        uint64_t pins = I8255_CS | I8255_RD;
        if (reg & 1) pins |= I8255_A0;
        if (reg & 2) pins |= I8255_A1;

        if (reg == 0) { // Port A (Keypad / TapeIn)
            uint8_t paData = computeKeypadRowInputs();
            I8255_SET_PA(pins, paData);
        }

        pins = i8255_tick(&_ppiPrimary, pins);
        return I8255_GET_DATA(pins);
    } else if ((port & 0xFC) == 0x40) { // Aux PPI (EPROM Programmer): 0x40 - 0x43
        uint8_t reg = port & 0x03;
        uint64_t pins = I8255_CS | I8255_RD;
        if (reg & 1) pins |= I8255_A0;
        if (reg & 2) pins |= I8255_A1;

        pins = i8255_tick(&_ppiAux, pins);
        return I8255_GET_DATA(pins);
    } else if ((port & 0xFC) == 0xC0) { // Parallel Link PPI: 0xC0 - 0xC3
        uint8_t reg = port & 0x03;
        uint64_t pins = I8255_CS | I8255_RD;
        if (reg & 1) pins |= I8255_A0;
        if (reg & 2) pins |= I8255_A1;

        if (reg == 1) { // Port B (Host Data In)
            I8255_SET_PB(pins, _hostLinkPortBIn);
        }

        pins = i8255_tick(&_ppiLink, pins);
        return I8255_GET_DATA(pins);
    }

    return 0xFF; // Floating bus on unmapped port
}

void Abc80Ppi::writePort(uint8_t port, uint8_t val)
{
    if ((port & 0xFC) == 0x80) { // Primary PPI: 0x80 - 0x83
        uint8_t reg = port & 0x03;
        uint64_t pins = I8255_CS | I8255_WR;
        if (reg & 1) pins |= I8255_A0;
        if (reg & 2) pins |= I8255_A1;
        I8255_SET_DATA(pins, val);

        i8255_tick(&_ppiPrimary, pins);

        if (reg == 1) { // Port B (Segments)
            _segmentOutput = val;
            updateDigitSegments(_digitStrobe, _segmentOutput);
        } else if (reg == 2) { // Port C (Digit strobes & speaker)
            updateDigitSegments(val, _segmentOutput);
        }
    } else if ((port & 0xFC) == 0x40) { // Aux PPI: 0x40 - 0x43
        uint8_t reg = port & 0x03;
        uint64_t pins = I8255_CS | I8255_WR;
        if (reg & 1) pins |= I8255_A0;
        if (reg & 2) pins |= I8255_A1;
        I8255_SET_DATA(pins, val);

        i8255_tick(&_ppiAux, pins);
    } else if ((port & 0xFC) == 0xC0) { // Parallel Link PPI: 0xC0 - 0xC3
        uint8_t reg = port & 0x03;
        uint64_t pins = I8255_CS | I8255_WR;
        if (reg & 1) pins |= I8255_A0;
        if (reg & 2) pins |= I8255_A1;
        I8255_SET_DATA(pins, val);

        i8255_tick(&_ppiLink, pins);
    }
}

void Abc80Ppi::setKeyPressed(uint8_t keyCode, bool pressed)
{
    if (keyCode < sizeof(KEYCODE_TO_MATRIX_POS)) {
        uint8_t pos = KEYCODE_TO_MATRIX_POS[keyCode];
        if (pos < _matrixKeys.size()) {
            _matrixKeys[pos] = pressed;
        }
    }
}

void Abc80Ppi::clearKeys()
{
    _matrixKeys.fill(false);
}

bool Abc80Ppi::isKeyPressed(uint8_t keyCode) const
{
    if (keyCode < sizeof(KEYCODE_TO_MATRIX_POS)) {
        uint8_t pos = KEYCODE_TO_MATRIX_POS[keyCode];
        if (pos < _matrixKeys.size()) {
            return _matrixKeys[pos];
        }
    }
    return false;
}

uint8_t Abc80Ppi::getDigitSegment(uint8_t digitIndex) const
{
    if (digitIndex < _digitSegments.size()) {
        return _digitSegments[digitIndex];
    }
    return 0x00;
}

void Abc80Ppi::setTapeInput(bool level)
{
    _tapeInputLevel = level;
}

void Abc80Ppi::hostWriteLinkPortB(uint8_t data)
{
    _hostLinkPortBIn = data;
}

uint8_t Abc80Ppi::hostReadLinkPortA() const
{
    return _ppiLink.pa.outp;
}

void Abc80Ppi::hostWriteLinkPortC(uint8_t val)
{
    _hostLinkPortCOut = val;
}

uint8_t Abc80Ppi::hostReadLinkPortC() const
{
    return _ppiLink.pc.outp;
}

uint8_t Abc80Ppi::getPrimaryControl() const noexcept
{
    return _ppiPrimary.control;
}

uint8_t Abc80Ppi::getLinkControl() const noexcept
{
    return _ppiLink.control;
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
