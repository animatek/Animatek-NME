#include "MidiSettingsDialog.h"
#include "AppTheme.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kText   (AppTheme::palette().textSecondary)
#define kDim    (AppTheme::palette().textMuted)
#define kCtrlBg (AppTheme::palette().inputBackground)
#define kCtrlBd (AppTheme::palette().borderColor)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)

static void styleLabel (juce::Label& l, bool bold = false)
{
    l.setFont (bold ? juce::Font (AppTheme::uiFont (11.0f).withStyle ("Bold"))
                    : juce::Font (AppTheme::uiFont (12.0f)));
    l.setColour (juce::Label::textColourId,       bold ? AppTheme::palette().textPrimary : kText);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

static void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, kCtrlBg);
    c.setColour (juce::ComboBox::outlineColourId,    kCtrlBd);
    c.setColour (juce::ComboBox::textColourId,       kText);
    c.setColour (juce::ComboBox::arrowColourId,      kText);
    c.setColour (juce::ComboBox::focusedOutlineColourId, kBtnOn);
}

// ─────────────────────────────────────────────────────────────────────────────
MidiSettingsDialog::MidiSettingsDialog()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this]() { close(); };
    addAndMakeVisible (closeButton);

    for (int i = 0; i < kMaxSynths; ++i)
    {
        auto& g = groups[static_cast<size_t> (i)];
        styleLabel (g.inLabel);
        styleLabel (g.outLabel);
        styleLabel (g.statusCaption);
        styleLabel (g.status);
        g.status.setColour (juce::Label::textColourId, kDim);
        styleCombo (g.inCombo);
        styleCombo (g.outCombo);
        g.enabled.setColour (juce::ToggleButton::textColourId, kText);
        g.enabled.setColour (juce::ToggleButton::tickColourId, AppTheme::palette().textPrimary);
        g.enabled.onClick = [this, i]() { updateEnabledState (i); };
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &g.inLabel, &g.outLabel, &g.inCombo, &g.outCombo, &g.enabled, &g.statusCaption, &g.status })
            addAndMakeVisible (c);
    }

    for (auto* b : { &okButton, &cancelButton, &applyButton })
    {
        b->setColour (juce::TextButton::buttonColourId,   kBtnBg);
        b->setColour (juce::TextButton::buttonOnColourId, kBtnOn);
        b->setColour (juce::TextButton::textColourOffId,  kText);
        b->setColour (juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
        addAndMakeVisible (b);
    }
    okButton.onClick     = [this]() { apply(); close(); };
    applyButton.onClick  = [this]() { apply(); };
    cancelButton.onClick = [this]() { close(); };

    refreshDeviceLists();
    setSize (480, 38 + kMaxSynths * 112 + 52);
}

// ─────────────────────────────────────────────────────────────────────────────
void MidiSettingsDialog::refreshDeviceLists()
{
    inputIds.clear();
    outputIds.clear();
    for (auto& g : groups) { g.inCombo.clear(); g.outCombo.clear(); }

    for (auto& d : ConnectionManager::getAvailableInputDevices())
    {
        inputIds.add (d.identifier);
        for (auto& g : groups)
            g.inCombo.addItem (d.name, inputIds.size());
    }
    for (auto& d : ConnectionManager::getAvailableOutputDevices())
    {
        outputIds.add (d.identifier);
        for (auto& g : groups)
            g.outCombo.addItem (d.name, outputIds.size());
    }
}

void MidiSettingsDialog::setPorts (const Ports& ports)
{
    applied = ports;
    for (int i = 0; i < kMaxSynths; ++i)
    {
        auto& g = groups[static_cast<size_t> (i)];
        const auto& p = ports[static_cast<size_t> (i)];
        const int inIdx  = inputIds.indexOf (p.inputId);
        const int outIdx = outputIds.indexOf (p.outputId);
        if (inIdx  >= 0) g.inCombo.setSelectedItemIndex  (inIdx,  juce::dontSendNotification);
        if (outIdx >= 0) g.outCombo.setSelectedItemIndex (outIdx, juce::dontSendNotification);
        g.enabled.setToggleState (p.enabled, juce::dontSendNotification);
        updateEnabledState (i);
    }
}

void MidiSettingsDialog::updateEnabledState (int i)
{
    auto& g = groups[static_cast<size_t> (i)];
    const bool on = g.enabled.getToggleState();
    g.inCombo.setEnabled (on);
    g.outCombo.setEnabled (on);
    g.inLabel.setEnabled (on);
    g.outLabel.setEnabled (on);
    if (! on && ! g.connected)
        g.status.setText ({}, juce::dontSendNotification);
}

void MidiSettingsDialog::setPortStatus (int port, const ConnectionManager::Status& status)
{
    if (port < 0 || port >= kMaxSynths)
        return;
    auto& g = groups[static_cast<size_t> (port)];
    g.connected = (status.state == ConnectionManager::State::Connected);
    g.status.setText (status.message, juce::dontSendNotification);

    juce::Colour col = kDim;
    if      (status.state == ConnectionManager::State::Connected)  col = AppTheme::palette().accentSuccess;
    else if (status.state == ConnectionManager::State::Connecting) col = AppTheme::palette().accentWarning;
    g.status.setColour (juce::Label::textColourId, col);
}

MidiSettingsDialog::Port MidiSettingsDialog::currentPort (int i) const
{
    const auto& g = groups[static_cast<size_t> (i)];
    Port p;
    p.enabled = g.enabled.getToggleState();
    const int inIdx  = g.inCombo.getSelectedItemIndex();
    const int outIdx = g.outCombo.getSelectedItemIndex();
    if (inIdx  >= 0) p.inputId  = inputIds[inIdx];
    if (outIdx >= 0) p.outputId = outputIds[outIdx];
    return p;
}

void MidiSettingsDialog::apply()
{
    for (int i = 0; i < kMaxSynths; ++i)
    {
        const auto wanted = currentPort (i);
        const auto& before = applied[static_cast<size_t> (i)];
        const bool changed = wanted.enabled != before.enabled
                          || wanted.inputId != before.inputId || wanted.outputId != before.outputId;
        // An enabled port that is not connected is tried again: pressing Apply is how
        // one asks for another look.
        const bool retry = wanted.enabled && ! groups[static_cast<size_t> (i)].connected;
        if ((changed || retry) && onApply)
            onApply (i, wanted);
        applied[static_cast<size_t> (i)] = wanted;
    }
}

void MidiSettingsDialog::close() { removeFromDesktop(); delete this; }

bool MidiSettingsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::returnKey) { okButton.triggerClick(); return true; }
    return false;
}

void MidiSettingsDialog::mouseDown (const juce::MouseEvent& e)
    { if (e.getPosition().getY() < 32) dragger.startDraggingComponent (this, e); }
void MidiSettingsDialog::mouseDrag (const juce::MouseEvent& e)
    { dragger.dragComponent (this, e, nullptr); }

// ─────────────────────────────────────────────────────────────────────────────
void MidiSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (AppTheme::palette().textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
    g.drawText ("MIDI Setup", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);

    // One framed group per port, titled as in the original editor.
    constexpr int pad = 12, groupH = 104;
    for (int i = 0; i < kMaxSynths; ++i)
    {
        const juce::Rectangle<float> r (static_cast<float> (pad), static_cast<float> (38 + i * 112 + 6),
                                        static_cast<float> (getWidth() - pad * 2), static_cast<float> (groupH - 6));
        g.setColour (kCtrlBd);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        const auto title = "Port " + juce::String (i + 1);
        g.setFont (juce::Font (AppTheme::uiFont (11.0f)).boldened());
        const int tw = static_cast<int> (g.getCurrentFont().getStringWidthFloat (title)) + 10;
        g.setColour (kBg);
        g.fillRect (r.getX() + 8, r.getY() - 7, static_cast<float> (tw), 14.0f);
        g.setColour (AppTheme::palette().textPrimary);
        g.drawText (title, static_cast<int> (r.getX()) + 8, static_cast<int> (r.getY()) - 7, tw, 14,
                    juce::Justification::centred);
    }
}

void MidiSettingsDialog::resized()
{
    constexpr int titleH = 32, pad = 12, rowH = 24;
    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    for (int i = 0; i < kMaxSynths; ++i)
    {
        auto& g = groups[static_cast<size_t> (i)];
        auto area = juce::Rectangle<int> (pad + 10, titleH + 6 + i * 112 + 14, getWidth() - pad * 2 - 20, 84);
        auto row1 = area.removeFromTop (rowH);
        g.inLabel.setBounds (row1.removeFromLeft (24));
        const int comboW = (row1.getWidth() - 34) / 2;
        g.inCombo.setBounds (row1.removeFromLeft (comboW));
        g.outLabel.setBounds (row1.removeFromLeft (34).withTrimmedLeft (8));
        g.outCombo.setBounds (row1);
        area.removeFromTop (6);
        auto row2 = area.removeFromTop (rowH);
        g.enabled.setBounds (row2.removeFromLeft (100).withTrimmedLeft (24));
        g.statusCaption.setBounds (row2.removeFromLeft (50));
        g.status.setBounds (row2);
    }

    constexpr int btnW = 84, btnH = 28;
    auto bottom = juce::Rectangle<int> (0, getHeight() - btnH - 12, getWidth(), btnH);
    const int total = btnW * 3 + 16;
    bottom = bottom.withSizeKeepingCentre (total, btnH);
    okButton.setBounds (bottom.removeFromLeft (btnW));
    bottom.removeFromLeft (8);
    cancelButton.setBounds (bottom.removeFromLeft (btnW));
    bottom.removeFromLeft (8);
    applyButton.setBounds (bottom.removeFromLeft (btnW));
}

// ─────────────────────────────────────────────────────────────────────────────
MidiSettingsDialog* MidiSettingsDialog::show (juce::Component* parent, const Ports& ports,
                                              std::function<void(int, const Port&)> applyCb)
{
    auto* dlg = new MidiSettingsDialog();
    dlg->setPorts (ports);
    dlg->onApply = std::move (applyCb);

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
    dlg->grabKeyboardFocus();
    return dlg;
}
