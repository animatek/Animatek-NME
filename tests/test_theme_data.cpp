#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/ThemeData.h"

// ThemeData turns the theme XML into the panel layout the canvas paints. What
// is pinned here is the part of that translation the XML does not say outright:
// the polarity switches, whose two states the inherited XML labels identically.

static ThemeData& themes()
{
    static ThemeData theme;
    static bool loaded = theme.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("classic-theme.xml"));
    REQUIRE(loaded);
    return theme;
}

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static const ThemeButton* buttonFor(const juce::String& moduleId, const juce::String& paramId)
{
    const auto* theme = themes().getModuleTheme(moduleId);
    REQUIRE(theme != nullptr);
    for (const auto& b : theme->buttons)
        if (b.componentId == paramId)
            return &b;
    return nullptr;
}

TEST_CASE("polarity switches name the polarity they are in")
{
    // Constant, LevMult, LevAdd and CtrlSeq: every module with a Uni/Bip switch.
    // The XML labels both states "Uni" on all four, which left the panel reading
    // the same whichever way the button was set (issue #69).
    struct Case { const char* module; const char* param; } const cases[] = {
        { "m43",  "p2"  },   // Constant  - unipolar
        { "m111", "p2"  },   // LevMult   - unipolar
        { "m112", "p2"  },   // LevAdd    - unipolar
        { "m91",  "p18" },   // CtrlSeq   - uni
    };

    for (const auto& c : cases)
    {
        CAPTURE(c.module);
        const auto* b = buttonFor(c.module, c.param);
        REQUIRE(b != nullptr);
        REQUIRE(b->labels.size() == 2);
        // Index is the parameter value: 0 = off = bipolar, 1 = on = unipolar,
        // which is the way round the hardware works.
        CHECK(b->labels[0] == "Bip");
        CHECK(b->labels[1] == "Uni");
        CHECK(b->cyclic);
    }
}

TEST_CASE("the polarity parameters really are two-state toggles")
{
    // The labels above are only right while the parameter has exactly the two
    // values they name. If a descriptor ever grew a third, the button would
    // fall back to labels[0] and lie.
    struct Case { const char* module; const char* param; } const cases[] = {
        { "Constant", "unipolar" },
        { "LevMult",  "unipolar" },
        { "LevAdd",   "unipolar" },
        { "CtrlSeq",  "uni"      },
    };

    for (const auto& c : cases)
    {
        CAPTURE(c.module);
        const auto* desc = descriptions().getModuleByName(c.module);
        REQUIRE(desc != nullptr);

        const ParameterDescriptor* param = nullptr;
        for (const auto& p : desc->parameters)
            if (p.name == c.param)
                param = &p;

        REQUIRE(param != nullptr);
        CHECK(param->minValue == 0);
        CHECK(param->maxValue == 1);
    }
}

TEST_CASE("ordinary toggles keep the labels the theme gives them")
{
    // The polarity rule is keyed off the parameter name, so it must not reach
    // the module's other buttons. CtrlSeq's Loop switch sits right beside the
    // Uni one and is labelled with images, not text.
    const auto* loop = buttonFor("m91", "p19");
    REQUIRE(loop != nullptr);
    CHECK(loop->labels.size() == 2);
    CHECK(loop->labels[0] != "Bip");
}
