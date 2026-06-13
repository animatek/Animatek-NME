#pragma once

#include "PatchVariations.h"
#include <juce_core/juce_core.h>
#include <functional>

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

    /** Random offsets on src. probability = chance per parameter [0..1],
        range = max offset as a fraction of the parameter span [0..1]. */
    ParamSnapshot mutate(const ParamSnapshot& src, const Patch& patch,
                         float probability, float range,
                         juce::Random& rng, const LockPredicate& isLocked);

    /** Completely random values; locked params copy their value from src. */
    ParamSnapshot randomize(const ParamSnapshot& src, const Patch& patch,
                            juce::Random& rng, const LockPredicate& isLocked);

    /** Linear blend mother→father at position t [0..1]. Ignores locks. */
    ParamSnapshot interpolate(const ParamSnapshot& mother, const ParamSnapshot& father, float t);

    /** Each gene copied verbatim from one parent; the source parent switches
        with `probability` at each gene (sequential) or independently per gene (independent).
        No random value changes. */
    ParamSnapshot cross(const ParamSnapshot& mother, const ParamSnapshot& father,
                        float probability, juce::Random& rng, bool independentCross = false);
}
