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
//                 "canvasTexture": false }
//   }
//
// Colours are "#rrggbb" or "#aarrggbb". Every key is optional: a missing or
// unreadable one keeps the value of the base theme, so a half-written file still
// loads. Render styles (Wireframe, Hardware look, Flat knobs) are not part of a
// theme yet; they stay global options.
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

    // "#rrggbb" for opaque colours, "#aarrggbb" otherwise.
    juce::String colourToString(juce::Colour c);
    // False when `s` is not a colour.
    bool colourFromString(const juce::String& s, juce::Colour& out);

    // Where the user's theme files live: a "themes" folder next to the settings file.
    juce::File userFolder();

    // Every *.json in `folder` that reads as a theme, sorted by name.
    std::vector<EditorTheme> loadFolder(const juce::File& folder, const EditorTheme& fallbackBase);
}
