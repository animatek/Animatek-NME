#include "SaveSlotSetDialog.h"
#include "AppTheme.h"

namespace
{
const char* const kLetters[] = { "A", "B", "C", "D" };

void styleButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,   AppTheme::palette().buttonBackground);
    b.setColour (juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
    b.setColour (juce::TextButton::textColourOffId,  AppTheme::palette().textSecondary);
    b.setColour (juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
}

void styleEditor (juce::TextEditor& e)
{
    e.setColour (juce::TextEditor::backgroundColourId,     AppTheme::palette().inputBackground);
    e.setColour (juce::TextEditor::textColourId,           AppTheme::palette().textPrimary);
    e.setColour (juce::TextEditor::outlineColourId,        AppTheme::palette().borderColor);
    e.setColour (juce::TextEditor::focusedOutlineColourId, AppTheme::palette().accentActive);
}

void styleLabel (juce::Label& l)
{
    l.setFont (juce::Font (AppTheme::uiFont (12.5f)).boldened());
    l.setColour (juce::Label::textColourId, AppTheme::palette().textMuted);
}
}

// ─────────────────────────────────────────────────────────────────────────────
SaveSlotSetDialog::SaveSlotSetDialog (const juce::String& suggestedName,
                                      const std::array<SlotInfo, 4>& slots,
                                      Callback cb)
    : callback (std::move (cb))
{
    closeButton.onClick = [this] { cancel(); };
    addAndMakeVisible (closeButton);

    styleLabel (nameLabel);
    addAndMakeVisible (nameLabel);
    styleEditor (nameEditor);
    nameEditor.setFont (juce::Font (AppTheme::uiFont (14.0f)));
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setIndents (10, 0);
    nameEditor.setText (suggestedName, juce::dontSendNotification);
    nameEditor.setTextToShowWhenEmpty ("e.g. Fractura", AppTheme::palette().textMuted);
    nameEditor.onTextChange = [this] { updateSaveButton(); };
    nameEditor.onReturnKey  = [this] { confirm(); };
    nameEditor.onEscapeKey  = [this] { cancel(); };
    addAndMakeVisible (nameEditor);

    for (int i = 0; i < 4; ++i)
    {
        const auto& info = slots[static_cast<size_t> (i)];

        auto& label = slotLabels[static_cast<size_t> (i)];
        juce::String text = juce::String (kLetters[i]) + ":  "
                           + (info.hasPatch ? info.patchName : juce::String ("(empty)"));
        if (info.hasPatch && info.enabledKnown && !info.enabled)
            text << "  (off)";
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (AppTheme::uiFont (14.0f)));
        label.setColour (juce::Label::textColourId, info.hasPatch ? AppTheme::palette().textPrimary
                                                                  : AppTheme::palette().textMuted);
        addAndMakeVisible (label);

        auto& box = slotModes[static_cast<size_t> (i)];
        box.addItem ("Include patch",   kInclude);
        box.addItem ("Switch slot off", kDisable);
        box.addItem ("Leave as is",     kKeep);
        box.setItemEnabled (kInclude, info.hasPatch);
        box.setSelectedId (info.hasPatch ? kInclude : kKeep, juce::dontSendNotification);
        box.setColour (juce::ComboBox::backgroundColourId, AppTheme::palette().inputBackground);
        box.setColour (juce::ComboBox::textColourId,       AppTheme::palette().textPrimary);
        box.setColour (juce::ComboBox::outlineColourId,    AppTheme::palette().borderColor);
        box.setColour (juce::ComboBox::arrowColourId,      AppTheme::palette().textSecondary);
        box.onChange = [this] { updateSaveButton(); };
        addAndMakeVisible (box);
    }

    styleLabel (notesLabel);
    addAndMakeVisible (notesLabel);
    styleEditor (notesEditor);
    notesEditor.setFont (juce::Font (AppTheme::uiFont (14.0f)));
    notesEditor.setIndents (10, 8);
    notesEditor.setMultiLine (true);
    notesEditor.setReturnKeyStartsNewLine (true);
    notesEditor.setTextToShowWhenEmpty ("Optional: tempo, what to bring in first...",
                                        AppTheme::palette().textMuted);
    addAndMakeVisible (notesEditor);

    styleButton (saveButton);
    styleButton (cancelButton);
    saveButton.setColour (juce::TextButton::buttonColourId,  AppTheme::palette().accentActive);   // Save does the thing
    saveButton.setColour (juce::TextButton::textColourOffId, AppTheme::palette().backgroundMain);
    saveButton.onClick   = [this] { confirm(); };
    cancelButton.onClick = [this] { cancel();  };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (cancelButton);

    setSize (520, 480);
    setWantsKeyboardFocus (true);
    updateSaveButton();
}

void SaveSlotSetDialog::paint (juce::Graphics& g)
{
    AppTheme::drawDialogChrome (g, getLocalBounds(), "Save Slot Set", 40, 68);

    // A rule between slot rows
    const auto& pal = AppTheme::palette();
    g.setColour (pal.borderColor.withAlpha (0.5f));
    for (size_t i = 0; i < 4; ++i)
        g.fillRect (24, slotLabels[i].getBottom() + 5, getWidth() - 48, 1);
}

void SaveSlotSetDialog::resized()
{
    constexpr int titleH = 40, footerH = 68, side = 24, fieldH = 36, rowH = 40, modeW = 170;

    closeButton.setBounds (getWidth() - 36, 6, 28, 28);

    int y = titleH + 16;
    nameLabel .setBounds (side, y, getWidth() - side * 2, 18);
    nameEditor.setBounds (side, y + 22, getWidth() - side * 2, fieldH);
    y += 22 + fieldH + 16;

    for (size_t i = 0; i < 4; ++i)
    {
        slotModes[i] .setBounds (getWidth() - side - modeW, y, modeW, 32);
        slotLabels[i].setBounds (side, y, getWidth() - side * 2 - modeW - 8, 32);
        y += rowH;
    }
    y += 6;

    notesLabel.setBounds (side, y, getWidth() - side * 2, 18);
    y += 22;
    notesEditor.setBounds (side, y, getWidth() - side * 2, getHeight() - footerH - 16 - y);

    const int btnW = 100, btnH = 36;
    const int btnY = getHeight() - footerH + (footerH - btnH) / 2;
    cancelButton.setBounds (getWidth() - side - btnW,          btnY, btnW, btnH);
    saveButton  .setBounds (getWidth() - side - btnW * 2 - 12, btnY, btnW, btnH);
}

bool SaveSlotSetDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { cancel();  return true; }
    if (key == juce::KeyPress::returnKey) { confirm(); return true; }
    return false;
}

void SaveSlotSetDialog::mouseDown (const juce::MouseEvent& e) { dragger.startDraggingComponent (this, e); }
void SaveSlotSetDialog::mouseDrag (const juce::MouseEvent& e) { dragger.dragComponent (this, e, nullptr); }

// A set needs a name that survives as a folder name, and has to do something:
// four "Leave as is" would save a set that loads nothing.
void SaveSlotSetDialog::updateSaveButton()
{
    bool changesAnything = false;
    for (auto& box : slotModes)
        changesAnything = changesAnything || box.getSelectedId() != kKeep;

    saveButton.setEnabled (changesAnything && slotSetFolderName (nameEditor.getText()).isNotEmpty());
}

void SaveSlotSetDialog::confirm()
{
    if (!saveButton.isEnabled())
        return;

    Result r;
    r.confirmed = true;
    r.name  = nameEditor.getText().trim();
    r.notes = notesEditor.getText().trim();
    for (size_t i = 0; i < 4; ++i)
    {
        const int id = slotModes[i].getSelectedId();
        r.modes[i] = id == kInclude ? SlotSet::Mode::Patch
                   : id == kDisable ? SlotSet::Mode::Disable
                                    : SlotSet::Mode::Keep;
    }
    if (callback) callback (r);
    closeSelf();
}

void SaveSlotSetDialog::cancel()
{
    Result r;
    if (callback) callback (r);
    closeSelf();
}

// ─────────────────────────────────────────────────────────────────────────────
void SaveSlotSetDialog::show (juce::Component* parent,
                              const juce::String& suggestedName,
                              const std::array<SlotInfo, 4>& slots,
                              Callback cb)
{
    auto* dlg = new SaveSlotSetDialog (suggestedName, slots, std::move (cb));

    if (parent != nullptr)
    {
        auto* top    = parent->getTopLevelComponent();
        auto  screen = top->localAreaToGlobal (top->getLocalBounds());
        dlg->setTopLeftPosition (screen.getX() + (screen.getWidth()  - dlg->getWidth())  / 2,
                                 screen.getY() + (screen.getHeight() - dlg->getHeight()) / 2);
    }

    dlg->addToDesktop (juce::ComponentPeer::windowHasDropShadow);
    dlg->setVisible (true);
    dlg->toFront (true);
    dlg->nameEditor.grabKeyboardFocus();
}
