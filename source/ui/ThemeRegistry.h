#pragma once

#include "AppTheme.h"
#include "ColorScheme.h"

#include <functional>

// A complete editor theme: app chrome palette + patch canvas color scheme.
// Canvas factories read AppTheme::palette() at call time, so callers must
// AppTheme::setPalette(theme.app) before invoking makeCanvas.
struct EditorTheme
{
    juce::String name;
    AppThemePalette app;
    std::function<ColorScheme()> makeCanvas;
};

namespace ThemeRegistry
{
    int count();
    const EditorTheme& get(int index);   // out-of-range falls back to "Dark"
    juce::StringArray names();

    // -1 when there is no theme of that name. Themes are saved by name, so adding
    // or removing user theme files never moves the saved choice to another theme.
    int indexOfName(const juce::String& name);

    // The built-in themes only (those the editor ships), by name; nullptr if none.
    const EditorTheme* findBuiltin(const juce::String& name);

    // Rescans the user's themes folder. The built-in themes keep their indices and
    // user themes follow them, so only indices past `builtinCount()` can change.
    // References returned by `get` are invalidated.
    void reloadUserThemes();
    int builtinCount();
}
