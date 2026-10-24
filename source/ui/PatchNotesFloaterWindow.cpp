#include "PatchNotesFloaterWindow.h"
#include "AppTheme.h"
#include "../model/Patch.h"

#define kBg      (AppTheme::palette().backgroundMain)
#define kText    (AppTheme::palette().textPrimary)
#define kTextDim (AppTheme::palette().textMuted)
#define kBorder  (AppTheme::palette().borderColor)

// ============================================================================
// PatchNotesFloaterPanel
// ============================================================================
PatchNotesFloaterPanel::PatchNotesFloaterPanel()
{
    editor.setMultiLine(true, /*wordWrap*/ false);
    editor.setReturnKeyStartsNewLine(true);
    editor.setTabKeyUsedAsCharacter(true);
    editor.setScrollbarsShown(true);
    editor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f,
                              juce::Font::plain));
    editor.setIndents(14, 12);   // the text does not touch the field's edge
    editor.onTextChange = [this]() {
        if (patch != nullptr)
            patch->patchNotes = editor.getText();
    };
    addAndMakeVisible(editor);

    setPatch(nullptr);
    applyTheme();
    setSize(420, 300);
}

void PatchNotesFloaterPanel::setPatch(Patch* newPatch)
{
    patch = newPatch;
    editor.setText(patch != nullptr ? patch->patchNotes : juce::String(),
                   juce::dontSendNotification);
    editor.setEnabled(patch != nullptr);
    editor.setTextToShowWhenEmpty(patch != nullptr ? "Type patch notes here..."
                                                   : "No patch loaded",
                                  kTextDim);
    repaint();
}

void PatchNotesFloaterPanel::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
}

void PatchNotesFloaterPanel::resized()
{
    editor.setBounds(getLocalBounds().reduced(16));
}

void PatchNotesFloaterPanel::applyTheme()
{
    editor.setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().inputBackground);
    editor.setColour(juce::TextEditor::textColourId, kText);
    editor.setColour(juce::TextEditor::outlineColourId, kBorder);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, AppTheme::palette().accentActive);
    editor.setColour(juce::CaretComponent::caretColourId, kText);
    editor.applyColourToAllText(kText);
    repaint();
}

// ============================================================================
// PatchNotesFloaterWindow
// ============================================================================
PatchNotesFloaterWindow::PatchNotesFloaterWindow()
    : juce::DocumentWindow("Patch Notes", kBg, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setContentNonOwned(&panel, true);
    setResizable(true, true);
    setResizeLimits(260, 160, 1200, 900);
}

void PatchNotesFloaterWindow::closeButtonPressed()
{
    setVisible(false);
    if (onClosed) onClosed();
}

void PatchNotesFloaterWindow::applyTheme()
{
    setBackgroundColour(kBg);
    panel.applyTheme();
    repaint();
}
