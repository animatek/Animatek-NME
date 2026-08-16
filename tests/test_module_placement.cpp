#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/ModulePlacement.h"
#include "model/Patch.h"

// makeRoomForModule clears a rectangle of the grid by pushing what is there
// down its column; canMakeRoomForModule is the question asked first, because
// a push past row 128 gets clamped and lands back on top of whatever pushed
// it (issue #54: a module at the bottom of the area was silently buried).

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

namespace
{
struct Rig
{
    Patch patch;
    const ModuleDescriptor* desc;
    int h;   // the module's height in grid rows

    Rig()
    {
        desc = descriptions().getModuleByName("Constant");
        REQUIRE(desc != nullptr);
        h = desc->height;
        REQUIRE(h >= 1);
    }

    Module* place(int gx, int gy, int section = 1)
    {
        auto* m = patch.createModule(section, desc->index, gx, gy, "M", descriptions());
        REQUIRE(m != nullptr);
        return m;
    }
};
}

TEST_CASE("an occupant of the target spot moves down the column")
{
    Rig rig;
    auto* a = rig.place(0, 0);

    auto pushed = makeRoomForModule(rig.patch.getPolyVoiceArea(), 1, 0, 0, rig.h);

    REQUIRE(pushed.size() == 1);
    CHECK(a->getPosition() == juce::Point<int>(0, rig.h));
    CHECK(pushed[0].oldPos == juce::Point<int>(0, 0));

    restorePushedModules(rig.patch, pushed);
    CHECK(a->getPosition() == juce::Point<int>(0, 0));
}

TEST_CASE("a stacked run stays a run when pushed")
{
    Rig rig;
    auto* a = rig.place(0, 0);
    auto* b = rig.place(0, rig.h);

    auto pushed = makeRoomForModule(rig.patch.getPolyVoiceArea(), 1, 0, 0, rig.h);

    CHECK(pushed.size() == 2);
    CHECK(a->getPosition().y == rig.h);
    CHECK(b->getPosition().y == 2 * rig.h);
}

TEST_CASE("other columns and ignored modules stay put")
{
    Rig rig;
    auto* other  = rig.place(1, 0);
    auto* keep   = rig.place(0, 0);

    auto pushed = makeRoomForModule(rig.patch.getPolyVoiceArea(), 1, 0, 0, rig.h,
                                    { keep->getContainerIndex() });

    CHECK(pushed.empty());
    CHECK(other->getPosition() == juce::Point<int>(1, 0));
    CHECK(keep->getPosition() == juce::Point<int>(0, 0));
}

TEST_CASE("text notes take part in the push and in the refusal")
{
    Rig rig;
    PatchComment note;
    note.section = 1;
    note.x = 0;
    note.y = 0;
    note.width = 2;   // covers columns 0 and 1
    note.height = 2;
    const int noteId = rig.patch.addComment(note);

    auto pushed = makeRoomForModule(rig.patch.getPolyVoiceArea(), 1, 1, 0, rig.h,
                                    {}, &rig.patch.getComments());

    REQUIRE(pushed.size() == 1);
    CHECK(pushed[0].commentId == noteId);
    CHECK(rig.patch.getCommentById(noteId)->y == rig.h);

    restorePushedModules(rig.patch, pushed);
    CHECK(rig.patch.getCommentById(noteId)->y == 0);
}

TEST_CASE("issue #54: a full column refuses instead of burying the bottom module")
{
    Rig rig;
    const int bottom = modulePlacementRows - rig.h;
    rig.place(0, bottom);

    auto& container = rig.patch.getPolyVoiceArea();

    // No room below the occupant: placing on top of it must be refused.
    CHECK_FALSE(canMakeRoomForModule(container, 1, 0, bottom, rig.h));

    // With room to spare the same question says yes.
    CHECK(canMakeRoomForModule(container, 1, 0, 0, rig.h));

    // A block that itself hangs past the bottom is refused outright.
    CHECK_FALSE(canMakeRoomForModule(container, 1, 0, modulePlacementRows - rig.h + 1, rig.h));
    CHECK_FALSE(canMakeRoomForModule(container, 1, 0, -1, rig.h));
}

TEST_CASE("issue #54: the refusal counts the whole chain of pushes")
{
    Rig rig;
    // Stack enough modules at the bottom of the column that one more push
    // chain cannot fit, while each individual gap looks harmless.
    const int n = 3;
    const int top = modulePlacementRows - n * rig.h;
    for (int i = 0; i < n; ++i)
        rig.place(0, top + i * rig.h);

    auto& container = rig.patch.getPolyVoiceArea();

    // Placing right at the top of the stack needs n*h rows below it: refused.
    CHECK_FALSE(canMakeRoomForModule(container, 1, 0, top, rig.h));

    // One module height above the stack there is exactly room for the block
    // itself, pushing nothing.
    CHECK(canMakeRoomForModule(container, 1, 0, top - rig.h, rig.h));
}

TEST_CASE("empty columns accept anything that fits the area")
{
    Rig rig;
    auto& container = rig.patch.getPolyVoiceArea();

    CHECK(canMakeRoomForModule(container, 1, 0, 0, rig.h));
    CHECK(canMakeRoomForModule(container, 1, 39, modulePlacementRows - rig.h, rig.h));
    CHECK(makeRoomForModule(container, 1, 5, 5, rig.h).empty());
}
