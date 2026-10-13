#pragma once

#include "ThemeRegistry.h"

// Themes as files. A theme file is JSON:
//
//   {
//     "format": 1,
//     "name":   "My theme",
//     "base":   "Dark",                      // built-in theme used for any key left out
//     "app":    { "backgroundMain": "#323232", ... },
//     "canvas": { "gridBackground": "#1f2125", ...,
//                 "morphColor": ["#cb4f4f", "#4fcb4f", "#4f4fcb", "#cbcb4f"],
//                 "canvasTexture": false },
//     "render": { "flatKnobs": true }          // optional, see below
//   }
//
// Colours are "#rrggbb" or "#aarrggbb". Every key is optional: a missing or
// unreadable one keeps the value of the base theme, so a half-written file still
// loads. "render" is the render style the theme turns on when it is chosen
// ("wireframe", "hardwareLook", "flatKnobs": true/false; "knobStyle": 0-2); it is
// not inherited from the base, and a key left out keeps the user's own setting.
namespace ThemeFile
{
    constexpr int kFormat = 1;

    // The theme as a complete snapshot: every colour written out.
    // `baseName` is written as the file's "base" (default: the theme's own name).
    juce::String toJson(const EditorTheme& theme, const juce::String& baseName = {});

    // Reads `text` over its base theme. False (with `error` set) only when it is not a theme
    // file at all; unknown keys and bad colours are skipped, not fatal.
    // The base is the built-in theme the file names in "base", else `fallbackBase`.
    bool fromJson(const juce::String& text, const EditorTheme& fallbackBase,
                  EditorTheme& out, juce::String& error);

    // An editable snapshot of a theme: the app palette and the finished canvas scheme.
    struct Doc
    {
        juce::String name;
        AppThemePalette app;
        ColorScheme canvas;
        ThemeRender render;
    };
    Doc makeDoc(const EditorTheme& theme);
    EditorTheme makeTheme(const Doc& doc);

    // One editable colour. `group` is what the editor sorts it under; `label` is plain English.
    // Exactly one of `app` / `canvas` is set, or `morph` is 0-3 for a morph group colour.
    struct Entry
    {
        const char* group;
        const char* label;
        juce::Colour AppThemePalette::* app = nullptr;
        juce::Colour ColorScheme::*     canvas = nullptr;
        int morph = -1;
        bool autoWhenTransparent = false;   // transparent means "derive from the theme"
    };
    const std::vector<Entry>& entries();
    juce::Colour& colourRef(Doc& doc, const Entry& e);

    // "#rrggbb" for opaque colours, "#aarrggbb" otherwise.
    juce::String colourToString(juce::Colour c);
    // False when `s` is not a colour.
    bool colourFromString(const juce::String& s, juce::Colour& out);

    // A built-in theme as the official file animatek/nme-themes carries: every colour,
    // "official": true and Animatek as author, LF line ends. NME hashes this to tell
    // whether the repository's copy still matches the theme it ships.
    juce::String officialJson(const EditorTheme& builtin);
    // "Gruvbox Dark" -> "gruvbox-dark": the file name a theme gets in the repository.
    juce::String slugOf(const juce::String& name);

    // Where the user's theme files live: a "themes" folder next to the settings file.
    juce::File userFolder();

    // A file bigger than this is not a theme: import refuses it before reading it.
    constexpr juce::int64 kMaxFileBytes = 256 * 1024;

    // Every *.json in `folder` that reads as a theme, sorted by name.
    std::vector<EditorTheme> loadFolder(const juce::File& folder, const EditorTheme& fallbackBase);
}
