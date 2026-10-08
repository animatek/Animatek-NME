#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/SynthSettings.h"
#include "FlatCloseButton.h"

class SynthSettingsDialog : public juce::Component
{
public:
    using Callback = std::function<void(const SynthSettings&)>;

    SynthSettingsDialog (const SynthSettings& current, Callback onOk);

    void setSettings (const SynthSettings& settings);
    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    static SynthSettingsDialog* show (juce::Component* parent, const SynthSettings& current, Callback onOk);

private:
    void close();
    void updateMasterTuneLabel();

    Callback okCallback;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;
    SynthSettings working;

    // ── Synth ──
    juce::Label  synthHdr     { {}, "SYNTH" };
    juce::Label  nameLbl      { {}, "Name" };
    juce::TextEditor nameEditor;
    juce::Label  tuneLbl      { {}, "Master Tune" };
    juce::Slider masterTuneSlider;
    juce::Label  tuneCentsLbl;

    // ── MIDI channels per slot ──
    juce::Label  chanHdr      { {}, "MIDI CHANNELS PER SLOT" };
    juce::Label  slotLbls[4];
    juce::Slider chanSliders[4];

    // ── MIDI ──
    juce::Label  midiHdr      { {}, "MIDI" };
    juce::Label  velScaleLbl  { {}, "Velocity scale" };
    juce::Label  velMinTag    { {}, "Min" },  velMaxTag { {}, "Max" };
    juce::Slider velMinSlider, velMaxSlider;
    juce::ToggleButton localOnTgl    { "Local On" };
    juce::ToggleButton ledsActiveTgl { "LEDs Active" };
    juce::ToggleButton pgmRecvTgl    { "Pgm Recv" };
    juce::ToggleButton pgmSendTgl    { "Pgm Send" };

    // ── Clock ──
    juce::Label  clockHdr     { {}, "CLOCK" };
    juce::ToggleButton clockInt { "Internal" }, clockExt { "External" };
    juce::Label  bpmLbl       { {}, "BPM" };
    juce::Slider bpmSlider;
    juce::Label  globalSyncLbl { {}, "Global Sync" };
    juce::Slider globalSyncSlider;   // quarter notes, 1-32

    // ── Behavior ──
    juce::Label  behavHdr     { {}, "BEHAVIOR" };
    juce::Label  knobModeLbl  { {}, "Knob mode" };
    juce::ToggleButton knobImm { "Immediate" }, knobHook { "Hook" };
    juce::Label  pedalLbl     { {}, "Pedal polarity" };
    juce::ToggleButton pedalNorm { "Normal" }, pedalInv { "Inverted" };
    juce::Label  kbModeLbl    { {}, "Keyboard mode" };
    juce::ToggleButton kbActive { "Active slot" }, kbSelected { "Selected slots" };

    // ── Buttons ──
    juce::TextButton okButton { "OK" }, cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthSettingsDialog)
};
