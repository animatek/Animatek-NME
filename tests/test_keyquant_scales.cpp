#include <doctest.h>
#include "model/KeyQuantScales.h"
#include "model/ModuleDescriptions.h"
#include <juce_core/juce_core.h>

// The Key Quantizer's scale presets: written once as intervals, moved to any
// root, and recognised again from the module's switches.

static int indexOf(const char* name)
{
    const auto& scales = KeyQuantScales::all();
    for (int i = 0; i < static_cast<int>(scales.size()); ++i)
        if (juce::String(scales[static_cast<size_t>(i)].name) == name)
            return i;
    FAIL("no scale named " << name);
    return -1;
}

static std::uint16_t mask(const char* name, int root)
{
    return KeyQuantScales::maskFor(KeyQuantScales::all()[static_cast<size_t>(indexOf(name))], root);
}

TEST_CASE("the scales on C write the same switches the menu always wrote")
{
    // The bitmaps of the original root-C menu, before scales took a root.
    CHECK(mask("Chromatic", 0) == 0xFFF);
    CHECK(mask("Major (Ionian)", 0) == 0xAB5);
    CHECK(mask("Natural Minor (Aeolian)", 0) == 0x5AD);
    CHECK(mask("Harmonic Minor", 0) == 0x9AD);
    CHECK(mask("Melodic Minor", 0) == 0xAAD);
    CHECK(mask("Dorian", 0) == 0x6AD);
    CHECK(mask("Phrygian", 0) == 0x5AB);
    CHECK(mask("Lydian", 0) == 0xAD5);
    CHECK(mask("Mixolydian", 0) == 0x6B5);
    CHECK(mask("Locrian", 0) == 0x56B);
    CHECK(mask("Pentatonic Major", 0) == 0x295);
    CHECK(mask("Pentatonic Minor", 0) == 0x4A9);
    CHECK(mask("Blues Major", 0) == 0x29D);
    CHECK(mask("Blues Minor", 0) == 0x4E9);
    CHECK(mask("Whole Tone", 0) == 0x555);
    CHECK(mask("Diminished (W-H)", 0) == 0xB6D);
}

TEST_CASE("a scale moves to another root with its notes")
{
    const auto pitch = [](std::initializer_list<int> classes) {
        std::uint16_t m = 0;
        for (int c : classes) m = static_cast<std::uint16_t>(m | (1u << c));
        return m;
    };
    // A natural minor is C major's notes.
    CHECK(mask("Natural Minor (Aeolian)", 9) == mask("Major (Ionian)", 0));
    // E Andalusian: E F G# A B C D.
    CHECK(mask("Phrygian Dominant (Andalusian)", 4) == pitch({ 4, 5, 8, 9, 11, 0, 2 }));
    // D Dorian: the white keys again.
    CHECK(mask("Dorian", 2) == 0xAB5);
    // Negative and out-of-range roots wrap.
    CHECK(mask("Major (Ionian)", -3) == mask("Major (Ionian)", 9));
    CHECK(mask("Major (Ionian)", 14) == mask("Major (Ionian)", 2));
}

TEST_CASE("the switches are recognised as every scale they spell")
{
    const auto found = KeyQuantScales::matches(0xAB5);
    const auto has = [&found](const char* name, int root) {
        for (auto& [i, r] : found)
            if (i == indexOf(name) && r == root)
                return true;
        return false;
    };
    CHECK(has("Major (Ionian)", 0));
    CHECK(has("Natural Minor (Aeolian)", 9));
    CHECK(has("Dorian", 2));
    CHECK(has("Lydian", 5));
    CHECK_FALSE(has("Major (Ionian)", 7));

    // Chromatic once, not on every root.
    const auto all = KeyQuantScales::matches(0xFFF);
    REQUIRE(all.size() == 1);
    CHECK(all[0].first == KeyQuantScales::chromaticIndex());

    // Something no scale spells.
    CHECK(KeyQuantScales::matches(0x001 | 0x002 | 0x004).empty());
}

TEST_CASE("every pitch class names one of the Key Quantizer's own switches")
{
    ModuleDescriptions descs;
    REQUIRE(descs.loadFromFile(juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml")));
    const auto* keyQuant = descs.getModuleByIndex(98);
    REQUIRE(keyQuant != nullptr);
    REQUIRE(keyQuant->name == "KeyQuant");

    const char* const expected[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
    for (int pc = 0; pc < 12; ++pc)
    {
        const juce::String id = KeyQuantScales::pitchComponentId(pc);
        const ParameterDescriptor* found = nullptr;
        for (auto& p : keyQuant->parameters)
            if (p.componentId == id)
                found = &p;
        REQUIRE_MESSAGE(found != nullptr, "no switch " << id);
        CHECK(found->name == expected[pc]);
        CHECK(juce::String(KeyQuantScales::rootName(pc)) == expected[pc]);
    }
}

TEST_CASE("scale names are unique and fit the ASCII the editor's own text keeps to")
{
    const auto& scales = KeyQuantScales::all();
    for (size_t i = 0; i < scales.size(); ++i)
    {
        CHECK(juce::String(scales[i].name).containsOnly(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ()-"));
        for (size_t j = i + 1; j < scales.size(); ++j)
            CHECK(juce::String(scales[i].name) != juce::String(scales[j].name));
    }
}
