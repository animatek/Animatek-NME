#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "NmProtocol.h"
#include <atomic>
#include <memory>

class MidiDeviceManager : private juce::MidiInputCallback
{
public:
    MidiDeviceManager(NmProtocol& protocol);
    ~MidiDeviceManager() override;

    // Available MIDI devices
    static juce::Array<juce::MidiDeviceInfo> getAvailableInputDevices();
    static juce::Array<juce::MidiDeviceInfo> getAvailableOutputDevices();

    // Connect to MIDI ports by identifier
    bool connect(const juce::String& inputId, const juce::String& outputId);
    void disconnect();

    bool isConnected() const { return midiInput != nullptr && midiOutput != nullptr; }

    // Send raw SysEx data
    void sendSysEx(const std::vector<uint8_t>& data);

    juce::String getInputDeviceName() const;
    juce::String getOutputDeviceName() const;

private:
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    NmProtocol& protocol;
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiDeviceManager)
};
