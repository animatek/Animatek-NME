#pragma once

#include <cstdint>
#include <utility>
#include <vector>

// Scale presets for the Key Quantizer (module 98), on any root.
//
// The module has twelve on/off note switches. A scale is written once, as the
// semitones above its root, and moved onto the switches for the root chosen, so
// "D Dorian" and "E Phrygian" need no table of their own. Masks use bit n for
// pitch class n, C = 0, the same bitmap the context menu has always written.
namespace KeyQuantScales
{
    struct Scale
    {
        const char* name;
        std::uint16_t intervals;   // bit n set: the note n semitones above the root
    };

    // Chromatic first, then the scales in menu order.
    const std::vector<Scale>& all();

    // Index of the chromatic scale in all(): the same on every root, so the menu
    // offers it once instead of twelve times.
    int chromaticIndex();

    // "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B", spelled as
    // the module's own switches are.
    const char* rootName(int root);

    // The switches for `scale` on `root` (0 = C .. 11 = B).
    std::uint16_t maskFor(const Scale& scale, int root);

    // Every (scale index, root) whose notes are exactly `mask`. Relative modes
    // share their notes, so C Major also answers A Natural Minor and D Dorian;
    // all of them are true, and all of them are returned. Chromatic is returned
    // once, on C.
    std::vector<std::pair<int, int>> matches(std::uint16_t mask);

    // The module's component id for the switch of pitch class n. Its parameters
    // start at E, not C: p3..p14 are E, F, F#, G, G#, A, Bb, B, C, C#, D, D#.
    const char* pitchComponentId(int pitchClass);
}
