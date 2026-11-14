#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include <set>

/**
 * Keyboard Floater: virtual MIDI keyboard that plays the synth over the
 * active slot's MIDI channel (modeled on the original Clavia editor).
 *
 * DRONE latches the clicked note until it is clicked again, another note is
 * clicked, or the mode is switched off. REPEAT sends a pulse (one note-on/off
 * pair, length set by the gate slider) at the rate slider's interval while a
 * key is held or a drone note is latched.
 *
 * With Caps Lock on, the computer keyboard plays it too (issue #124): the keys
 * go through the same keyboard state, so DRONE, REPEAT and the octave apply and
 * the keys light up as they sound. ComputerKeys.h has the layout.
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

    // A key from the computer keyboard: plays or shifts the octave and returns
    // true when Caps Lock is on and the key is one of the playing keys. Only in
    // the standalone app; a DAW has its own computer keyboard.
    bool playComputerKey(const juce::KeyPress& key);

    // Told the new octave after Z/X or the Oct buttons, as "C4" for the C the
    // computer keyboard starts on
    std::function<void(const juce::String&)> onOctaveChanged;

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
    void pulse(int note);
    void shiftOctave(int semitones);
    void releaseLiftedComputerKeys();

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState,
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::TextButton octDownButton { "Oct -" };
    juce::TextButton octUpButton   { "Oct +" };
    juce::TextButton droneButton  { "DRONE" };
    juce::TextButton repeatButton { "REPEAT" };
    juce::Label rateLabel;
    juce::Slider rateSlider;
    juce::Label gateLabel;
    juce::Slider gateSlider;

    bool internalChange = false;  // guards programmatic keyboardState calls
    int droneNote = -1;
    bool dronePendingOff = false; // same drone note clicked again: release it on mouse-up
    int heldNote = -1;            // key currently held by the mouse
    int octave = 0;               // octaves from the opening view (C3 shown, C4 played)
    std::set<int> soundingNotes;

    // Computer keys held down, key code -> the note it started. The note is kept
    // so that an octave shift while it is held still releases the right one.
    std::map<int, int> computerKeys;

    // Watches the held computer keys and releases the lifted ones: a key-up does
    // not always reach a component (focus moves, another window takes it).
    struct KeyWatch : juce::Timer
    {
        KeyboardFloaterPanel& owner;
        explicit KeyWatch(KeyboardFloaterPanel& o) : owner(o) {}
        void timerCallback() override { owner.releaseLiftedComputerKeys(); }
    } keyWatch { *this };

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
    std::function<void(const juce::String&)> onOctaveChanged;

    // App-wide shortcuts (Ctrl+1..8) forwarded while this floater has focus
    std::function<bool(const juce::KeyPress&)> onGlobalKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        return panel.playComputerKey(key)
            || (onGlobalKey && onGlobalKey(key))
            || juce::DocumentWindow::keyPressed(key);
    }

    bool playComputerKey(const juce::KeyPress& key) { return panel.playComputerKey(key); }

private:
    KeyboardFloaterPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardFloaterWindow)
};
