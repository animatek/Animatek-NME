#include "SynthSettingsDialog.h"
#include "AppTheme.h"

// ─────────────────────────────────────────────────────────────────────────────
// Colours (shared palette with PatchSettingsDialog)
// ─────────────────────────────────────────────────────────────────────────────
#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kGold   (AppTheme::palette().accentActive)
#define kText   (AppTheme::palette().textSecondary)
#define kDim    (AppTheme::palette().textMuted)
#define kCtrlBg (AppTheme::palette().inputBackground)
#define kCtrlBd (AppTheme::palette().borderColor)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)

static void styleSlider (juce::Slider& s, int tbW = 44)
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
    nameEditor.setFont (juce::Font (AppTheme::uiFont (14.0f)));
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setIndents (10, 0);
    nameEditor.setColour (juce::TextEditor::backgroundColourId,  kCtrlBg);
    nameEditor.setColour (juce::TextEditor::textColourId,        AppTheme::palette().textPrimary);
    nameEditor.setColour (juce::TextEditor::outlineColourId,     kCtrlBd);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, kGold);

    styleLabel (tuneLbl);
    masterTuneSlider.setRange (-127, 127, 1);
    masterTuneSlider.setValue (working.masterTuneCents(), juce::dontSendNotification);
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
    clockInt.setToggleState (working.clockIsInternal(), juce::dontSendNotification);
    clockExt.setToggleState (! working.clockIsInternal(), juce::dontSendNotification);
    styleToggle (clockInt);  styleToggle (clockExt);
    styleLabel (bpmLbl);
    bpmSlider.setRange (24, 250, 1);
    bpmSlider.setValue (juce::jlimit (24, 250, working.midiClockBpm), juce::dontSendNotification);
    styleSlider (bpmSlider, 50);
    styleLabel (globalSyncLbl);
    globalSyncLbl.setTooltip ("Quarter notes between the Sync pulses of the MIDI Global module");
    globalSyncSlider.setRange (1, 32, 1);
    globalSyncSlider.setValue (working.globalSyncQuarters(), juce::dontSendNotification);
    styleSlider (globalSyncSlider, 34);

    addAndMakeVisible (clockHdr);
    addAndMakeVisible (clockInt);  addAndMakeVisible (clockExt);
    addAndMakeVisible (bpmLbl);    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (globalSyncLbl);  addAndMakeVisible (globalSyncSlider);

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
    styleBtn (okButton);
    okButton.setColour (juce::TextButton::buttonColourId,  kGold);   // OK does the thing
    okButton.setColour (juce::TextButton::textColourOffId, AppTheme::palette().backgroundMain);
    styleBtn (cancelButton);
    okButton.onClick = [this]()
    {
        if (okCallback)
        {
            SynthSettings r = working;
            r.name               = nameEditor.getText().toStdString();
            r.setMasterTuneCents (static_cast<int> (masterTuneSlider.getValue()));
            for (int i = 0; i < 4; ++i)
                r.midiChannelSlot[i] = static_cast<int> (chanSliders[i].getValue()) - 1;
            r.midiVelScaleMin        = static_cast<int> (velMinSlider.getValue());
            r.midiVelScaleMax        = static_cast<int> (velMaxSlider.getValue());
            r.localOn                = localOnTgl   .getToggleState() ? 1 : 0;
            r.ledsActive             = ledsActiveTgl.getToggleState() ? 1 : 0;
            r.programChangeReceive   = pgmRecvTgl   .getToggleState() ? 1 : 0;
            r.programChangeSend      = pgmSendTgl   .getToggleState() ? 1 : 0;
            r.setClockInternal (clockInt.getToggleState());
            r.midiClockBpm           = static_cast<int> (bpmSlider.getValue());
            r.setGlobalSyncQuarters (static_cast<int> (globalSyncSlider.getValue()));
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

    setSize (720, 640);
}

void SynthSettingsDialog::setSettings (const SynthSettings& settings)
{
    working = settings;

    nameEditor.setText (working.name, juce::dontSendNotification);
    masterTuneSlider.setValue (working.masterTuneCents(), juce::dontSendNotification);
    updateMasterTuneLabel();

    for (int i = 0; i < 4; ++i)
        chanSliders[i].setValue ((working.midiChannelSlot[i] & 0x0F) + 1, juce::dontSendNotification);

    velMinSlider.setValue (working.midiVelScaleMin, juce::dontSendNotification);
    velMaxSlider.setValue (working.midiVelScaleMax, juce::dontSendNotification);
    localOnTgl.setToggleState (working.localOn != 0, juce::dontSendNotification);
    ledsActiveTgl.setToggleState (working.ledsActive != 0, juce::dontSendNotification);
    pgmRecvTgl.setToggleState (working.programChangeReceive != 0, juce::dontSendNotification);
    pgmSendTgl.setToggleState (working.programChangeSend != 0, juce::dontSendNotification);
    clockInt.setToggleState (working.clockIsInternal(), juce::dontSendNotification);
    clockExt.setToggleState (! working.clockIsInternal(), juce::dontSendNotification);
    bpmSlider.setValue (juce::jlimit (24, 250, working.midiClockBpm), juce::dontSendNotification);
    globalSyncSlider.setValue (working.globalSyncQuarters(), juce::dontSendNotification);
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
    int cents = static_cast<int> (masterTuneSlider.getValue());
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
    if (e.getPosition().getY() < 40)
        dragger.startDraggingComponent (this, e);
}

void SynthSettingsDialog::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getMouseDownPosition().getY() >= 40)
        return;   // only a drag that began on the title bar moves the window (the press position says where it began)
    dragger.dragComponent (this, e, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
void SynthSettingsDialog::paint (juce::Graphics& g)
{
    AppTheme::drawDialogChrome (g, getLocalBounds(), "Synth Settings", 40, 68);

    g.setColour (AppTheme::palette().borderColor.withAlpha (0.6f));
    for (int sy : { masterTuneSlider.getBottom() + 14, chanSliders[0].getBottom() + 14,
                    localOnTgl.getBottom() + 14, globalSyncSlider.getBottom() + 14 })
        g.fillRect (28, sy, getWidth() - 56, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
void SynthSettingsDialog::resized()
{
    const int titleH = 40, footerH = 68;
    const int pad    = 28;
    const int rowH   = 34;
    const int secH   = 18;
    const int W      = getWidth() - pad * 2;

    closeButton.setBounds (getWidth() - 36, 6, 28, 28);

    int y = titleH + 16;

    // ── Synth name + tune ────────────────────────────────────────────────────
    synthHdr.setBounds (pad, y, W, secH);
    y += secH + 8;
    nameLbl.setBounds          (pad,       y, 56,  rowH);
    nameEditor.setBounds       (pad + 58,  y, 200, rowH);
    tuneLbl.setBounds          (pad + 282, y, 100, rowH);
    masterTuneSlider.setBounds (pad + 384, y, 150, rowH);
    tuneCentsLbl.setBounds     (pad + 540, y, W - 540, rowH);
    y += rowH + 28;

    // ── MIDI channels ────────────────────────────────────────────────────────
    chanHdr.setBounds (pad, y, W, secH);
    y += secH + 8;
    const int cW = (W - 3 * 12) / 4;
    for (int i = 0; i < 4; ++i)
    {
        const int x = pad + i * (cW + 12);
        slotLbls[i].setBounds    (x, y, 52, rowH);
        chanSliders[i].setBounds (x + 54, y, cW - 54, rowH);
    }
    y += rowH + 28;

    // ── MIDI ─────────────────────────────────────────────────────────────────
    midiHdr.setBounds (pad, y, W, secH);
    y += secH + 8;
    velScaleLbl.setBounds  (pad,       y, 110, rowH);
    velMinTag.setBounds    (pad + 112, y, 36,  rowH);
    velMinSlider.setBounds (pad + 150, y, 150, rowH);
    velMaxTag.setBounds    (pad + 320, y, 36,  rowH);
    velMaxSlider.setBounds (pad + 358, y, 150, rowH);
    y += rowH + 8;
    const int tW = (W - 3 * 10) / 4;
    localOnTgl   .setBounds (pad,             y, tW, rowH);
    ledsActiveTgl.setBounds (pad + tW + 10,   y, tW, rowH);
    pgmRecvTgl   .setBounds (pad + (tW+10)*2, y, tW, rowH);
    pgmSendTgl   .setBounds (pad + (tW+10)*3, y, tW, rowH);
    y += rowH + 28;

    // ── Clock ─────────────────────────────────────────────────────────────────
    clockHdr.setBounds (pad, y, W, secH);
    y += secH + 8;
    clockInt.setBounds         (pad,       y, 100, rowH);
    clockExt.setBounds         (pad + 104, y, 100, rowH);
    bpmLbl.setBounds           (pad + 224, y, 44,  rowH);
    bpmSlider.setBounds        (pad + 270, y, 150, rowH);
    globalSyncLbl.setBounds    (pad + 440, y, 90,  rowH);
    globalSyncSlider.setBounds (pad + 532, y, W - 532, rowH);
    y += rowH + 28;

    // ── Behavior ─────────────────────────────────────────────────────────────
    behavHdr.setBounds (pad, y, W, secH);
    y += secH + 8;
    knobModeLbl.setBounds (pad,       y, 100, rowH);
    knobImm.setBounds     (pad + 104, y, 104, rowH);
    knobHook.setBounds    (pad + 210, y, 80,  rowH);
    pedalLbl.setBounds    (pad + 320, y, 110, rowH);
    pedalNorm.setBounds   (pad + 432, y, 90,  rowH);
    pedalInv.setBounds    (pad + 524, y, 100, rowH);
    y += rowH + 8;
    kbModeLbl.setBounds   (pad,       y, 120, rowH);
    kbActive.setBounds    (pad + 124, y, 120, rowH);
    kbSelected.setBounds  (pad + 248, y, 150, rowH);

    // ── Buttons, in the footer band ──────────────────────────────────────────
    const int btnW = 100, btnH = 36;
    const int btnY = getHeight() - footerH + (footerH - btnH) / 2;
    cancelButton.setBounds (getWidth() - pad - btnW * 2 - 12, btnY, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW,          btnY, btnW, btnH);
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
