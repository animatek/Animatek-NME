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

    // The A-B Snapshot (the header bar's morph fader): its two ends and the
    // control that drives it. It belongs to the patch like the variations do,
    // so it is saved with them and travels with a slot set (issue #119).
    ParamSnapshot morphA, morphB;
    struct MorphFader
    {
        int section = -1, module = -1, param = -1;   // the control driving it, -1: none
        int knobIndex = -1;                          // front-panel knob assigned as its carrier, -1: none
        int min = 0, max = 127;                      // range of that control
    } morphFader;
    float morphPos = 0.0f;                           // where the fader was left, 0 = A, 1 = B

    bool hasMorphAB() const { return morphA.filled || morphB.filled || morphFader.param >= 0; }

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

// The same text the .var file holds, so the extras store can keep variations
// without a second serialiser. `varFromText` reads what `varToText` wrote.
juce::String varToText(const PatchVariations& vars);
bool varFromText(PatchVariations& vars, const juce::StringArray& lines);
