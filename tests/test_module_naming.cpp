#include <doctest.h>
#include "model/ModuleDescriptions.h"

// modules.xml is inherited from nmedit, which reverse-engineered it. Where we
// have decided it is wrong, the correction is easy to lose: a resync from
// upstream would quietly put the old name or the old default back. This file
// pins the divergences we chose on purpose.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static const ParameterDescriptor* param(const juce::String& moduleName, int index)
{
    const auto* m = descriptions().getModuleByName(moduleName);
    REQUIRE(m != nullptr);
    for (const auto& p : m->parameters)
        if (p.paramClass == "parameter" && p.index == index)
            return &p;
    return nullptr;
}

TEST_CASE("The mixers' input attenuators are named in level, not in sense")
{
    // Clavia's own documentation calls these attenuation controls and never
    // uses "sense", which came from the community's parameter dump. EqMid and
    // EqShelving already called the same kind of control "in level" (issue #78).
    for (int i = 0; i < 3; ++i)
        CHECK(param("Mixer (3)", i)->name == "in level " + juce::String(i + 1));
    for (int i = 0; i < 8; ++i)
        CHECK(param("Mixer (8)", i)->name == "in level " + juce::String(i + 1));

    const auto* m3 = descriptions().getModuleByName("Mixer (3)");
    const auto* m8 = descriptions().getModuleByName("Mixer (8)");
    REQUIRE(m3 != nullptr);
    REQUIRE(m8 != nullptr);
    for (const auto* m : { m3, m8 })
        for (const auto& p : m->parameters)
            CHECK_MESSAGE(!p.name.contains("sense"), p.name.toStdString());
}

TEST_CASE("Both note sequencers call their Loop button Loop, and start it on")
{
    // NoteSeqA's arrived named "active", which is not what its button says, and
    // both arrived at off while the original editor starts them looping (#76).
    for (const char* module : { "NoteSeqA", "NoteSeqB" })
    {
        const auto* loop = param(module, 20);
        REQUIRE(loop != nullptr);
        CHECK(loop->name == "loop");
        CHECK(loop->defaultValue == 1);
        CHECK(loop->maxValue == 1);
    }
}
