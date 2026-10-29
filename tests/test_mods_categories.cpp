#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "protocol/ParameterEncoder.h"
#include "midi/ConnectionManager.h"
#include <set>

// The G1-Mods layer (data/mods-modules.xml): module types only a modified OS has, loaded on top
// of Clavia's modules.xml, in "<family> (mods)" categories. Offered only when asked for; off,
// NME offers exactly what it offers without the layer.

static juce::File dataFile(const char* name)
{
    return juce::File(NME_TEST_DATA_DIR).getChildFile(name);
}

static ModuleDescriptions clavias()
{
    ModuleDescriptions descs;
    REQUIRE(descs.loadFromFile(dataFile("modules.xml")));
    return descs;
}

static ModuleDescriptions withLayer(bool shown)
{
    auto descs = clavias();
    REQUIRE(descs.addFromXmlString(dataFile("mods-modules.xml").loadFileAsString()));
    descs.setModsShown(shown);
    return descs;
}

// The types the G1's OS leaves free, the only ones G1-Mods can use (its table has 128 and a patch
// keeps a type in 7 bits; G1-Mods' tools/g1rom.py, FREE_TYPES).
static bool isFreeType(int index)
{
    static const std::set<int> free { 6, 41, 42, 55, 56, 60, 93, 101, 109, 116,
                                      119, 120, 121, 122, 123, 124, 125, 126 };
    return free.count(index) > 0;
}

TEST_CASE("the layer adds modules of free types only, and replaces none of Clavia's")
{
    const auto before = clavias();
    const auto after = withLayer(true);
    CHECK(after.getModuleCount() > before.getModuleCount());
    for (const auto& d : before.getAllModules())
    {
        const auto* same = after.getModuleByIndex(d.index);
        REQUIRE(same != nullptr);
        CHECK(same->name == d.name);
        CHECK(same->category == d.category);
    }
    for (const auto& d : after.getAllModules())
        if (before.getModuleByIndex(d.index) == nullptr)
        {
            INFO(d.name << " (" << d.index << ")");
            CHECK(isFreeType(d.index));
            CHECK(ModuleDescriptions::isModsCategory(d.category));
        }
}

TEST_CASE("off, NME offers exactly Clavia's modules")
{
    const auto before = clavias();
    const auto after = withLayer(false);
    CHECK(after.getCategories() == before.getCategories());
    for (const auto& cat : before.getCategories())
        CHECK(after.getModulesInCategory(cat).size() == before.getModulesInCategory(cat).size());
    for (const auto& d : after.getAllModules())
    {
        CHECK(after.getModulesInCategory(ModuleDescriptions::modsCategoryFor(d.category)).empty());
        CHECK(after.isOffered(d) == !ModuleDescriptions::isModsCategory(d.category));
    }
    // a patch with one still opens: the module is known, only not offered
    CHECK(after.getModuleByIndex(120) != nullptr);
}

TEST_CASE("on, each (mods) category comes right after its family")
{
    const auto descs = withLayer(true);
    const auto cats = descs.getCategories();
    int mods = 0;
    for (int i = 0; i < cats.size(); ++i)
    {
        if (!ModuleDescriptions::isModsCategory(cats[i]))
            continue;
        ++mods;
        REQUIRE(i > 0);
        // one whose family the G1 has not (Note Out's MIDI) goes at the end instead
        const auto family = cats[i].dropLastCharacters(juce::String(" (mods)").length());
        if (cats.contains(family))
            CHECK(ModuleDescriptions::modsCategoryFor(cats[i - 1]) == cats[i]);
        else
            CHECK(ModuleDescriptions::isModsCategory(cats[cats.size() - 1]));
    }
    CHECK(mods > 0);
}

TEST_CASE("on, a (mods) category holds only modules that are not Clavia's")
{
    const auto descs = withLayer(true);
    for (const auto& cat : descs.getCategories())
        for (const auto* d : descs.getModulesInCategory(cat))
        {
            INFO(cat << ": " << d->name << " (" << d->index << ")");
            CHECK(ModuleDescriptions::isModsCategory(cat) == isFreeType(d->index));
        }
}

TEST_CASE("the encoder's switch knows none of the layer's types: their widths come from data")
{
    const auto descs = withLayer(true);
    for (const auto& d : descs.getAllModules())
        if (ModuleDescriptions::isModsCategory(d.category))
        {
            INFO(d.name << " (" << d.index << ")");
            CHECK(ParameterEncoder::getParameterBitWidths(d.index).empty());
        }
    ParameterEncoder::setWidthsFromData(120, { 1 });
    CHECK(ParameterEncoder::getParameterBitWidths(120) == std::vector<int> { 1 });
    CHECK(ParameterEncoder::getParameterBitWidths(3) == std::vector<int> { 7 });   // the switch's
}

TEST_CASE("a synth whose OS is not a G1-Mods build takes none of their modules")
{
    // Several synths at once: a Clavia OS next to a modified one must never be sent one.
    const auto descs = withLayer(true);
    ConnectionManager clavia, modified;
    modified.setModsOs(true);
    int mods = 0;
    for (const auto& d : descs.getAllModules())
    {
        INFO(d.name << " (" << d.index << ")");
        const bool isMods = ModuleDescriptions::isModsCategory(d.category);
        mods += isMods;
        CHECK(clavia.takesModule(d) == !isMods);
        CHECK(modified.takesModule(d));
    }
    CHECK(mods > 0);
}

TEST_CASE("a G1-Mods OS is told apart by its IAm: version 3.09, not Clavia's 3.03")
{
    CHECK(ConnectionManager::isModsVersion(3, 9));
    CHECK_FALSE(ConnectionManager::isModsVersion(3, 3));   // Clavia's 3.03
    CHECK_FALSE(ConnectionManager::isModsVersion(3, 0));
    CHECK_FALSE(ConnectionManager::isModsVersion(2, 9));
    ConnectionManager fresh;
    CHECK_FALSE(fresh.hasModsOs());   // until a synth says so
}
