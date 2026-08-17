#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../midi/ConnectionManager.h"
#include "FlatCloseButton.h"

class MidiSettingsDialog : public juce::Component
{
public:
    MidiSettingsDialog();

    void refreshDeviceLists();
    void setSelectedPorts(const juce::String& inputId, const juce::String& outputId);
    void setConnectedState(const ConnectionManager::Status& status);

    std::function<void(const juce::String& inputId, const juce::String& outputId)> onConnectionRequest;
    std::function<void()> onDisconnectionRequest;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    // Returns the dialog, so a caller can watch for it closing.
    static juce::Component* show(juce::Component* parent,
                     const juce::String& currentInputId,
                     const juce::String& currentOutputId,
                     const ConnectionManager::Status& status,
                     std::function<void(const juce::String&, const juce::String&)> connectCb,
                     std::function<void()> disconnectCb);

private:
    void close();
    void updateButtonState();

    bool connected = false;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    juce::Label    inputLabel    { {}, "MIDI INPUT" };
    juce::ComboBox inputCombo;
    juce::Label    outputLabel   { {}, "MIDI OUTPUT" };
    juce::ComboBox outputCombo;
    juce::TextButton connectButton;
    juce::Label    statusLabel;

    juce::StringArray inputIds;
    juce::StringArray outputIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiSettingsDialog)
};
