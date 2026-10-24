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
        styleLabel (g.inLabel, true);
        styleLabel (g.outLabel, true);
        styleLabel (g.statusCaption);
        styleLabel (g.status);
        g.inLabel.setColour (juce::Label::textColourId, kDim);
        g.outLabel.setColour (juce::Label::textColourId, kDim);
        g.status.setColour (juce::Label::textColourId, kDim);
        g.status.setFont (juce::Font (AppTheme::uiFont (13.0f)));
        g.status.setJustificationType (juce::Justification::centredRight);
        styleCombo (g.inCombo);
        styleCombo (g.outCombo);
        g.enabled.setColour (juce::ToggleButton::textColourId, kText);
        g.enabled.setColour (juce::ToggleButton::tickColourId, AppTheme::palette().textPrimary);
        g.enabled.onClick = [this, i]() { updateEnabledState (i); };
        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &g.inLabel, &g.outLabel, &g.inCombo, &g.outCombo, &g.enabled, &g.statusCaption, &g.status })
            content.addAndMakeVisible (c);
    }

    for (auto* b : { &okButton, &cancelButton, &applyButton })
    {
        const bool primary = (b == &okButton);   // OK is the one that does the thing
        b->setColour (juce::TextButton::buttonColourId,   primary ? AppTheme::palette().accentActive : kBtnBg);
        b->setColour (juce::TextButton::buttonOnColourId, kBtnOn);
        b->setColour (juce::TextButton::textColourOffId,  primary ? AppTheme::palette().backgroundMain : kText);
        b->setColour (juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
        addAndMakeVisible (b);
    }
    okButton.onClick     = [this]() { apply(); close(); };
    applyButton.onClick  = [this]() { apply(); };
    cancelButton.onClick = [this]() { close(); };

    content.owner = this;
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    refreshDeviceLists();
    setSize (640, kTitleH + 8 + juce::jmin (kMaxSynths * kGroupH, 3 * kGroupH + kGroupH * 2 / 5) + kButtonsH);
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
    // A port whose device is not in the lists (an emulator that has gone) shows nothing selected.
    // That is not the user choosing nothing: it keeps the device it had, or OK would hand the
    // editor a change nobody made, and the port was taken as switched off by hand.
    const auto& had = applied[static_cast<size_t> (i)];
    p.inputId  = inIdx  >= 0 ? inputIds[inIdx]   : had.inputId;
    p.outputId = outIdx >= 0 ? outputIds[outIdx] : had.outputId;
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
    { if (e.getPosition().getY() < kTitleH) dragger.startDraggingComponent (this, e); }
void MidiSettingsDialog::mouseDrag (const juce::MouseEvent& e)
    { if (e.getMouseDownPosition().getY() < kTitleH) dragger.dragComponent (this, e, nullptr); }  // only a drag that began on the title bar moves the window (the press position says where it began)

// ─────────────────────────────────────────────────────────────────────────────
void MidiSettingsDialog::paint (juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    g.fillAll (kBg);

    // Title bar, as in Editor Options
    g.setColour (pal.textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (15.0f)).boldened());
    g.drawText ("MIDI Setup", 18, 0, getWidth() - 60, kTitleH, juce::Justification::centredLeft);
    g.setColour (pal.borderColor);
    g.fillRect (0, kTitleH - 1, getWidth(), 1);

    // Footer band under the cards
    g.setColour (pal.backgroundPanel);
    g.fillRect (0, getHeight() - kButtonsH, getWidth(), kButtonsH);
    g.setColour (pal.borderColor);
    g.fillRect (0, getHeight() - kButtonsH, getWidth(), 1);
}

void MidiSettingsDialog::Content::paint (juce::Graphics& g)
{
    // One card per port: its number and name, how it is doing and a switch at the top, its In and
    // Out ports below.
    const auto& pal = AppTheme::palette();
    constexpr int pad = 24;
    for (int i = 0; i < kMaxSynths; ++i)
    {
        const auto& group = owner->groups[static_cast<size_t> (i)];
        const juce::Rectangle<float> r (static_cast<float> (pad), static_cast<float> (i * kGroupH + 8),
                                        static_cast<float> (getWidth() - pad * 2), static_cast<float> (kGroupH - 12));
        g.setColour (pal.backgroundPanel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (pal.borderColor);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        const bool on = group.enabled.getToggleState();
        g.setColour (on ? pal.accentActive : pal.textMuted);
        g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
        g.drawText (juce::String (i + 1), static_cast<int> (r.getX()) + 16, static_cast<int> (r.getY()) + 12, 18, 30,
                    juce::Justification::centredLeft);
        g.setColour (on ? pal.textPrimary : pal.textSecondary);
        g.drawText ("Port " + juce::String (i + 1), static_cast<int> (r.getX()) + 36, static_cast<int> (r.getY()) + 12, 200, 30,
                    juce::Justification::centredLeft);

        // The connection dot, between the status words and the switch
        const auto dot = group.status.getBounds().toFloat();
        g.setColour (group.connected ? pal.accentSuccess : pal.textMuted.withAlpha (0.6f));
        g.fillEllipse (dot.getRight() + 8.0f, dot.getCentreY() - 4.5f, 9.0f, 9.0f);
    }
}

void MidiSettingsDialog::Content::resized()
{
    constexpr int pad = 24;
    for (int i = 0; i < kMaxSynths; ++i)
    {
        auto& g = owner->groups[static_cast<size_t> (i)];
        const juce::Rectangle<int> card (pad, i * kGroupH + 8, getWidth() - pad * 2, kGroupH - 12);

        // The switch at the right of the card's heading, the status words to the left of it
        auto head = juce::Rectangle<int> (card.getX() + 16, card.getY() + 12, card.getWidth() - 32, 30);
        g.enabled.setBounds (head.removeFromRight (52));
        head.removeFromRight (34);
        g.status.setBounds (head.removeFromRight (juce::jmin (head.getWidth() - 110, 320)));

        // In and Out, one under the other, each a full-width list
        auto body = juce::Rectangle<int> (card.getX() + 16, card.getY() + 56, card.getWidth() - 32, 34);
        g.inLabel.setBounds (body.removeFromLeft (44));
        g.inCombo.setBounds (body);
        body = juce::Rectangle<int> (card.getX() + 16, card.getY() + 98, card.getWidth() - 32, 34);
        g.outLabel.setBounds (body.removeFromLeft (44));
        g.outCombo.setBounds (body);
        g.statusCaption.setVisible (false);   // "Status:" is said by the dot now
    }
}

void MidiSettingsDialog::resized()
{
    closeButton.setBounds (getWidth() - 36, 6, 28, 28);

    viewport.setBounds (0, kTitleH + 8, getWidth(), getHeight() - kTitleH - 8 - kButtonsH);
    content.setSize (viewport.getWidth() - (kMaxSynths * kGroupH > viewport.getHeight() ? viewport.getScrollBarThickness() : 0),
                     kMaxSynths * kGroupH);

    // At the right of the footer: Apply, then OK, then Cancel last
    constexpr int btnW = 100, btnH = 36;
    const int by = getHeight() - kButtonsH + (kButtonsH - btnH) / 2;
    cancelButton.setBounds (getWidth() - 24 - btnW, by, btnW, btnH);
    okButton.setBounds (cancelButton.getX() - 12 - btnW, by, btnW, btnH);
    applyButton.setBounds (okButton.getX() - 12 - btnW, by, btnW, btnH);
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
