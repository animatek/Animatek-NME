#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <set>

/**
 * Keyboard Floater: virtual MIDI keyboard that plays the synth over the
 * active slot's MIDI channel (modeled on the original Clavia editor).
 *
 * DRONE latches the clicked note until it is clicked again, another note is
 * clicked, or the mode is switched off. REPEAT retriggers the sounding note
 * at the interval set by the rate slider.
 */
class KeyboardFloaterPanel : public juce::Component,
                             private juce::MidiKeyboardState::Listener,
                             private juce::Timer
{
public:
    KeyboardFloaterPanel();
    ~KeyboardFloaterPanel() override;

    void allNotesOff();
    void applyTheme();
    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(int note, int velocity)> onNoteOn;
    std::function<void(int note)> onNoteOff;

private:
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void timerCallback() override;

    void sendOn(int note);
    void sendOff(int note);
    void shiftOctave(int semitones);

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::TextButton octDownButton { "Oct -" };
    juce::TextButton octUpButton   { "Oct +" };
    juce::TextButton droneButton  { "DRONE" };
    juce::TextButton repeatButton { "REPEAT" };
    juce::Label rateLabel;
    juce::Slider rateSlider;

    bool internalChange = false;  // guards programmatic keyboardState calls
    int droneNote = -1;
    bool dronePendingOff = false; // same drone note clicked again: release it on mouse-up
    int lastNote = -1;
    std::set<int> soundingNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardFloaterPanel)
};

class KeyboardFloaterWindow : public juce::DocumentWindow
{
public:
    KeyboardFloaterWindow();

    void allNotesOff();
    void closeButtonPressed() override;
    void applyTheme();

    std::function<void(int note, int velocity)> onNoteOn;
    std::function<void(int note)> onNoteOff;
    std::function<void()> onClosed;

private:
    KeyboardFloaterPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardFloaterWindow)
};
