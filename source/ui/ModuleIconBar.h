#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/ModuleDescriptions.h"

// The original editor's module palette: a row of category tabs with the modules
// of the chosen category underneath, dragged straight onto the patch area.
// Long-time users navigate that bar by muscle memory, and until now the editor
// offered only the text tree and Quick Add (issue #17).
//
// Each module is a thin outlined chip carrying its name. Pictograms are what the
// original shows and what this will eventually show, but only once the artwork
// is ours and follows the theme (issue #52); recycling Nomad's coloured discs in
// the meantime would have looked like someone else's editor.
class ModuleIconBar : public juce::Component
{
public:
    static constexpr int tabRowHeight  = 20;
    static constexpr int chipRowHeight = 27;
    static constexpr int chipHeight    = 18;
    static constexpr int chipPadding   = 9;   // each side of the label
    static constexpr int chipGap       = 4;
    static constexpr int chipTopGap    = 3;   // the rest falls below, so the bar breathes
    static constexpr int separatorHeight = 1; // hairline between the bar and the canvas
    /** Height the bar wants: one tab row, one row of chips, and the separator. */
    static constexpr int preferredHeight = tabRowHeight + chipRowHeight + separatorHeight;

    ModuleIconBar();

    void setModuleDescriptions(const ModuleDescriptions* descriptions);

    /** The category tab currently showing, so it can be restored next session. */
    juce::String getSelectedCategory() const;
    void setSelectedCategory(const juce::String& category);
    /** Fired when the user picks a different tab, so the choice can be saved. */
    std::function<void(const juce::String&)> onCategoryChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void applyTheme();

private:
    // The chips live in a viewport so a narrow window scrolls rather than
    // clipping: the oscillator tab alone holds sixteen.
    class ChipStrip : public juce::Component
    {
    public:
        explicit ChipStrip(ModuleIconBar& o) : owner(o) {}

        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        void setModules(std::vector<const ModuleDescriptor*> mods);
        /** The ANME tab: what this editor adds to the patch that Clavia never
            shipped. Nothing here has a module descriptor. */
        void setEditorExtras();

    private:
        struct Chip
        {
            const ModuleDescriptor* descriptor = nullptr;  // null for the ANME chips
            juce::String label;
            juce::Rectangle<int> bounds;
        };

        void layOutChips();
        int chipIndexAt(juce::Point<int> p) const;
        /** The chip on its own, for the pointer to carry while dragging. */
        juce::Image renderChipImage(const Chip& chip) const;

        ModuleIconBar& owner;
        std::vector<Chip> chips;
        int hoverIndex = -1;
        bool dragStarted = false;
    };

    void rebuildTabs();
    void showCategory(int tabIndex);

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    const ModuleDescriptions* moduleDescs = nullptr;
    juce::StringArray categories;      // as they appear in modules.xml
    juce::StringArray tabLabels;       // shortened, as the original prints them
    std::vector<juce::Rectangle<int>> tabBounds;
    int selectedTab = 0;
    int hoverTab = -1;

    juce::Viewport viewport;
    ChipStrip strip { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleIconBar)
};
