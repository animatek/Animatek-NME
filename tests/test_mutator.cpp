#include <doctest.h>
#include "model/Mutator.h"
#include "model/MutationCategories.h"
#include "model/ModuleDescriptions.h"

#include <cmath>

// The Patch Mutator after the G2's own (G2-Emu/NOTES.md, "El Patch Mutator de
// Clavia"): each parameter has a shape, set-up switches are never moved, a
// mutation comes back in at the edges, and Probability and Range are tied.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static MutCurve curveOf(const char* module, const char* param)
{
    auto* m = descriptions().getModuleByName(module);
    REQUIRE(m != nullptr);
    for (auto& p : m->parameters)
        if (p.paramClass == "parameter" && p.name.equalsIgnoreCase(param))
            return getMutationCurve(*m, p);
    FAIL("no parameter " << param << " on " << module);
    return MutCurve::Fixed;
}

TEST_CASE("Mutator: G1 parameters get the shape the G2 gives the same control")
{
    CHECK(curveOf("OscA", "freq coarse") == MutCurve::Centered);
    CHECK(curveOf("OscA", "freq fine") == MutCurve::Centered);
    CHECK(curveOf("OscA", "fma mod") == MutCurve::FineLow);
    CHECK(curveOf("OscA", "pitch mod 1") == MutCurve::Linear);
    CHECK(curveOf("OscA", "waveform") == MutCurve::Linear);   // the G2 moves waveforms
    CHECK(curveOf("OscA", "mute") == MutCurve::Fixed);
    CHECK(curveOf("LFOA", "range") == MutCurve::Fixed);
    CHECK(curveOf("LFOA", "mono") == MutCurve::Fixed);
    CHECK(curveOf("LFOA", "rate") == MutCurve::Linear);
    CHECK(curveOf("FilterE", "frequency") == MutCurve::Linear);
    CHECK(curveOf("FilterE", "bypass") == MutCurve::Fixed);
    CHECK(curveOf("DrumSynth", "MDecay") == MutCurve::Centered);
    CHECK(curveOf("DrumSynth", "Click") == MutCurve::FineHigh);
    CHECK(curveOf("DrumSynth", "Filter mode") == MutCurve::Linear);
    CHECK(curveOf("DrumSynth", "Filter sweep") == MutCurve::FineLow);
    CHECK(curveOf("Delay", "time") == MutCurve::FineLow);
    CHECK(curveOf("EqMid", "gain") == MutCurve::Centered);
    CHECK(curveOf("EqMid", "in level") == MutCurve::Fixed);
    CHECK(curveOf("EventSeq", "stepcount") == MutCurve::Fixed);
    CHECK(curveOf("EventSeq", "seq 1, step 1") == MutCurve::Linear);  // steps are the music
    CHECK(curveOf("KeyQuant", "cont") == MutCurve::Fixed);
}

TEST_CASE("Mutator: Probability and Range are tied by the G2's curve")
{
    // The G2's own window: Range 19 % beside Probability 38 %
    CHECK(Mutator::probabilityForRange(0.19f) == doctest::Approx(0.37f).epsilon(0.03));
    CHECK(Mutator::probabilityForRange(0.0f) == doctest::Approx(1.0f));
    CHECK(Mutator::rangeForProbability(0.0f) == doctest::Approx(Mutator::kMaxRange));
    for (float r : { 0.05f, 0.1f, 0.2f, 0.3f, 0.4f })
        CHECK(Mutator::rangeForProbability(Mutator::probabilityForRange(r)) == doctest::Approx(r).epsilon(0.01));
}

TEST_CASE("Mutator: a Fixed parameter never moves")
{
    juce::Random rng(1);
    for (int i = 0; i < 100; ++i)
    {
        CHECK(Mutator::mutateValue(1, 0, 1, MutCurve::Fixed, 0.5f, rng) == 1);
        CHECK(Mutator::randomValue(1, 0, 1, MutCurve::Fixed, rng) == 1);
    }
}

TEST_CASE("Mutator: a mutation stays in range and comes back in at the edges")
{
    juce::Random rng(2);
    int atTop = 0;
    const int n = 20000;
    for (int i = 0; i < n; ++i)
    {
        const int v = Mutator::mutateValue(127, 0, 127, MutCurve::Linear, 0.3f, rng);
        CHECK(v >= 0);
        CHECK(v <= 127);
        if (v == 127) ++atTop;
    }
    // Clamping would leave half of them on 127; reflected, about one in 38 lands there
    CHECK(atTop < n / 10);
}

TEST_CASE("Mutator: the shapes are finer where they should be")
{
    auto meanStep = [](int from, MutCurve curve) {
        juce::Random rng(3);
        double sum = 0;
        const int n = 20000;
        for (int i = 0; i < n; ++i)
            sum += std::abs(Mutator::mutateValue(from, 0, 127, curve, 0.2f, rng) - from);
        return sum / n;
    };
    // A short attack moves less than a linear knob at the same place...
    CHECK(meanStep(10, MutCurve::FineLow) < meanStep(10, MutCurve::Linear));
    // ...a level near the top too...
    CHECK(meanStep(117, MutCurve::FineHigh) < meanStep(117, MutCurve::Linear));
    // ...and a tuning knob near its centre
    CHECK(meanStep(64, MutCurve::Centered) < meanStep(64, MutCurve::Linear));
}

TEST_CASE("Mutator: Randomize covers the whole range")
{
    juce::Random rng(4);
    int lo = 127, hi = 0;
    for (int i = 0; i < 5000; ++i)
    {
        const int v = Mutator::randomValue(64, 0, 127, MutCurve::FineLow, rng);
        lo = juce::jmin(lo, v);
        hi = juce::jmax(hi, v);
    }
    CHECK(lo == 0);
    CHECK(hi == 127);
}

TEST_CASE("Mutator: the chromosome walks the parameters in pairs")
{
    ParamSnapshot snap;
    CHECK(Mutator::chromosomePath(snap).empty());

    snap.filled = true;
    // Turn 0° and walk 10, turn +90° and walk 20
    snap.entries = { { 0, 0, 0, 63 }, { 0, 0, 1, 9 }, { 0, 0, 2, 153 }, { 0, 0, 3, 19 } };
    const auto path = Mutator::chromosomePath(snap);
    REQUIRE(path.size() == 3);
    CHECK(path[1].first == doctest::Approx(10.0f));
    CHECK(path[1].second == doctest::Approx(0.0f));
    CHECK(path[2].first == doctest::Approx(10.0f).epsilon(0.001));
    CHECK(path[2].second == doctest::Approx(20.0f));
}
