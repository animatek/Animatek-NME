#include "PatchSettingsDialog.h"
#include "AppTheme.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kGold   (AppTheme::palette().accentActive)
#define kAmber  (AppTheme::palette().accentWarning)
#define kText   (AppTheme::palette().textSecondary)
#define kDim    (AppTheme::palette().textMuted)
#define kCtrlBg (AppTheme::palette().inputBackground)
#define kCtrlBd (AppTheme::palette().borderColor)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)

static const juce::Colour kOkBg   { 0xff1e3a1e };
static const juce::Colour kOkOn   { 0xff2a5a2a };

static void styleSlider (juce::Slider& s, int tbW = 50)
{
    s.setColour (juce::Slider::textBoxTextColourId,       kAmber);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kCtrlBg);
    s.setColour (juce::Slider::textBoxOutlineColourId,    kCtrlBd);
    s.setColour (juce::Slider::textBoxHighlightColourId,  kBtnOn);
    s.setSliderStyle (juce::Slider::IncDecButtons);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, tbW, 22);
}
static void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId,         kText);
    b.setColour (juce::ToggleButton::tickColourId,         kAmber);
    b.setColour (juce::ToggleButton::tickDisabledColourId, kDim);
}
static void styleLabel (juce::Label& l, bool section = false)
{
    l.setFont (section ? juce::Font (juce::FontOptions (11.0f, juce::Font::bold))
                       : juce::Font (juce::FontOptions (12.0f)));
    l.setColour (juce::Label::textColourId,       section ? kGold : kText);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}
static void styleBtn (juce::TextButton& b, bool isOk = false)
{
    b.setColour (juce::TextButton::buttonColourId,   isOk ? kOkBg : kBtnBg);
    b.setColour (juce::TextButton::buttonOnColourId, isOk ? kOkOn : kBtnOn);
    b.setColour (juce::TextButton::textColourOffId,  isOk ? juce::Colour (0xffaaffaa) : kText);
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
    styleBtn (okButton, true);
    styleBtn (cancelButton, false);
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

    setSize (520, 420);
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
    { if (e.getPosition().getY() < 32) dragger.startDraggingComponent (this, e); }
void PatchSettingsDialog::mouseDrag (const juce::MouseEvent& e)
    { dragger.dragComponent (this, e, nullptr); }

// ─────────────────────────────────────────────────────────────────────────────
void PatchSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Title
    g.setColour (kGold);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText ("Patch Settings", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    // Title bar separator
    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);

    // Section separators — positions mirror resized() math
    const float x0 = 14.0f, x1 = static_cast<float>(getWidth() - 14);
    for (int sy : { 71, 165, 215, 265, 315 })
    {
        g.setColour (kSep);
        g.drawHorizontalLine (sy, x0, x1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchSettingsDialog::resized()
{
    // Layout constants
    constexpr int titleH = 32;
    constexpr int pad    = 14;
    constexpr int rowH   = 24;
    constexpr int secH   = 18;
    constexpr int gap    = 8;
    constexpr int spnW   = 120;   // slider (text+buttons) width
    constexpr int lblW   = 50;    // short label

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = titleH + gap;   // y = 40

    // ── Voices ──────────────────────────────────────────────────────────────
    voicesLabel .setBounds (pad,        y, 80,   rowH);
    voicesSlider.setBounds (pad + 84,   y, spnW, rowH);
    y += rowH + gap - 1;   // y = 71  (sep drawn here)

    y += gap;              // y = 79
    // ── Velocity & Keyboard ─────────────────────────────────────────────────
    velKeyLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 4;         // y = 101

    // Row 1: velocity
    velLoLbl   .setBounds (pad,               y, lblW, rowH);
    velMinSlider.setBounds (pad + lblW + 2,   y, spnW, rowH);
    velHiLbl   .setBounds (pad + lblW + spnW + 14, y, lblW, rowH);
    velMaxSlider.setBounds (pad + lblW*2 + spnW + 16, y, spnW, rowH);
    y += rowH + 4;         // y = 129

    // Row 2: keyboard
    keyLoLbl   .setBounds (pad,               y, lblW, rowH);
    keyMinSlider.setBounds (pad + lblW + 2,   y, spnW, rowH);
    keyHiLbl   .setBounds (pad + lblW + spnW + 14, y, lblW, rowH);
    keyMaxSlider.setBounds (pad + lblW*2 + spnW + 16, y, spnW, rowH);
    y += rowH + gap - 1;   // y = 161  (next sep at ~165 — drawn 4px below)
    y += gap;              // y = 169  (or keep going)

    // ── Pedal & Bend ────────────────────────────────────────────────────────
    pedalBendLabel.setBounds (pad, y - gap + 4, getWidth() - pad * 2, secH);
    int secY = y - gap + 4 + secH + 4;  // y of the row

    pedalLbl    .setBounds (pad,            secY, 42, rowH);
    pedalSustain.setBounds (pad + 44,       secY, 72, rowH);
    pedalOnOff  .setBounds (pad + 118,      secY, 66, rowH);
    bendLbl     .setBounds (pad + 200,      secY, 36, rowH);
    bendSlider  .setBounds (pad + 238,      secY, 110, rowH);
    bendUnitLbl .setBounds (pad + 350,      secY, 24, rowH);
    y = secY + rowH + gap - 1;  // sep

    y += gap;
    // ── Portamento ──────────────────────────────────────────────────────────
    portaLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 4;

    portaNormal    .setBounds (pad,         y, 72, rowH);
    portaAuto      .setBounds (pad + 74,    y, 62, rowH);
    portaTimeLbl   .setBounds (pad + 148,   y, 40, rowH);
    portaTimeSlider.setBounds (pad + 190,   y, spnW, rowH);
    y += rowH + gap - 1;   // sep

    y += gap;
    // ── Octave & Retrigger ──────────────────────────────────────────────────
    octRetrigLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 4;

    octLbl.setBounds (pad, y, 48, rowH);
    int ox = pad + 50;
    constexpr int octW = 42;
    for (int i = 0; i < 5; ++i) { octaveButtons[i].setBounds (ox, y, octW, rowH); ox += octW + 2; }
    ox += 8;
    retrigLbl   .setBounds (ox,      y, 46, rowH); ox += 48;
    retrigPoly  .setBounds (ox,      y, 50, rowH); ox += 52;
    retrigCommon.setBounds (ox,      y, 72, rowH);
    y += rowH + gap * 2;

    // ── Buttons ─────────────────────────────────────────────────────────────
    constexpr int btnW = 88, btnH = 28;
    cancelButton.setBounds (getWidth() - pad - btnW * 2 - 8, y, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW,         y, btnW, btnH);
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchSettingsDialog::show (juce::Component* parent,
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
}
