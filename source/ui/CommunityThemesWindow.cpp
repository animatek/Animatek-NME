#include "CommunityThemesWindow.h"
#include "AppTheme.h"

namespace
{
using State = CommunityThemes::State;

// One theme: name and author on the left, a badge, and the button that installs it.
class ThemeRow : public juce::Component
{
public:
    std::function<void()> onButton;

    ThemeRow()
    {
        button.onClick = [this] { if (onButton) onButton(); };
        addAndMakeVisible(button);
    }

    void set(const CommunityThemesWindow::Row& r)
    {
        row = r;
        button.setButtonText(r.inUse ? "In use"
                           : r.state == State::Installed || r.state == State::BuiltIn ? "Use"
                           : r.state == State::Changed ? "Update" : "Install");
        button.setEnabled(!r.inUse);
        repaint();
    }

    void setBusy(bool busy) { button.setEnabled(!busy && !row.inUse); }

    void resized() override
    {
        button.setBounds(getLocalBounds().removeFromRight(96).reduced(8, 12));
    }

    void paint(juce::Graphics& g) override
    {
        const auto& p = AppTheme::palette();
        auto r = getLocalBounds().reduced(10, 6);
        r.removeFromRight(96);
        g.setColour(p.borderColor);
        g.drawHorizontalLine(getHeight() - 1, 6.0f, static_cast<float>(getWidth() - 6));

        juce::String badge = row.isNew ? (row.state == State::Changed ? "Updated" : "New")
                           : row.state == State::Changed ? "Update"
                           : row.entry.official ? "Built in" : juce::String();
        if (badge.isNotEmpty())
        {
            const juce::Font f(juce::FontOptions(13.0f, juce::Font::bold));
            const int bw = juce::GlyphArrangement::getStringWidthInt(f, badge) + 14;
            auto b = r.removeFromRight(bw).withSizeKeepingCentre(bw, 22);
            g.setColour(p.accentActive);
            g.fillRoundedRectangle(b.toFloat(), 11.0f);
            g.setColour(p.backgroundMain);
            g.setFont(f);
            g.drawText(badge, b, juce::Justification::centred);
            r.removeFromRight(8);
        }

        g.setColour(p.textPrimary);
        g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
        g.drawText(row.entry.name, r.removeFromTop(r.getHeight() / 2), juce::Justification::bottomLeft, true);

        juce::String sub = row.entry.author.isNotEmpty() ? "by " + row.entry.author : juce::String();
        if (row.entry.description.isNotEmpty())
            sub << (sub.isNotEmpty() ? "  -  " : "") << row.entry.description;
        g.setColour(p.textSecondary);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText(sub, r, juce::Justification::topLeft, true);
    }

private:
    CommunityThemesWindow::Row row;
    juce::TextButton button;
};
}

class CommunityThemesWindow::Content : public juce::Component
{
public:
    explicit Content(CommunityThemesWindow& o) : owner(o)
    {
        title.setText("Community themes", juce::dontSendNotification);
        title.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        addAndMakeVisible(title);
        from.setText("From github.com/animatek/nme-themes. Anyone can add one there by pull request.",
                     juce::dontSendNotification);
        from.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(from);

        checkButton.onClick = [this] { if (owner.onCheckNow) owner.onCheckNow(); };
        repoButton.onClick = [] { juce::URL(CommunityThemes::kRepoUrl).launchInDefaultBrowser(); };
        addAndMakeVisible(checkButton);
        addAndMakeVisible(repoButton);
        status.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(status);

        viewport.setViewedComponent(&list, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);
        applyColours();
        setSize(560, 520);
    }

    void applyColours()
    {
        const auto& p = AppTheme::palette();
        title.setColour(juce::Label::textColourId, p.textPrimary);
        from.setColour(juce::Label::textColourId, p.textSecondary);
        status.setColour(juce::Label::textColourId, p.textSecondary);
    }

    void setRows(std::vector<Row> newRows)
    {
        rows.clear();
        for (auto& r : newRows)
        {
            auto c = std::make_unique<ThemeRow>();
            c->set(r);
            const auto entry = r.entry;
            c->onButton = [this, entry] { if (owner.onInstall) owner.onInstall(entry); };
            c->setBusy(busy);
            list.addAndMakeVisible(*c);
            rows.push_back(std::move(c));
        }
        empty = rows.empty();
        resized();
        repaint();
    }

    void setStatus(const juce::String& t) { status.setText(t, juce::dontSendNotification); }

    void setBusy(bool b)
    {
        busy = b;
        checkButton.setEnabled(!b);
        for (auto& r : rows) r->setBusy(b);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        title.setBounds(r.removeFromTop(28));
        from.setBounds(r.removeFromTop(22));
        r.removeFromTop(6);
        auto bottom = r.removeFromBottom(30);
        checkButton.setBounds(bottom.removeFromLeft(110));
        bottom.removeFromLeft(8);
        repoButton.setBounds(bottom.removeFromLeft(130));
        r.removeFromBottom(6);
        status.setBounds(r.removeFromBottom(22));
        r.removeFromBottom(4);
        viewport.setBounds(r);
        const int w = viewport.getMaximumVisibleWidth();
        int y = 0;
        for (auto& row : rows) { row->setBounds(0, y, w, 62); y += 62; }
        list.setSize(w, juce::jmax(y, 1));
    }

    void paint(juce::Graphics& g) override
    {
        const auto& p = AppTheme::palette();
        g.fillAll(p.backgroundMain);
        g.setColour(p.backgroundPanel);
        g.fillRect(viewport.getBounds());
        if (empty)
        {
            g.setColour(p.textMuted);
            g.setFont(juce::FontOptions(15.0f));
            g.drawText("No list yet: choose Check Now.", viewport.getBounds(), juce::Justification::centred);
        }
    }

private:
    CommunityThemesWindow& owner;
    juce::Label title, from, status;
    juce::TextButton checkButton { "Check Now" }, repoButton { "Open on GitHub" };
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<ThemeRow>> rows;
    bool busy = false, empty = true;
};

CommunityThemesWindow::CommunityThemesWindow()
    : juce::DocumentWindow("Community themes", AppTheme::palette().backgroundMain, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(true);
    content = new Content(*this);
    setContentOwned(content, true);
    setResizable(true, false);
    centreWithSize(getWidth(), getHeight());
}

CommunityThemesWindow::~CommunityThemesWindow() = default;

void CommunityThemesWindow::setRows(std::vector<Row> rows) { content->setRows(std::move(rows)); }
void CommunityThemesWindow::setStatus(const juce::String& text) { content->setStatus(text); }
void CommunityThemesWindow::setBusy(bool busy) { content->setBusy(busy); }

void CommunityThemesWindow::closeButtonPressed()
{
    if (onClosed)
        onClosed();
}
