#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "PatchCanvasComponent.h"
#include "SlotMdiArea.h"
#include "InspectorPanel.h"
#include "PatchBrowserPanel.h"
#include "StatusBar.h"
#include "PatchHeaderBar.h"
#include "PresetBrowserWindow.h"
#include "ModuleIconBar.h"
#include "../model/ModuleDescriptions.h"

// Custom slot selector panel — shows 4 slot buttons with patch names.
// Mirrors the hardware slot LEDs: fixed = enabled, blinking = focused,
// off = disabled. Ctrl+click a row to toggle that slot's enable state
// (like the original 3.3 editor); plain click moves focus.
class SlotBar : public juce::Component,
                public juce::DragAndDropTarget,
                private juce::Timer
{
public:
    SlotBar();
    ~SlotBar() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void resized() override;

    // A patch dragged out of either browser can be dropped on a slot row to
    // load it there (issue #50). The rows are the target that always works: a
    // slot whose sub-window is closed still has one here.
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    std::function<void(int section, int position, int slot)> onPatchDroppedOnSlot;
    std::function<void(const juce::File& file, int slot)> onPatchFileDroppedOnSlot;

    void setCurrentTab(int index);
    int  getCurrentTabIndex() const { return activeIndex; }
    void setSlotName(int slot, const juce::String& patchName);
    void setSlotsEnabled(const std::array<bool, 4>& enabled);
    void setSlotLocal(int slot, bool local);  // show a "LOCAL" (not-synced) badge

    std::function<void(int)> onSlotChanged;
    std::function<void(int)> onSlotEnableToggled;  // Ctrl+click on this slot
    std::function<void(int)> onSlotViewToggled;  // Right-click: show/hide this slot

private:
    void timerCallback() override;
    juce::Rectangle<int> ledBounds(int slot) const;

    static constexpr int numSlots = 4;
    int activeIndex = 0;
    bool slotEnabledFlags[numSlots] = {};
    bool slotLocalFlags[numSlots] = {};
    bool blinkPhase = false;
    juce::String slotNames[numSlots];  // patch names per slot
    juce::Rectangle<int> slotBounds[numSlots];
    // Row a patch is currently being dragged over, -1 when none. Painted so the
    // drop says where it is going before it happens.
    int dropTargetSlot = -1;
    void updateDropTarget(int slot);
    int slotAt(juce::Point<int> pos) const;

    static constexpr const char* slotLetters[] = { "A", "B", "C", "D" };

    // Keyboard icon SVG path (simplified synth icon)
    void drawSlotIcon(juce::Graphics& g, juce::Rectangle<int> area, bool active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotBar)
};

// Narrow clickable strip with a chevron that collapses/expands the panel it
// sits next to. It stays put when the panel is hidden, so it is also the way
// back.
class PanelToggleStrip : public juce::Component
{
public:
    explicit PanelToggleStrip(bool leftEdge);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void setPanelOpen(bool open);

    std::function<void()> onToggle;

    static constexpr int stripWidth = 14;

private:
    const bool isLeftEdge;      // strip for the left panel, chevron mirrored
    bool panelOpen = true;
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelToggleStrip)
};

class MainLayout : public juce::Component,
                   public juce::DragAndDropContainer
{
public:
    MainLayout(ModuleDescriptions& moduleDescs);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void applyTheme();

    // The four slots live here as sub-windows. There is deliberately no
    // getCanvas() returning "the" canvas: with several on screen, resolving a
    // canvas without naming a slot is wrong by construction (docs/MDI_PLAN.md).
    SlotMdiArea&          getPatchArea()   { return patchArea; }
    void setTheme(const ColorScheme& cs)
    {
        patchArea.forEachCanvas([&cs](int, PatchCanvasComponent& c) { c.setTheme(cs); });
    }
    void setTheme(const ColorScheme& cs, ThemeId id)
    {
        patchArea.forEachCanvas([&cs, id](int, PatchCanvasComponent& c) { c.setTheme(cs, id); });
    }
    InspectorPanel&       getInspector()   { return inspectorPanel; }
    ModuleIconBar&        getModuleIconBar() { return moduleIconBar; }

    // The icon bar is optional: some people work from the text browser or Quick
    // Add and would rather have the pixels (issue #17).
    void setModuleIconBarVisible(bool visible);
    bool isModuleIconBarVisible() const { return moduleIconBarVisible; }
    PatchBrowserPanel&    getPatchBrowser() { return patchBrowserPanel; }
    DiskPresetBrowserPanel& getDiskPresetBrowser() { return diskPresetBrowserPanel; }
    StatusBar&            getStatusBar()   { return statusBar; }
    PatchHeaderBar&       getHeaderBar()   { return headerBar; }
    SlotBar&              getSlotBar()     { return slotBar; }
    void showDiskPresetBrowser();

    // Side panel visibility (issue #38). Collapsing a panel hands its width to
    // the canvas and remembers the width it had, so re-showing restores the
    // size the user had dragged it to. The slot bar rides in the left column,
    // so while that one is hidden slots stay reachable through Ctrl+1..4.
    void setLeftPanelVisible(bool visible);
    void setRightPanelVisible(bool visible);
    bool isLeftPanelVisible() const  { return leftPanelVisible; }
    bool isRightPanelVisible() const { return rightPanelVisible; }

    // Chevron strip clicked: true for the left panel, false for the right one.
    // Routed out so the toggle goes through the same place as the shortcuts and
    // reports in the status bar. Falls back to toggling directly if unwired.
    std::function<void(bool)> onPanelToggleRequested;

    std::function<void(int)> onSlotChanged;  // called with slot index 0-3
    std::function<void(int)> onSlotViewToggled;  // right-click a slot row: show/hide its sub-window
    std::function<void()> onMidiSettingsClicked;
    std::function<void()> onStoreToBankClicked;
    std::function<void()> onLibraryFolderClicked;

private:
    // Left column: inspector + toolbar + slots
    SlotBar           slotBar;
    InspectorPanel    inspectorPanel;

    // Toolbar buttons
    juce::TextButton midiButton { "MIDI" };
    juce::TextButton libraryButton { "Library" };
    juce::TextButton storeButton { "Store" };
    juce::Component   leftColumn;   // groups all left elements

    PanelToggleStrip  leftToggleStrip { true };
    PanelToggleStrip  rightToggleStrip { false };

    ModuleIconBar     moduleIconBar;      // full width, under the header bar
    SlotMdiArea       patchArea;          // centre — the slots as sub-windows
    PatchBrowserPanel patchBrowserPanel;  // right tab — synth patch browser
    DiskPresetBrowserPanel diskPresetBrowserPanel; // right tab — disk presets/snippets
    juce::TabbedComponent rightBrowserTabs { juce::TabbedButtonBar::TabsAtTop };
    StatusBar         statusBar;
    PatchHeaderBar    headerBar;

    // Stretchable layout: [leftColumn | bar | canvas | bar | patchBrowser]
    juce::StretchableLayoutManager layoutManager;
    juce::StretchableLayoutResizerBar resizerBar1 { &layoutManager, 1, true };
    juce::StretchableLayoutResizerBar resizerBar2 { &layoutManager, 3, true };

    bool moduleIconBarVisible = true;
    bool leftPanelVisible  = true;
    bool rightPanelVisible = true;
    int  savedLeftWidth    = 210;   // preferred sizes below, restored on re-show
    int  savedRightWidth   = 220;

    static constexpr int statusBarHeight = 24;
    static constexpr int slotBarHeight   = 100;  // 4 rows × 25px
    static constexpr int toolbarHeight   = 28;
    static constexpr int headerBarHeight = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainLayout)
};
