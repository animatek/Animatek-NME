#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "format/ValueFormatters.h"

// How a value reads, and where a flat knob's arc starts, can depend on more than
// the parameter's own formatter: the Constant's polarity switch changes both
// (issue #122), and a rate that defaults to 64 is not a bipolar control (#123).

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static std::unique_ptr<Module> make(const juce::String& name)
{
    const auto* d = descriptions().getModuleByName(name);
    REQUIRE(d != nullptr);
    return Module::createFromDescriptor(*d);
}

TEST_CASE("The Constant reads 0 to 64 in halves when unipolar, -64 to +64 when not")
{
    auto c = make("Constant");
    auto* value = c->getParameter(0);
    auto* unipolar = c->getParameter(1);
    REQUIRE(value != nullptr);
    REQUIRE(unipolar != nullptr);
    const auto& pd = *value->getDescriptor();

    unipolar->setValue(0);
    CHECK(ValueFormatters::format(ValueFormatters::inModule(c.get(), pd), 0) == "-64");
    CHECK(ValueFormatters::format(ValueFormatters::inModule(c.get(), pd), 64) == "0");
    CHECK(ValueFormatters::arcOrigin(pd, c.get()) == doctest::Approx(64.0 / 127.0));

    unipolar->setValue(1);
    const auto shown = ValueFormatters::inModule(c.get(), pd);
    CHECK(ValueFormatters::format(shown, 0) == "0.0");
    CHECK(ValueFormatters::format(shown, 1) == "0.5");
    CHECK(ValueFormatters::format(shown, 126) == "63.0");
    CHECK(ValueFormatters::format(shown, 127) == "64.0");
    CHECK(ValueFormatters::arcOrigin(pd, c.get()) == 0.0f);

    // No module to look at: the plain reading, as before.
    CHECK(ValueFormatters::format(ValueFormatters::inModule(nullptr, pd), 0) == "-64");
}

TEST_CASE("A flat knob's arc starts at the zero of a signed reading, else at the minimum")
{
    auto clk = make("ClkGen");
    CHECK(ValueFormatters::arcOrigin(*clk->getParameter(0)->getDescriptor(), clk.get()) == 0.0f);

    auto nq = make("NoteQuant");
    CHECK(ValueFormatters::arcOrigin(*nq->getParameter(0)->getDescriptor(), nq.get())
          == doctest::Approx(64.0 / 127.0));

    // Bare numbers whose default sits in the middle stay centred: EqMid's gain.
    auto eq = make("EqMid");
    CHECK(ValueFormatters::arcOrigin(*eq->getParameter(1)->getDescriptor(), eq.get())
          == doctest::Approx(64.0 / 127.0));

    // A morph range, -127..127, grows from its 0.
    ParameterDescriptor morph;
    morph.minValue = -127;
    morph.maxValue = 127;
    CHECK(ValueFormatters::arcOrigin(morph) == doctest::Approx(0.5));
}
