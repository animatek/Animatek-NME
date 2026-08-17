#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"

// findNetOutput answers the one hardware rule the editor has to enforce while
// you patch: a net is driven by at most one output. The canvas asks it before
// every cable it lets you make, and a re-route asks it about a net it is
// halfway through changing, which is what the ignore pair is for (#67).

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static Module* add(Patch& patch, const juce::String& typeName, int gridY)
{
    const auto* desc = descriptions().getModuleByName(typeName);
    REQUIRE(desc != nullptr);
    auto* m = patch.createModule(0, desc->index, 0, gridY, typeName, descriptions());
    REQUIRE(m != nullptr);
    return m;
}

// First output / first input of a module, which is all these tests need.
static Connector* out(Module& m)
{
    for (auto& c : m.getConnectors())
        if (c.getDescriptor()->isOutput)
            return &c;
    return nullptr;
}

static Connector* in(Module& m)
{
    for (auto& c : m.getConnectors())
        if (!c.getDescriptor()->isOutput)
            return &c;
    return nullptr;
}

TEST_CASE("a net reports the output driving it")
{
    Patch patch;
    auto* src  = add(patch, "Constant", 0);
    auto* dst  = add(patch, "LevAdd", 2);
    auto& area = patch.getCommonArea();

    auto* srcOut = out(*src);
    auto* dstIn  = in(*dst);
    REQUIRE(srcOut != nullptr);
    REQUIRE(dstIn != nullptr);

    // Unpatched, an input is a net of its own with nothing driving it.
    CHECK(area.findNetOutput(dstIn) == nullptr);

    area.addConnection(srcOut, dstIn);
    CHECK(area.findNetOutput(dstIn) == srcOut);
}

TEST_CASE("the cable a re-route carries is walked around")
{
    Patch patch;
    auto* src  = add(patch, "Constant", 0);
    auto* dst  = add(patch, "LevAdd", 2);
    auto& area = patch.getCommonArea();

    auto* srcOut = out(*src);
    auto* dstIn  = in(*dst);
    area.addConnection(srcOut, dstIn);

    REQUIRE(area.findNetOutput(dstIn) == srcOut);

    // Mid-re-route the cable is still in the patch, because nothing is changed
    // until the drop lands. Asked to step over it, the walk answers about the
    // net as it will be once the move has happened: undriven.
    CHECK(area.findNetOutput(dstIn, srcOut, dstIn) == nullptr);

    // And the cable really is still there: the ignore is a question, not an edit.
    CHECK(area.getConnections().size() == 1);
    CHECK(area.findNetOutput(dstIn) == srcOut);
}

TEST_CASE("stepping over one cable leaves the rest of the net alone")
{
    Patch patch;
    auto* src   = add(patch, "Constant", 0);
    auto* mid   = add(patch, "LevAdd", 2);
    auto* tail  = add(patch, "LevMult", 4);
    auto& area  = patch.getCommonArea();

    auto* srcOut = out(*src);
    auto* midIn  = in(*mid);
    auto* tailIn = in(*tail);

    // One net: the output feeds mid, and mid's input is chained on to tail's.
    area.addConnection(srcOut, midIn);
    area.addConnection(midIn, tailIn);
    REQUIRE(area.findNetOutput(tailIn) == srcOut);

    // Lifting the chained cable cuts tail loose but leaves mid driven.
    CHECK(area.findNetOutput(tailIn, midIn, tailIn) == nullptr);
    CHECK(area.findNetOutput(midIn,  midIn, tailIn) == srcOut);

    // Lifting the driving cable instead cuts both loose, since tail only ever
    // reached the output through mid.
    CHECK(area.findNetOutput(tailIn, srcOut, midIn) == nullptr);
    CHECK(area.findNetOutput(midIn,  srcOut, midIn) == nullptr);
}

TEST_CASE("an output is its own net whatever is ignored")
{
    Patch patch;
    auto* src  = add(patch, "Constant", 0);
    auto& area = patch.getCommonArea();

    auto* srcOut = out(*src);
    CHECK(area.findNetOutput(srcOut) == srcOut);
    CHECK(area.findNetOutput(srcOut, srcOut, nullptr) == srcOut);
}
