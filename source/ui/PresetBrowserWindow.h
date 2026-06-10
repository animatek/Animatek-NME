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

    private:
        std::unique_ptr<juce::Drawable> icon;
    };

    enum class TypeFilter { All, Patches, Snippets, Banks };

    struct Entry
    {
        enum class Type { Patch, Snippet, Bank };
        Type type = Type::Patch;
        juce::File file;
        juce::String displayName;
        juce::String relativePath;
    };

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    void rebuildVisibleEntries();
    void scanFolder(const juce::File& folder, Entry::Type type);
    juce::String getTypeLabel(Entry::Type type) const;
    bool entryPassesTypeFilter(const Entry& entry) const;

    juce::Label searchLabel;
    juce::TextEditor searchBox;
    juce::TextButton allButton { "All" };
    juce::TextButton patchesButton { "Patches" };
    juce::TextButton snippetsButton { "Snippets" };
    juce::TextButton banksButton { "Banks" };
    RefreshIconButton refreshButton;
    juce::Label statusLabel;
    juce::ListBox listBox { "Disk Presets", this };

    juce::File libraryRoot;
    TypeFilter typeFilter = TypeFilter::All;
    std::vector<Entry> allEntries;
    std::vector<int> visibleEntryIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiskPresetBrowserPanel)
};

class PresetBrowserWindow : public juce::DocumentWindow
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
