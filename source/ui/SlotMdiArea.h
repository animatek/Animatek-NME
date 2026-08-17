#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>
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
    //
    // Tracked here rather than read back from MultiDocumentPanel. Its
    // setActiveDocument() does not assign the active document: it calls
    // toFront() and lets updateActiveDocumentFromUIState() re-derive it from
    // TopLevelWindow::isActiveWindow(). That works for real desktop windows and
    // not for child windows inside a work area — focusSlot() was silently doing
    // nothing, which is why the outline and the inspector followed the wrong
    // slot after a reorder.
    int getFocusedSlot() const { return isSlotOpen(focusedSlot) ? focusedSlot : -1; }
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
    void setTileMode(TileMode mode);
    void retile();  // back to Auto, and lay out now

    // A sub-window's bounds as fractions of the work area. Persisting these
    // rather than pixels is what lets a Free-mode layout survive a collapsed
    // panel, a resized main window or a different monitor. Empty when the slot
    // is closed. Only meaningful in Free mode; Auto recomputes the tiling.
    juce::Rectangle<float> getNormalisedSlotBounds(int slot) const;
    void setNormalisedSlotBounds(int slot, juce::Rectangle<float> bounds);

    // Which tile each slot sits in. The tiling itself is fixed by how many slots
    // are open, but which slot lands where is the user's to change — the same
    // thing Hyprland's swapwindow and rollnext do within a layout.
    // Directions are geometric, not positions in a list: with four slots the
    // tiles are a 2x2 grid, so "left" has to mean the tile to the left and not
    // the previous one in sequence. Nothing happens at an edge — no wrapping,
    // which is what made "move left" appear to move a slot right.
    enum class Direction { Left, Right, Up, Down };
    void moveFocusedTile(Direction direction);
    void rotateTiles(int direction);       // shift every slot round one tile
    // Back to the canonical arrangement: A, B, C, D reading left to right and
    // top to bottom, in the tiling the open count calls for (#51). Undoes a
    // shuffle, a drag into Free mode and focus mode in one go, which is what
    // makes it useful after loading patches into slots in whatever order.
    // Nothing to do with one slot open: JUCE gives a lone document the whole
    // area with no window frame at all.
    void resetTileOrder();
    // True when resetTileOrder() would visibly change something, so a button can
    // grey itself out rather than lie about being available.
    bool canResetTileOrder() const;
    juce::String getTileOrderString() const;
    void setTileOrderString(const juce::String& order);

    // Slide sub-windows to their new tiles instead of jumping. Off restores the
    // instant behaviour; either way the end state is identical.
    void setAnimated(bool shouldAnimate) { animateLayout = shouldAnimate; }

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
    // The maximise button on a sub-window's title bar. Routed out rather than
    // handled here so it goes through the same place as F11, message and all.
    std::function<void(int)> onSlotMaximiseRequested;
    void requestMaximise(juce::Component* view);
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
    struct WindowWatcher : public juce::ComponentListener,
                           public juce::MouseListener
    {
        explicit WindowWatcher(SlotMdiArea& o) : owner(o) {}
        void componentMovedOrResized(juce::Component& c, bool moved, bool resized) override
        {
            owner.windowMovedOrResized(c, moved, resized);
        }
        // Clicking anywhere in a slot's canvas focuses that slot. Registered on
        // each SlotView with wantsEventsForAllNestedChildComponents, so it does
        // not matter what the click actually lands on.
        void mouseDown(const juce::MouseEvent& e) override { owner.viewClicked(e); }
        SlotMdiArea& owner;
    };
    void viewClicked(const juce::MouseEvent& e);
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
    void applyLayout(bool animate = false, bool useProxy = true);
    void placeWindow(juce::MultiDocumentPanelWindow& window,
                     juce::Rectangle<int> bounds, bool animate, bool useProxy);
    // Open slots in tile order, which is what applyLayout lays out.
    std::vector<int> openSlotsInTileOrder() const;
    // Free mode keeps whatever the user arranged, but in proportion: when the
    // work area changes size the windows scale with it instead of being
    // stranded in a corner.
    void rescaleFreeWindows(juce::Rectangle<int> area);
    // Outline the sub-window you are editing in the theme's primary colour.
    void updateFocusHighlight();
    void reassertFocus(int slot);

    static constexpr int minUsableSize = 120;

    WindowWatcher watcher { *this };
    std::array<std::unique_ptr<SlotView>, numSlots> views;
    bool suppressFocusCallback = false;
    int focusedSlot = -1;
    TileMode tileMode = TileMode::Auto;
    bool focusMode = false;
    // Set while we are the ones moving windows, so our own setBounds calls are
    // not mistaken for the user dragging one and do not drop us into Free.
    bool applyingLayout = false;
    bool animateLayout = true;
    static constexpr int animationMs = 120;
    juce::Rectangle<int> lastArea;
    // A permutation of slot indices: tileOrder[i] is the slot in tile i.
    std::array<int, numSlots> tileOrder { { 0, 1, 2, 3 } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotMdiArea)
};
