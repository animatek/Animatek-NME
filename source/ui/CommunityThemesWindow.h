#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CommunityThemes.h"

// View > Theme > Community Themes (#91): the themes in animatek/nme-themes, each with
// an Install / Update / Use button and a "New" badge for the ones the user had not
// seen when the window opened. MainComponent does the fetching; this only shows it.
class CommunityThemesWindow : public juce::DocumentWindow
{
public:
    CommunityThemesWindow();
    ~CommunityThemesWindow() override;

    struct Row
    {
        CommunityThemes::Entry entry;
        CommunityThemes::State state = CommunityThemes::State::NotInstalled;
        bool isNew = false;
        bool inUse = false;
    };
    void setRows(std::vector<Row> rows);
    void setStatus(const juce::String& text);
    void setBusy(bool busy);   // a check or a download is running

    std::function<void(const CommunityThemes::Entry&)> onInstall;   // install, update or use
    std::function<void()> onCheckNow;
    std::function<void()> onClosed;

    void closeButtonPressed() override;

private:
    class Content;
    Content* content = nullptr;
};
