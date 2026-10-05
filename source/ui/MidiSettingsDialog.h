#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "../midi/SynthHub.h"
#include "FlatCloseButton.h"

// MIDI Setup, as the original editor has it: one group per port, each with its In and
// Out ports, an Enabled box and the Status of the synth found there. A port is one
// synth with its own four slots. OK applies and closes, Apply applies, Cancel closes.
class MidiSettingsDialog : public juce::Component
{
public:
    struct Port
    {
        juce::String inputId, outputId;
        bool enabled = false;
    };
    using Ports = std::array<Port, kMaxSynths>;

    MidiSettingsDialog();

    void refreshDeviceLists();
    void setPorts(const Ports& ports);
    // What a port's connection says: "Looking...", the synth's name once it answers, or why not.
    void setPortStatus(int port, const ConnectionManager::Status& status);

    // Called for every port whose settings changed (or that is enabled but not
    // connected) when OK or Apply is pressed.
    std::function<void(int port, const Port& wanted)> onApply;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    // Returns the dialog, so a caller can watch for it closing and keep its
    // port status current.
    static MidiSettingsDialog* show(juce::Component* parent, const Ports& ports,
                                    std::function<void(int, const Port&)> applyCb);

private:
    struct PortGroup
    {
        juce::Label    inLabel  { {}, "In" };
        juce::Label    outLabel { {}, "Out" };
        juce::ComboBox inCombo, outCombo;
        juce::ToggleButton enabled { "Enabled" };
        juce::Label    statusCaption { {}, "Status:" };
        juce::Label    status;
        bool connected = false;
    };

    void close();
    void apply();
    Port currentPort(int i) const;
    void updateEnabledState(int i);

    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;
    std::array<PortGroup, kMaxSynths> groups;
    std::array<Port, kMaxSynths> applied;   // what the synth was last told
    juce::TextButton okButton { "OK" }, cancelButton { "Cancel" }, applyButton { "Apply" };

    juce::StringArray inputIds;
    juce::StringArray outputIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiSettingsDialog)
};
