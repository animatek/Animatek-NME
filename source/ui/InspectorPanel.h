#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"

// Forward declaration
class AssignmentsListComponent;

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

    // Called when the user selects a module on the canvas
    void setModule(Module* module, int section);
    void clearModule();

    // Callbacks
    std::function<void(int section, Module*, const juce::String&)> onNameChanged;
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

    // Called by canvas when a morph assignment changes (so inspector can refresh)
    void refreshMorphList();

private:
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorFocusLost(juce::TextEditor&) override;
    void commitName();

    Module* currentModule = nullptr;
    Patch*  currentPatch  = nullptr;
    int currentSection = -1;

    // Header
    juce::Label titleLabel;
    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::Label sectionLabel;

    // Assignments list (morphs + knobs + CCs)
    juce::Viewport morphViewport;
    std::unique_ptr<AssignmentsListComponent> assignmentsList;

    static constexpr int margin = 8;
    static constexpr int rowH   = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};
