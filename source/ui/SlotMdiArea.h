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
    int getNumOpenSlots() const;

    // Dynamic tiling, the way niri and Hyprland do it: the layout is a function
    // of how many sub-windows are open, not something the user arranges. One
    // fills the area, two split it in half, three in thirds, four go 2x2. It
    // re-flows on every open, close and resize.
    //
    // Free is what dragging or resizing a window drops you into — from then on
    // the windows stay where they were put, until retile() is asked for.
    enum class TileMode { Auto, Free };

    TileMode getTileMode() const { return tileMode; }
    void retile();  // back to Auto, and lay out now

    // Focus mode: the focused sub-window fills the area while the others stay
    // tiled behind it. Toggling it off drops straight back into the tiling that
    // was there before, so it costs nothing to look at one patch up close.
    void setFocusMode(bool shouldBeFocused);
    bool isFocusMode() const { return focusMode; }

    // Re-read the palette: the panel background, each sub-window's background
    // (captured when it was created) and the focus outline.
    void applyTheme();

    // Which slots are open, the tile mode or the focus mode changed — the View
    // menu's tick marks are stale until this fires.
    std::function<void()> onLayoutChanged;

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
    // juce::MultiDocumentPanel inherits ComponentListener privately, so this
    // class cannot be registered as one. A forwarder does the job.
    struct WindowWatcher : public juce::ComponentListener
    {
        explicit WindowWatcher(SlotMdiArea& o) : owner(o) {}
        void componentMovedOrResized(juce::Component& c, bool moved, bool resized) override
        {
            owner.windowMovedOrResized(c, moved, resized);
        }
        SlotMdiArea& owner;
    };
    void windowMovedOrResized(juce::Component&, bool wasMoved, bool wasResized);

    // The container window a slot's view currently sits in, or null when the
    // slot is closed or the panel is in its single-document fullscreen mode.
    juce::MultiDocumentPanelWindow* windowFor(int slot) const;
    // Give a sub-window usable bounds if it has none. JUCE sizes a new window to
    // its content, and a SlotView that has never been laid out measures 0x0,
    // which lands on screen as a sliver of title bar.
    void giveUsableBounds(juce::MultiDocumentPanelWindow& window, int slot);
    // Re-flow the open sub-windows for the current mode and count. No-op in Free
    // mode, and with fewer than two open (JUCE gives a lone document the whole
    // area itself, with no window frame at all).
    void applyLayout();
    // Outline the sub-window you are editing in the theme's accent colour.
    void updateFocusHighlight();

    static constexpr int minUsableSize = 120;

    WindowWatcher watcher { *this };
    std::array<std::unique_ptr<SlotView>, numSlots> views;
    bool suppressFocusCallback = false;
    TileMode tileMode = TileMode::Auto;
    bool focusMode = false;
    // Set while we are the ones moving windows, so our own setBounds calls are
    // not mistaken for the user dragging one and do not drop us into Free.
    bool applyingLayout = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotMdiArea)
};
