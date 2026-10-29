#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModulePresets.h"

// Forward declaration
class AssignmentsListComponent;
class ThemeData;

class InspectorPanel : public juce::Component,
                       public juce::TextEditor::Listener
{
public:
    InspectorPanel();
    ~InspectorPanel() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void applyTheme();

    // Set the current patch (for patch-wide morph view when no module selected)
    void setPatch(Patch* patch);

    // A Micro Modular has three knobs, not the G1's eighteen: its miniature of the front panel
    // shows just those (issue #111). Assignments on the other knobs stay in the patch and in the list.
    void setMicroModel(bool micro);

    // Called when the user selects a module on the canvas
    void setModule(Module* module, int section);
    void clearModule();

    // Callbacks
    std::function<void(int section, Module*, const juce::String& oldName, const juce::String& newName)> onNameChanged;
    // section, module, paramIndex, newGroup (-1=remove)
    std::function<void(int section, Module*, int paramIndex, int morphGroup)> onMorphGroupChanged;
    // section, module, paramIndex, span (0-127), direction (0=+, 1=-)
    std::function<void(int section, Module*, int paramIndex, int span, int direction)> onMorphRangeChanged;
    // section, moduleId, paramId, knobIndex=-1 (deassign)
    std::function<void(int section, int moduleId, int paramId, int knobIndex)> onKnobRemoved;
    // section, moduleId, paramId, midiCC=-1 (deassign)
    std::function<void(int section, int moduleId, int paramId, int midiCC)> onMidiCtrlRemoved;

    // Morph A/B fader carrier knob shown in the patch-wide assignments view.
    void setMorphFaderKnob(int knobIndex, int carrierGroup);
    std::function<void()> onMorphFaderKnobRemove;

    // A parameter edited in the inspector's own Parameters list: live while the
    // value moves, then once for the whole gesture so it undoes in one step.
    // Same pair the canvas and the knob floater use.
    std::function<void(int section, Module*, int paramIndex, int value)> onParameterChanged;
    std::function<void(int section, Module*, int paramIndex, int oldValue, int newValue)> onParameterEditComplete;

    // Called by canvas when a morph assignment changes (so inspector can refresh)
    void refreshMorphList();

    /** Redraws the values without rebuilding the list, for a knob turned
        somewhere else. A rebuild here would drop the row a drag is holding. */
    void repaintValues();

    /** The module face descriptions, so the Parameters list can tell which of a
        module's parameters are buttons on its front and draw them as buttons
        here too, with the same labels the module wears. Without this they all
        fall back to plain numbers, which is what a switch reads worst as. */
    void setThemeData(const ThemeData* themeData);

    // Module presets. The panel only displays and reports clicks; the owner
    // holds the library and performs the recall, save and delete, then calls
    // refreshMorphList() so the section redraws from what was actually written.
    void setPresetLibrary(const ModulePresetLibrary* library);
    // Where the folded/unfolded state of the Presets section is remembered.
    static void setSharedSettings(juce::PropertiesFile* settings);
    // index into the selected module type's preset list
    std::function<void(int section, Module*, int presetIndex)> onPresetRecall;
    std::function<void(int section, Module*, int presetIndex)> onPresetDelete;
    std::function<void(int section, Module*, int presetIndex)> onPresetRename;
    std::function<void(int section, Module*)>                  onPresetSave;

private:
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorFocusLost(juce::TextEditor&) override;
    void commitName();

    // The module on show, named by where it lives rather than by pointer: the
    // patch can destroy it between two repaints of this panel, and it used to
    // be read afterwards (issue #61). resolve() is the only way to read it.
    ModuleRef currentRef;
    Patch*    currentPatch = nullptr;

    Module* currentModule() const
    {
        return currentPatch != nullptr ? currentPatch->getModule(currentRef) : nullptr;
    }
    int currentSection() const { return currentRef.section; }

    // Header
    juce::Label titleLabel;
    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::Label sectionLabel;
    juce::Label dspLabel;      // selected module's DSP cost, right of sectionLabel

    // Assignments list (morphs + knobs + CCs)
    juce::Viewport morphViewport;
    std::unique_ptr<AssignmentsListComponent> assignmentsList;

    static constexpr int margin = 8;
    static constexpr int rowH   = 24;
    static constexpr int moduleHeaderH = 32;   // the "MODULE" heading
    static constexpr int moduleInfoRowH = 28;  // each line of the Module block
    int moduleTopY = 0, moduleBottomY = 0, nameRowY = -1;
    bool microModel = false;    // the synth being edited is a Micro Modular: three knobs
    bool moduleShown = false;   // the Module block is there only while a module is selected

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};
