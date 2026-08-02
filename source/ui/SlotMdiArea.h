#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include "SlotView.h"

// The main window's central work area: the four slots as internal sub-windows,
// the way the original Clavia editor and Nomad arranged patches (docs/MDI_PLAN.md).
//
// All four SlotViews exist for the whole session whether or not they are on
// screen, so a slot's canvas keeps its zoom, scroll and selection while closed
// and, more importantly, every per-slot callback can be wired exactly once at
// startup against a canvas that never moves.
class SlotMdiArea : public juce::MultiDocumentPanel
{
public:
    static constexpr int numSlots = 4;

    SlotMdiArea();
    ~SlotMdiArea() override;

    PatchCanvasComponent& getCanvas(int slot)  { return views[(size_t) slot]->getCanvas(); }
    SlotView&             getView(int slot)    { return *views[(size_t) slot]; }

    void openSlot(int slot);
    void closeSlot(int slot);
    bool isSlotOpen(int slot) const;
    void focusSlot(int slot);

    // -1 when no slot is open.
    int getFocusedSlot() const;

    // Phase 1 of the MDI keeps exactly one slot on screen, filling the area,
    // which is how the editor behaves today. Phase 2 opens several at once and
    // this collapses into openSlot() + focusSlot().
    void showOnlySlot(int slot);

    // Every slot's canvas, open or not. Editor-wide settings (theme, the F5-F10
    // overlay modes) have to reach the closed ones too, or a canvas comes back
    // on screen still drawing the previous state.
    template <typename Fn>
    void forEachCanvas(Fn&& fn)
    {
        for (auto& v : views)
            fn(v->getSlot(), v->getCanvas());
    }

    std::function<void(int)> onSlotFocused;

    void activeDocumentChanged() override;

    void tryToCloseDocumentAsync(juce::Component*, std::function<void(bool)> callback) override
    {
        // Closing a sub-window only takes the slot off screen; its patch, undo
        // history and variations stay exactly where they are. So there is never
        // anything to save first.
        if (callback)
            callback(true);
    }

   #if JUCE_MODAL_LOOPS_PERMITTED
    bool tryToCloseDocument(juce::Component*) override { return true; }
   #endif

private:
    std::array<std::unique_ptr<SlotView>, numSlots> views;
    bool suppressFocusCallback = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotMdiArea)
};
