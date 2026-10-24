#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatCloseButton.h"
#include "ThemeEditorPanel.h"
#include "CommunityThemesPanel.h"
#include <vector>

// Everything about themes in one window, laid out like Editor Options: pages at the left (Themes, Colours,
// Community, Files), the page at the right. It replaces the long Theme submenu of View and the two windows
// the editor and the community list used to be. It keeps colours of its own (ThemesLook.h), so the window
// does not move under the mouse while a theme is edited.
class ThemesDialog : public juce::Component
{
public:
    enum Page { Themes, Colours, Community, Files, numPages };

    struct ThemeInfo
    {
        juce::String name;
        juce::Colour ground, panel, accent, text;   // a little preview of the theme
    };

    ThemesDialog();

    void setThemes(std::vector<ThemeInfo> infos, int current);
    void setCurrent(int index);
    void setCommunityNewCount(int n);     // a dot beside Community while something there is new
    void showPage(Page p);
    Page getPage() const { return page; }
    ThemeEditorPanel& colours()       { return coloursPanel; }
    CommunityThemesPanel& community() { return communityPanel; }

    // Closes it from outside, as its own close does; a second press of what opened it
    void dismiss() { close(); }

    std::function<void(int index)> onChooseTheme;
    // The menu id of the file action: 194 import, 195 export, 190 save as file, 191 reload, 192 open folder
    std::function<void(int id)> onFileAction;
    std::function<void(Page)> onPageShown;
    std::function<void()> onClosed;       // called as it closes, before it is deleted

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { draggingWindow = false; }

    static ThemesDialog* show (juce::Component* parent);

private:
    class List : public juce::Component
    {
    public:
        explicit List (ThemesDialog& o) : owner (o) {}
        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override;
        static constexpr int rowH = 60;
        int rowAt (juce::Point<int> p) const;
        ThemesDialog& owner;
        int hover = -1;
    };

    void close();
    juce::Rectangle<int> pageButtonBounds (int index) const;
    juce::Rectangle<int> contentBounds() const;
    static juce::String pageName (int p);
    static juce::String pageNote (int p);

    static constexpr int dialogW = 920, dialogH = 680, titleBarH = 40, sidebarW = 200;

    FlatCloseButton closeButton;
    juce::ComponentDragger dragger;
    bool draggingWindow = false;
    Page page = Themes;

    std::vector<ThemeInfo> themes;
    int current = -1;
    int communityNew = 0;

    juce::Viewport themesViewport;
    List themesList { *this };
    ThemeEditorPanel coloursPanel;
    CommunityThemesPanel communityPanel;

    struct FileRow { int id; juce::String title, description, button; };
    std::vector<FileRow> fileRows;
    std::vector<std::unique_ptr<juce::TextButton>> fileButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThemesDialog)
};
