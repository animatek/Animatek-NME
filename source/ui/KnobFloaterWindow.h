#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"

/**
 * Knob Floater: overview of the synth's 18 assignable knobs plus Pedal,
 * On/Off switch and After touch (wire indices 0-17, 19, 22, 20).
 *
 * Knobs are interactive: dragging one changes the assigned parameter's value
 * through the same callback path the canvas uses. Right-click offers
 * reassignment to a free knob slot.
 */
class KnobFloaterPanel : public juce::Component
{
public:
    KnobFloaterPanel();
    ~KnobFloaterPanel() override;  // out-of-line: KnobCell is incomplete here

    void setPatch(Patch* p);   // nullptr-safe, refreshes all cells
    void refresh();            // re-resolve assignments/values from the patch
    void applyTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Same shapes as the canvas parameter callbacks (section 0/1 only)
    std::function<void(int section, int moduleId, int paramId, int value)> onParameterChanged;
    std::function<void(int section, int moduleId, int paramId, int oldValue, int newValue)> onParameterDragComplete;
    std::function<void(int morphIndex, int value)> onMorphChanged;
    std::function<void(int fromKnob, int toKnob)> onReassignRequested;

private:
    class KnobCell;

    void cellDragStart(int wireIndex);
    void cellValueChanged(int wireIndex, int value);
    void cellDragEnd(int wireIndex);
    void cellRightClicked(int wireIndex);

    Patch* patch = nullptr;
    int draggingKnobIndex = -1;  // wire index being dragged, -1 = none
    int dragOldValue = 0;

    std::vector<std::unique_ptr<KnobCell>> cells;
    juce::TooltipWindow tooltipWindow { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobFloaterPanel)
};

class KnobFloaterWindow : public juce::DocumentWindow
{
public:
    KnobFloaterWindow();

    void setPatch(Patch* p);
    void refresh();
    void closeButtonPressed() override;
    void applyTheme();

    std::function<void(int section, int moduleId, int paramId, int value)> onParameterChanged;
    std::function<void(int section, int moduleId, int paramId, int oldValue, int newValue)> onParameterDragComplete;
    std::function<void(int morphIndex, int value)> onMorphChanged;
    std::function<void(int fromKnob, int toKnob)> onReassignRequested;
    std::function<void()> onClosed;

private:
    KnobFloaterPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobFloaterWindow)
};
