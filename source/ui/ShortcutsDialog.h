#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AppTheme.h"
#include "FlatCloseButton.h"
#include <vector>

// Keyboard Shortcuts, laid out like Editor Options: the groups (File, Edit, Canvas...) in a list at the
// left, the shortcuts of the one chosen at the right with the action on the left and the keys on the
// right, and a filter on top that searches every group at once.
class ShortcutsDialog : public juce::Component
{
public:
    ShortcutsDialog();

    // Closes it from outside, as its own close does; a second press of what opened it
    void dismiss() { close(); }
    // For the picture-taking run: which group shows, and what the filter says
    void showGroupForSnapshot (int index) { showGroup (index); }
    void setFilterForSnapshot (const juce::String& t) { filter.setText (t, juce::dontSendNotification); rebuildLines(); }

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override { AppTheme::drawWindowFrame (g, getLocalBounds()); }
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { draggingWindow = false; }

    // Opens it on the desktop, centred on the parent's window
    static ShortcutsDialog* show (juce::Component* parent);

private:
    struct Entry   { juce::String keys, action; };
    struct Group   { juce::String name, note; std::vector<Entry> entries; };

    // What the list shows now: a heading, a shortcut or a line of explanation
    struct Line    { enum Kind { Heading, Shortcut, Note } kind; juce::String a, b; };

    class List : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
        std::vector<Line> lines;
        static int heightOf (const Line& l);
        int totalHeight() const;
    };

    void close();
    void showGroup (int index);
    void rebuildLines();
    juce::Rectangle<int> groupButtonBounds (int index) const;

    static const std::vector<Group>& groups();

    static constexpr int dialogW = 880, dialogH = 640, titleBarH = 40, sidebarW = 200;

    FlatCloseButton closeButton;
    juce::TextEditor filter;
    juce::Viewport viewport;
    List list;
    juce::ComponentDragger dragger;
    bool draggingWindow = false;
    int group = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShortcutsDialog)
};
