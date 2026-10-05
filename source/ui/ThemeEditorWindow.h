#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "ThemeFile.h"

// Live theme editor (#90, step 2): one swatch per colour, grouped, with a colour
// selector on click. Every change is handed to `onChange` at once so the canvas
// repaints while the colour is being dragged. Save writes a user theme file.
class ThemeEditorWindow : public juce::DocumentWindow
{
public:
    // `start` is the theme to edit; `baseName` the built-in the saved file falls back to.
    ThemeEditorWindow(const EditorTheme& start, const juce::String& baseName);
    ~ThemeEditorWindow() override;

    std::function<void(const EditorTheme&)> onChange;             // live preview
    std::function<void(const EditorTheme&, const juce::File&)> onSaved;   // after the file is written
    std::function<void()> onClosed;

    void closeButtonPressed() override;

private:
    class Content;
    Content* content = nullptr;
};
