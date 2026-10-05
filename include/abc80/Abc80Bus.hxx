/*
 * Abc80Bus.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_BUS_HXX
#define ABC80_BUS_HXX

#include <abc80/Abc80Types.hxx>
#include <array>
#include <cstdint>
#include <cstddef>

namespace abc80 {

class Abc80Bus {
public:
    Abc80Bus();
    ~Abc80Bus() = default;

    // Reset RAM and bus state
    void reset();

    // Standard memory bus read and write operations
    uint8_t readByte(uint16_t addr) const;
    void writeByte(uint16_t addr, uint8_t val);

    // 16-bit word operations (Little-Endian)
    uint16_t readWord(uint16_t addr) const;
    void writeWord(uint16_t addr, uint16_t val);

    // ROM loading methods
    Abc80Status loadRom0(const uint8_t *data, size_t size);
    Abc80Status loadRom1(const uint8_t *data, size_t size);
    Abc80Status loadRom(const uint8_t *data, size_t size);
    Abc80Status loadRomFile(const char *filepath);
    Abc80Status loadAbcFormat(const char *text, size_t length);

    // Direct buffer accessors (for testing, inspection, and DMA simulation)
    const uint8_t *getRom0Data() const noexcept { return _rom0.data(); }
    const uint8_t *getRom1Data() const noexcept { return _rom1.data(); }
    const uint8_t *getRamData() const noexcept { return _ram.data(); }
    uint8_t *getRamData() noexcept { return _ram.data(); }

    // Memory classification helpers
    static constexpr bool isRom(uint16_t addr) noexcept {
        return addr <= ABC80_ROM1_END;
    }

    static constexpr bool isRam(uint16_t addr) noexcept {
        return addr >= ABC80_RAM_BASE;
    }

private:
    std::array<uint8_t, ABC80_ROM0_SIZE> _rom0; // 2KB Monitor ROM (0x0000 - 0x07FF, U2)
    std::array<uint8_t, ABC80_ROM1_SIZE> _rom1; // 2KB Expansion ROM (0x0800 - 0x0FFF, U3)
    std::array<uint8_t, ABC80_RAM_SIZE>  _ram;  // 60KB Contiguous RAM (0x1000 - 0xFFFF)
};

} // namespace abc80

#endif // ABC80_BUS_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
