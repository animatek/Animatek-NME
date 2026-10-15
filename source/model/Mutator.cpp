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
    // Index alone does not identify a parameter: the "custom" display-units
    // entries (e.g. "freq display units", min 0 max 2) share index 0 with the
    // module's first real parameter and come first in the list. Matching without
    // the class filter picked those up and clamped oscillator pitch and filter
    // cutoff into 0..2 — the snapshot only ever holds "parameter" entries, and
    // Module::getParameter filters the same way.
    for (const auto& p : mod->getParameters())
        if (p.getDescriptor() && p.getDescriptor()->index == e.paramId
            && p.getDescriptor()->paramClass == "parameter")
            return p.getDescriptor();
    return nullptr;
}

// Each shape is a map of the parameter's range [0, top] onto itself; a change is
// drawn evenly on the map's side, so it comes out finer where the map stretches.
// The maps are odd around 0, so a span that crosses the bottom edge still maps.
double toShape(MutCurve curve, double x, double top)
{
    const double s = x < 0.0 ? -1.0 : 1.0;
    const double a = std::abs(x) / top;
    switch (curve)
    {
        case MutCurve::FineLow:  return s * top * std::pow(a, 0.4);
        case MutCurve::FineHigh: return s * top * std::pow(a, 2.5);
        case MutCurve::Centered: return top * 0.5 * (1.0 + std::cbrt(2.0 * x / top - 1.0));
        default:                 return x;
    }
}

double fromShape(MutCurve curve, double w, double top)
{
    const double s = w < 0.0 ? -1.0 : 1.0;
    const double a = std::abs(w) / top;
    switch (curve)
    {
        case MutCurve::FineLow:  return s * top * std::pow(a, 2.5);
        case MutCurve::FineHigh: return s * top * std::pow(a, 0.4);
        case MutCurve::Centered: { const double u = 2.0 * w / top - 1.0; return top * 0.5 * (1.0 + u * u * u); }
        default:                 return w;
    }
}

MutCurve curveOf(const Patch& patch, const ParamSnapshot::Entry& e, const ParameterDescriptor** pdOut)
{
    *pdOut = findDescriptor(patch, e);
    const auto* mod = patch.getContainer(e.section).getModuleByIndex(e.moduleId);
    const auto* md = mod ? mod->getDescriptor() : nullptr;
    return (*pdOut && md) ? getMutationCurve(*md, **pdOut) : MutCurve::Fixed;
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

float probabilityForRange(float range)
{
    const double p = 0.22 - 0.78 * (std::sqrt(static_cast<double>(range)) - 0.7) / 0.53;
    return static_cast<float>(juce::jlimit(0.0, 1.0, p * p));
}

float rangeForProbability(float probability)
{
    const double r = 0.7 - 0.53 * (std::sqrt(static_cast<double>(probability)) - 0.22) / 0.78;
    return static_cast<float>(juce::jlimit(0.0, static_cast<double>(kMaxRange), r * r));
}

int mutateValue(int value, int minValue, int maxValue, MutCurve curve, float range, juce::Random& rng)
{
    const int span = maxValue - minValue;
    if (curve == MutCurve::Fixed || span <= 0)
        return value;

    // The value is the middle of its step, so truncating back does not drift down.
    const double top = span + 0.999;
    const double x = (value - minValue) + 0.5;
    const double reach = static_cast<double>(range) * span;
    const double lo = toShape(curve, x - reach, top);
    const double hi = toShape(curve, x + reach, top);
    double y = fromShape(curve, lo + (hi - lo) * rng.nextDouble(), top);

    // Past an edge it comes back in, as the G2 does, rather than piling up on the edge.
    if (y < 0.0)  y = -y;
    if (y > top)  y = 2.0 * top - y;
    return juce::jlimit(minValue, maxValue, minValue + static_cast<int>(std::floor(juce::jlimit(0.0, top, y))));
}

int randomValue(int value, int minValue, int maxValue, MutCurve curve, juce::Random& rng)
{
    const int span = maxValue - minValue;
    if (curve == MutCurve::Fixed || span <= 0)
        return value;
    const double top = span + 0.999;
    const double w = toShape(curve, 0.0, top)
                   + (toShape(curve, top, top) - toShape(curve, 0.0, top)) * rng.nextDouble();
    return juce::jlimit(minValue, maxValue, minValue + static_cast<int>(std::floor(fromShape(curve, w, top))));
}

ParamSnapshot mutate(const ParamSnapshot& src, const Patch& patch,
                     float probability, float range,
                     juce::Random& rng, const LockPredicate& isLocked)
{
    ParamSnapshot out = src;
    for (auto& e : out.entries)
    {
        if (isLocked && isLocked(e.section, e.moduleId, e.paramId)) continue;

        const ParameterDescriptor* pd = nullptr;
        const auto curve = curveOf(patch, e, &pd);
        if (curve == MutCurve::Fixed) continue;
        if (rng.nextFloat() >= probability) continue;

        e.value = mutateValue(e.value, pd->minValue, pd->maxValue, curve, range, rng);
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

        const ParameterDescriptor* pd = nullptr;
        const auto curve = curveOf(patch, e, &pd);
        if (curve == MutCurve::Fixed) continue;

        e.value = randomValue(e.value, pd->minValue, pd->maxValue, curve, rng);
    }
    return out;
}

std::vector<std::pair<float, float>> chromosomePath(const ParamSnapshot& snap)
{
    std::vector<std::pair<float, float>> path;
    if (!snap.filled || snap.entries.empty())
        return path;

    path.reserve(snap.entries.size() / 2 + 2);
    float x = 0.0f, y = 0.0f;
    float heading = 0.0f;   // degrees
    path.emplace_back(x, y);
    for (size_t i = 0; i + 1 < snap.entries.size(); i += 2)
    {
        heading += static_cast<float>(snap.entries[i].value - 63);
        const float step = static_cast<float>(snap.entries[i + 1].value) + 1.0f;
        const float rad = juce::degreesToRadians(heading);
        x += step * std::cos(rad);
        y += step * std::sin(rad);
        path.emplace_back(x, y);
    }
    return path;
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
