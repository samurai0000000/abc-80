/*
 * Abc80Rom.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <abc80/Abc80Rom.hxx>

namespace abc80 {

namespace {

// Key profiles (frames): hold 1; press spacing = ceil((R6 + R1) / frame) + 1; release gap = ceil(R2 time / frame) + 1,
// each one frame above the minimum the Abc80Keypad sweeps measure (monitor 4 / 1, 1993 7 / 3).
// Monitor: book ROM, src/rom.asm assembled to build/rom.bin; main loop at 0x009B.
// 1993: vintage/rom.abc; after boot beeps it polls the PC link at 0x0B04.
const RomProfile kProfiles[] = {
    { RomId::Monitor,     "monitor", "build/rom.bin",     0x009B, { 1, 5, 2 } },
    { RomId::Vintage1993, "1993",    "vintage/rom.abc",   0x0B04, { 1, 8, 4 } },
};

} // namespace

const RomProfile &romProfile(RomId id) noexcept
{
    return id == RomId::Vintage1993 ? kProfiles[1] : kProfiles[0];
}

bool parseRomId(const std::string &name, RomId &out) noexcept
{
    for (const RomProfile &p : kProfiles) {
        if (name == p.name) {
            out = p.id;
            return true;
        }
    }
    return false;
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
