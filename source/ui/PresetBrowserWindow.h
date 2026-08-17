#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DiskPresetBrowserPanel : public juce::Component,
                               private juce::ListBoxModel
{
public:
    DiskPresetBrowserPanel();

    void setLibraryRoot(const juce::File& root);
    void refresh();
    void resized() override;
    void paint(juce::Graphics& g) override;
    void applyTheme();

    std::function<void(const juce::File&)> onPatchChosen;
    std::function<void(const juce::File&)> onSnippetChosen;

private:
    class RefreshIconButton : public juce::Button
    {
    public:
        RefreshIconButton();
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

    };

    class FilterIconButton : public juce::Button
    {
    public:
        enum class Icon { All, Patch, Snippet, Bank, Legacy };
        FilterIconButton(const juce::String& name, Icon iconType);
        void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

    private:
        Icon icon;
    };

    enum class TypeFilter { All, Patches, Snippets, Banks };

    struct Entry
    {
        enum class Type { Patch, Snippet, Bank };
        Type type = Type::Patch;
        juce::File file;
        juce::String displayName;
        juce::String relativePath;
        int legacyPatch210 = -1;  // -1 = not sniffed yet, 0 = no, 1 = yes
    };

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    void rebuildVisibleEntries();
    void scanFolder(const juce::File& folder, Entry::Type type);
    juce::String getTypeLabel(Entry::Type type) const;
    juce::String getTypeLabel(Entry& entry) const;
    // Sniffs the file on first use and caches the result on the entry, so
    // scanning a large library never touches file contents up front.
    static bool isLegacyPatch210(Entry& entry);
    bool entryPassesTypeFilter(const Entry& entry) const;

    juce::Label searchLabel;
    juce::TextEditor searchBox;
    FilterIconButton allButton { "All presets", FilterIconButton::Icon::All };
    FilterIconButton patchesButton { "Patches", FilterIconButton::Icon::Patch };
    FilterIconButton snippetsButton { "Snippets", FilterIconButton::Icon::Snippet };
    FilterIconButton banksButton { "Banks", FilterIconButton::Icon::Bank };
    FilterIconButton hidePch2Button { "Hide PCH2", FilterIconButton::Icon::Legacy };
    RefreshIconButton refreshButton;
    juce::Label statusLabel;
    juce::ListBox listBox { "Disk Presets", this };

    juce::File libraryRoot;
    TypeFilter typeFilter = TypeFilter::All;
    bool hidePch2 = false;   // when true, legacy 2.10 (.pch2) patches are hidden
    std::vector<Entry> allEntries;
    std::vector<int> visibleEntryIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiskPresetBrowserPanel)
};

// Also a DragAndDropContainer: the same panel sits in the main window's Disk tab,
// where MainLayout provides one, and without this the identical list simply
// refused to start a drag when it was in this window instead. JUCE lets the drag
// image live on the desktop, so a patch dragged out of here still lands on a slot
// in the main window.
class PresetBrowserWindow : public juce::DocumentWindow,
                            public juce::DragAndDropContainer
{
public:
    PresetBrowserWindow();

    void setLibraryRoot(const juce::File& root);
    void refresh();
    void closeButtonPressed() override;
    void applyTheme();

    std::function<void(const juce::File&)> onPatchChosen;
    std::function<void(const juce::File&)> onSnippetChosen;
    std::function<void()> onChooseLibraryFolder;

private:
    DiskPresetBrowserPanel browserPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserWindow)
};
