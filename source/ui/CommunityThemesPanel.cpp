#include "CommunityThemesPanel.h"
#include "ThemesLook.h"

namespace
{
using State = CommunityThemes::State;

// The Themes window keeps colours of its own (see ThemesLook.h); the names are the palette's
struct Pal
{
    juce::Colour accentActive = ThemesLook::accent, backgroundMain = ThemesLook::ground, borderColor = ThemesLook::border,
                 textPrimary = ThemesLook::text, textMuted = ThemesLook::muted, textSecondary = ThemesLook::muted.brighter(0.35f),
                 buttonBackground = ThemesLook::button, backgroundPanel = ThemesLook::panel;
};
const Pal& pal() { static const Pal p; return p; }

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

    void set(const CommunityThemesPanel::Row& r)
    {
        row = r;
        button.setButtonText(r.inUse ? "In use"
                           : r.state == State::Installed || r.state == State::BuiltIn ? "Use"
                           : r.state == State::Changed ? "Update" : "Install");
        button.setEnabled(!r.inUse);
        // What asks something of you (install, update) is filled with the accent; the rest is quiet
        const bool asks = !r.inUse && (r.state == State::Changed || (r.state != State::Installed && r.state != State::BuiltIn));
        button.setColour(juce::TextButton::buttonColourId, asks ? pal().accentActive : pal().buttonBackground);
        button.setColour(juce::TextButton::textColourOffId, asks ? pal().backgroundMain : pal().textSecondary);
        repaint();
    }

    void setBusy(bool busy) { button.setEnabled(!busy && !row.inUse); }

    void resized() override
    {
        button.setBounds(getLocalBounds().removeFromRight(112).withTrimmedRight(16).withSizeKeepingCentre(96, 36));
    }

    void paint(juce::Graphics& g) override
    {
        const auto& p = pal();
        auto r = getLocalBounds().reduced(20, 10);
        r.removeFromRight(112);
        g.setColour(p.borderColor.withAlpha(0.5f));
        g.fillRect(16, getHeight() - 1, getWidth() - 32, 1);

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
        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        g.drawText(row.entry.name, r.removeFromTop(r.getHeight() / 2), juce::Justification::bottomLeft, true);

        juce::String sub = row.entry.author.isNotEmpty() ? "by " + row.entry.author : juce::String();
        if (row.entry.description.isNotEmpty())
            sub << (sub.isNotEmpty() ? "  -  " : "") << row.entry.description;
        g.setColour(p.textMuted);
        g.setFont(juce::FontOptions(13.5f));
        g.drawText(sub, r.withTrimmedTop(2), juce::Justification::topLeft, true);
    }

private:
    CommunityThemesPanel::Row row;
    juce::TextButton button;
};
}

class CommunityThemesPanel::Content : public juce::Component
{
public:
    explicit Content(CommunityThemesPanel& o) : owner(o)
    {

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
    }

    void applyColours()
    {
        const auto& p = pal();
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
        auto r = getLocalBounds();
        auto bottom = r.removeFromBottom(36);
        checkButton.setBounds(bottom.removeFromLeft(120));
        bottom.removeFromLeft(10);
        repoButton.setBounds(bottom.removeFromLeft(150));
        bottom.removeFromLeft(14);
        status.setBounds(bottom);
        r.removeFromBottom(14);
        viewport.setBounds(r);
        const int w = viewport.getMaximumVisibleWidth();
        int y = 0;
        for (auto& row : rows) { row->setBounds(0, y, w, 72); y += 72; }
        list.setSize(w, juce::jmax(y, 1));
    }

    void paint(juce::Graphics& g) override
    {
        const auto& p = pal();
        g.setColour(p.backgroundPanel);
        g.fillRoundedRectangle(viewport.getBounds().toFloat(), 8.0f);
        g.setColour(p.borderColor);
        g.drawRoundedRectangle(viewport.getBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
        if (empty)
        {
            g.setColour(p.textMuted);
            g.setFont(juce::FontOptions(15.0f));
            g.drawText("No list yet: choose Check Now.", viewport.getBounds(), juce::Justification::centred);
        }
    }

private:
    CommunityThemesPanel& owner;
    juce::Label status;
    juce::TextButton checkButton { "Check Now" }, repoButton { "Open on GitHub" };
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<ThemeRow>> rows;
    bool busy = false, empty = true;
};

CommunityThemesPanel::CommunityThemesPanel()
{
    content = std::make_unique<Content>(*this);
    addAndMakeVisible(*content);
}

CommunityThemesPanel::~CommunityThemesPanel() = default;

void CommunityThemesPanel::setRows(std::vector<Row> rows) { content->setRows(std::move(rows)); }
void CommunityThemesPanel::setStatus(const juce::String& text) { content->setStatus(text); }
void CommunityThemesPanel::setBusy(bool busy) { content->setBusy(busy); }

void CommunityThemesPanel::resized() { content->setBounds(getLocalBounds()); }
