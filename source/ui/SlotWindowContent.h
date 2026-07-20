#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PatchCanvasComponent.h"
#include "InspectorPanel.h"
#include "PatchHeaderBar.h"
#include "../model/ModuleDescriptions.h"

// Slim per-slot content for a SlotWindow pop-out: a canvas + inspector +
// header bar bound to one fixed slot, without the SlotBar/browsers/status
// bar that only make sense in the main window (see the multi-window plan —
// slot windows are an additional view alongside the main window's tabs, not
// a replacement for it).
class SlotWindowContent : public juce::Component
{
public:
    SlotWindowContent();

    void paint(juce::Graphics& g) override;
    void resized() override;

    PatchCanvasComponent& getCanvas()     { return canvas; }
    InspectorPanel&       getInspector()  { return inspector; }
    PatchHeaderBar&       getHeaderBar()  { return headerBar; }

    void setTheme(const ColorScheme& cs) { canvas.setTheme(cs); }

private:
    PatchHeaderBar headerBar;
    InspectorPanel inspector;
    PatchCanvasComponent canvas;

    static constexpr int headerBarHeight = 48;
    static constexpr int inspectorWidth = 210;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotWindowContent)
};
