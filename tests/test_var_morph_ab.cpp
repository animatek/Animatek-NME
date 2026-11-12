#include <doctest.h>
#include "model/PatchVariations.h"

// The A-B Snapshot is saved in the .var sidecar with the variations, so it
// travels with the patch, the library and a slot set (issue #119).

static PatchVariations roundTrip(const PatchVariations& in)
{
    PatchVariations out;
    juce::StringArray lines;
    lines.addLines(varToText(in));
    REQUIRE(varFromText(out, lines));
    return out;
}

TEST_CASE("The A-B Snapshot survives the .var file")
{
    PatchVariations v;
    v.slots[2].filled = true;
    v.slots[2].entries.push_back({1, 3, 0, 40});
    v.activeIndex = 2;

    v.morphA.filled = true;
    v.morphA.morphValues = {1, 2, 3, 4};
    v.morphA.entries.push_back({1, 3, 0, 10});
    v.morphB.filled = true;
    v.morphB.entries.push_back({1, 3, 0, 120});
    v.morphFader = {2, 1, 3, 7, 0, 127};
    v.morphPos = 0.25f;

    const auto r = roundTrip(v);
    CHECK(r.morphA.filled);
    CHECK(r.morphA.morphValues == std::array<int, 4>{1, 2, 3, 4});
    REQUIRE(r.morphA.entries.size() == 1);
    CHECK(r.morphA.entries[0].value == 10);
    REQUIRE(r.morphB.entries.size() == 1);
    CHECK(r.morphB.entries[0].value == 120);
    CHECK(r.morphFader.param == 3);
    CHECK(r.morphFader.knobIndex == 7);
    CHECK(r.morphPos == doctest::Approx(0.25f));

    // The variations are untouched by the sections around them.
    CHECK(r.activeIndex == 2);
    REQUIRE(r.slots[2].entries.size() == 1);
    CHECK(r.slots[2].entries[0].value == 40);
    for (int i = 0; i < PatchVariations::kNumSlots; ++i)
        if (i != 2) CHECK_FALSE(r.slots[i].filled);
}

TEST_CASE("The A-B sections come before any variation, where an older reader skips them")
{
    PatchVariations v;
    v.slots[0].filled = true;
    v.slots[0].entries.push_back({0, 1, 0, 5});
    v.morphA.filled = true;
    v.morphA.entries.push_back({0, 1, 0, 99});

    const auto text = varToText(v);
    CHECK(text.indexOf("[MorphA]") < text.indexOf("[Variation 0]"));
    CHECK(text.indexOf("[MorphFader]") < text.indexOf("[Variation 0]"));
}

TEST_CASE("A .var with no A-B Snapshot reads as none, and an unknown section is skipped")
{
    juce::StringArray lines;
    lines.addLines("[VarFile]\nVersion=1\nActive=-1\n[Something New]\n1 1 1 1\n"
                   "[Variation 0]\nMorph=0 0 0 0\n0 1 0 5\n[End]\n");
    PatchVariations r;
    REQUIRE(varFromText(r, lines));
    CHECK_FALSE(r.hasMorphAB());
    REQUIRE(r.slots[0].entries.size() == 1);
}
