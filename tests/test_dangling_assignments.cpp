#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"

// Deleting a module clears its morph and knob assignments in the editor, but
// nothing in the protocol unassigns a morph, so the synth's maps keep naming a
// module that is gone. Re-fetching that patch brings the leftovers back, and
// they latch onto whatever module lands on the same index next -- which the G1
// does not survive. Every patch coming in gets swept, from the wire or a file.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static Module* add(Patch& patch, int section, const juce::String& typeName, int gridY)
{
    const auto* desc = descriptions().getModuleByName(typeName);
    REQUIRE(desc != nullptr);
    auto* m = patch.createModule(section, desc->index, 0, gridY, typeName, descriptions());
    REQUIRE(m != nullptr);
    return m;
}

TEST_CASE("assignments naming a module the patch does not have are dropped")
{
    Patch patch;
    auto* kept = add(patch, 1, "Constant", 0);
    const int keptIndex = kept->getContainerIndex();
    const int goneIndex = keptIndex + 1;   // never created

    patch.morphAssignments.push_back({ 1, keptIndex, 0, 0, 127 });
    patch.morphAssignments.push_back({ 1, goneIndex, 2, 0, 127 });
    patch.ctrlAssignments.push_back({ 7, 1, goneIndex, 0 });
    patch.knobAssignments[0] = { true, 1, keptIndex, 0 };
    patch.knobAssignments[1] = { true, 1, goneIndex, 0 };
    // Section 2 is the morph section: its module is not in a voice area's list
    // and must survive the sweep.
    patch.knobAssignments[2] = { true, 2, 1, 0 };

    const auto dropped = patch.dropDanglingAssignments();
    CHECK(dropped.total() == 3);
    CHECK(dropped.morphs == 1);
    // The knob and CC lists are what reaches the synth: the panel keeps a
    // dropped assignment's LED lit until it is deassigned by index.
    CHECK(dropped.knobs == std::vector<int>{ 1 });
    CHECK(dropped.ctrls == std::vector<int>{ 7 });

    REQUIRE(patch.morphAssignments.size() == 1);
    CHECK(patch.morphAssignments[0].module == keptIndex);
    CHECK(patch.ctrlAssignments.empty());
    CHECK(patch.knobAssignments[0].assigned);
    CHECK_FALSE(patch.knobAssignments[1].assigned);
    CHECK(patch.knobAssignments[2].assigned);
    CHECK(patch.knobAssignments[2].section == 2);

    // Idempotent: a clean patch loses nothing on a second pass.
    CHECK(patch.dropDanglingAssignments().total() == 0);
}

TEST_CASE("the two voice areas are separate namespaces for assignments")
{
    Patch patch;
    auto* poly = add(patch, 1, "Constant", 0);
    const int index = poly->getContainerIndex();

    // Same index, but in the common area, where no module exists.
    patch.morphAssignments.push_back({ 0, index, 0, 0, 127 });
    patch.morphAssignments.push_back({ 1, index, 0, 1, 127 });

    CHECK(patch.dropDanglingAssignments().total() == 1);
    REQUIRE(patch.morphAssignments.size() == 1);
    CHECK(patch.morphAssignments[0].section == 1);
}
