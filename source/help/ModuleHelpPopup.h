#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModuleHelpData.h"
#include "../ui/FlatCloseButton.h"

/**
 * Floating help popup shown when F1 is pressed with a module selected.
 * Self-owning (heap-allocated). Press F1/Escape or click X to close.
 * Draggable by the title bar area.
 */
class ModuleHelpPopup : public juce::Component
{
public:
    ModuleHelpPopup(const NordHelp::ModuleHelp& help, juce::Component* relativeTo);

    void resized() override;
    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    /** Opens the help popup for the given module fullname.
     *  Returns nullptr if no help data found for that module. */
    static ModuleHelpPopup* show(const juce::String& moduleFullname,
                                 juce::Component* relativeTo);

private:
    juce::Label      titleLabel;
    FlatCloseButton  closeButton;
    juce::Viewport   viewport;
    juce::ComponentDragger dragger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleHelpPopup)
};
