#pragma once

#include "PatchVariations.h"
#include "MutationCategories.h"
#include <juce_core/juce_core.h>
#include <functional>
#include <utility>
#include <vector>

class Patch;

/**
 * Patch Mutator core — genetic operations on ParamSnapshots, modeled on the
 * Nord Modular G2 Patch Mutator (Mutate / Randomize / Interpolate / Cross).
 * Pure functions, no UI and no synth I/O.
 */
namespace Mutator
{
    // true = parameter must not be changed (Parameter::locked, module excluded,
    // or quick-locked category). Interpolate deliberately ignores this (G2 spec).
    using LockPredicate = std::function<bool(int section, int moduleId, int paramId)>;

    ParamSnapshot captureCurrent(const Patch& patch);

    /** Range never goes past half the span, as on the G2. */
    constexpr float kMaxRange = 0.5f;

    /** The G2 ties the two knobs: a wide range changes few parameters, a narrow
        one many. Each gives the other; both are fractions [0..1]. */
    float probabilityForRange(float range);
    float rangeForProbability(float probability);

    /** One parameter's mutation: drawn evenly within ±range·span in the shape of
        its curve, coming back in at the edges. Fixed returns the value as is. */
    int mutateValue(int value, int minValue, int maxValue, MutCurve curve, float range, juce::Random& rng);

    /** One parameter drawn anew over its whole range, evenly in its curve's shape. */
    int randomValue(int value, int minValue, int maxValue, MutCurve curve, juce::Random& rng);

    /** Random offsets on src. probability = chance per parameter [0..1],
        range = max offset as a fraction of the parameter span [0..kMaxRange].
        Parameters whose curve is Fixed (switches, modes) are left alone. */
    ParamSnapshot mutate(const ParamSnapshot& src, const Patch& patch,
                         float probability, float range,
                         juce::Random& rng, const LockPredicate& isLocked);

    /** New values over each parameter's whole range; locked and Fixed
        parameters keep their value from src. */
    ParamSnapshot randomize(const ParamSnapshot& src, const Patch& patch,
                            juce::Random& rng, const LockPredicate& isLocked);

    /** The sound drawn as a path: a turtle reads the parameters in pairs, the
        first turning it (value − 63 degrees) and the second walking that far.
        Related sounds walk similar paths. Not scaled: the caller fits it. */
    std::vector<std::pair<float, float>> chromosomePath(const ParamSnapshot& snap);

    /** Linear blend mother→father at position t [0..1]. Ignores locks. */
    ParamSnapshot interpolate(const ParamSnapshot& mother, const ParamSnapshot& father, float t);

    /** Each gene copied verbatim from one parent; the source parent switches
        with `probability` at each gene (sequential) or independently per gene (independent).
        No random value changes. */
    ParamSnapshot cross(const ParamSnapshot& mother, const ParamSnapshot& father,
                        float probability, juce::Random& rng, bool independentCross = false);
}
