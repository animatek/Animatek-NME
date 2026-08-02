#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

enum class AppThemeId
{
    SoftDarkGrey = 0,
    DeepDarkGrey = 1
};

struct AppThemePalette
{
    juce::Colour backgroundMain;
    juce::Colour backgroundPanel;
    juce::Colour backgroundSecondary;
    juce::Colour backgroundElevated;
    juce::Colour inputBackground;
    juce::Colour buttonBackground;
    juce::Colour buttonHover;
    juce::Colour buttonActive;
    juce::Colour borderColor;
    juce::Colour gridLine;
    juce::Colour gridLineStrong;
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour textMuted;
    juce::Colour accentActive;
    juce::Colour accentWarning;
    juce::Colour accentSuccess;
    juce::Colour accentInfo;     // cool accent (blue/cyan), distinct from the warm ones
};

namespace AppTheme
{
    AppThemeId currentTheme();
    const AppThemePalette& palette();
    const AppThemePalette& palette(AppThemeId id);

    // Every piece of text the application chrome paints itself goes through
    // here: panels, browsers, the inspector, the header bar, the status bar and
    // the dialogs. The sizes were chosen module-by-module and ended up smaller
    // than the rest of the desktop, so one scale lifts them together and keeps
    // their relative weights intact. Module bodies on the canvas are deliberately
    // NOT scaled: their text has to stay inside a fixed hardware-derived grid.
    float uiFontScale();
    void setUiFontScale(float scale);

    // Chains like any FontOptions: AppTheme::uiFont(11.0f).withStyle("Bold").
    juce::FontOptions uiFont(float points);

    // Ink for text that sits on top of a morph/macro colour, or next to it. The
    // four macro colours span light and dark, so painting a label in its own
    // colour left green Macro 2 unreadable on the light Nord Classic chrome.
    juce::Colour macroLabelInk();

    juce::String displayName(AppThemeId id);
    AppThemeId themeFromInt(int value);

    void setTheme(AppThemeId id);
    void setPalette(const AppThemePalette& p);
    void applyLookAndFeel();
}
