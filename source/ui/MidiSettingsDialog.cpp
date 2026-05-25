#include "MidiSettingsDialog.h"

static const juce::Colour kBg     { 0xff323232 };
static const juce::Colour kSep    { 0xff444A53 };
static const juce::Colour kGold   { 0xffffcc44 };
static const juce::Colour kAmber  { 0xffffaa44 };
static const juce::Colour kText   { 0xffcccccc };
static const juce::Colour kDim    { 0xff888899 };
static const juce::Colour kCtrlBg { 0xff25282E };
static const juce::Colour kCtrlBd { 0xff555B64 };
static const juce::Colour kBtnBg  { 0xff25282E };
static const juce::Colour kBtnOn  { 0xff444A53 };
static const juce::Colour kOkBg   { 0xff1e3a1e };
static const juce::Colour kOkOn   { 0xff2a5a2a };
static const juce::Colour kWarnBg { 0xff3a1e1e };
static const juce::Colour kWarnOn { 0xff5a2a2a };

static void styleLabel (juce::Label& l, bool section = false)
{
    l.setFont (section ? juce::Font (juce::FontOptions (10.0f, juce::Font::bold))
                       : juce::Font (juce::FontOptions (12.0f)));
    l.setColour (juce::Label::textColourId,       section ? kGold : kText);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

static void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, kCtrlBg);
    c.setColour (juce::ComboBox::outlineColourId,    kCtrlBd);
    c.setColour (juce::ComboBox::textColourId,       kText);
    c.setColour (juce::ComboBox::arrowColourId,      kAmber);
    c.setColour (juce::ComboBox::focusedOutlineColourId, kGold);
}

// ─────────────────────────────────────────────────────────────────────────────
MidiSettingsDialog::MidiSettingsDialog()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this]() { close(); };
    addAndMakeVisible (closeButton);

    styleLabel (inputLabel,  true);
    styleLabel (outputLabel, true);
    styleCombo (inputCombo);
    styleCombo (outputCombo);
    addAndMakeVisible (inputLabel);
    addAndMakeVisible (inputCombo);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (outputCombo);

    statusLabel.setColour (juce::Label::textColourId,       kDim);
    statusLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setText ("Disconnected", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    connectButton.setButtonText ("Connect");
    connectButton.setColour (juce::TextButton::buttonColourId,  kOkBg);
    connectButton.setColour (juce::TextButton::buttonOnColourId,kOkOn);
    connectButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffaaffaa));
    connectButton.onClick = [this]()
    {
        if (connected)
        {
            if (onDisconnectionRequest) onDisconnectionRequest();
        }
        else
        {
            auto inIdx  = inputCombo.getSelectedItemIndex();
            auto outIdx = outputCombo.getSelectedItemIndex();
            if (inIdx >= 0 && outIdx >= 0 && onConnectionRequest)
                onConnectionRequest (inputIds[inIdx], outputIds[outIdx]);
        }
    };
    addAndMakeVisible (connectButton);

    refreshDeviceLists();
    setSize (400, 210);
}

// ─────────────────────────────────────────────────────────────────────────────
void MidiSettingsDialog::refreshDeviceLists()
{
    inputCombo.clear();   inputIds.clear();
    outputCombo.clear();  outputIds.clear();

    for (auto& d : ConnectionManager::getAvailableInputDevices())
    {
        inputCombo.addItem (d.name, inputIds.size() + 1);
        inputIds.add (d.identifier);
    }
    for (auto& d : ConnectionManager::getAvailableOutputDevices())
    {
        outputCombo.addItem (d.name, outputIds.size() + 1);
        outputIds.add (d.identifier);
    }
}

void MidiSettingsDialog::setSelectedPorts(const juce::String& inputId, const juce::String& outputId)
{
    auto inIdx  = inputIds.indexOf (inputId);
    if (inIdx  >= 0) inputCombo.setSelectedItemIndex  (inIdx,  juce::dontSendNotification);
    auto outIdx = outputIds.indexOf (outputId);
    if (outIdx >= 0) outputCombo.setSelectedItemIndex (outIdx, juce::dontSendNotification);
}

void MidiSettingsDialog::setConnectedState(const ConnectionManager::Status& status)
{
    connected = (status.state == ConnectionManager::State::Connected);
    statusLabel.setText (status.message, juce::dontSendNotification);

    juce::Colour col = kDim;
    if      (status.state == ConnectionManager::State::Connected)  col = juce::Colour (0xff55dd55);
    else if (status.state == ConnectionManager::State::Connecting) col = juce::Colour (0xffdddd44);
    statusLabel.setColour (juce::Label::textColourId, col);

    updateButtonState();
}

void MidiSettingsDialog::updateButtonState()
{
    if (connected)
    {
        connectButton.setButtonText ("Disconnect");
        connectButton.setColour (juce::TextButton::buttonColourId,  kWarnBg);
        connectButton.setColour (juce::TextButton::buttonOnColourId,kWarnOn);
        connectButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffaaaa));
    }
    else
    {
        connectButton.setButtonText ("Connect");
        connectButton.setColour (juce::TextButton::buttonColourId,  kOkBg);
        connectButton.setColour (juce::TextButton::buttonOnColourId,kOkOn);
        connectButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffaaffaa));
    }
}

void MidiSettingsDialog::close() { removeFromDesktop(); delete this; }

bool MidiSettingsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::returnKey) { connectButton.triggerClick(); return true; }
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

    g.setColour (kGold);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText ("MIDI Settings", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);

    const float x0 = 14.0f, x1 = static_cast<float> (getWidth() - 14);
    g.drawHorizontalLine (142, x0, x1);
}

void MidiSettingsDialog::resized()
{
    constexpr int titleH = 32, pad = 14, secH = 14, rowH = 26, gap = 6;

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = titleH + gap;

    inputLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 3;
    inputCombo.setBounds (pad, y, getWidth() - pad * 2, rowH);
    y += rowH + gap + 4;

    outputLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 3;
    outputCombo.setBounds (pad, y, getWidth() - pad * 2, rowH);
    y += rowH + gap * 3; // → separator at 142

    // Status + Connect button
    y += gap;
    statusLabel.setBounds (pad, y, getWidth() - pad * 2 - 110, 22);
    connectButton.setBounds (getWidth() - pad - 100, y - 2, 100, 28);
}

// ─────────────────────────────────────────────────────────────────────────────
void MidiSettingsDialog::show(juce::Component* parent,
                               const juce::String& currentInputId,
                               const juce::String& currentOutputId,
                               const ConnectionManager::Status& status,
                               std::function<void(const juce::String&, const juce::String&)> connectCb,
                               std::function<void()> disconnectCb)
{
    auto* dlg = new MidiSettingsDialog();
    dlg->setSelectedPorts (currentInputId, currentOutputId);
    dlg->setConnectedState (status);
    dlg->onConnectionRequest    = std::move (connectCb);
    dlg->onDisconnectionRequest = std::move (disconnectCb);

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
}
