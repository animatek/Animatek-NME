#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PatchCanvasComponent.h"

// One slot's patch canvas, as an MDI document inside the main window's work
// area (see docs/MDI_PLAN.md). It deliberately carries nothing but the canvas:
// the inspector, header bar, browsers and status bar stay shared in the main
// window and follow whichever slot has focus.
//
// The title shown in the sub-window's caption is just this component's name —
// MultiDocumentPanel listens for name changes and mirrors them into the
// container window, so setPatchTitle() is all that is needed to retitle it.
class SlotView : public juce::Component
{
public:
    explicit SlotView(int slot);

    int getSlot() const { return slot_; }
    PatchCanvasComponent& getCanvas() { return canvas; }

    void resized() override;

    void setPatchTitle(const juce::String& patchName);

private:
    const int slot_;
    PatchCanvasComponent canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotView)
};
