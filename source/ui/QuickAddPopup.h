#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/ModuleDescriptions.h"

// Spotlight-style popup for quickly adding modules.
// Press Enter (or double-click) on canvas → popup appears at mouse position.
// Type to filter, arrows/hover to navigate, Enter or click to add, Escape or
// clicking outside to close. Clicking the star on a row marks the module as
// favorite: favorites are listed first and persist in the app settings.
class QuickAddPopup : public juce::Component,
                      public juce::TextEditor::Listener,
                      public juce::KeyListener
{
public:
    using OnSelectCallback = std::function<void(const ModuleDescriptor*, int gridX, int gridY)>;
    using OnDismissCallback = std::function<void()>;

    QuickAddPopup(const ModuleDescriptions& descs, juce::Point<int> screenPos,
                  int gridX, int gridY, OnSelectCallback cb, OnDismissCallback dismissCb);
    ~QuickAddPopup() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor&) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorEscapeKeyPressed(juce::TextEditor&) override;

    // KeyListener (for arrow keys)
    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;

    // Mouse: click a row to add, hover to select; registered as a global mouse
    // listener so a click anywhere outside the popup dismisses it
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Lose focus → close
    void focusLost(FocusChangeType) override {}
    void inputAttemptWhenModal() override { dismiss(); }

    void grabFocusNow() { searchField.grabKeyboardFocus(); }

    // Called by PatchCanvas destructor to prevent callbacks firing on a dead parent
    void clearCallbacks() { onSelect = nullptr; onDismiss = nullptr; }

    // Favorites are stored in the app settings ("favoriteModules"); set once at startup
    static void setSharedSettings(juce::PropertiesFile* settings);

private:
    static constexpr int popupWidth  = 320;
    static constexpr int rowHeight   = 22;
    static constexpr int maxVisible  = 12;
    static constexpr int fieldHeight = 32;

    void rebuildFiltered(const juce::String& text);
    void confirmSelection();
    void dismiss();
    int totalHeight() const;
    int rowIndexAt(juce::Point<int> localPos) const;  // -1 if not on a row
    bool isStarZone(juce::Point<int> localPos) const;
    bool isFavorite(const juce::String& moduleName) const;
    void toggleFavorite(const juce::String& moduleName);

    const ModuleDescriptions& descs;
    juce::Point<int> spawnGridPos;
    OnSelectCallback onSelect;
    OnDismissCallback onDismiss;

    juce::TextEditor searchField;

    struct Entry { const ModuleDescriptor* desc; juce::String category; };
    std::vector<Entry> filtered;
    int selectedIdx = 0;
    int scrollOffset = 0;  // index of the first visible row

    void scrollSelectedIntoView();

    static juce::PropertiesFile* sharedSettings;
    juce::StringArray favorites;
    juce::Time lastMouseDownTime;  // dedup: events arrive direct AND via global listener

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickAddPopup)
};
