#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class Patch;

/**
 * Patch Notes Floater: free-text notes for the active slot's patch. The text
 * round-trips through the .pch [Notes] section (file-only — the wire protocol
 * has no notes, so patches fetched from the synth start empty). Monospaced,
 * since original patches often carry ASCII tables/diagrams.
 */
class PatchNotesFloaterPanel : public juce::Component
{
public:
    PatchNotesFloaterPanel();

    void setPatch(Patch* patch);
    void applyTheme();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextEditor editor;
    Patch* patch = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchNotesFloaterPanel)
};

class PatchNotesFloaterWindow : public juce::DocumentWindow
{
public:
    PatchNotesFloaterWindow();

    void setPatch(Patch* patch) { panel.setPatch(patch); }
    void closeButtonPressed() override;
    void applyTheme();

    std::function<void()> onClosed;

private:
    PatchNotesFloaterPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchNotesFloaterWindow)
};
