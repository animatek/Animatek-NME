#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/PatchVariations.h"
#include "../model/MutationCategories.h"
#include <functional>
#include <vector>
#include <memory>

/**
 * Patch Mutator floater — interactive evolution of patch parameter sets,
 * modeled on the Nord Modular G2 Patch Mutator (Mother + 6 Children + Father,
 * Mutate/Randomize/Interpolate/Cross, Temporary Storage, Variations row,
 * Quick Locks).
 *
 * The panel owns only ParamSnapshots; all patch access goes through callbacks
 * into MainComponent (safe across slot switches and patch loads).
 */
class MutatorPanel : public juce::Component,
                     public juce::DragAndDropContainer
{
public:
    enum class GenOp { Mutate, Randomize, Interpolate, Cross };

    struct GenParams
    {
        float mutateProb  = 0.30f;
        float mutateRange = 0.30f;
        float crossProb   = 0.15f;
        float interpT     = 0.0f;
        bool lock[kNumMutCategories] = {};
        bool solo[kNumMutCategories] = {};
    };

    MutatorPanel();
    ~MutatorPanel() override;

    std::function<ParamSnapshot()> onCaptureCurrent;
    std::function<ParamSnapshot(GenOp, const ParamSnapshot& mother,
                                const ParamSnapshot& father,
                                const GenParams&)> onGenerate;
    std::function<void(const ParamSnapshot&, float auditionSeconds)> onAudition;
    std::function<void(int index, const ParamSnapshot&)> onStoreToVariation;

    void clearAll();      // call when the underlying patch changes
    void setVariations(const PatchVariations& vars);  // sync the Variations row
    void applyTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class SnapshotBox;
    class KnobLookAndFeel;

    // Box indices: 0 = Mother, 1..6 = Children, 7 = Father,
    // 8..31 = Temp Storage, 32..39 = Variations mirror
    static constexpr int kMother = 0;
    static constexpr int kFather = 7;
    static constexpr int kTempFirst = 8;
    static constexpr int kVarFirst = 32;
    static constexpr int kNumBoxes = 40;

    SnapshotBox& box(int i) { return *boxes[static_cast<size_t>(i)]; }
    GenParams currentParams(float interpT = 0.0f) const;
    float auditionSeconds() const;

    void focusBox(int index, bool audition);
    void boxDropped(int srcIndex, int dstIndex, const juce::ModifierKeys& mods);
    void showBoxMenu(int index);
    void copyToParent(int srcIndex, int parentIndex);
    void saveFocusedToTemp();
    void generate(GenOp op);
    void ensureMother();

    std::vector<std::unique_ptr<SnapshotBox>> boxes;
    int focusedIndex = -1;

    std::unique_ptr<KnobLookAndFeel> knobLnF;

    juce::TextButton motherFromPatchBtn { "Mother = Patch" };
    juce::TextButton mutateBtn      { "Mutate" };
    juce::TextButton randomizeBtn   { "Randomize" };
    juce::TextButton interpolateBtn { "Interpolate" };
    juce::TextButton crossBtn       { "Cross" };

    juce::Slider probSlider, rangeSlider, crossProbSlider;
    juce::Label probLabel, rangeLabel, crossProbLabel;
    juce::Label tempLabel, variationsLabel, quickLockLabel;
    juce::ToggleButton linkToggle { "Link" };
    bool adjustingLinked = false;

    juce::Label auditionLabel;
    juce::ComboBox auditionTimeBox;

    std::unique_ptr<juce::TextButton> lockBtn[kNumMutCategories];
    std::unique_ptr<juce::TextButton> soloBtn[kNumMutCategories];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MutatorPanel)
};

class MutatorWindow : public juce::DocumentWindow
{
public:
    MutatorWindow();

    MutatorPanel& getPanel() { return panel; }
    void closeButtonPressed() override;
    void applyTheme();

    std::function<void()> onClosed;

    // App-wide shortcuts (Ctrl+1..8) forwarded while this floater has focus
    std::function<bool(const juce::KeyPress&)> onGlobalKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        return (onGlobalKey && onGlobalKey(key)) || juce::DocumentWindow::keyPressed(key);
    }

private:
    MutatorPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MutatorWindow)
};
