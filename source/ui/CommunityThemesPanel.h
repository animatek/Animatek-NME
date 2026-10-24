#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CommunityThemes.h"

// The Community page of the Themes window (#91): the themes in animatek/nme-themes, each with an
// Install / Update / Use button and a "New" badge for the ones the user had not seen when the page
// opened. MainComponent does the fetching; this only shows it.
class CommunityThemesPanel : public juce::Component
{
public:
    CommunityThemesPanel();
    ~CommunityThemesPanel() override;

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

    void resized() override;

private:
    class Content;
    std::unique_ptr<Content> content;
};
