#include "SynthSettingsDialog.h"
#include "AppTheme.h"

// ─────────────────────────────────────────────────────────────────────────────
// Colours (shared palette with PatchSettingsDialog)
// ─────────────────────────────────────────────────────────────────────────────
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

static void styleSlider (juce::Slider& s, int tbW = 44)
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
SynthSettingsDialog::SynthSettingsDialog (const SynthSettings& current, Callback onOk)
    : okCallback (std::move (onOk)), working (current)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this]() { close(); };
    addAndMakeVisible (closeButton);

    // ── Synth name + master tune ─────────────────────────────────────────────
    styleLabel (synthHdr, true);
    styleLabel (nameLbl);
    nameEditor.setText (working.name, juce::dontSendNotification);
    nameEditor.setInputRestrictions (16);
    nameEditor.setColour (juce::TextEditor::backgroundColourId,  kCtrlBg);
    nameEditor.setColour (juce::TextEditor::textColourId,        kAmber);
    nameEditor.setColour (juce::TextEditor::outlineColourId,     kCtrlBd);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, kGold);

    styleLabel (tuneLbl);
    masterTuneSlider.setRange (0, 127, 1);
    masterTuneSlider.setValue (working.masterTune, juce::dontSendNotification);
    masterTuneSlider.onValueChange = [this]() { updateMasterTuneLabel(); };
    styleSlider (masterTuneSlider, 44);
    styleLabel (tuneCentsLbl);
    updateMasterTuneLabel();

    addAndMakeVisible (synthHdr);
    addAndMakeVisible (nameLbl);       addAndMakeVisible (nameEditor);
    addAndMakeVisible (tuneLbl);       addAndMakeVisible (masterTuneSlider);
    addAndMakeVisible (tuneCentsLbl);

    // ── MIDI channels ────────────────────────────────────────────────────────
    styleLabel (chanHdr, true);
    addAndMakeVisible (chanHdr);
    const char* sNames[] = { "Slot A", "Slot B", "Slot C", "Slot D" };
    for (int i = 0; i < 4; ++i)
    {
        slotLbls[i].setText (sNames[i], juce::dontSendNotification);
        styleLabel (slotLbls[i]);
        chanSliders[i].setRange (1, 16, 1);
        chanSliders[i].setValue ((working.midiChannelSlot[i] & 0x0F) + 1, juce::dontSendNotification);
        styleSlider (chanSliders[i], 38);
        addAndMakeVisible (slotLbls[i]);
        addAndMakeVisible (chanSliders[i]);
    }

    // ── MIDI ─────────────────────────────────────────────────────────────────
    styleLabel (midiHdr, true);
    styleLabel (velScaleLbl);
    styleLabel (velMinTag);  styleLabel (velMaxTag);
    velMinSlider.setRange (0, 127, 1);  velMinSlider.setValue (working.midiVelScaleMin, juce::dontSendNotification);
    velMaxSlider.setRange (0, 127, 1);  velMaxSlider.setValue (working.midiVelScaleMax, juce::dontSendNotification);
    styleSlider (velMinSlider);  styleSlider (velMaxSlider);
    localOnTgl   .setToggleState (working.localOn            != 0, juce::dontSendNotification);
    ledsActiveTgl.setToggleState (working.ledsActive         != 0, juce::dontSendNotification);
    pgmRecvTgl   .setToggleState (working.programChangeReceive != 0, juce::dontSendNotification);
    pgmSendTgl   .setToggleState (working.programChangeSend  != 0, juce::dontSendNotification);
    for (auto* t : { &localOnTgl, &ledsActiveTgl, &pgmRecvTgl, &pgmSendTgl }) styleToggle (*t);

    addAndMakeVisible (midiHdr);
    for (auto* v : { &velScaleLbl, &velMinTag, &velMaxTag }) addAndMakeVisible (*v);
    addAndMakeVisible (velMinSlider);  addAndMakeVisible (velMaxSlider);
    for (auto* t : { &localOnTgl, &ledsActiveTgl, &pgmRecvTgl, &pgmSendTgl }) addAndMakeVisible (*t);

    // ── Clock ────────────────────────────────────────────────────────────────
    styleLabel (clockHdr, true);
    clockInt.setRadioGroupId (101);  clockExt.setRadioGroupId (101);
    clockInt.setToggleState (working.midiClockSource == 0, juce::dontSendNotification);
    clockExt.setToggleState (working.midiClockSource != 0, juce::dontSendNotification);
    styleToggle (clockInt);  styleToggle (clockExt);
    styleLabel (bpmLbl);
    bpmSlider.setRange (24, 250, 1);
    bpmSlider.setValue (juce::jlimit (24, 250, working.midiClockBpm), juce::dontSendNotification);
    styleSlider (bpmSlider, 50);
    globalSyncTgl.setToggleState (working.globalSync != 0, juce::dontSendNotification);
    styleToggle (globalSyncTgl);

    addAndMakeVisible (clockHdr);
    addAndMakeVisible (clockInt);  addAndMakeVisible (clockExt);
    addAndMakeVisible (bpmLbl);    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (globalSyncTgl);

    // ── Behavior ─────────────────────────────────────────────────────────────
    styleLabel (behavHdr, true);
    styleLabel (knobModeLbl);  styleLabel (pedalLbl);  styleLabel (kbModeLbl);
    knobImm .setRadioGroupId (102);  knobHook.setRadioGroupId (102);
    knobImm .setToggleState (working.knobMode == 0, juce::dontSendNotification);
    knobHook.setToggleState (working.knobMode != 0, juce::dontSendNotification);
    pedalNorm.setRadioGroupId (103);  pedalInv.setRadioGroupId (103);
    pedalNorm.setToggleState (working.pedalPolarity == 0, juce::dontSendNotification);
    pedalInv .setToggleState (working.pedalPolarity != 0, juce::dontSendNotification);
    kbActive  .setRadioGroupId (104);  kbSelected.setRadioGroupId (104);
    kbActive  .setToggleState (working.keyboardMode == 0, juce::dontSendNotification);
    kbSelected.setToggleState (working.keyboardMode != 0, juce::dontSendNotification);
    for (auto* t : { &knobImm, &knobHook, &pedalNorm, &pedalInv, &kbActive, &kbSelected })
        styleToggle (*t);

    addAndMakeVisible (behavHdr);
    for (auto* v : { &knobModeLbl, &pedalLbl, &kbModeLbl }) addAndMakeVisible (*v);
    for (auto* t : { &knobImm, &knobHook, &pedalNorm, &pedalInv, &kbActive, &kbSelected })
        addAndMakeVisible (*t);

    // ── OK / Cancel ──────────────────────────────────────────────────────────
    styleBtn (okButton, true);
    styleBtn (cancelButton, false);
    okButton.onClick = [this]()
    {
        if (okCallback)
        {
            SynthSettings r = working;
            r.name               = nameEditor.getText().toStdString();
            r.masterTune         = static_cast<int> (masterTuneSlider.getValue());
            for (int i = 0; i < 4; ++i)
                r.midiChannelSlot[i] = static_cast<int> (chanSliders[i].getValue()) - 1;
            r.midiVelScaleMin        = static_cast<int> (velMinSlider.getValue());
            r.midiVelScaleMax        = static_cast<int> (velMaxSlider.getValue());
            r.localOn                = localOnTgl   .getToggleState() ? 1 : 0;
            r.ledsActive             = ledsActiveTgl.getToggleState() ? 1 : 0;
            r.programChangeReceive   = pgmRecvTgl   .getToggleState() ? 1 : 0;
            r.programChangeSend      = pgmSendTgl   .getToggleState() ? 1 : 0;
            r.midiClockSource        = clockExt     .getToggleState() ? 1 : 0;
            r.midiClockBpm           = static_cast<int> (bpmSlider.getValue());
            r.globalSync             = globalSyncTgl.getToggleState() ? 1 : 0;
            r.knobMode               = knobHook     .getToggleState() ? 1 : 0;
            r.pedalPolarity          = pedalInv     .getToggleState() ? 1 : 0;
            r.keyboardMode           = kbSelected   .getToggleState() ? 1 : 0;
            okCallback (r);
        }
        close();
    };
    cancelButton.onClick = [this]() { close(); };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (560, 452);
}

void SynthSettingsDialog::setSettings (const SynthSettings& settings)
{
    working = settings;

    nameEditor.setText (working.name, juce::dontSendNotification);
    masterTuneSlider.setValue (working.masterTune, juce::dontSendNotification);
    updateMasterTuneLabel();

    for (int i = 0; i < 4; ++i)
        chanSliders[i].setValue ((working.midiChannelSlot[i] & 0x0F) + 1, juce::dontSendNotification);

    velMinSlider.setValue (working.midiVelScaleMin, juce::dontSendNotification);
    velMaxSlider.setValue (working.midiVelScaleMax, juce::dontSendNotification);
    localOnTgl.setToggleState (working.localOn != 0, juce::dontSendNotification);
    ledsActiveTgl.setToggleState (working.ledsActive != 0, juce::dontSendNotification);
    pgmRecvTgl.setToggleState (working.programChangeReceive != 0, juce::dontSendNotification);
    pgmSendTgl.setToggleState (working.programChangeSend != 0, juce::dontSendNotification);
    clockInt.setToggleState (working.midiClockSource == 0, juce::dontSendNotification);
    clockExt.setToggleState (working.midiClockSource != 0, juce::dontSendNotification);
    bpmSlider.setValue (juce::jlimit (24, 250, working.midiClockBpm), juce::dontSendNotification);
    globalSyncTgl.setToggleState (working.globalSync != 0, juce::dontSendNotification);
    knobImm.setToggleState (working.knobMode == 0, juce::dontSendNotification);
    knobHook.setToggleState (working.knobMode != 0, juce::dontSendNotification);
    pedalNorm.setToggleState (working.pedalPolarity == 0, juce::dontSendNotification);
    pedalInv.setToggleState (working.pedalPolarity != 0, juce::dontSendNotification);
    kbActive.setToggleState (working.keyboardMode == 0, juce::dontSendNotification);
    kbSelected.setToggleState (working.keyboardMode != 0, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
void SynthSettingsDialog::updateMasterTuneLabel()
{
    int cents = static_cast<int> (masterTuneSlider.getValue()) - 64;
    juce::String s;
    s << (cents >= 0 ? "+" : "") << cents << " cents";
    tuneCentsLbl.setText (s, juce::dontSendNotification);
}

void SynthSettingsDialog::close()
{
    removeFromDesktop();
    delete this;
}

bool SynthSettingsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::returnKey) { okButton.triggerClick(); return true; }
    return false;
}

void SynthSettingsDialog::mouseDown (const juce::MouseEvent& e)
{
    if (e.getPosition().getY() < 32)
        dragger.startDraggingComponent (this, e);
}

void SynthSettingsDialog::mouseDrag (const juce::MouseEvent& e)
{
    dragger.dragComponent (this, e, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
void SynthSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);  // title bar separator

    const int W = getWidth();
    auto sep = [&](int y) {
        g.setColour (kSep);
        g.drawHorizontalLine (y, 14.0f, static_cast<float>(W - 14));
    };

    // Title
    g.setColour (kGold);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText ("Synth Settings", 10, 0, W - 44, 32, juce::Justification::centredLeft);

    sep (74);   // after synth row
    sep (122);  // after channels
    sep (188);  // after MIDI
    sep (236);  // after clock
    sep (310);  // after behavior
}

// ─────────────────────────────────────────────────────────────────────────────
void SynthSettingsDialog::resized()
{
    const int titleH = 32;
    const int pad    = 14;
    const int rowH   = 24;
    const int secH   = 18;
    const int gap    = 8;
    const int W      = getWidth() - pad * 2;

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = titleH + gap;

    // ── Synth name + tune ────────────────────────────────────────────────────
    synthHdr.setBounds (pad, y, W, secH);
    y += secH + 4;

    nameLbl.setBounds       (pad,      y, 44, rowH);
    nameEditor.setBounds    (pad + 46, y, 180, rowH);
    tuneLbl.setBounds       (pad + 240, y, 88, rowH);
    masterTuneSlider.setBounds (pad + 330, y, 130, rowH);
    tuneCentsLbl.setBounds  (pad + 464, y, W - 464, rowH);
    y += rowH + gap + 2;  // → sep 74

    y += gap;
    // ── MIDI channels ────────────────────────────────────────────────────────
    chanHdr.setBounds (pad, y, W, secH);
    y += secH + 4;

    const int cW = (W - 3 * 8) / 4;
    for (int i = 0; i < 4; ++i)
    {
        int x = pad + i * (cW + 8);
        slotLbls[i].setBounds   (x, y, 52, rowH);
        chanSliders[i].setBounds (x + 54, y, cW - 54, rowH);
    }
    y += rowH + gap + 2;  // → sep 122

    y += gap;
    // ── MIDI ─────────────────────────────────────────────────────────────────
    midiHdr.setBounds (pad, y, W, secH);
    y += secH + 4;

    velScaleLbl.setBounds (pad, y, 100, rowH);
    velMinTag.setBounds   (pad + 102, y, 30, rowH);
    velMinSlider.setBounds(pad + 134, y, 130, rowH);
    velMaxTag.setBounds   (pad + 274, y, 30, rowH);
    velMaxSlider.setBounds(pad + 306, y, 130, rowH);
    y += rowH + 4;

    const int tW = (W - 3 * 6) / 4;
    localOnTgl   .setBounds (pad,              y, tW, rowH);
    ledsActiveTgl.setBounds (pad + tW + 6,     y, tW, rowH);
    pgmRecvTgl   .setBounds (pad + (tW+6)*2,   y, tW, rowH);
    pgmSendTgl   .setBounds (pad + (tW+6)*3,   y, tW, rowH);
    y += rowH + gap + 2;  // → sep 188

    y += gap;
    // ── Clock ─────────────────────────────────────────────────────────────────
    clockHdr.setBounds (pad, y, W, secH);
    y += secH + 4;

    clockInt.setBounds    (pad,       y, 90, rowH);
    clockExt.setBounds    (pad + 92,  y, 90, rowH);
    bpmLbl.setBounds      (pad + 194, y, 36, rowH);
    bpmSlider.setBounds   (pad + 232, y, 130, rowH);
    globalSyncTgl.setBounds (pad + 376, y, 110, rowH);
    y += rowH + gap + 2;  // → sep 236

    y += gap;
    // ── Behavior ─────────────────────────────────────────────────────────────
    behavHdr.setBounds (pad, y, W, secH);
    y += secH + 4;

    knobModeLbl.setBounds (pad,       y, 88, rowH);
    knobImm.setBounds     (pad + 90,  y, 88, rowH);
    knobHook.setBounds    (pad + 180, y, 70, rowH);
    pedalLbl.setBounds    (pad + 264, y, 100, rowH);
    pedalNorm.setBounds   (pad + 366, y, 78, rowH);
    pedalInv.setBounds    (pad + 446, y, 80, rowH);
    y += rowH + 4;

    kbModeLbl.setBounds   (pad,       y, 108, rowH);
    kbActive.setBounds    (pad + 110, y, 100, rowH);
    kbSelected.setBounds  (pad + 212, y, 120, rowH);
    y += rowH + gap + 2;  // → sep 310

    y += gap;
    // ── Buttons ──────────────────────────────────────────────────────────────
    const int btnW = 88, btnH = 28;
    cancelButton.setBounds (getWidth() - pad - btnW * 2 - 8, y, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW,         y, btnW, btnH);
}

// ─────────────────────────────────────────────────────────────────────────────
SynthSettingsDialog* SynthSettingsDialog::show (juce::Component* parent,
                                                const SynthSettings& current,
                                                Callback onOk)
{
    auto* dlg = new SynthSettingsDialog (current, std::move (onOk));

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
