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
class SlotView : public juce::Component,
                 public juce::DragAndDropTarget
{
public:
    explicit SlotView(int slot);

    int getSlot() const { return slot_; }
    PatchCanvasComponent& getCanvas() { return canvas; }

    void resized() override;
    void paintOverChildren(juce::Graphics& g) override;

    // A patch dragged out of the Synth browser can be dropped on this window to
    // load it into this slot (issue #50). The canvas inside is itself a drop
    // target, for module types and snippets, and turns this payload down; JUCE
    // then walks up the parent chain and finds this.
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    std::function<void(int section, int position, int slot)> onPatchDropped;

    void setPatchTitle(const juce::String& patchName);
    // A slot is "local" when its patch is not known to match the synth. The slot
    // bar already badges it; the sub-window title has to say so too, or with
    // four of them tiled you cannot tell which one is out of sync.
    void setLocal(bool isLocal);

private:
    void refreshTitle();

    const int slot_;
    juce::String patchName_;
    bool local_ = false;
    bool dropArmed_ = false;   // a patch is hovering over this window
    PatchCanvasComponent canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotView)
};
