#include "KeyboardFloaterWindow.h"
#include "AppTheme.h"

#define kBg      (AppTheme::palette().backgroundMain)
#define kText    (AppTheme::palette().textPrimary)
#define kTextDim (AppTheme::palette().textMuted)
#define kBorder  (AppTheme::palette().borderColor)
#define kButton  (AppTheme::palette().buttonBackground)
#define kAccent  (AppTheme::palette().accentActive)

// ============================================================================
// KeyboardFloaterPanel
// ============================================================================
KeyboardFloaterPanel::KeyboardFloaterPanel()
{
    keyboard.setAvailableRange(24, 108);
    keyboard.setLowestVisibleKey(48);
    keyboard.setOctaveForMiddleC(4);
    keyboard.setScrollButtonsVisible(false);  // octave buttons handle navigation
    addAndMakeVisible(keyboard);
    keyboardState.addListener(this);

    octDownButton.onClick = [this]() { shiftOctave(-12); };
    octUpButton.onClick   = [this]() { shiftOctave(12); };
    addAndMakeVisible(octDownButton);
    addAndMakeVisible(octUpButton);

    droneButton.setClickingTogglesState(true);
    droneButton.onClick = [this]() {
        if (!droneButton.getToggleState())
            allNotesOff();
    };
    addAndMakeVisible(droneButton);

    repeatButton.setClickingTogglesState(true);
    repeatButton.onClick = [this]() {
        if (repeatButton.getToggleState())
            startTimer(juce::roundToInt(rateSlider.getValue()));
        else
        {
            stopTimer();
            // The last retrigger left the note on — silence it unless it is
            // still held by the mouse or latched by drone
            if (droneNote < 0 && lastNote >= 0 && soundingNotes.count(lastNote) > 0
                && !keyboardState.isNoteOn(1, lastNote))
                sendOff(lastNote);
        }
    };
    addAndMakeVisible(repeatButton);

    rateLabel.setText("Rate", juce::dontSendNotification);
    addAndMakeVisible(rateLabel);
    rateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    rateSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    rateSlider.setRange(50.0, 2000.0, 1.0);
    rateSlider.setSkewFactorFromMidPoint(400.0);
    rateSlider.setValue(250.0, juce::dontSendNotification);
    rateSlider.onValueChange = [this]() {
        if (repeatButton.getToggleState())
            startTimer(juce::roundToInt(rateSlider.getValue()));
    };
    addAndMakeVisible(rateSlider);

    applyTheme();
    setSize(640, 168);
}

KeyboardFloaterPanel::~KeyboardFloaterPanel()
{
    keyboardState.removeListener(this);
    stopTimer();
}

void KeyboardFloaterPanel::shiftOctave(int semitones)
{
    keyboard.setLowestVisibleKey(juce::jlimit(24, 96, keyboard.getLowestVisibleKey() + semitones));
}

void KeyboardFloaterPanel::sendOn(int note)
{
    soundingNotes.insert(note);
    lastNote = note;
    if (onNoteOn) onNoteOn(note, 100);  // protocol Note command carries no velocity
}

void KeyboardFloaterPanel::sendOff(int note)
{
    soundingNotes.erase(note);
    if (onNoteOff) onNoteOff(note);
}

void KeyboardFloaterPanel::handleNoteOn(juce::MidiKeyboardState*, int, int note, float)
{
    if (internalChange) return;

    if (droneButton.getToggleState())
    {
        if (note == droneNote)
        {
            // Same note clicked again: release it when the mouse lets go
            dronePendingOff = true;
            return;
        }

        if (droneNote >= 0)
        {
            sendOff(droneNote);
            internalChange = true;
            keyboardState.noteOff(1, droneNote, 0.0f);
            internalChange = false;
        }
        droneNote = note;
        dronePendingOff = false;
        sendOn(note);
        return;
    }

    sendOn(note);
}

void KeyboardFloaterPanel::handleNoteOff(juce::MidiKeyboardState*, int, int note, float)
{
    if (internalChange) return;

    if (droneButton.getToggleState() && note == droneNote)
    {
        if (dronePendingOff)
        {
            dronePendingOff = false;
            droneNote = -1;
            sendOff(note);
            return;
        }

        // Keep the drone note held: re-light the key the mouse released
        internalChange = true;
        keyboardState.noteOn(1, note, 1.0f);
        internalChange = false;
        return;
    }

    sendOff(note);
}

void KeyboardFloaterPanel::timerCallback()
{
    // Retrigger the drone note, or the last clicked note (even after release)
    int note = droneNote >= 0 ? droneNote : lastNote;
    if (note < 0) return;

    sendOff(note);
    sendOn(note);
}

void KeyboardFloaterPanel::allNotesOff()
{
    auto notes = soundingNotes;  // sendOff mutates the set
    for (int note : notes)
        sendOff(note);

    droneNote = -1;
    dronePendingOff = false;
    lastNote = -1;  // stop the repeat timer from retriggering after close

    internalChange = true;
    keyboardState.allNotesOff(1);
    internalChange = false;
}

void KeyboardFloaterPanel::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
}

void KeyboardFloaterPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    auto controls = area.removeFromTop(26);

    octDownButton.setBounds(controls.removeFromLeft(52));
    controls.removeFromLeft(4);
    octUpButton.setBounds(controls.removeFromLeft(52));
    controls.removeFromLeft(16);
    droneButton.setBounds(controls.removeFromLeft(64));
    controls.removeFromLeft(12);
    repeatButton.setBounds(controls.removeFromLeft(64));
    controls.removeFromLeft(8);
    rateLabel.setBounds(controls.removeFromLeft(34));
    rateSlider.setBounds(controls);

    area.removeFromTop(6);
    keyboard.setBounds(area);
    if (area.getWidth() > 0)
        keyboard.setKeyWidth(area.getWidth() / 36.0f);  // ~5 octaves of white keys visible
}

void KeyboardFloaterPanel::applyTheme()
{
    rateLabel.setColour(juce::Label::textColourId, kTextDim);

    for (auto* button : { &octDownButton, &octUpButton, &droneButton, &repeatButton })
    {
        button->setColour(juce::TextButton::buttonColourId, kButton);
        button->setColour(juce::TextButton::buttonOnColourId, kAccent);
        button->setColour(juce::TextButton::textColourOffId, kText);
        button->setColour(juce::TextButton::textColourOnId, kText);
    }

    rateSlider.setColour(juce::Slider::trackColourId, kAccent);
    rateSlider.setColour(juce::Slider::backgroundColourId, kBorder);
    rateSlider.setColour(juce::Slider::thumbColourId, kText);

    repaint();
}

// ============================================================================
// KeyboardFloaterWindow
// ============================================================================
KeyboardFloaterWindow::KeyboardFloaterWindow()
    : juce::DocumentWindow("Keyboard Floater", kBg, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setResizable(false, false);
    setContentNonOwned(&panel, true);

    panel.onNoteOn = [this](int note, int velocity) {
        if (onNoteOn) onNoteOn(note, velocity);
    };
    panel.onNoteOff = [this](int note) {
        if (onNoteOff) onNoteOff(note);
    };
}

void KeyboardFloaterWindow::allNotesOff() { panel.allNotesOff(); }

void KeyboardFloaterWindow::closeButtonPressed()
{
    panel.allNotesOff();
    setVisible(false);
    if (onClosed) onClosed();
}

void KeyboardFloaterWindow::applyTheme()
{
    setBackgroundColour(kBg);
    panel.applyTheme();
    repaint();
}
