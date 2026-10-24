#include "ThemesDialog.h"
#include "AppTheme.h"
#include "ThemesLook.h"

namespace fx = ThemesLook;

namespace
{
constexpr int kPad = 32;
}

juce::String ThemesDialog::pageName (int p)
{
    switch (p)
    {
        case Themes:    return "Themes";
        case Colours:   return "Colours";
        case Community: return "Community";
        default:        return "Files";
    }
}

juce::String ThemesDialog::pageNote (int p)
{
    switch (p)
    {
        case Themes:    return "Pick the colours of the editor and the modules. Ctrl+T cycles through them.";
        case Colours:   return "Change any colour of the theme in use and see it at once. Save keeps it as a theme of your own.";
        case Community: return "Themes shared in github.com/animatek/nme-themes. Anyone can add one there by pull request.";
        default:        return "Move themes in and out of the editor as files.";
    }
}

ThemesDialog::ThemesDialog()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);

    themesViewport.setViewedComponent (&themesList, false);
    themesViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (themesViewport);
    addChildComponent (coloursPanel);
    addChildComponent (communityPanel);

    fileRows = {
        { 194, "Import theme file...",        "Add a theme someone sent you as a single .json file.",              "Import" },
        { 195, "Export current theme...",     "Save the theme in use as one file you can pass around.",            "Export" },
        { 190, "Save current theme as file...", "Keep the theme in use, with what you changed, as a file you can edit.", "Save" },
        { 191, "Reload theme files",          "Read the themes folder again, after editing a file by hand.",       "Reload" },
        { 192, "Open themes folder",          "Show the folder where your own themes live.",                       "Open" },
    };
    for (const auto& row : fileRows)
    {
        auto b = std::make_unique<juce::TextButton> (row.button);
        b->setColour (juce::TextButton::buttonColourId, fx::button);
        b->setColour (juce::TextButton::textColourOffId, fx::text);
        const int id = row.id;
        b->onClick = [this, id] { if (onFileAction) onFileAction (id); };
        addChildComponent (*b);
        fileButtons.push_back (std::move (b));
    }

    setSize (dialogW, dialogH);
    showPage (Themes);
}

// ─── data ────────────────────────────────────────────────────────────────────

void ThemesDialog::setThemes (std::vector<ThemeInfo> infos, int cur)
{
    themes = std::move (infos);
    current = cur;
    themesList.setSize (juce::jmax (1, themesViewport.getWidth() - themesViewport.getScrollBarThickness()),
                        static_cast<int> (themes.size()) * List::rowH + 8);
    themesList.repaint();
}

void ThemesDialog::setCurrent (int index)
{
    current = index;
    themesList.repaint();
}

void ThemesDialog::setCommunityNewCount (int n)
{
    communityNew = n;
    repaint();
}

void ThemesDialog::showPage (Page p)
{
    page = p;
    themesViewport.setVisible (p == Themes);
    coloursPanel.setVisible (p == Colours);
    communityPanel.setVisible (p == Community);
    for (auto& b : fileButtons) b->setVisible (p == Files);
    resized();
    repaint();
    if (onPageShown) onPageShown (p);
}

// ─── layout ──────────────────────────────────────────────────────────────────

juce::Rectangle<int> ThemesDialog::pageButtonBounds (int index) const
{
    return { 8, titleBarH + 14 + index * 42, sidebarW - 16, 38 };
}

juce::Rectangle<int> ThemesDialog::contentBounds() const
{
    const int x0 = sidebarW + kPad;
    return { x0, titleBarH + 84, getWidth() - x0 - kPad, getHeight() - titleBarH - 84 - 24 };
}

void ThemesDialog::resized()
{
    closeButton.setBounds (getWidth() - 36, 6, 28, 28);
    const auto c = contentBounds();
    themesViewport.setBounds (c.withTrimmedRight (-8));
    themesList.setSize (juce::jmax (1, themesViewport.getWidth() - themesViewport.getScrollBarThickness()),
                        static_cast<int> (themes.size()) * List::rowH + 8);
    coloursPanel.setBounds (c);
    communityPanel.setBounds (c);

    // The file rows: a title and a line on the left, the button on the right
    int y = c.getY();
    for (size_t i = 0; i < fileButtons.size(); ++i)
    {
        fileButtons[i]->setBounds (c.getRight() - 128, y + 20, 128, 36);
        y += 76;
    }
}

// ─── painting ────────────────────────────────────────────────────────────────

void ThemesDialog::paint (juce::Graphics& g)
{
    g.fillAll (fx::ground);

    g.setColour (fx::text);
    g.setFont (juce::Font (AppTheme::uiFont (15.0f)).boldened());
    g.drawText ("Themes", 18, 0, getWidth() - 60, titleBarH, juce::Justification::centredLeft);
    g.setColour (fx::border);
    g.fillRect (0, titleBarH - 1, getWidth(), 1);

    // The sidebar
    g.setColour (fx::panel);
    g.fillRect (0, titleBarH, sidebarW, getHeight() - titleBarH);
    g.setColour (fx::border);
    g.fillRect (sidebarW - 1, titleBarH, 1, getHeight() - titleBarH);
    for (int i = 0; i < numPages; ++i)
    {
        const auto r = pageButtonBounds (i).toFloat();
        const bool on = (i == page);
        if (on)
        {
            g.setColour (fx::button);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (fx::accent);
            g.fillRoundedRectangle (r.getX(), r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);
        }
        g.setColour (on ? fx::text : fx::muted.brighter (0.35f));
        g.setFont (juce::Font (AppTheme::uiFont (14.0f).withStyle (on ? "Bold" : "Regular")));
        g.drawText (pageName (i), r.toNearestInt().withTrimmedLeft (18), juce::Justification::centredLeft, false);

        if (i == Community && communityNew > 0)   // something new there: the red dot
        {
            g.setColour (juce::Colour (0xffe5484d));
            g.fillEllipse (r.getRight() - 24.0f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
        }
    }

    // The page's name and what it is for
    const int x0 = sidebarW + kPad;
    g.setColour (fx::text);
    g.setFont (juce::Font (AppTheme::uiFont (20.0f)).boldened());
    g.drawText (pageName (page), x0, titleBarH + 16, 400, 32, juce::Justification::centredLeft);
    g.setColour (fx::muted);
    g.setFont (juce::Font (AppTheme::uiFont (13.5f)));
    g.drawText (pageNote (page), x0, titleBarH + 48, getWidth() - x0 - kPad, 22, juce::Justification::centredLeft, true);

    if (page == Files)
    {
        const auto c = contentBounds();
        int y = c.getY();
        for (const auto& row : fileRows)
        {
            g.setColour (fx::border.withAlpha (0.6f));
            g.fillRect (c.getX(), y + 75, c.getWidth(), 1);
            g.setColour (fx::text);
            g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
            g.drawText (row.title, c.getX(), y + 14, c.getWidth() - 160, 22, juce::Justification::centredLeft, true);
            g.setColour (fx::muted);
            g.setFont (juce::Font (AppTheme::uiFont (12.5f)));
            g.drawText (row.description, c.getX(), y + 38, c.getWidth() - 160, 22, juce::Justification::centredLeft, true);
            y += 76;
        }
    }
}

void ThemesDialog::paintOverChildren (juce::Graphics& g)
{
    g.setColour (fx::border.brighter (0.4f));
    g.drawRect (getLocalBounds(), 1);
}

// ─── the list of themes ──────────────────────────────────────────────────────

int ThemesDialog::List::rowAt (juce::Point<int> p) const
{
    const int i = (p.y - 4) / rowH;
    return (p.y >= 4 && i >= 0 && i < static_cast<int> (owner.themes.size())) ? i : -1;
}

void ThemesDialog::List::mouseMove (const juce::MouseEvent& e)
{
    const int h = rowAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ThemesDialog::List::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;
    const int i = rowAt (e.getPosition());
    if (i >= 0 && owner.onChooseTheme)
        owner.onChooseTheme (i);
}

void ThemesDialog::List::paint (juce::Graphics& g)
{
    int y = 4;
    for (int i = 0; i < static_cast<int> (owner.themes.size()); ++i)
    {
        const auto& t = owner.themes[static_cast<size_t> (i)];
        const juce::Rectangle<int> r (0, y, getWidth(), rowH);
        const bool on = (i == owner.current);

        if (i == hover)
        {
            g.setColour (fx::panel);
            g.fillRoundedRectangle (r.reduced (0, 3).toFloat(), 8.0f);
        }
        g.setColour (fx::border.withAlpha (0.5f));
        g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);

        // A radio, as in the options
        const auto ring = juce::Rectangle<float> (static_cast<float> (r.getX()) + 6.0f, static_cast<float> (r.getCentreY()) - 10.0f, 20.0f, 20.0f);
        g.setColour (on ? fx::accent : (i == hover ? fx::muted : fx::border));
        g.drawEllipse (ring.reduced (1.0f), 2.0f);
        if (on)
        {
            g.setColour (fx::accent);
            g.fillEllipse (ring.reduced (6.0f));
        }

        g.setColour (fx::text);
        g.setFont (juce::Font (AppTheme::uiFont (15.0f).withStyle (on ? "Bold" : "Regular")));
        g.drawText (t.name, r.getX() + 42, r.getY(), r.getWidth() - 200, r.getHeight(), juce::Justification::centredLeft, true);

        // The preview: the ground, the panel, the accent and the text of the theme
        int x = r.getRight() - 24;
        for (auto col : { t.text, t.accent, t.panel, t.ground })
        {
            x -= 30;
            const auto chip = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (r.getCentreY()) - 11.0f, 24.0f, 22.0f);
            g.setColour (col);
            g.fillRoundedRectangle (chip, 6.0f);
            g.setColour (fx::border.brighter (0.5f));
            g.drawRoundedRectangle (chip.reduced (0.5f), 6.0f, 1.0f);
        }
        y += rowH;
    }
}

// ─── window behaviour ────────────────────────────────────────────────────────

bool ThemesDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void ThemesDialog::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numPages; ++i)
        if (pageButtonBounds (i).contains (e.getPosition())) { showPage (static_cast<Page> (i)); return; }

    // Only a press on the title bar drags the window
    draggingWindow = e.getPosition().getY() < titleBarH;
    if (draggingWindow)
        dragger.startDraggingComponent (this, e);
}

void ThemesDialog::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingWindow)
        dragger.dragComponent (this, e, nullptr);
}

void ThemesDialog::close()
{
    if (onClosed) onClosed();
    removeFromDesktop();
    delete this;
}

ThemesDialog* ThemesDialog::show (juce::Component* parent)
{
    auto* dlg = new ThemesDialog();
    if (parent != nullptr)
    {
        auto* top = parent->getTopLevelComponent();
        auto screen = top->localAreaToGlobal (top->getLocalBounds());
        dlg->setTopLeftPosition (screen.getX() + (screen.getWidth()  - dlg->getWidth())  / 2,
                                 screen.getY() + (screen.getHeight() - dlg->getHeight()) / 2);
    }
    dlg->addToDesktop (juce::ComponentPeer::windowHasDropShadow);
    dlg->setVisible (true);
    dlg->toFront (true);
    dlg->grabKeyboardFocus();
    return dlg;
}
