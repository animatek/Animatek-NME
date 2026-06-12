#pragma once

#include "AppTheme.h"
#include "ColorScheme.h"

// A complete editor theme: app chrome palette + patch canvas color scheme.
// Canvas factories read AppTheme::palette() at call time, so callers must
// AppTheme::setPalette(theme.app) before invoking makeCanvas.
struct EditorTheme
{
    juce::String name;
    AppThemePalette app;
    ColorScheme (*makeCanvas)();
};

namespace ThemeRegistry
{
    int count();
    const EditorTheme& get(int index);   // out-of-range falls back to "Dark"
    juce::StringArray names();
}
