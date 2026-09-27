#include "KeyQuantScales.h"

#include <cstddef>
#include <initializer_list>

namespace KeyQuantScales
{
namespace
{
    constexpr std::uint16_t notes(std::initializer_list<int> semitones)
    {
        std::uint16_t mask = 0;
        for (int s : semitones)
            mask = static_cast<std::uint16_t>(mask | (1u << s));
        return mask;
    }
}

const std::vector<Scale>& all()
{
    static const std::vector<Scale> scales = {
        { "Chromatic",                 notes({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }) },

        { "Major (Ionian)",            notes({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Natural Minor (Aeolian)",   notes({ 0, 2, 3, 5, 7, 8, 10 }) },
        { "Harmonic Minor",            notes({ 0, 2, 3, 5, 7, 8, 11 }) },
        { "Melodic Minor",             notes({ 0, 2, 3, 5, 7, 9, 11 }) },

        { "Dorian",                    notes({ 0, 2, 3, 5, 7, 9, 10 }) },
        { "Phrygian",                  notes({ 0, 1, 3, 5, 7, 8, 10 }) },
        { "Lydian",                    notes({ 0, 2, 4, 6, 7, 9, 11 }) },
        { "Mixolydian",                notes({ 0, 2, 4, 5, 7, 9, 10 }) },
        { "Locrian",                   notes({ 0, 1, 3, 5, 6, 8, 10 }) },

        { "Pentatonic Major",          notes({ 0, 2, 4, 7, 9 }) },
        { "Pentatonic Minor",          notes({ 0, 3, 5, 7, 10 }) },
        { "Blues Major",               notes({ 0, 2, 3, 4, 7, 9 }) },
        { "Blues Minor",               notes({ 0, 3, 5, 6, 7, 10 }) },

        { "Phrygian Dominant (Andalusian)", notes({ 0, 1, 4, 5, 7, 8, 10 }) },
        { "Double Harmonic",           notes({ 0, 1, 4, 5, 7, 8, 11 }) },
        { "Hungarian Minor",           notes({ 0, 2, 3, 6, 7, 8, 11 }) },
        { "Neapolitan Minor",          notes({ 0, 1, 3, 5, 7, 8, 11 }) },
        { "Neapolitan Major",          notes({ 0, 1, 3, 5, 7, 9, 11 }) },
        { "Hirajoshi",                 notes({ 0, 2, 3, 7, 8 }) },
        { "In-sen",                    notes({ 0, 1, 5, 7, 10 }) },

        { "Whole Tone",                notes({ 0, 2, 4, 6, 8, 10 }) },
        { "Diminished (W-H)",          notes({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
        { "Diminished (H-W)",          notes({ 0, 1, 3, 4, 6, 7, 9, 10 }) },
    };
    return scales;
}

int chromaticIndex()
{
    return 0;
}

const char* rootName(int root)
{
    static const char* const names[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B"
    };
    return (root >= 0 && root < 12) ? names[root] : "?";
}

std::uint16_t maskFor(const Scale& scale, int root)
{
    root = ((root % 12) + 12) % 12;
    std::uint16_t mask = 0;
    for (int s = 0; s < 12; ++s)
        if (scale.intervals & (1u << s))
            mask = static_cast<std::uint16_t>(mask | (1u << ((s + root) % 12)));
    return mask;
}

std::vector<std::pair<int, int>> matches(std::uint16_t mask)
{
    mask &= 0xFFF;
    std::vector<std::pair<int, int>> found;
    const auto& scales = all();
    for (int i = 0; i < static_cast<int>(scales.size()); ++i)
    {
        if (i == chromaticIndex())
        {
            if (mask == 0xFFF)
                found.push_back({ i, 0 });
            continue;
        }
        for (int root = 0; root < 12; ++root)
            if (maskFor(scales[static_cast<std::size_t>(i)], root) == mask)
                found.push_back({ i, root });
    }
    return found;
}

const char* pitchComponentId(int pitchClass)
{
    static const char* const ids[12] = {
        "p11", "p12", "p13", "p14", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10"
    };
    return (pitchClass >= 0 && pitchClass < 12) ? ids[pitchClass] : "";
}
}
