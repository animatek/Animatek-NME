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

    juce::String displayName(AppThemeId id);
    AppThemeId themeFromInt(int value);

    void setTheme(AppThemeId id);
    void setPalette(const AppThemePalette& p);
    void applyLookAndFeel();
}
