#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AppTheme.h"

// The close button of a floating window: the gold cross of the dialogs' own, as a real Button, which
// is what a window's title bar takes
class WindowCloseButton : public juce::Button
{
public:
    WindowCloseButton() : juce::Button ("close") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        if (highlighted)
        {
            g.setColour (AppTheme::palette().inputBackground);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
        }
        g.setColour (highlighted ? juce::Colours::white : AppTheme::palette().accentActive);
        g.setFont (juce::Font (AppTheme::uiFont (15.0f).withStyle ("Bold")));
        g.drawText ("x", getLocalBounds(), juce::Justification::centred);
    }
};

// The maximise button of a slot's sub-window: a small rounded square, in the same ink as the cross
class WindowMaximiseButton : public juce::Button
{
public:
    WindowMaximiseButton() : juce::Button ("maximise") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        if (highlighted)
        {
            g.setColour (AppTheme::palette().inputBackground);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
        }
        g.setColour (highlighted ? juce::Colours::white : AppTheme::palette().accentActive);
        const auto c = getLocalBounds().toFloat().getCentre();
        g.drawRoundedRectangle (c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f, 2.0f, 1.6f);
    }
};

// A small on/off switch, the way the options pages draw theirs, for a place where a row of text
// has already said what it is for
class SwitchButton : public juce::ToggleButton
{
public:
    SwitchButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const auto& pal = AppTheme::palette();
        const bool on = getToggleState();
        const float fade = isEnabled() ? 1.0f : 0.45f;
        const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (44.0f, 22.0f);
        g.setColour ((on ? pal.accentActive : pal.inputBackground).withAlpha (fade));
        g.fillRoundedRectangle (r, 11.0f);
        if (! on)
        {
            g.setColour (pal.borderColor.withAlpha (fade));
            g.drawRoundedRectangle (r.reduced (0.5f), 11.0f, 1.0f);
        }
        g.setColour ((on ? pal.backgroundMain : pal.textSecondary).withAlpha (fade));
        g.fillEllipse (on ? r.getRight() - 20.0f : r.getX() + 2.0f, r.getCentreY() - 9.0f, 18.0f, 18.0f);
    }
};

// The app's default LookAndFeel: JUCE's V4, coloured by AppTheme, plus a red dot on a
// menu bar title that ends in `newMarker()` (View, when there are community themes
// the user has not seen, #91). The dot as text took the menu's colour and was easy
// to miss.
class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static juce::String newMarker() { return juce::String::fromUTF8(" \xe2\x80\xa2"); }
    static juce::Colour newColour() { return juce::Colour(0xffe5484d); }

    // A red dot as a menu item icon (Theme, Community Themes).
    static std::unique_ptr<juce::Drawable> newDot()
    {
        auto d = std::make_unique<juce::DrawablePath>();
        juce::Path p;
        p.addEllipse(0.0f, 0.0f, 10.0f, 10.0f);
        d->setPath(p);
        d->setFill(newColour());
        return d;
    }

    // Tabs (the browser's Synth / Disk): just the word, with an underline under the front one
    static juce::Font tabFont() { return juce::Font(AppTheme::uiFont(13.0f).withStyle("Bold")); }

    int getTabButtonBestWidth(juce::TabBarButton& button, int) override
    {
        return juce::roundToInt(juce::GlyphArrangement::getStringWidth(tabFont(), button.getButtonText())) + 36;
    }

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool) override
    {
        const auto& pal = AppTheme::palette();
        const auto r = button.getLocalBounds();
        const bool front = button.isFrontTab();
        g.setColour(front || isMouseOver ? pal.textPrimary : pal.textSecondary);
        g.setFont(tabFont());
        // The word sits in the middle of the room above the underline
        g.drawText(button.getButtonText(), r.withTrimmedBottom(4), juce::Justification::centred, false);
        // A hairline between one tab and the next (Synth | Disk), level with the word
        if (button.getIndex() < button.getTabbedButtonBar().getNumTabs() - 1)
        {
            g.setColour(pal.borderColor.brighter(0.4f));
            const float cy = static_cast<float>(r.getHeight() - 4) * 0.5f;
            g.fillRect(static_cast<float>(r.getRight()) - 1.0f, cy - 8.0f, 1.0f, 16.0f);
        }
        if (front)
        {
            g.setColour(pal.accentActive);
            g.fillRect(r.getX() + 8, r.getBottom() - 4, r.getWidth() - 16, 3);
        }
    }

    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics& g, int w, int h) override
    {
        g.setColour(AppTheme::palette().borderColor);
        g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(w));
    }

    // The open/close mark on a tree row: a chevron, pointing right when closed and down when open
    void drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour,
                                  bool isOpen, bool isMouseOver) override
    {
        const auto& pal = AppTheme::palette();
        const float cx = area.getCentreX(), cy = area.getCentreY(), s = 3.6f;
        juce::Path chevron;
        if (isOpen)
        {
            chevron.startNewSubPath(cx - s, cy - s * 0.55f);
            chevron.lineTo(cx, cy + s * 0.55f);
            chevron.lineTo(cx + s, cy - s * 0.55f);
        }
        else
        {
            chevron.startNewSubPath(cx - s * 0.55f, cy - s);
            chevron.lineTo(cx + s * 0.55f, cy);
            chevron.lineTo(cx - s * 0.55f, cy + s);
        }
        g.setColour(isMouseOver ? pal.textPrimary : pal.textSecondary);
        g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A check box with rounded corners that fills with the accent when it is on
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool isMouseOver, bool) override
    {
        const auto& pal = AppTheme::palette();
        const float size = juce::jmin(18.0f, static_cast<float>(button.getHeight()) - 6.0f);
        const auto box = juce::Rectangle<float>(2.0f, (static_cast<float>(button.getHeight()) - size) * 0.5f, size, size);
        const bool on = button.getToggleState();
        const bool enabled = button.isEnabled();

        // A button in a group where only one can be on is a round radio; a lone one is a checkbox
        const bool radio = button.getRadioGroupId() != 0;
        if (radio)
        {
            g.setColour(on ? pal.accentActive : isMouseOver ? pal.textSecondary : pal.borderColor);
            g.drawEllipse(box.reduced(1.0f), 2.0f);
            if (on)
            {
                g.setColour(pal.accentActive);
                g.fillEllipse(box.reduced(5.0f));
            }
        }
        else if (!on)
        {
            g.setColour(isMouseOver ? pal.buttonHover : pal.inputBackground);
            g.fillRoundedRectangle(box, 5.0f);
            g.setColour(pal.borderColor);
            g.drawRoundedRectangle(box.reduced(0.5f), 5.0f, 1.0f);
        }
        else
        {
            g.setColour(pal.accentActive);
            g.fillRoundedRectangle(box, 5.0f);
        }
        if (radio)
        {
            // nothing more to draw on a radio: its dot is the state
        }
        else if (on)
        {
            juce::Path tick;
            tick.startNewSubPath(box.getX() + size * 0.26f, box.getCentreY());
            tick.lineTo(box.getX() + size * 0.43f, box.getY() + size * 0.70f);
            tick.lineTo(box.getX() + size * 0.76f, box.getY() + size * 0.30f);
            g.setColour(pal.backgroundMain);
            g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(button.findColour(juce::ToggleButton::textColourId).withAlpha(enabled ? 1.0f : 0.5f));
        g.setFont(AppTheme::uiFont(12.0f));
        g.drawText(button.getButtonText(),
                   button.getLocalBounds().withTrimmedLeft(static_cast<int>(size) + 10),
                   juce::Justification::centredLeft, true);
    }

    // ---- Menus: rounded, roomy, a hairline for a separator, a light highlight, quiet shortcuts ----
    juce::Font getPopupMenuFont() override { return juce::Font(AppTheme::uiFont(14.0f)); }
    // 12px between the window's edge and the rows: 6 of them are empty margin (the rounded box is
    // drawn inside it, so it floats a little away from the menu bar and the title it opened from),
    // and 6 are the box's own padding
    int getPopupMenuBorderSize() override { return 12; }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        const auto& pal = AppTheme::palette();
        // Where a window cannot be see-through (JUCE on Linux, unless it is built with XRender) its
        // corners cannot be left clear either, and a rounded box on a square ground reads as a
        // mistake: there the menu is one clean rectangle, with its rows rounded inside it. Where it
        // can (macOS, Windows), the rounded box floats inside its window with a margin around it.
        if (!juce::Desktop::getInstance().canUseSemiTransparentWindows())
        {
            g.fillAll(pal.backgroundPanel);
            g.setColour(pal.borderColor.brighter(0.2f));
            g.drawRect(0, 0, width, height, 1);
            return;
        }
        const auto box = juce::Rectangle<float>(6.5f, 6.5f, static_cast<float>(width) - 13.0f, static_cast<float>(height) - 13.0f);
        g.setColour(pal.backgroundPanel);
        g.fillRoundedRectangle(box, 9.0f);
        g.setColour(pal.borderColor.brighter(0.2f));
        g.drawRoundedRectangle(box, 9.0f, 1.0f);
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                   int& idealWidth, int& idealHeight) override
    {
        LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
        if (isSeparator)
            idealHeight = 11;
        else if (standardMenuItemHeight < 0)
            idealWidth += 40;   // a section heading: JUCE asks with -1, and adds half its height again
        else
        {
            idealHeight = juce::jmax(idealHeight, 32);
            idealWidth += 44;   // room to breathe between the words and the shortcut, and for the margins
        }
    }

    // A section heading ("value" over a parameter's menu): bold, in the accent, level with the words
    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName) override
    {
        g.setFont(getPopupMenuFont().boldened());
        g.setColour(findColour(juce::PopupMenu::headerTextColourId));
        g.drawFittedText(sectionName, area.reduced(6 + 14, 0).withTrimmedTop(2), juce::Justification::centredLeft, 1);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& fullArea, bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override
    {
        const auto& pal = AppTheme::palette();
        // JUCE gives the rows the whole width of the window, margin included; the box is drawn
        // 6px inside it, so the rows come in by the same 6px to stay within the box
        const auto area = fullArea.reduced(6, 0);

        if (isSeparator)
        {
            g.setColour(pal.borderColor);
            g.fillRect(area.reduced(8, 0).withSizeKeepingCentre(area.getWidth() - 16, 1));
            return;
        }

        const auto textColour = textColourToUse == nullptr ? findColour(juce::PopupMenu::textColourId) : *textColourToUse;

        if (isHighlighted && isActive)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRoundedRectangle(area.reduced(6, 1).toFloat(), 6.0f);   // 6px clear of the box on each side
            g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour(textColour.withMultipliedAlpha(isActive ? 1.0f : 0.5f));
        }

        auto r = area.reduced(14, 0);   // so the words sit 8px inside the highlight
        auto font = getPopupMenuFont();
        g.setFont(font);

        // The mark column: an icon, or a check in the accent when the item is on
        auto mark = r.removeFromLeft(16).toFloat();
        if (icon != nullptr)
            icon->drawWithin(g, mark, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
        else if (isTicked)
        {
            juce::Path tick;
            tick.startNewSubPath(mark.getX() + 3.0f, mark.getCentreY());
            tick.lineTo(mark.getX() + 7.0f, mark.getCentreY() + 4.0f);
            tick.lineTo(mark.getX() + 14.0f, mark.getCentreY() - 4.5f);
            g.saveState();
            g.setColour(isHighlighted && isActive ? findColour(juce::PopupMenu::highlightedTextColourId) : pal.accentActive);
            g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.restoreState();
        }
        r.removeFromLeft(4);

        if (hasSubMenu)
        {
            auto arrow = r.removeFromRight(12);
            const float cx = static_cast<float>(arrow.getCentreX()), cy = static_cast<float>(arrow.getCentreY());
            juce::Path chevron;
            chevron.startNewSubPath(cx - 2.5f, cy - 4.5f);
            chevron.lineTo(cx + 2.5f, cy);
            chevron.lineTo(cx - 2.5f, cy + 4.5f);
            g.strokePath(chevron, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            r.removeFromRight(4);
        }

        // The shortcut, quiet and monospaced, at the right; the words get what is left
        int shortcutW = 0;
        if (shortcutKeyText.isNotEmpty())
        {
            const juce::Font mono(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                                    12.0f * AppTheme::uiFontScale(), juce::Font::plain));
            shortcutW = static_cast<int>(juce::GlyphArrangement::getStringWidth(mono, shortcutKeyText)) + 6;
            g.setColour((isHighlighted && isActive ? findColour(juce::PopupMenu::highlightedTextColourId) : pal.textMuted)
                            .withMultipliedAlpha(isActive ? 1.0f : 0.5f));
            g.setFont(mono);
            g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
            g.setColour(isHighlighted && isActive ? findColour(juce::PopupMenu::highlightedTextColourId)
                                                  : textColour.withMultipliedAlpha(isActive ? 1.0f : 0.5f));
            g.setFont(font);
        }

        g.drawFittedText(text, r.withTrimmedRight(shortcutW + (shortcutW > 0 ? 12 : 0)), juce::Justification::centredLeft, 1);
    }

    // A flat progress bar: a rounded track, the fill in the accent, the percentage in plain type
    void drawProgressBar(juce::Graphics& g, juce::ProgressBar& bar, int width, int height, double progress,
                         const juce::String& textToShow) override
    {
        const auto& pal = AppTheme::palette();
        const auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        const float radius = juce::jmin(8.0f, r.getHeight() * 0.5f);
        g.setColour(pal.inputBackground);
        g.fillRoundedRectangle(r, radius);
        g.setColour(pal.borderColor);
        g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
        if (progress >= 0.0 && progress <= 1.0)
        {
            g.setColour(bar.findColour(juce::ProgressBar::foregroundColourId));
            g.fillRoundedRectangle(r.withWidth(juce::jmax(r.getHeight(), r.getWidth() * static_cast<float>(progress))), radius);
        }
        if (textToShow.isNotEmpty())
        {
            g.setFont(juce::Font(AppTheme::uiFont(13.0f)).boldened());
            g.setColour(pal.textPrimary);
            g.drawText(textToShow, r.toNearestInt(), juce::Justification::centred, false);
        }
    }

    // Tooltips: a rounded box in the panel colour with a hairline, the text in the UI font
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
    {
        const auto font = juce::Font (AppTheme::uiFont (13.0f));
        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (tipText, font, juce::Colours::white);
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, 360.0f);
        const int w = static_cast<int> (std::ceil (tl.getWidth())) + 24;
        const int h = static_cast<int> (std::ceil (tl.getHeight())) + 16;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 12 : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - h - 6 : screenPos.y + 18, w, h)
                   .constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        const auto& pal = AppTheme::palette();
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
        g.setColour (pal.backgroundElevated);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (pal.borderColor.brighter (0.4f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, juce::Font (AppTheme::uiFont (13.0f)), pal.textPrimary);
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, 360.0f);
        tl.draw (g, r.reduced (12.0f, 8.0f));
    }

    // The alert windows: JUCE's own icon is a huge triangle that the window clips. A small ring with a
    // mark in it (! warning, ? question, i info) in the accent says the same, and the text sits as before
    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea,
                      juce::TextLayout& textLayout) override
    {
        const auto& pal = AppTheme::palette();
        g.fillAll(pal.backgroundMain);

        juce::String mark;
        juce::Colour ink = pal.accentActive;
        switch (alert.getAlertType())
        {
            case juce::MessageBoxIconType::WarningIcon:  mark = "!"; ink = pal.accentWarning; break;
            case juce::MessageBoxIconType::QuestionIcon: mark = "?"; break;
            case juce::MessageBoxIconType::InfoIcon:     mark = "i"; ink = pal.accentInfo; break;
            default: break;
        }
        if (mark.isNotEmpty())
        {
            const auto ring = juce::Rectangle<float>(22.0f, static_cast<float>(textArea.getY()) + 2.0f, 36.0f, 36.0f);
            g.setColour(ink);
            g.drawEllipse(ring.reduced(1.0f), 2.0f);
            g.setFont(juce::Font(AppTheme::uiFont(20.0f)).boldened());
            g.drawText(mark, ring.toNearestInt(), juce::Justification::centred, false);
        }

        // JUCE counts 80px at the left for the icon in the window's size; the text starts after it
        const int iconSpace = mark.isNotEmpty() ? 80 : 0;
        g.setColour(pal.textSecondary);
        textLayout.draw(g, juce::Rectangle<int>(textArea.getX() + iconSpace, textArea.getY(),
                                                textArea.getWidth() - iconSpace, textArea.getHeight()).toFloat());
        g.setColour(pal.borderColor.brighter(0.6f));
        g.drawRect(alert.getLocalBounds(), 1);
    }

    // The alert windows (confirmations, renames): type a size up from JUCE's, taller buttons
    juce::Font getAlertWindowTitleFont() override   { return juce::Font(AppTheme::uiFont(16.0f)).boldened(); }
    juce::Font getAlertWindowMessageFont() override { return juce::Font(AppTheme::uiFont(14.0f)); }
    juce::Font getAlertWindowFont() override        { return juce::Font(AppTheme::uiFont(13.0f)); }
    int getAlertWindowButtonHeight() override       { return 36; }

    // The title bar of the windows JUCE draws itself (the floaters, the browser window, About): the
    // same as the dialogs', the ground colour, the title bold at the left, a rule under it
    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w, int h, int, int,
                                    const juce::Image*, bool) override
    {
        const auto& pal = AppTheme::palette();
        g.fillAll(pal.backgroundMain);
        g.setColour(pal.borderColor);
        g.fillRect(0, h - 1, w, 1);
        g.setColour(pal.textPrimary);
        g.setFont(juce::Font(AppTheme::uiFont(14.0f).withStyle("Bold")));
        g.drawText(window.getName(), 14, 0, w - 60, h, juce::Justification::centredLeft, true);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        if (buttonType == juce::DocumentWindow::closeButton)
            return new WindowCloseButton();
        if (buttonType == juce::DocumentWindow::maximiseButton)
            return new WindowMaximiseButton();
        return LookAndFeel_V4::createDocumentWindowButton(buttonType);
    }

    // Every window of the editor wears a thin outline, so it does not melt into what is behind it
    void drawResizableWindowBorder(juce::Graphics& g, int width, int height, const juce::BorderSize<int>&,
                                   juce::ResizableWindow&) override
    {
        AppTheme::drawWindowFrame(g, { 0, 0, width, height });
    }

    // Text fields with rounded corners, like the buttons
    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 6.0f);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        if (!editor.isEnabled())
            return;
        const bool focused = editor.hasKeyboardFocus(true) && !editor.isReadOnly();
        const auto colour = editor.findColour(focused ? juce::TextEditor::focusedOutlineColourId
                                                      : juce::TextEditor::outlineColourId);
        if (colour.isTransparent())
            return;
        g.setColour(colour);
        g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f,
                               6.0f, focused ? 1.8f : 1.0f);
    }

    // The menu bar: the ground colour with a rule under it, each title a pill that lights up under the
    // mouse and stays lit while its menu is open; a red dot after a title says something is new there
    juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override
    {
        return juce::Font(AppTheme::uiFont(14.0f));
    }

    int getMenuBarItemWidth(juce::MenuBarComponent& bar, int itemIndex, const juce::String& itemText) override
    {
        const bool marked = itemText.endsWith(newMarker());
        const auto plain = marked ? itemText.dropLastCharacters(newMarker().length()) : itemText;
        const int textW = static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(getMenuBarFont(bar, itemIndex, plain), plain)));
        return textW + 28 + (marked ? 14 : 0);
    }

    void drawMenuBarBackground(juce::Graphics& g, int width, int height, bool, juce::MenuBarComponent&) override
    {
        const auto& pal = AppTheme::palette();
        g.fillAll(pal.backgroundMain);
        g.setColour(pal.borderColor);
        g.fillRect(0, height - 1, width, 1);
    }

    void drawMenuBarItem(juce::Graphics& g, int width, int height, int itemIndex,
                         const juce::String& itemText, bool isMouseOverItem, bool isMenuOpen,
                         bool, juce::MenuBarComponent& menuBar) override
    {
        const auto& pal = AppTheme::palette();
        const bool marked = itemText.endsWith(newMarker());
        const auto plain = marked ? itemText.dropLastCharacters(newMarker().length()) : itemText;
        const bool lit = isMouseOverItem || isMenuOpen;

        if (lit)
        {
            g.setColour(isMenuOpen ? pal.buttonActive : pal.buttonBackground);
            g.fillRoundedRectangle(juce::Rectangle<float>(1.0f, 3.0f, static_cast<float>(width) - 2.0f, static_cast<float>(height) - 7.0f), 6.0f);
        }

        g.setColour(lit ? pal.textPrimary : pal.textSecondary);
        const auto font = getMenuBarFont(menuBar, itemIndex, plain);
        g.setFont(font);
        const float textW = juce::GlyphArrangement::getStringWidth(font, plain);
        const float total = textW + (marked ? 14.0f : 0.0f);
        const float x = (static_cast<float>(width) - total) * 0.5f;
        g.drawText(plain, juce::Rectangle<float>(x, 0.0f, textW + 2.0f, static_cast<float>(height) - 1.0f),
                   juce::Justification::centredLeft, false);
        if (marked)
        {
            const float r = 3.5f;
            g.setColour(newColour());
            g.fillEllipse(x + textW + 5.0f, static_cast<float>(height) * 0.5f - r - 3.0f, r * 2.0f, r * 2.0f);
        }
    }
};
