#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "NmProtocol.h"
#include "DirectLink.h"
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

    bool isConnected() const { return (midiInput != nullptr && midiOutput != nullptr) || link.isOpen(); }
    // Connected to G1-Emu over its direct link rather than through MIDI ports.
    bool isDirectLink() const { return link.isOpen(); }

    // Send raw SysEx data
    void sendSysEx(const std::vector<uint8_t>& data);

    juce::String getInputDeviceName() const;
    juce::String getOutputDeviceName() const;

    // Called on the message thread when a port this connection has open
    // leaves the system (G1-Emu closed, USB interface unplugged), with the
    // names of the ports that went. The connection is dead from then on:
    // nothing reports it otherwise, and the editor stayed "Connected".
    void setPortsGoneCallback(std::function<void(const juce::String&)> cb) { portsGoneCallback = std::move(cb); }

private:
    void checkPortsStillThere();

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    // A whole SysEx from either transport, on any thread: filtered, logged and handed to the protocol
    // on the message thread.
    void deliverSysEx(const uint8_t* data, int size);
    bool connectLink(int port);

    DirectLink::Client link;

    NmProtocol& protocol;
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::String inputId, outputId, inputName, outputName;
    juce::MidiDeviceListConnection deviceListConnection;
    std::function<void(const juce::String&)> portsGoneCallback;
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiDeviceManager)
};
