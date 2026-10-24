#include "PatchSettingsDialog.h"
#include "AppTheme.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kText   (AppTheme::palette().textSecondary)
#define kDim    (AppTheme::palette().textMuted)
#define kCtrlBg (AppTheme::palette().inputBackground)
#define kCtrlBd (AppTheme::palette().borderColor)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)

static void styleSlider (juce::Slider& s, int tbW = 50)
{
    s.setColour (juce::Slider::textBoxTextColourId,       AppTheme::palette().textPrimary);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kCtrlBg);
    s.setColour (juce::Slider::textBoxOutlineColourId,    kCtrlBd);
    s.setColour (juce::Slider::textBoxHighlightColourId,  kBtnOn);
    s.setSliderStyle (juce::Slider::IncDecButtons);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, tbW, 30);
}
static void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId,         kText);
    b.setColour (juce::ToggleButton::tickColourId,         AppTheme::palette().textPrimary);
    b.setColour (juce::ToggleButton::tickDisabledColourId, kDim);
}
static void styleLabel (juce::Label& l, bool section = false)
{
    l.setFont (section ? juce::Font (AppTheme::uiFont (12.5f).withStyle ("Bold"))
                       : juce::Font (AppTheme::uiFont (14.0f)));
    l.setColour (juce::Label::textColourId,       section ? AppTheme::palette().textMuted : kText);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}
static void styleBtn (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,   kBtnBg);
    b.setColour (juce::TextButton::buttonOnColourId, kBtnOn);
    b.setColour (juce::TextButton::textColourOffId,  kText);
    b.setColour (juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
}

// ─────────────────────────────────────────────────────────────────────────────
PatchSettingsDialog::PatchSettingsDialog (const PatchHeader& header, Callback onOk)
    : okCallback (std::move (onOk))
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this]() { close(); };
    addAndMakeVisible (closeButton);

    // Voices
    styleLabel (voicesLabel);
    voicesSlider.setRange (1, 32, 1);
    voicesSlider.setValue (header.voices, juce::dontSendNotification);
    styleSlider (voicesSlider, 44);
    addAndMakeVisible (voicesLabel);
    addAndMakeVisible (voicesSlider);

    // Velocity / Keyboard
    styleLabel (velKeyLabel, true);
    for (auto* l : { &velLoLbl, &velHiLbl, &keyLoLbl, &keyHiLbl }) styleLabel (*l);
    velMinSlider.setRange (0, 127, 1);  velMinSlider.setValue (header.velRangeMin, juce::dontSendNotification);
    velMaxSlider.setRange (0, 127, 1);  velMaxSlider.setValue (header.velRangeMax, juce::dontSendNotification);
    keyMinSlider.setRange (0, 127, 1);  keyMinSlider.setValue (header.keyRangeMin, juce::dontSendNotification);
    keyMaxSlider.setRange (0, 127, 1);  keyMaxSlider.setValue (header.keyRangeMax, juce::dontSendNotification);
    for (auto* s : { &velMinSlider, &velMaxSlider, &keyMinSlider, &keyMaxSlider }) styleSlider (*s);
    addAndMakeVisible (velKeyLabel);
    for (auto* l : { &velLoLbl, &velHiLbl, &keyLoLbl, &keyHiLbl }) addAndMakeVisible (*l);
    for (auto* s : { &velMinSlider, &velMaxSlider, &keyMinSlider, &keyMaxSlider }) addAndMakeVisible (*s);

    // Pedal + Bend
    styleLabel (pedalBendLabel, true);
    styleLabel (pedalLbl);  styleLabel (bendLbl);  styleLabel (bendUnitLbl);
    pedalSustain.setRadioGroupId (1);  pedalOnOff.setRadioGroupId (1);
    pedalSustain.setToggleState (header.pedalMode == 0, juce::dontSendNotification);
    pedalOnOff  .setToggleState (header.pedalMode != 0, juce::dontSendNotification);
    styleToggle (pedalSustain);  styleToggle (pedalOnOff);
    bendSlider.setRange (1, 24, 1);
    bendSlider.setValue (header.bendRange, juce::dontSendNotification);
    styleSlider (bendSlider, 40);
    addAndMakeVisible (pedalBendLabel);
    for (auto* l : { &pedalLbl, &bendLbl, &bendUnitLbl }) addAndMakeVisible (*l);
    addAndMakeVisible (pedalSustain);  addAndMakeVisible (pedalOnOff);
    addAndMakeVisible (bendSlider);

    // Portamento
    styleLabel (portaLabel, true);
    styleLabel (portaTimeLbl);
    portaNormal.setRadioGroupId (2);  portaAuto.setRadioGroupId (2);
    portaNormal.setToggleState (!header.portamento, juce::dontSendNotification);
    portaAuto  .setToggleState ( header.portamento, juce::dontSendNotification);
    styleToggle (portaNormal);  styleToggle (portaAuto);
    portaTimeSlider.setRange (0, 127, 1);
    portaTimeSlider.setValue (header.portamentoTime, juce::dontSendNotification);
    styleSlider (portaTimeSlider);
    addAndMakeVisible (portaLabel);
    addAndMakeVisible (portaTimeLbl);
    addAndMakeVisible (portaNormal);  addAndMakeVisible (portaAuto);
    addAndMakeVisible (portaTimeSlider);

    // Octave + Retrigger
    styleLabel (octRetrigLabel, true);
    styleLabel (octLbl);  styleLabel (retrigLbl);
    const char* octNames[] = { "-2", "-1", "0", "+1", "+2" };
    for (int i = 0; i < 5; ++i)
    {
        octaveButtons[i].setButtonText (octNames[i]);
        octaveButtons[i].setRadioGroupId (3);
        octaveButtons[i].setToggleState (header.octaveShift == i, juce::dontSendNotification);
        styleToggle (octaveButtons[i]);
        addAndMakeVisible (octaveButtons[i]);
    }
    retrigPoly  .setToggleState (header.voiceRetriggerPoly   != 0, juce::dontSendNotification);
    retrigCommon.setToggleState (header.voiceRetriggerCommon != 0, juce::dontSendNotification);
    styleToggle (retrigPoly);  styleToggle (retrigCommon);
    addAndMakeVisible (octRetrigLabel);
    addAndMakeVisible (octLbl);  addAndMakeVisible (retrigLbl);
    addAndMakeVisible (retrigPoly);  addAndMakeVisible (retrigCommon);

    // OK / Cancel
    styleBtn (okButton);
    styleBtn (cancelButton);
    okButton.setColour (juce::TextButton::buttonColourId,  AppTheme::palette().accentActive);   // OK does the thing
    okButton.setColour (juce::TextButton::textColourOffId, AppTheme::palette().backgroundMain);
    okButton.onClick = [this]()
    {
        if (okCallback)
        {
            Result r;
            r.voices           = static_cast<int> (voicesSlider.getValue());
            r.velRangeMin      = static_cast<int> (velMinSlider.getValue());
            r.velRangeMax      = static_cast<int> (velMaxSlider.getValue());
            r.keyRangeMin      = static_cast<int> (keyMinSlider.getValue());
            r.keyRangeMax      = static_cast<int> (keyMaxSlider.getValue());
            r.pedalMode        = pedalOnOff.getToggleState() ? 1 : 0;
            r.bendRange        = static_cast<int> (bendSlider.getValue());
            r.portamento       = portaAuto.getToggleState();
            r.portamentoTime   = static_cast<int> (portaTimeSlider.getValue());
            r.octaveShift      = 2;
            for (int i = 0; i < 5; ++i)
                if (octaveButtons[i].getToggleState()) { r.octaveShift = i; break; }
            r.voiceRetriggerPoly   = retrigPoly.getToggleState();
            r.voiceRetriggerCommon = retrigCommon.getToggleState();
            okCallback (r);
        }
        close();
    };
    cancelButton.onClick = [this]() { close(); };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (620, 572);
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchSettingsDialog::close()  { removeFromDesktop(); delete this; }

bool PatchSettingsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::returnKey) { okButton.triggerClick(); return true; }
    return false;
}
void PatchSettingsDialog::mouseDown (const juce::MouseEvent& e)
    { if (e.getPosition().getY() < 40) dragger.startDraggingComponent (this, e); }
void PatchSettingsDialog::mouseDrag (const juce::MouseEvent& e)
    { if (e.getMouseDownPosition().getY() < 40) dragger.dragComponent (this, e, nullptr); }  // only a drag that began on the title bar moves the window (the press position says where it began)

// ─────────────────────────────────────────────────────────────────────────────
void PatchSettingsDialog::paint (juce::Graphics& g)
{
    AppTheme::drawDialogChrome (g, getLocalBounds(), "Patch Settings", 40, 68);

    // Derive dividers from the laid-out rows so they cannot cross controls.
    g.setColour (AppTheme::palette().borderColor.withAlpha (0.6f));
    for (int sy : { voicesSlider.getBottom() + 14, keyMaxSlider.getBottom() + 14,
                    bendSlider.getBottom() + 14, portaTimeSlider.getBottom() + 14 })
        g.fillRect (28, sy, getWidth() - 56, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchSettingsDialog::resized()
{
    constexpr int titleH = 40, footerH = 68, pad = 28;
    constexpr int rowH = 34, secH = 18, spnW = 150, lblW = 62;

    closeButton.setBounds (getWidth() - 36, 6, 28, 28);

    int y = titleH + 16;

    // ── Voices ──────────────────────────────────────────────────────────────
    voicesLabel .setBounds (pad,      y, 80,   rowH);
    voicesSlider.setBounds (pad + 84, y, spnW, rowH);
    y += rowH + 28;

    // ── Velocity & Keyboard ─────────────────────────────────────────────────
    velKeyLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 8;
    const int x2 = pad + lblW + spnW + 28;
    velLoLbl    .setBounds (pad,           y, lblW, rowH);
    velMinSlider.setBounds (pad + lblW,    y, spnW, rowH);
    velHiLbl    .setBounds (x2,            y, lblW, rowH);
    velMaxSlider.setBounds (x2 + lblW,     y, spnW, rowH);
    y += rowH + 8;
    keyLoLbl    .setBounds (pad,           y, lblW, rowH);
    keyMinSlider.setBounds (pad + lblW,    y, spnW, rowH);
    keyHiLbl    .setBounds (x2,            y, lblW, rowH);
    keyMaxSlider.setBounds (x2 + lblW,     y, spnW, rowH);
    y += rowH + 28;

    // ── Pedal & Bend ────────────────────────────────────────────────────────
    pedalBendLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 8;
    pedalLbl    .setBounds (pad,        y, 50,  rowH);
    pedalSustain.setBounds (pad + 54,   y, 96,  rowH);
    pedalOnOff  .setBounds (pad + 154,  y, 84,  rowH);
    bendLbl     .setBounds (pad + 270,  y, 46,  rowH);
    bendSlider  .setBounds (pad + 318,  y, 130, rowH);
    bendUnitLbl .setBounds (pad + 454,  y, 30,  rowH);
    y += rowH + 28;

    // ── Portamento ──────────────────────────────────────────────────────────
    portaLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 8;
    portaNormal    .setBounds (pad,       y, 92,  rowH);
    portaAuto      .setBounds (pad + 96,  y, 80,  rowH);
    portaTimeLbl   .setBounds (pad + 210, y, 46,  rowH);
    portaTimeSlider.setBounds (pad + 258, y, spnW, rowH);
    y += rowH + 28;

    // ── Octave & Retrigger ──────────────────────────────────────────────────
    octRetrigLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 8;
    octLbl.setBounds (pad, y, 62, rowH);
    int ox = pad + 64;
    constexpr int octW = 50;
    for (int i = 0; i < 5; ++i) { octaveButtons[i].setBounds (ox, y, octW, rowH); ox += octW + 2; }
    ox += 18;
    retrigLbl   .setBounds (ox, y, 56, rowH); ox += 58;
    retrigPoly  .setBounds (ox, y, 62, rowH); ox += 66;
    retrigCommon.setBounds (ox, y, 90, rowH);

    // ── Buttons, in the footer band ─────────────────────────────────────────
    constexpr int btnW = 100, btnH = 36;
    const int btnY = getHeight() - footerH + (footerH - btnH) / 2;
    cancelButton.setBounds (getWidth() - pad - btnW * 2 - 12, btnY, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW,          btnY, btnW, btnH);
}

// ─────────────────────────────────────────────────────────────────────────────
juce::Component* PatchSettingsDialog::show (juce::Component* parent,
                                            const PatchHeader& header,
                                            Callback onOk)
{
    auto* dlg = new PatchSettingsDialog (header, std::move (onOk));

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
