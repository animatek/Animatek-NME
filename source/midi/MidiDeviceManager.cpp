#include "MidiDeviceManager.h"
#include "MidiMonitor.h"
#include <cstdlib>
#include <iostream>

MidiDeviceManager::MidiDeviceManager(NmProtocol& proto)
    : protocol(proto)
{
    // Wire the protocol's send function to our output
    protocol.setSendFunction([this](const std::vector<uint8_t>& data)
    {
        sendSysEx(data);
    });
}

MidiDeviceManager::~MidiDeviceManager()
{
    disconnect();
}

// The running G1-Emu instances, listed among the MIDI devices with the same id on both sides, so
// MIDI Setup offers them like any port. One that already has an editor (this one, on another
// port, or someone else's) is listed too: it is still there.
static void addDirectLinks(juce::Array<juce::MidiDeviceInfo>& devices)
{
    for (const auto& inst : DirectLink::discover())
        devices.add(juce::MidiDeviceInfo(
            (inst.name.isNotEmpty() ? inst.name : juce::String("G1-Emu")) + " (direct link"
                + (inst.busy ? ", in use)" : ")"),
            DirectLink::deviceId(inst.port)));
}

juce::Array<juce::MidiDeviceInfo> MidiDeviceManager::getAvailableInputDevices()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    addDirectLinks(devices);
    return devices;
}

juce::Array<juce::MidiDeviceInfo> MidiDeviceManager::getAvailableOutputDevices()
{
    auto devices = juce::MidiOutput::getAvailableDevices();
    addDirectLinks(devices);
    return devices;
}

bool MidiDeviceManager::connectLink(int port)
{
    alive = std::make_shared<std::atomic<bool>>(true);
    auto aliveFlag = alive;
    const bool ok = link.open(port,
        [this, aliveFlag](std::vector<uint8_t> m) {
            if (*aliveFlag)
                deliverSysEx(m.data(), static_cast<int>(m.size()));
        },
        [this, aliveFlag] {
            // The emulator closed (quit, or its instance was removed): like a MIDI port that left.
            auto cb = portsGoneCallback;
            const auto name = inputName;
            juce::MessageManager::callAsync([cb, aliveFlag, name] {
                if (*aliveFlag && cb)
                    cb(name);
            });
        });
    if (!ok)
        return false;

    protocol.setSendFunction([this](const std::vector<uint8_t>& data) { sendSysEx(data); });
    inputId = outputId = DirectLink::deviceId(port);
    inputName = outputName = link.name() + " (direct link)";
    DBG("G1-Emu direct link connected: " + inputName);
    return true;
}

bool MidiDeviceManager::connect(const juce::String& inputId_, const juce::String& outputId_)
{
    disconnect();

    // A direct link carries both directions: either side naming one is enough.
    if (DirectLink::isDeviceId(inputId_) || DirectLink::isDeviceId(outputId_))
        return connectLink(DirectLink::portOf(DirectLink::isDeviceId(inputId_) ? inputId_ : outputId_));

    midiInput = juce::MidiInput::openDevice(inputId_, this);
    midiOutput = juce::MidiOutput::openDevice(outputId_);

    if (midiInput == nullptr || midiOutput == nullptr)
    {
        disconnect();
        return false;
    }

    alive = std::make_shared<std::atomic<bool>>(true);
    protocol.setSendFunction([this](const std::vector<uint8_t>& data) { sendSysEx(data); });
    midiInput->start();

    inputId = inputId_;
    outputId = outputId_;
    inputName = midiInput->getName();
    outputName = midiOutput->getName();
    deviceListConnection = juce::MidiDeviceListConnection::make([this] { checkPortsStillThere(); });

    DBG("MIDI connected: input=" + getInputDeviceName() + " output=" + getOutputDeviceName());
    return true;
}

void MidiDeviceManager::disconnect()
{
    *alive = false;  // Invalidate received messages already posted to the UI thread.
    deviceListConnection = {};
    inputId.clear();
    outputId.clear();
    protocol.setSendFunction({});
    link.close();
    if (midiInput)
    {
        midiInput->stop();
        midiInput.reset();
    }
    midiOutput.reset();
}

void MidiDeviceManager::sendSysEx(const std::vector<uint8_t>& data)
{
    // data already contains the full SysEx frame (F0 ... F7) from SysEx::encode(),
    // so construct MidiMessage directly — createSysExMessage would double-wrap.
    if (midiOutput && !data.empty())
    {
#if JUCE_DEBUG
        juce::String hex;
        for (auto b : data)
            hex += juce::String::toHexString(b).paddedLeft('0', 2) + " ";
        DBG("TX SysEx [" + juce::String(data.size()) + "]: " + hex.trimEnd());
#endif
        MidiMonitor::instance().record(MidiMonitor::Direction::Tx, data.data(), data.size());
        midiOutput->sendMessageNow(juce::MidiMessage(data.data(), static_cast<int>(data.size())));
    }
    else if (link.isOpen() && !data.empty())
    {
        MidiMonitor::instance().record(MidiMonitor::Direction::Tx, data.data(), data.size());
        link.send(data);
    }
}


void MidiDeviceManager::checkPortsStillThere()
{
    // A direct link reports its own end; probing the emulator here would knock on its door.
    if (!isConnected() || !portsGoneCallback || link.isOpen())
        return;

    const auto present = [](const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& id) {
        for (const auto& d : devices)
            if (d.identifier == id)
                return true;
        return false;
    };

    // The MIDI lists alone: this runs on every device change, and the full lists knock on emulators.
    juce::StringArray gone;
    if (!present(juce::MidiInput::getAvailableDevices(), inputId))
        gone.add(inputName);
    if (!present(juce::MidiOutput::getAvailableDevices(), outputId))
        gone.addIfNotAlreadyThere(outputName);
    if (gone.isEmpty())
        return;

    // The callback tears this object down; run it after this one returns.
    auto cb = portsGoneCallback;
    auto aliveFlag = alive;
    const auto names = gone.joinIntoString(", ");
    juce::MessageManager::callAsync([cb, aliveFlag, names] {
        if (*aliveFlag)
            cb(names);
    });
}

juce::String MidiDeviceManager::getInputDeviceName() const
{
    return midiInput ? midiInput->getName() : link.isOpen() ? inputName : juce::String();
}

juce::String MidiDeviceManager::getOutputDeviceName() const
{
    return midiOutput ? midiOutput->getName() : link.isOpen() ? outputName : juce::String();
}

void MidiDeviceManager::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (!message.isSysEx())
        return;

    // Use getRawData to get the full SysEx frame (F0 ... F7) that SysEx::decode() expects.
    // getSysExData() strips the leading F0 which would break our decoder.
    deliverSysEx(message.getRawData(), message.getRawDataSize());
}

void MidiDeviceManager::deliverSysEx(const uint8_t* data, int size)
{

    // Log every incoming SysEx (even non-Nord frames) before filtering, so the
    // monitor can surface unexpected traffic.
    MidiMonitor::instance().record(MidiMonitor::Direction::Rx,
                                   data, static_cast<std::size_t>(size));

    // Wire format: F0 33 [header] 06 ...
    if (size < 5 || data[1] != 0x33 || data[3] != 0x06)
        return;

    // Forward to protocol on the message thread
    auto sysexCopy = std::make_shared<std::vector<uint8_t>>(data, data + size);
    auto aliveFlag = alive;  // prevent use-after-free on plugin close
    juce::MessageManager::callAsync([this, sysexCopy, aliveFlag]()
    {
        if (!*aliveFlag) return;
#if JUCE_DEBUG
        // Hex-dumping every incoming SysEx is expensive (string building +
        // synchronous console writes) and noticeably slows patch fetches.
        // Opt in with NME_MIDI_LOG=1 when debugging protocol issues.
        static const bool midiLogEnabled = (std::getenv("NME_MIDI_LOG") != nullptr);
        if (midiLogEnabled)
        {
            // Suppress logging for high-frequency NMInfo messages (Lights=0x39, Meters=0x3a, VoiceCount=0x05)
            bool suppress = false;
            if (sysexCopy->size() >= 6)
            {
                int cc = ((*sysexCopy)[2] >> 2) & 0x1F;
                uint8_t sc = (*sysexCopy)[5];
                if (cc == 0x14 && (sc == 0x39 || sc == 0x3a || sc == 0x05))
                    suppress = true;
            }
            if (!suppress)
            {
                juce::String hex;
                for (auto b : *sysexCopy)
                    hex += juce::String::toHexString(b).paddedLeft('0', 2) + " ";
                DBG("RX SysEx [" + juce::String(sysexCopy->size()) + "]: " + hex.trimEnd());
            }
        }
#endif
        protocol.processIncoming(sysexCopy->data(), sysexCopy->size());
    });
}
