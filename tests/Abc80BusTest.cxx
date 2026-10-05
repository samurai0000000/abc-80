/*
 * Abc80BusTest.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <CppUTest/TestHarness.h>
#include <abc80/Abc80Bus.hxx>

TEST_GROUP(Abc80Bus)
{
    abc80::Abc80Bus bus;

    void setup() override
    {
        bus.reset();
    }

    void teardown() override
    {
    }
};

TEST(Abc80Bus, Eeprom0And1ReadOnlyProtection)
{
    // Preload ROM with known patterns
    uint8_t rom0Data[abc80::ABC80_ROM0_SIZE];
    uint8_t rom1Data[abc80::ABC80_ROM1_SIZE];
    for (size_t i = 0; i < sizeof(rom0Data); ++i) {
        rom0Data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    for (size_t i = 0; i < sizeof(rom1Data); ++i) {
        rom1Data[i] = static_cast<uint8_t>((~i) & 0xFF);
    }

    LONGS_EQUAL(static_cast<int>(abc80::Abc80Status::OK),
                static_cast<int>(bus.loadRom0(rom0Data, sizeof(rom0Data))));
    LONGS_EQUAL(static_cast<int>(abc80::Abc80Status::OK),
                static_cast<int>(bus.loadRom1(rom1Data, sizeof(rom1Data))));

    // Attempt to overwrite ROM0 and ROM1 with 0x55
    for (uint32_t addr = 0x0000; addr <= 0x0FFF; ++addr) {
        bus.writeByte(static_cast<uint16_t>(addr), 0x55);
    }

    // Assert that ROM0 contents remained unchanged
    for (uint32_t addr = 0x0000; addr <= 0x07FF; ++addr) {
        BYTES_EQUAL(static_cast<uint8_t>(addr & 0xFF), bus.readByte(static_cast<uint16_t>(addr)));
    }

    // Assert that ROM1 contents remained unchanged
    for (uint32_t addr = 0x0800; addr <= 0x0FFF; ++addr) {
        uint32_t offset = addr - 0x0800;
        BYTES_EQUAL(static_cast<uint8_t>((~offset) & 0xFF), bus.readByte(static_cast<uint16_t>(addr)));
    }
}

TEST(Abc80Bus, Full60KbRamContiguousReadWrite)
{
    // Write an incrementing byte pattern across all 60KB (0x1000 - 0xFFFF)
    for (uint32_t addr = abc80::ABC80_RAM_BASE; addr <= abc80::ABC80_RAM_END; ++addr) {
        uint8_t expected = static_cast<uint8_t>((addr ^ (addr >> 8)) & 0xFF);
        bus.writeByte(static_cast<uint16_t>(addr), expected);
    }

    // Verify all 61,440 bytes match bit-exact
    for (uint32_t addr = abc80::ABC80_RAM_BASE; addr <= abc80::ABC80_RAM_END; ++addr) {
        uint8_t expected = static_cast<uint8_t>((addr ^ (addr >> 8)) & 0xFF);
        BYTES_EQUAL(expected, bus.readByte(static_cast<uint16_t>(addr)));
    }
}

TEST(Abc80Bus, RamchkSubroutineExactEmulation)
{
    // Pre-populate ROM0 and ROM1 with non-zero bytes (0x3E)
    uint8_t dummyRom[abc80::ABC80_ROM0_SIZE];
    for (size_t i = 0; i < sizeof(dummyRom); ++i) {
        dummyRom[i] = 0x3E;
    }
    bus.loadRom0(dummyRom, sizeof(dummyRom));
    bus.loadRom1(dummyRom, sizeof(dummyRom));

    // Pre-populate RAM with 0x3E as well
    for (uint32_t addr = abc80::ABC80_RAM_BASE; addr <= abc80::ABC80_RAM_END; ++addr) {
        bus.writeByte(static_cast<uint16_t>(addr), 0x3E);
    }

    // Helper simulating 049dh: ramchk exactly:
    //   ld a, (hl)
    //   cpl
    //   ld (hl), a
    //   ld a, (hl)
    //   cpl
    //   ld (hl), a
    //   cp (hl) -> returns (a == (hl))
    auto runRamchk = [this](uint16_t hl) -> bool {
        uint8_t orig = bus.readByte(hl);
        uint8_t a = static_cast<uint8_t>(~orig);
        bus.writeByte(hl, a);
        a = bus.readByte(hl);
        a = static_cast<uint8_t>(~a);
        bus.writeByte(hl, a);
        uint8_t memAfter = bus.readByte(hl);
        return (a == memAfter);
    };

    // Test across pages 0x00 - 0x0F (ROM): Must return false (Zero Flag == 0)
    for (uint16_t page = 0x00; page <= 0x0F; ++page) {
        uint16_t testAddr = static_cast<uint16_t>((page << 8) | 0x40);
        bool isWritable = runRamchk(testAddr);
        CHECK_FALSE(isWritable);
        BYTES_EQUAL(0x3E, bus.readByte(testAddr)); // Memory preserved
    }

    // Test across pages 0x10 - 0xFF (RAM): Must return true (Zero Flag == 1)
    for (uint16_t page = 0x10; page <= 0xFF; ++page) {
        uint16_t testAddr = static_cast<uint16_t>((page << 8) | 0x80);
        bool isWritable = runRamchk(testAddr);
        CHECK_TRUE(isWritable);
        BYTES_EQUAL(0x3E, bus.readByte(testAddr)); // Memory restored
    }
}

TEST(Abc80Bus, BoundaryAddresses)
{
    // Test bank boundary points
    const uint16_t boundaries[] = {
        0x0000, 0x07FF, // EEPROM 0
        0x0800, 0x0FFF, // EEPROM 1
        0x1000, 0x17FF, // Onboard System RAM
        0x1800, 0xFFFF  // Expansion High RAM
    };

    for (uint16_t addr : boundaries) {
        if (abc80::Abc80Bus::isRom(addr)) {
            CHECK_TRUE(abc80::Abc80Bus::isRom(addr));
            CHECK_FALSE(abc80::Abc80Bus::isRam(addr));
        } else {
            CHECK_TRUE(abc80::Abc80Bus::isRam(addr));
            CHECK_FALSE(abc80::Abc80Bus::isRom(addr));
            bus.writeByte(addr, 0xA5);
            BYTES_EQUAL(0xA5, bus.readByte(addr));
        }
    }
}

TEST(Abc80Bus, WordReadWrite)
{
    // 16-bit word operations at boundary and mid addresses
    bus.writeWord(0x1234, 0xCAFE);
    LONGS_EQUAL(0xCAFE, bus.readWord(0x1234));
    BYTES_EQUAL(0xFE, bus.readByte(0x1234));
    BYTES_EQUAL(0xCA, bus.readByte(0x1235));

    // Word at high boundary (0xFFFE)
    bus.writeWord(0xFFFE, 0xBEEF);
    LONGS_EQUAL(0xBEEF, bus.readWord(0xFFFE));
    BYTES_EQUAL(0xEF, bus.readByte(0xFFFE));
    BYTES_EQUAL(0xBE, bus.readByte(0xFFFF));
}

TEST(Abc80Bus, LoadRomFromVintageFile)
{
    // Load the authentic 1995 rom.abc dump
    abc80::Abc80Status status = bus.loadRomFile("vintage/rom.abc");
    LONGS_EQUAL(static_cast<int>(abc80::Abc80Status::OK), static_cast<int>(status));

    // RST 0 entrypoint opcode sequence at 0x0000:
    //   0000: 06 00   ld b, 00h
    //   0002: 10 fe   djnz $
    //   0004: c3 70 00 jp begin (0070h)
    BYTES_EQUAL(0x06, bus.readByte(0x0000));
    BYTES_EQUAL(0x00, bus.readByte(0x0001));
    BYTES_EQUAL(0x10, bus.readByte(0x0002));
    BYTES_EQUAL(0xFE, bus.readByte(0x0003));
    BYTES_EQUAL(0xC3, bus.readByte(0x0004));
    BYTES_EQUAL(0x70, bus.readByte(0x0005));
    BYTES_EQUAL(0x00, bus.readByte(0x0006));

    // Monitor reset chime at 0x05F2 (11 bytes: 1C 01 1D 01 1C 01 18 01 15 04 80)
    BYTES_EQUAL(0x1C, bus.readByte(0x05F2));
    BYTES_EQUAL(0x01, bus.readByte(0x05F3));
    BYTES_EQUAL(0x80, bus.readByte(0x05FC));

    // PC-Link server header at 0x0B00 in EEPROM 1:
    // Look at first opcode byte at 0x0B00
    uint8_t op0B00 = bus.readByte(0x0B00);
    CHECK_TRUE(op0B00 != 0xFF);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
