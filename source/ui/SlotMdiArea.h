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
    // Fired after a sub-window has actually gone, including from its own close
    // button, which bypasses closeSlot() entirely.
    std::function<void(int)> onSlotClosed;

    void activeDocumentChanged() override;
    void tryToCloseDocumentAsync(juce::Component*, std::function<void(bool)> callback) override;
    juce::MultiDocumentPanelWindow* createNewDocumentWindow() override;
    void resized() override;

   #if JUCE_MODAL_LOOPS_PERMITTED
    bool tryToCloseDocument(juce::Component*) override { return true; }
   #endif

private:
    // The container window a slot's view currently sits in, or null when the
    // slot is closed or the panel is in its single-document fullscreen mode.
    juce::MultiDocumentPanelWindow* windowFor(int slot) const;
    // Give a sub-window usable bounds if it has none. JUCE sizes a new window to
    // its content, and a SlotView that has never been laid out measures 0x0,
    // which lands on screen as a sliver of title bar.
    void giveUsableBounds(juce::MultiDocumentPanelWindow& window, int slot);

    static constexpr int minUsableSize = 120;

    std::array<std::unique_ptr<SlotView>, numSlots> views;
    bool suppressFocusCallback = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotMdiArea)
};
