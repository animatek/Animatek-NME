#include "Mutator.h"
#include "Patch.h"

#include <map>

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

        const float offset = (rng.nextFloat() * 2.0f - 1.0f) * range * static_cast<float>(span);
        e.value = juce::jlimit(pd->minValue, pd->maxValue,
                               e.value + juce::roundToInt(offset));
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
                    float probability, juce::Random& rng)
{
    ParamSnapshot out = mother;
    const auto fatherValues = valueMap(father);

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
    for (size_t m = 0; m < 4; ++m)
        out.morphValues[m] = rng.nextBool() ? mother.morphValues[m] : father.morphValues[m];
    return out;
}

}
