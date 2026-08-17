#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "FlatCloseButton.h"

class PatchSettingsDialog : public juce::Component
{
public:
    struct Result
    {
        int  voices;
        int  velRangeMin, velRangeMax;
        int  keyRangeMin, keyRangeMax;
        int  pedalMode;           // 0=Sustain 1=On/Off
        int  bendRange;           // 1-24 semitones
        bool portamento;          // false=Normal true=Auto
        int  portamentoTime;      // 0-127
        int  octaveShift;         // 0-4 → -2..+2
        bool voiceRetriggerPoly;
        bool voiceRetriggerCommon;
    };

    using Callback = std::function<void(const Result&)>;

    PatchSettingsDialog (const PatchHeader& header, Callback onOk);

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    // Returns the dialog, so a caller can watch for it closing.
    static juce::Component* show (juce::Component* parent, const PatchHeader& header, Callback onOk);

private:
    void close();

    Callback okCallback;
    juce::ComponentDragger dragger;

    // Title bar
    FlatCloseButton closeButton;

    // Voices
    juce::Label  voicesLabel   { {}, "Voices" };
    juce::Slider voicesSlider;

    // Velocity / Keyboard  (two rows)
    juce::Label  velKeyLabel   { {}, "VELOCITY  &  KEYBOARD" };
    juce::Label  velLoLbl  { {}, "Vel Lo" },  velHiLbl  { {}, "Vel Hi" };
    juce::Label  keyLoLbl  { {}, "Key Lo" },  keyHiLbl  { {}, "Key Hi" };
    juce::Slider velMinSlider, velMaxSlider;
    juce::Slider keyMinSlider, keyMaxSlider;

    // Pedal + Bend
    juce::Label  pedalBendLabel { {}, "PEDAL  &  BEND" };
    juce::Label  pedalLbl  { {}, "Pedal" };
    juce::ToggleButton pedalSustain { "Sustain" }, pedalOnOff { "On/Off" };
    juce::Label  bendLbl   { {}, "Bend" };
    juce::Slider bendSlider;
    juce::Label  bendUnitLbl { {}, "st" };

    // Portamento
    juce::Label  portaLabel      { {}, "PORTAMENTO" };
    juce::ToggleButton portaNormal { "Normal" }, portaAuto { "Auto" };
    juce::Label  portaTimeLbl    { {}, "Time" };
    juce::Slider portaTimeSlider;

    // Octave + Retrigger
    juce::Label  octRetrigLabel  { {}, "OCTAVE SHIFT  &  VOICE RETRIGGER" };
    juce::Label  octLbl    { {}, "Octave" };
    juce::ToggleButton octaveButtons[5];   // -2 -1 0 +1 +2
    juce::Label  retrigLbl { {}, "Retrig" };
    juce::ToggleButton retrigPoly { "Poly" }, retrigCommon { "Common" };

    // Buttons
    juce::TextButton okButton { "OK" }, cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSettingsDialog)
};
