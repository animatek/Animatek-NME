#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
