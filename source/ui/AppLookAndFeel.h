#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AppTheme.h"

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
        // The word sits above the underline with air under it, not on top of the rule
        g.drawText(button.getButtonText(), r.withTrimmedBottom(10), juce::Justification::centred, false);
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

        g.setColour(on ? pal.accentActive : isMouseOver ? pal.buttonHover : pal.inputBackground);
        g.fillRoundedRectangle(box, 5.0f);
        if (!on)
        {
            g.setColour(pal.borderColor);
            g.drawRoundedRectangle(box.reduced(0.5f), 5.0f, 1.0f);
        }
        else
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

    void drawMenuBarItem(juce::Graphics& g, int width, int height, int itemIndex,
                         const juce::String& itemText, bool isMouseOverItem, bool isMenuOpen,
                         bool isMouseOverBar, juce::MenuBarComponent& menuBar) override
    {
        if (!itemText.endsWith(newMarker()))
        {
            LookAndFeel_V4::drawMenuBarItem(g, width, height, itemIndex, itemText,
                                            isMouseOverItem, isMenuOpen, isMouseOverBar, menuBar);
            return;
        }
        // The title without the marker, centred as usual in the wider item, and the dot after it.
        const auto plain = itemText.dropLastCharacters(newMarker().length());
        LookAndFeel_V4::drawMenuBarItem(g, width, height, itemIndex, plain,
                                        isMouseOverItem, isMenuOpen, isMouseOverBar, menuBar);
        const auto textW = juce::GlyphArrangement::getStringWidth(getMenuBarFont(menuBar, itemIndex, plain), plain);
        const float r = 3.5f;
        const float cx = (static_cast<float>(width) + textW) * 0.5f + 3.0f + r;
        g.setColour(newColour());
        g.fillEllipse(cx - r, static_cast<float>(height) * 0.5f - r - 3.0f, r * 2.0f, r * 2.0f);
    }
};
