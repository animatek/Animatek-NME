#include <doctest.h>
#include "model/LightMeterLayout.h"
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/ThemeData.h"

// The slot table tells each module where its LEDs and meters live in the two
// 128-slot arrays the synth streams. The canvas caches it against
// LightMeterLayout::fingerprint(), so these tests are about two things: the
// table says what the wire says, and the fingerprint moves exactly when it has
// to. A fingerprint that missed a change would freeze every meter in the
// editor; one that changed too eagerly would only cost the rebuild back.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static ThemeData& themes()
{
    static ThemeData theme;
    static bool loaded = theme.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("classic-theme.xml"));
    REQUIRE(loaded);
    return theme;
}

static Module* add(Patch& patch, int section, const juce::String& typeName,
                   int gridY, int gridX = 0)
{
    const auto* desc = descriptions().getModuleByName(typeName);
    REQUIRE(desc != nullptr);
    auto* m = patch.createModule(section, desc->index, gridX, gridY, typeName, descriptions());
    REQUIRE(m != nullptr);
    return m;
}

TEST_CASE("slots are handed out in wire order: poly first, then common")
{
    Patch patch;
    auto* polyA   = add(patch, 1, "Constant", 0);
    auto* polyB   = add(patch, 1, "OscA", 10);
    auto* commonA = add(patch, 0, "Constant", 0);

    const auto table = LightMeterLayout::build(&patch, &themes());
    REQUIRE(table.ranges.size() == 3);

    // Poly entries come first, sorted by container index, then common.
    CHECK(table.ranges[0].section == 1);
    CHECK(table.ranges[1].section == 1);
    CHECK(table.ranges[2].section == 0);
    CHECK(table.ranges[0].containerIndex < table.ranges[1].containerIndex);

    // Bases run cumulatively and never overlap.
    for (size_t i = 1; i < table.ranges.size(); ++i)
    {
        CHECK(table.ranges[i].lightBase
              == table.ranges[i - 1].lightBase + table.ranges[i - 1].lightCount);
        CHECK(table.ranges[i].meterBase
              == table.ranges[i - 1].meterBase + table.ranges[i - 1].meterCount);
    }

    // Lookup agrees with the table, and modules of the two areas do not
    // collide even when they share a container index.
    REQUIRE(table.find(1, polyA->getContainerIndex()) != nullptr);
    REQUIRE(table.find(1, polyB->getContainerIndex()) != nullptr);
    REQUIRE(table.find(0, commonA->getContainerIndex()) != nullptr);
    CHECK(table.find(1, polyA->getContainerIndex())->section == 1);
    CHECK(table.find(0, commonA->getContainerIndex())->section == 0);
    CHECK(table.find(1, 99) == nullptr);
}

TEST_CASE("meters come in pairs and LEDs are counted one by one")
{
    // AudioIn is the module that has both in the classic theme: two meters and
    // two LEDs. NOMAD hands meters out in MeterMessage pairs, so a module with
    // any meter at all takes two slots even when it shows one.
    Patch patch;
    add(patch, 0, "AudioIn", 0);

    const auto table = LightMeterLayout::build(&patch, &themes());
    REQUIRE(table.ranges.size() == 1);
    CHECK(table.ranges[0].lightCount == 2);
    CHECK(table.ranges[0].meterCount == 2);

    // A module with no lights of any kind takes no slots, so it never shifts
    // the modules that follow it.
    Patch plain;
    add(plain, 1, "Constant", 0);
    const auto plainTable = LightMeterLayout::build(&plain, &themes());
    REQUIRE(plainTable.ranges.size() == 1);
    CHECK(plainTable.ranges[0].lightCount == 0);
    CHECK(plainTable.ranges[0].meterCount == 0);
}

TEST_CASE("a module with lights shifts the slots of everything after it")
{
    // The bases are cumulative in wire order, so inserting a lit module ahead
    // of another moves the second one's slots along: this is exactly what the
    // fingerprint has to catch, and what a stale table would get wrong.
    Patch patch;
    add(patch, 1, "AudioIn", 0);
    auto* second = add(patch, 1, "AudioIn", 40);

    const auto table = LightMeterLayout::build(&patch, &themes());
    const auto* slots = table.find(1, second->getContainerIndex());
    REQUIRE(slots != nullptr);
    CHECK(slots->lightBase == 2);
    CHECK(slots->meterBase == 2);
}

TEST_CASE("an empty patch and a missing theme give an empty table")
{
    Patch empty;
    CHECK(LightMeterLayout::build(&empty, &themes()).ranges.empty());
    CHECK(LightMeterLayout::build(nullptr, &themes()).ranges.empty());
    CHECK(LightMeterLayout::build(&empty, nullptr).ranges.empty());
}

TEST_CASE("the fingerprint moves when the table would, and only then")
{
    Patch patch;
    auto* first = add(patch, 1, "Constant", 0);
    add(patch, 1, "OscA", 10);

    const auto before = LightMeterLayout::fingerprint(&patch, &themes());

    SUBCASE("editing a parameter does not disturb it")
    {
        REQUIRE_FALSE(first->getParameters().empty());
        first->getParameters()[0].setValue(first->getParameters()[0].getValue() + 1);
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) == before);
    }
    SUBCASE("moving a module does not disturb it")
    {
        // Position has nothing to do with which slots a module owns, and this
        // is the case the cache is there for: a drag must not rebuild it.
        first->setPosition({ 5, 60 });
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) == before);
    }
    SUBCASE("renaming a module does not disturb it")
    {
        first->setTitle("Something else");
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) == before);
    }
    SUBCASE("a cable does not disturb it")
    {
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) == before);
    }

    SUBCASE("adding a module changes it")
    {
        add(patch, 1, "Constant", 40);
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) != before);
    }
    SUBCASE("adding a module to the other area changes it")
    {
        add(patch, 0, "Constant", 0);
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) != before);
    }
    SUBCASE("removing a module changes it")
    {
        patch.getPolyVoiceArea().removeModule(first);
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) != before);
    }
    SUBCASE("swapping a module for a different type at the same index changes it")
    {
        // Delete and re-add at the same container index with another type: the
        // count matches and the indices match, so only the type tells them
        // apart, and the two have different numbers of lights.
        const int index = first->getContainerIndex();
        patch.getPolyVoiceArea().removeModule(first);
        auto* replacement = add(patch, 1, "2Output", 0);
        replacement->setContainerIndex(index);
        CHECK(LightMeterLayout::fingerprint(&patch, &themes()) != before);
    }
    SUBCASE("a different patch changes it")
    {
        Patch other;
        CHECK(LightMeterLayout::fingerprint(&other, &themes()) != before);
    }
    SUBCASE("no patch at all changes it")
    {
        CHECK(LightMeterLayout::fingerprint(nullptr, &themes()) != before);
    }
}

TEST_CASE("a matching fingerprint really does mean a matching table")
{
    // The property the cache rests on, checked by brute force over a sequence
    // of edits: whenever the fingerprint is unchanged, rebuilding produces the
    // same table, so serving the cached one is indistinguishable.
    Patch patch;
    auto* a = add(patch, 1, "Constant", 0);
    auto* b = add(patch, 1, "OscA", 10);
    add(patch, 0, "2Output", 0);

    auto snapshot = [&]
    {
        const auto fp = LightMeterLayout::fingerprint(&patch, &themes());
        const auto table = LightMeterLayout::build(&patch, &themes());
        return std::make_pair(fp, table.ranges);
    };

    auto sameTable = [](const std::vector<LightMeterLayout::ModuleSlots>& x,
                        const std::vector<LightMeterLayout::ModuleSlots>& y)
    {
        if (x.size() != y.size()) return false;
        for (size_t i = 0; i < x.size(); ++i)
            if (x[i].section != y[i].section || x[i].containerIndex != y[i].containerIndex
                || x[i].lightBase != y[i].lightBase || x[i].lightCount != y[i].lightCount
                || x[i].meterBase != y[i].meterBase || x[i].meterCount != y[i].meterCount)
                return false;
        return true;
    };

    auto previous = snapshot();

    const std::vector<std::function<void()>> edits {
        [&] { a->setPosition({ 3, 30 }); },
        [&] { b->setTitle("renamed"); },
        [&] { add(patch, 1, "Constant", 60); },
        [&] { a->getParameters()[0].setValue(1); },
        [&] { patch.getPolyVoiceArea().removeModule(b); },
        [&] { add(patch, 0, "OscA", 20); },
    };

    for (const auto& edit : edits)
    {
        edit();
        auto current = snapshot();
        if (current.first == previous.first)
            CHECK(sameTable(current.second, previous.second));
        previous = current;
    }
}
