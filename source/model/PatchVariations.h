#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

class Patch;

// Full capture of the patch's parameter state (one "variation").
struct ParamSnapshot
{
    struct Entry { int section, moduleId, paramId, value; };
    std::vector<Entry> entries;
    std::array<int, 4> morphValues { 0, 0, 0, 0 };
    bool filled = false;
};

// The 8 variations of one patch slot, plus per-module mutation exclusions
// (used by the Patch Mutator). Persisted in a "<patch>.var" sidecar file so
// the .pch itself stays 100% compatible with the original editors.
class PatchVariations
{
public:
    static constexpr int kNumSlots = 8;

    ParamSnapshot slots[kNumSlots];
    int activeIndex = -1;
    std::vector<std::pair<int, int>> mutationExcluded;  // {section, containerIndex}

    void captureFrom(const Patch& patch, int slot);
    void copySlot(int from, int to);
    void initFromDefaults(const Patch& patch, int slot);

    // Write-through: live parameter edits land in the active variation.
    void updateValue(int section, int moduleId, int paramId, int value);

    void clear();
    bool anyFilled() const;
};

bool saveVarFile(const PatchVariations& vars, const juce::File& file);
bool loadVarFile(PatchVariations& vars, const juce::File& file);
