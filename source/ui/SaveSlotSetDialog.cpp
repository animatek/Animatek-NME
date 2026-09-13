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
    l.setFont (juce::Font (AppTheme::uiFont (12.0f)));
    l.setColour (juce::Label::textColourId, AppTheme::palette().textSecondary);
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
        label.setFont (juce::Font (AppTheme::uiFont (13.0f)));
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
    notesEditor.setMultiLine (true);
    notesEditor.setReturnKeyStartsNewLine (true);
    notesEditor.setTextToShowWhenEmpty ("Optional: tempo, what to bring in first...",
                                        AppTheme::palette().textMuted);
    addAndMakeVisible (notesEditor);

    styleButton (saveButton);
    styleButton (cancelButton);
    saveButton.onClick   = [this] { confirm(); };
    cancelButton.onClick = [this] { cancel();  };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (cancelButton);

    setSize (440, 360);
    setWantsKeyboardFocus (true);
    updateSaveButton();
}

void SaveSlotSetDialog::paint (juce::Graphics& g)
{
    g.fillAll (AppTheme::palette().backgroundMain);

    g.setColour (AppTheme::palette().textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
    g.drawText ("Save Slot Set", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (AppTheme::palette().buttonActive);
    g.fillRect (0, 31, getWidth(), 1);

    const int sepY = getHeight() - 50;
    g.drawHorizontalLine (sepY, 14.0f, static_cast<float> (getWidth() - 14));
}

void SaveSlotSetDialog::resized()
{
    constexpr int titleH = 32, pad = 14, rowH = 26, gap = 4, labelW = 52, modeW = 150;

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = titleH + 10;
    nameLabel .setBounds (pad, y, labelW, rowH);
    nameEditor.setBounds (pad + labelW, y, getWidth() - pad * 2 - labelW, rowH);
    y += rowH + 12;

    for (size_t i = 0; i < 4; ++i)
    {
        slotModes[i] .setBounds (getWidth() - pad - modeW, y, modeW, rowH);
        slotLabels[i].setBounds (pad, y, getWidth() - pad * 2 - modeW - 8, rowH);
        y += rowH + gap;
    }
    y += 8;

    notesLabel.setBounds (pad, y, labelW, 20);
    y += 20;
    const int btnW = 80, btnH = 26;
    const int btnY = getHeight() - pad - btnH;
    notesEditor.setBounds (pad, y, getWidth() - pad * 2, (getHeight() - 50 - 10) - y);

    cancelButton.setBounds (getWidth() - pad - btnW,         btnY, btnW, btnH);
    saveButton  .setBounds (getWidth() - pad - btnW * 2 - 8, btnY, btnW, btnH);
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
