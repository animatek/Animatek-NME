#pragma once

#include "AppTheme.h"
#include "ColorScheme.h"

#include <functional>
#include <optional>

// The render styles, which are Editor Options: Wireframe, Hardware look (with its
// knob style) and Flat knobs.
struct RenderStyle
{
    bool wireframe = false;
    bool hardwareLook = false;
    int  knobStyle = 2;   // 0 domed cap, 1 black knurled, 2 aluminium knurled
    bool flatKnobs = false;

    bool operator==(const RenderStyle& o) const
    {
        return wireframe == o.wireframe && hardwareLook == o.hardwareLook
            && knobStyle == o.knobStyle && flatKnobs == o.flatKnobs;
    }
    bool operator!=(const RenderStyle& o) const { return !(*this == o); }
};

// The render styles a theme sets when it is chosen (#90). A key left empty keeps
// the user's own; a theme that sets none leaves the render style alone.
struct ThemeRender
{
    std::optional<bool> wireframe, hardwareLook, flatKnobs;
    std::optional<int>  knobStyle;

    bool any() const { return wireframe || hardwareLook || flatKnobs || knobStyle; }
    RenderStyle over(RenderStyle s) const
    {
        if (wireframe)    s.wireframe = *wireframe;
        if (hardwareLook) s.hardwareLook = *hardwareLook;
        if (knobStyle)    s.knobStyle = *knobStyle;
        if (flatKnobs)    s.flatKnobs = *flatKnobs;
        return s;
    }
    static ThemeRender all(const RenderStyle& s)
    {
        return { s.wireframe, s.hardwareLook, s.flatKnobs, s.knobStyle };
    }
};

// A complete editor theme: app chrome palette + patch canvas color scheme.
// Canvas factories read AppTheme::palette() at call time, so callers must
// AppTheme::setPalette(theme.app) before invoking makeCanvas.
struct EditorTheme
{
    juce::String name;
    AppThemePalette app;
    std::function<ColorScheme()> makeCanvas;
    ThemeRender render;
    // An official theme file (from animatek/nme-themes) with a built-in's name: it
    // replaces that built-in in place instead of being listed beside it.
    bool official = false;
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

    // Adds the user's themes to `all` (built-ins first): an official one with a built-in's
    // name takes that built-in's place; any other name already taken gets " (user)".
    void mergeUserThemes(std::vector<EditorTheme>& all, std::vector<EditorTheme> user);

    // The render style after switching to a theme that sets `next`. While a theme
    // sets a style, `saved` keeps the user's own, so a theme's flat knobs do not
    // follow the user into every theme after it: leaving it gives the style back.
    RenderStyle switchRender(const RenderStyle& current, std::optional<RenderStyle>& saved,
                             const ThemeRender& next);
}
