#include "KeyboardFloaterWindow.h"
#include "AppTheme.h"
#include "CapsLock.h"
#include "midi/ComputerKeys.h"

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
    // The component's own key mapping only worked while it had the focus and an
    // octave away from what it showed; Caps Lock plays it now (playComputerKey).
    keyboard.clearKeyMappings();
    keyboard.setWantsKeyboardFocus(false);
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
        {
            // Pulses replace sustained notes: silence whatever is sounding,
            // the timer takes over from here
            auto notes = soundingNotes;  // sendOff mutates the set
            for (int note : notes)
                sendOff(note);
            startTimer(juce::roundToInt(rateSlider.getValue()));
        }
        else
        {
            stopTimer();
            if (droneNote >= 0)
                sendOn(droneNote);  // drone goes back to a sustained note
        }
    };
    addAndMakeVisible(repeatButton);

    rateLabel.setText("Rate", juce::dontSendNotification);
    addAndMakeVisible(rateLabel);
    rateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    rateSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    rateSlider.setRange(100.0, 500.0, 1.0);
    rateSlider.setValue(250.0, juce::dontSendNotification);
    rateSlider.onValueChange = [this]() {
        if (repeatButton.getToggleState())
            startTimer(juce::roundToInt(rateSlider.getValue()));
    };
    addAndMakeVisible(rateSlider);

    gateLabel.setText("Gate", juce::dontSendNotification);
    addAndMakeVisible(gateLabel);
    gateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gateSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    gateSlider.setRange(20.0, 400.0, 1.0);
    gateSlider.setValue(100.0, juce::dontSendNotification);
    addAndMakeVisible(gateSlider);

    applyTheme();
    setSize(760, 202);
}

KeyboardFloaterPanel::~KeyboardFloaterPanel()
{
    keyWatch.stopTimer();
    keyboardState.removeListener(this);
    stopTimer();
}

bool KeyboardFloaterPanel::playComputerKey(const juce::KeyPress& key)
{
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        return false;

    const int code = key.getKeyCode();
    const int offset = ComputerKeys::offsetForKey(code);
    const auto mods = key.getModifiers();
    if (offset == -1 || mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false;
    if (!CapsLock::isOn())
        return false;

    if (offset == ComputerKeys::octaveDown || offset == ComputerKeys::octaveUp)
    {
        shiftOctave(offset == ComputerKeys::octaveUp ? 12 : -12);
        return true;
    }

    if (computerKeys.count(code) > 0)
        return true;   // the key repeating while held

    // The keyboard's C is the octave above the lowest one shown: C4 as it opens
    // (setOctaveForMiddleC(4), so 60 reads C4 on the keys)
    const int note = 60 + 12 * octave + offset;
    if (note > 127)
        return true;

    computerKeys[code] = note;
    keyboardState.noteOn(1, note, 1.0f);
    keyWatch.startTimer(20);
    return true;
}

void KeyboardFloaterPanel::releaseLiftedComputerKeys()
{
    for (auto it = computerKeys.begin(); it != computerKeys.end();)
    {
        if (juce::KeyPress::isKeyCurrentlyDown(it->first))
        {
            ++it;
            continue;
        }
        const int note = it->second;
        it = computerKeys.erase(it);

        // Two keys can end on the same note after an octave shift: let it go
        // with the last of them
        bool stillHeld = false;
        for (const auto& held : computerKeys)
            stillHeld = stillHeld || held.second == note;
        if (!stillHeld)
            keyboardState.noteOff(1, note, 0.0f);
    }

    if (computerKeys.empty())
        keyWatch.stopTimer();
}

void KeyboardFloaterPanel::shiftOctave(int semitones)
{
    // The octave is kept here, not read back from the view: JUCE moves the view
    // on its own when it resizes, and a closed floater has no size at all.
    // C3 to C6 can start the view, so the computer keys play from C1 up.
    octave = juce::jlimit(-2, 3, octave + semitones / 12);
    keyboard.setLowestVisibleKey(48 + 12 * octave);
    if (onOctaveChanged)
        onOctaveChanged("C" + juce::String(4 + octave));
}

void KeyboardFloaterPanel::sendOn(int note)
{
    soundingNotes.insert(note);
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

    bool repeating = repeatButton.getToggleState();

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
            if (soundingNotes.count(droneNote) > 0)
                sendOff(droneNote);
            internalChange = true;
            keyboardState.noteOff(1, droneNote, 0.0f);
            internalChange = false;
        }
        droneNote = note;
        dronePendingOff = false;
        if (repeating)
        {
            pulse(note);
            startTimer(juce::roundToInt(rateSlider.getValue()));  // align the ticks with the click
        }
        else
            sendOn(note);
        return;
    }

    heldNote = note;
    if (repeating)
    {
        pulse(note);
        startTimer(juce::roundToInt(rateSlider.getValue()));
    }
    else
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
            if (soundingNotes.count(note) > 0)
                sendOff(note);
            return;
        }

        // Keep the drone note held: re-light the key the mouse released
        internalChange = true;
        keyboardState.noteOn(1, note, 1.0f);
        internalChange = false;
        return;
    }

    if (note == heldNote)
        heldNote = -1;
    if (soundingNotes.count(note) > 0)
        sendOff(note);
}

void KeyboardFloaterPanel::timerCallback()
{
    // One pulse per tick, on the drone note or the key held by the mouse
    int note = droneNote >= 0 ? droneNote : heldNote;
    if (note < 0) return;

    pulse(note);
}

void KeyboardFloaterPanel::pulse(int note)
{
    if (soundingNotes.count(note) > 0)  // previous pulse still open: close it
        sendOff(note);
    sendOn(note);

    juce::Component::SafePointer<KeyboardFloaterPanel> safe(this);
    juce::Timer::callAfterDelay(juce::roundToInt(gateSlider.getValue()), [safe, note]() {
        if (safe != nullptr && safe->soundingNotes.count(note) > 0)
            safe->sendOff(note);
    });
}

void KeyboardFloaterPanel::allNotesOff()
{
    auto notes = soundingNotes;  // sendOff mutates the set
    for (int note : notes)
        sendOff(note);

    droneNote = -1;
    dronePendingOff = false;
    heldNote = -1;  // stop the repeat timer from pulsing after close
    computerKeys.clear();
    keyWatch.stopTimer();

    internalChange = true;
    keyboardState.allNotesOff(1);
    internalChange = false;
}

void KeyboardFloaterPanel::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    g.setColour(kBorder);
    g.fillRect(0, 16 + 32 + 7, getWidth(), 1);   // under the controls
}

void KeyboardFloaterPanel::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto controls = area.removeFromTop(32);

    octDownButton.setBounds(controls.removeFromLeft(60));
    controls.removeFromLeft(6);
    octUpButton.setBounds(controls.removeFromLeft(60));
    controls.removeFromLeft(18);
    droneButton.setBounds(controls.removeFromLeft(70));
    controls.removeFromLeft(8);
    repeatButton.setBounds(controls.removeFromLeft(70));
    controls.removeFromLeft(18);
    rateLabel.setBounds(controls.removeFromLeft(40));
    int sliderWidth = (controls.getWidth() - 62) / 2;  // room for gate label + gap
    rateSlider.setBounds(controls.removeFromLeft(sliderWidth));
    controls.removeFromLeft(14);
    gateLabel.setBounds(controls.removeFromLeft(40));
    gateSlider.setBounds(controls);

    area.removeFromTop(15);
    keyboard.setBounds(area);
    if (area.getWidth() > 0)
        keyboard.setKeyWidth(area.getWidth() / 36.0f);  // ~5 octaves of white keys visible
}

void KeyboardFloaterPanel::applyTheme()
{
    rateLabel.setFont(AppTheme::uiFont(13.0f).withStyle("Bold"));
    gateLabel.setFont(AppTheme::uiFont(13.0f).withStyle("Bold"));
    rateLabel.setColour(juce::Label::textColourId, kTextDim);
    gateLabel.setColour(juce::Label::textColourId, kTextDim);

    for (auto* button : { &octDownButton, &octUpButton, &droneButton, &repeatButton })
    {
        button->setColour(juce::TextButton::buttonColourId, kButton);
        button->setColour(juce::TextButton::buttonOnColourId, kAccent);
        button->setColour(juce::TextButton::textColourOffId, kText);
        button->setColour(juce::TextButton::textColourOnId, AppTheme::palette().backgroundMain);   // on = accent fill
    }

    for (auto* slider : { &rateSlider, &gateSlider })
    {
        slider->setColour(juce::Slider::trackColourId, kAccent);
        slider->setColour(juce::Slider::backgroundColourId, kBorder);
        slider->setColour(juce::Slider::thumbColourId, kText);
    }

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
    panel.onOctaveChanged = [this](const juce::String& c) {
        if (onOctaveChanged) onOctaveChanged(c);
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
