#include "PatchVariations.h"
#include "Patch.h"

namespace
{
void captureContainer(const ModuleContainer& container, int section, ParamSnapshot& snap,
                      bool useDefaults)
{
    for (auto& modPtr : container.getModules())
    {
        if (!modPtr) continue;
        for (auto& param : modPtr->getParameters())
        {
            auto* pd = param.getDescriptor();
            if (!pd || pd->paramClass != "parameter") continue;
            snap.entries.push_back({section, modPtr->getContainerIndex(), pd->index,
                                    useDefaults ? pd->defaultValue : param.getValue()});
        }
    }
}

void capture(const Patch& patch, ParamSnapshot& snap, bool useDefaults)
{
    snap.entries.clear();
    captureContainer(patch.getPolyVoiceArea(), 1, snap, useDefaults);
    captureContainer(patch.getCommonArea(), 0, snap, useDefaults);
    snap.morphValues = useDefaults ? std::array<int, 4>{0, 0, 0, 0} : patch.morphValues;
    snap.filled = true;
}
}

void PatchVariations::captureFrom(const Patch& patch, int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;
    capture(patch, slots[slot], false);
    activeIndex = slot;
}

void PatchVariations::copySlot(int from, int to)
{
    if (from < 0 || from >= kNumSlots || to < 0 || to >= kNumSlots || from == to) return;
    if (!slots[from].filled) return;
    slots[to] = slots[from];
}

void PatchVariations::initFromDefaults(const Patch& patch, int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;
    capture(patch, slots[slot], true);
}

void PatchVariations::updateValue(int section, int moduleId, int paramId, int value)
{
    if (activeIndex < 0 || activeIndex >= kNumSlots) return;
    auto& snap = slots[activeIndex];
    if (!snap.filled) return;
    for (auto& e : snap.entries)
    {
        if (e.section == section && e.moduleId == moduleId && e.paramId == paramId)
        {
            e.value = value;
            return;
        }
    }
}

void PatchVariations::clear()
{
    for (auto& s : slots)
    {
        s.entries.clear();
        s.morphValues = {0, 0, 0, 0};
        s.filled = false;
    }
    activeIndex = -1;
    mutationExcluded.clear();
}

bool PatchVariations::anyFilled() const
{
    for (auto& s : slots)
        if (s.filled) return true;
    return false;
}

// ─── Sidecar file IO ─────────────────────────────────────────────────────────

bool saveVarFile(const PatchVariations& vars, const juce::File& file)
{
    juce::String out;
    out << "[VarFile]\n";
    out << "Version=1\n";
    out << "Active=" << vars.activeIndex << "\n";

    for (int i = 0; i < PatchVariations::kNumSlots; ++i)
    {
        const auto& s = vars.slots[i];
        if (!s.filled) continue;
        out << "[Variation " << i << "]\n";
        out << "Morph=" << s.morphValues[0] << " " << s.morphValues[1] << " "
            << s.morphValues[2] << " " << s.morphValues[3] << "\n";
        for (const auto& e : s.entries)
            out << e.section << " " << e.moduleId << " " << e.paramId << " " << e.value << "\n";
    }

    if (!vars.mutationExcluded.empty())
    {
        out << "[MutationExclude]\n";
        for (const auto& [section, moduleId] : vars.mutationExcluded)
            out << section << " " << moduleId << "\n";
    }

    out << "[End]\n";
    return file.replaceWithText(out);
}

bool loadVarFile(PatchVariations& vars, const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());

    vars.clear();

    enum class Mode { None, Variation, Exclude };
    Mode mode = Mode::None;
    ParamSnapshot* current = nullptr;
    bool valid = false;

    for (const auto& raw : lines)
    {
        auto line = raw.trim();
        if (line.isEmpty()) continue;

        if (line == "[VarFile]")     { valid = true; continue; }
        if (!valid)                  return false;
        if (line == "[End]")         break;
        if (line.startsWith("Version=")) continue;

        if (line.startsWith("Active="))
        {
            int idx = line.fromFirstOccurrenceOf("=", false, false).getIntValue();
            vars.activeIndex = (idx >= 0 && idx < PatchVariations::kNumSlots) ? idx : -1;
            continue;
        }
        if (line.startsWith("[Variation "))
        {
            int idx = line.fromFirstOccurrenceOf("[Variation ", false, false)
                          .upToFirstOccurrenceOf("]", false, false).getIntValue();
            if (idx < 0 || idx >= PatchVariations::kNumSlots) { current = nullptr; mode = Mode::None; continue; }
            current = &vars.slots[idx];
            current->filled = true;
            mode = Mode::Variation;
            continue;
        }
        if (line == "[MutationExclude]") { mode = Mode::Exclude; current = nullptr; continue; }

        if (mode == Mode::Variation && current != nullptr)
        {
            if (line.startsWith("Morph="))
            {
                auto tokens = juce::StringArray::fromTokens(
                    line.fromFirstOccurrenceOf("=", false, false), " ", "");
                for (int i = 0; i < 4 && i < tokens.size(); ++i)
                    current->morphValues[static_cast<size_t>(i)] = tokens[i].getIntValue();
                continue;
            }
            auto tokens = juce::StringArray::fromTokens(line, " ", "");
            if (tokens.size() == 4)
                current->entries.push_back({tokens[0].getIntValue(), tokens[1].getIntValue(),
                                            tokens[2].getIntValue(), tokens[3].getIntValue()});
        }
        else if (mode == Mode::Exclude)
        {
            auto tokens = juce::StringArray::fromTokens(line, " ", "");
            if (tokens.size() == 2)
                vars.mutationExcluded.emplace_back(tokens[0].getIntValue(), tokens[1].getIntValue());
        }
    }

    // Active index must point at a filled slot
    if (vars.activeIndex >= 0 && !vars.slots[vars.activeIndex].filled)
        vars.activeIndex = -1;

    return valid;
}
