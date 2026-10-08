/*
 * Abc80Rom.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ABC80_ROM_HXX
#define ABC80_ROM_HXX

#include <cstdint>
#include <string>

namespace abc80 {

// The two ROMs the board can boot (book monitor, or the author's 1993 ROM).
enum class RomId { Monitor, Vintage1993 };

// Key timing in frames (1 frame = 1/60 s of emulated time). Derived from the
// measured ROM timings in plan Section 2.2; verified by the key-queue sweep
// tests (plan Task 1.4).
struct KeyProfile {
    uint8_t holdFrames;
    uint8_t pressSpacingFrames;
    uint8_t releaseGapFrames;
};

struct RomProfile {
    RomId id;
    const char *name;     // "monitor" | "1993"
    const char *relPath;  // relative to the repository root
    uint16_t readyAddr;   // first opcode fetch here means the ROM reached steady state
    KeyProfile keys;
};

// Profile for a ROM id (never fails).
const RomProfile &romProfile(RomId id) noexcept;

// Parses "monitor" or "1993"; returns false (leaving out untouched) for anything else.
bool parseRomId(const std::string &name, RomId &out) noexcept;

} // namespace abc80

#endif /* ABC80_ROM_HXX */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
