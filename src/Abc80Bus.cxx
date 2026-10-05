/*
 * Abc80Bus.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Bus.hxx>
#include <algorithm>
#include <fstream>
#include <vector>

namespace abc80 {

Abc80Bus::Abc80Bus()
{
    _rom0.fill(0xFF);
    _rom1.fill(0xFF);
    _ram.fill(0x00);
}

void Abc80Bus::reset()
{
    _ram.fill(0x00);
}

uint8_t Abc80Bus::readByte(uint16_t addr) const
{
    if (addr <= ABC80_ROM0_END) {
        return _rom0[addr];
    } else if (addr <= ABC80_ROM1_END) {
        return _rom1[addr - ABC80_ROM1_BASE];
    } else {
        return _ram[addr - ABC80_RAM_BASE];
    }
}

void Abc80Bus::writeByte(uint16_t addr, uint8_t val)
{
    if (addr <= ABC80_ROM1_END) {
        // ROM write protection enforced: writes to EEPROM 0 & 1 are discarded
        return;
    }

    _ram[addr - ABC80_RAM_BASE] = val;
}

uint16_t Abc80Bus::readWord(uint16_t addr) const
{
    const uint8_t low = readByte(addr);
    const uint8_t high = readByte(static_cast<uint16_t>(addr + 1));
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
}

void Abc80Bus::writeWord(uint16_t addr, uint16_t val)
{
    writeByte(addr, static_cast<uint8_t>(val & 0xFF));
    writeByte(static_cast<uint16_t>(addr + 1), static_cast<uint8_t>((val >> 8) & 0xFF));
}

Abc80Status Abc80Bus::loadRom0(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return Abc80Status::INVALID_ARG;
    }

    const size_t copySize = std::min(size, _rom0.size());
    std::copy_n(data, copySize, _rom0.begin());
    return Abc80Status::OK;
}

Abc80Status Abc80Bus::loadRom1(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return Abc80Status::INVALID_ARG;
    }

    const size_t copySize = std::min(size, _rom1.size());
    std::copy_n(data, copySize, _rom1.begin());
    return Abc80Status::OK;
}

Abc80Status Abc80Bus::loadRom(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return Abc80Status::INVALID_ARG;
    }

    if (size <= _rom0.size()) {
        return loadRom0(data, size);
    }

    // Load first 2KB into EEPROM 0
    loadRom0(data, _rom0.size());

    // Load subsequent bytes (up to 2KB) into EEPROM 1
    const size_t remaining = size - _rom0.size();
    loadRom1(data + _rom0.size(), remaining);

    return Abc80Status::OK;
}

Abc80Status Abc80Bus::loadAbcFormat(const char *text, size_t length)
{
    if (!text || length == 0) {
        return Abc80Status::INVALID_ARG;
    }

    size_t i = 0;
    // Skip whitespace and optional leading start address number
    while (i < length && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
        ++i;
    }
    while (i < length && text[i] >= '0' && text[i] <= '9') {
        ++i;
    }
    // Scan until opening quotation mark
    while (i < length && text[i] != '"') {
        ++i;
    }
    if (i < length && text[i] == '"') {
        ++i;
    }

    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::vector<uint8_t> binary;
    binary.reserve(4096);

    while (i + 1 < length && text[i] != '"') {
        int hi = hexVal(text[i]);
        int lo = hexVal(text[i + 1]);
        if (hi < 0 || lo < 0) {
            break;
        }
        binary.push_back(static_cast<uint8_t>((hi << 4) | lo));
        i += 2;
    }

    if (binary.empty()) {
        return Abc80Status::ERROR;
    }

    return loadRom(binary.data(), binary.size());
}

Abc80Status Abc80Bus::loadRomFile(const char *filepath)
{
    if (!filepath) {
        return Abc80Status::INVALID_ARG;
    }

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return Abc80Status::NOT_FOUND;
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        return Abc80Status::ERROR;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char *>(buffer.data()), fileSize)) {
        return Abc80Status::ERROR;
    }

    // Check if file is in vintage QuickBASIC .abc ASCII format
    if (fileSize >= 3 && ((buffer[0] >= '0' && buffer[0] <= '9') || buffer[0] == '"')) {
        Abc80Status abcStatus = loadAbcFormat(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        if (abcStatus == Abc80Status::OK) {
            return Abc80Status::OK;
        }
    }

    return loadRom(buffer.data(), buffer.size());
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
