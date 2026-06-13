#include "Mutator.h"
#include "Patch.h"
#include "MutationCategories.h"

#include <map>
#include <cmath>

namespace
{
struct GeneKey
{
    int section, moduleId, paramId;
    bool operator<(const GeneKey& o) const
    {
        if (section != o.section) return section < o.section;
        if (moduleId != o.moduleId) return moduleId < o.moduleId;
        return paramId < o.paramId;
    }
};

const ParameterDescriptor* findDescriptor(const Patch& patch, const ParamSnapshot::Entry& e)
{
    const auto& container = patch.getContainer(e.section);
    const auto* mod = container.getModuleByIndex(e.moduleId);
    if (!mod) return nullptr;
    for (const auto& p : mod->getParameters())
        if (p.getDescriptor() && p.getDescriptor()->index == e.paramId)
            return p.getDescriptor();
    return nullptr;
}

std::map<GeneKey, int> valueMap(const ParamSnapshot& snap)
{
    std::map<GeneKey, int> out;
    for (const auto& e : snap.entries)
        out[{e.section, e.moduleId, e.paramId}] = e.value;
    return out;
}
}

namespace Mutator
{

ParamSnapshot captureCurrent(const Patch& patch)
{
    PatchVariations tmp;
    tmp.captureFrom(patch, 0);
    return tmp.slots[0];
}

ParamSnapshot mutate(const ParamSnapshot& src, const Patch& patch,
                     float probability, float range,
                     juce::Random& rng, const LockPredicate& isLocked)
{
    ParamSnapshot out = src;
    for (auto& e : out.entries)
    {
        if (isLocked && isLocked(e.section, e.moduleId, e.paramId)) continue;
        if (rng.nextFloat() >= probability) continue;

        const auto* pd = findDescriptor(patch, e);
        if (!pd) continue;
        const int span = pd->maxValue - pd->minValue;
        if (span <= 0) continue;

        // Check if this parameter is OscFreq for harmonic mutation
        const auto& container = patch.getContainer(e.section);
        const auto* mod = container.getModuleByIndex(e.moduleId);
        const auto* md = mod ? mod->getDescriptor() : nullptr;
        const bool isOscFreq = md && getCategoryForParam(*md, *pd) == static_cast<int>(MutCategory::OscFreq);

        if (isOscFreq)
        {
            // Harmonic intervals (in semitones): prefer musical ratios
            static const int harmonicIntervals[] = { 0, 7, 12, 19, 24, -7, -12, -5 };
            const int idx = rng.nextInt(static_cast<int>(std::size(harmonicIntervals)));
            const int newVal = juce::jlimit(pd->minValue, pd->maxValue,
                                           e.value + harmonicIntervals[idx]);
            e.value = newVal;
        }
        else
        {
            // Standard Gaussian distribution: prefers small mutations to large ones
            const float u1 = rng.nextFloat() + 1e-7f;  // avoid log(0)
            const float u2 = rng.nextFloat();
            const float gauss = std::sqrt(-2.0f * std::log(u1))
                              * std::cos(2.0f * juce::MathConstants<float>::pi * u2);
            const float sigma = range * static_cast<float>(span) * 0.5f;
            const float offset = gauss * sigma;
            e.value = juce::jlimit(pd->minValue, pd->maxValue,
                                   e.value + juce::roundToInt(offset));
        }
    }
    return out;
}

ParamSnapshot randomize(const ParamSnapshot& src, const Patch& patch,
                        juce::Random& rng, const LockPredicate& isLocked)
{
    ParamSnapshot out = src;
    for (auto& e : out.entries)
    {
        if (isLocked && isLocked(e.section, e.moduleId, e.paramId)) continue;

        const auto* pd = findDescriptor(patch, e);
        if (!pd) continue;
        const int span = pd->maxValue - pd->minValue;
        if (span <= 0) continue;

        e.value = pd->minValue + rng.nextInt(span + 1);
    }
    return out;
}

ParamSnapshot interpolate(const ParamSnapshot& mother, const ParamSnapshot& father, float t)
{
    ParamSnapshot out = mother;
    const auto fatherValues = valueMap(father);

    for (auto& e : out.entries)
    {
        auto it = fatherValues.find({e.section, e.moduleId, e.paramId});
        if (it == fatherValues.end()) continue;
        e.value = juce::roundToInt(static_cast<float>(e.value)
                                   + (static_cast<float>(it->second - e.value)) * t);
    }
    for (size_t m = 0; m < 4; ++m)
        out.morphValues[m] = juce::roundToInt(
            static_cast<float>(mother.morphValues[m])
            + static_cast<float>(father.morphValues[m] - mother.morphValues[m]) * t);
    return out;
}

ParamSnapshot cross(const ParamSnapshot& mother, const ParamSnapshot& father,
                    float probability, juce::Random& rng, bool independentCross)
{
    ParamSnapshot out = mother;
    const auto fatherValues = valueMap(father);

    if (independentCross)
    {
        // Each gene independently picks father with probability p
        for (auto& e : out.entries)
        {
            if (rng.nextFloat() < probability)
            {
                auto it = fatherValues.find({e.section, e.moduleId, e.paramId});
                if (it != fatherValues.end())
                    e.value = it->second;
            }
        }
    }
    else
    {
        // Sequential: parent source switches with probability p at each gene
        bool useMother = rng.nextBool();
        for (auto& e : out.entries)
        {
            if (rng.nextFloat() < probability)
                useMother = !useMother;
            if (useMother) continue;

            auto it = fatherValues.find({e.section, e.moduleId, e.paramId});
            if (it != fatherValues.end())
                e.value = it->second;
        }
    }
    for (size_t m = 0; m < 4; ++m)
        out.morphValues[m] = rng.nextBool() ? mother.morphValues[m] : father.morphValues[m];
    return out;
}

}
