#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"

// A ModuleRef names a module by where it lives, not by where it sits in
// memory. These tests are the reason it exists: the UI keeps hold of modules
// across the very events that destroy them, and a raw pointer kept across one
// of those is a use-after-free that is silent on Linux and fatal on macOS
// (issue #61).

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

TEST_CASE("a reference resolves to the module it was taken from")
{
    Patch patch;
    auto* poly   = add(patch, 1, "Constant", 0);
    auto* common = add(patch, 0, "Constant", 0);

    const auto polyRef   = Patch::refTo(*poly, 1);
    const auto commonRef = Patch::refTo(*common, 0);

    CHECK(patch.getModule(polyRef) == poly);
    CHECK(patch.getModule(commonRef) == common);

    // The two areas are separate namespaces: the same container index in the
    // other area is a different module, and must not be confused for it.
    CHECK(polyRef.containerIndex == commonRef.containerIndex);
    CHECK(polyRef != commonRef);
    CHECK(patch.getModule(polyRef) != patch.getModule(commonRef));
}

TEST_CASE("a reference to a deleted module resolves to nothing")
{
    // The whole point. Under the old scheme this pointer was left dangling and
    // the next paint read freed memory.
    Patch patch;
    auto* m = add(patch, 1, "Constant", 0);
    const auto ref = Patch::refTo(*m, 1);
    REQUIRE(patch.getModule(ref) != nullptr);

    patch.getPolyVoiceArea().removeModule(m);

    CHECK(patch.getModule(ref) == nullptr);
}

TEST_CASE("a reference survives the delete-then-undo that a pointer never did")
{
    // Undo re-inserts a module at the index it had, so a selection or a hover
    // taken before the delete still names the right module afterwards. A raw
    // pointer could not: the object was a different one at a different address.
    Patch patch;
    auto* original = add(patch, 1, "Constant", 0);
    const auto ref = Patch::refTo(*original, 1);
    const int index = original->getContainerIndex();

    auto stashed = patch.getPolyVoiceArea().extractModule(original);
    REQUIRE(stashed != nullptr);
    CHECK(patch.getModule(ref) == nullptr);

    // What an undo does: put the very same module back.
    patch.getPolyVoiceArea().addModuleSilent(std::move(stashed));

    auto* restored = patch.getModule(ref);
    REQUIRE(restored != nullptr);
    CHECK(restored->getContainerIndex() == index);
}

TEST_CASE("an invalid or out-of-range reference resolves to nothing")
{
    Patch patch;
    add(patch, 1, "Constant", 0);

    CHECK(patch.getModule(ModuleRef{}) == nullptr);             // default: invalid
    CHECK(patch.getModule({ -1, 5 }) == nullptr);
    CHECK(patch.getModule({ 1, -1 }) == nullptr);
    CHECK(patch.getModule({ 1, 126 }) == nullptr);              // nothing there
    CHECK(patch.getModule({ 0, 1 }) == nullptr);                // empty area
}

TEST_CASE("references compare and order by section then index")
{
    const ModuleRef a { 1, 3 }, b { 1, 3 }, c { 1, 4 }, d { 0, 3 };

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a != d);
    CHECK(a.isValid());
    CHECK_FALSE(ModuleRef{}.isValid());

    // Ordered, so a reference can key a map or be sorted for a binary search.
    CHECK(d < a);
    CHECK(a < c);
    CHECK_FALSE(a < b);

    ModuleRef cleared = a;
    cleared.clear();
    CHECK_FALSE(cleared.isValid());
}

TEST_CASE("a reference follows the spot, so a reused index names the new tenant")
{
    // Deliberate and documented: container indices are reused, so a reference
    // outliving its module can land on a module that took its place. That is a
    // harmless mix-up in the UI (a selection moves to another module) where a
    // stale pointer was memory corruption, and it is what makes undo work.
    Patch patch;
    auto* first = add(patch, 1, "Constant", 0);
    const auto ref = Patch::refTo(*first, 1);
    const int index = first->getContainerIndex();

    patch.getPolyVoiceArea().removeModule(first);
    CHECK(patch.getModule(ref) == nullptr);

    auto* replacement = add(patch, 1, "OscA", 0);
    REQUIRE(replacement->getContainerIndex() == index);   // lowest free index
    CHECK(patch.getModule(ref) == replacement);
}
