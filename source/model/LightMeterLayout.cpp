#include "LightMeterLayout.h"
#include <algorithm>

namespace LightMeterLayout
{

const ModuleSlots* Table::find(int section, int containerIndex) const
{
    const auto it = byModule.find(key(section, containerIndex));
    return it == byModule.end() ? nullptr : &ranges[it->second];
}

std::uint64_t fingerprint(const Patch* patch, const ThemeData* theme)
{
    // FNV-1a, in unsigned arithmetic: the multiply is meant to wrap round, and
    // wrapping a signed integer is undefined behaviour rather than a hash.
    std::uint64_t hash = 1469598103934665603ULL;   // offset basis
    auto fold = [&hash](std::uint64_t value)
    {
        hash = (hash ^ value) * 1099511628211ULL;
    };

    fold(reinterpret_cast<std::uintptr_t>(patch));
    fold(reinterpret_cast<std::uintptr_t>(theme));

    if (patch == nullptr)
        return hash;

    auto foldSection = [&](const ModuleContainer& container, int sec)
    {
        fold(static_cast<std::uint64_t>(sec));
        fold(static_cast<std::uint64_t>(container.getModules().size()));
        for (auto& m : container.getModules())
        {
            fold(static_cast<std::uint64_t>(m->getContainerIndex()));
            fold(static_cast<std::uint64_t>(
                m->getDescriptor() != nullptr ? m->getDescriptor()->index : -1));
        }
    };

    foldSection(patch->getPolyVoiceArea(), 1);
    foldSection(patch->getCommonArea(), 0);
    return hash;
}

Table build(const Patch* patch, const ThemeData* theme)
{
    Table table;
    table.fingerprint = fingerprint(patch, theme);

    if (patch == nullptr || theme == nullptr)
        return table;

    // Wire order: poly (section 1) first, then common, each sorted by
    // container index.
    struct ModuleRef { const Module* mod; int section; };
    std::vector<ModuleRef> ordered;

    auto addSection = [&](const ModuleContainer& container, int sec)
    {
        const size_t start = ordered.size();
        for (auto& m : container.getModules())
            ordered.push_back({ m.get(), sec });
        std::sort(ordered.begin() + static_cast<std::ptrdiff_t>(start), ordered.end(),
                  [](const ModuleRef& a, const ModuleRef& b) {
                      return a.mod->getContainerIndex() < b.mod->getContainerIndex();
                  });
    };

    addSection(patch->getPolyVoiceArea(), 1);
    addSection(patch->getCommonArea(), 0);

    int lightBase = 0;
    int meterBase = 0;
    for (auto& ref : ordered)
    {
        int lightCount = 0;
        int meterCount = 0;

        const auto compId = ref.mod->getDescriptor() != nullptr
                          ? ref.mod->getDescriptor()->componentId : juce::String();

        if (const ModuleTheme* mt = theme->getModuleTheme(compId))
        {
            bool hasMeterOrLedArray = false;
            int meterSlots = 0;
            for (auto& light : mt->lights)
            {
                if (light.type == "meter" || light.type == "led-array")
                {
                    hasMeterOrLedArray = true;
                    if (light.type == "meter")
                        ++meterSlots;
                }
                if (light.type == "led")
                    ++lightCount;
            }

            // NOMAD registers meters and sequencer led-arrays as MeterMessage
            // pairs. A single led-array/single meter still consumes two slots.
            if (hasMeterOrLedArray)
                meterCount = juce::jmax(2, meterSlots);
        }

        const int containerIndex = ref.mod->getContainerIndex();
        table.byModule[key(ref.section, containerIndex)] = table.ranges.size();
        table.ranges.push_back({ ref.section, containerIndex,
                                 lightBase, lightCount, meterBase, meterCount });
        lightBase += lightCount;
        meterBase += meterCount;
    }
    return table;
}

}
