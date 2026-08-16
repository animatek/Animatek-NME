#pragma once

#include "Patch.h"
#include "ThemeData.h"
#include <cstdint>
#include <map>
#include <vector>

// Where each module's LEDs and meters live in the two 128-slot arrays the synth
// streams. NOMAD's LightProcessor hands out the slots in wire order: the poly
// area first, then common, each sorted by container index, every module taking
// as many slots as its theme says it has lights.
//
// This lives in the model rather than in the canvas because working it out
// costs a sort and a theme lookup per module, and the canvas needs it twice per
// module per paint plus once per light frame from the synth: it has to be
// cached, and a cache nobody can test is a bug waiting for a hardware session.
namespace LightMeterLayout
{
    struct ModuleSlots
    {
        int section, containerIndex;
        int lightBase, lightCount;
        int meterBase, meterCount;
    };

    struct Table
    {
        std::vector<ModuleSlots> ranges;
        std::map<int, size_t> byModule;   // key(section, containerIndex) -> index into ranges
        // Unsigned on purpose: a hash is meant to wrap round, and signed
        // overflow is undefined behaviour, which is what UBSan called this out
        // for the first time the table was built.
        std::uint64_t fingerprint = 0;

        const ModuleSlots* find(int section, int containerIndex) const;
    };

    /** Key used by Table::byModule. Container indices are 7-bit on the wire. */
    inline int key(int section, int containerIndex) { return section * 1024 + containerIndex; }

    /** A cheap summary of everything the table is derived from: which modules
        exist, at which indices, of which type, read from which patch and theme.
        No allocation and no sorting, so a caller can ask on every frame whether
        the table it holds is still the right one.

        Conservative by design: it changes for things the table does not
        actually depend on (a reordering of the storage vector), which costs a
        rebuild that was not needed. It must never fail to change when the table
        would differ, which is what makes it safe to cache against. */
    std::uint64_t fingerprint(const Patch* patch, const ThemeData* theme);

    /** Builds the table. `fingerprint` is filled in from the same inputs. */
    Table build(const Patch* patch, const ThemeData* theme);
}
