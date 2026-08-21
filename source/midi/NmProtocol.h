#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "SysExCodec.h"
#include "NmMessages.h"
#include <vector>
#include <deque>
#include <functional>

class NmProtocol : public juce::Timer
{
public:
    NmProtocol();
    ~NmProtocol() override;

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onIAmReceived(const IAmMessage&) {}
        virtual void onParameterChanged(const ParameterChangeMessage&) {}
        virtual void onAckReceived(const AckMessage&) {}
        virtual void onPatchListReceived(const AckMessage&) {}  // ACK with type=0x13/0x15
        virtual void onPatchReceived(const PatchMessage&) {}
        virtual void onNMInfoReceived(const NMInfoMessage&) {}
        virtual void onPatchPacketReceived(const PatchPacketMessage&) {}
        virtual void onError(const ErrorMessage&) {}
        virtual void onConnectionChanged(bool /*connected*/) {}
        // Every message the synth sends, by cc, before the typed callbacks
        // above. For anything that only needs to know the synth is there and
        // talking, whatever it is saying.
        virtual void onSynthMessage(int /*cc*/) {}
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Enqueue a message for sending
    void sendMessage(int cc, int slot, const std::vector<uint8_t>& payload,
                     bool expectsReply = false, bool addChecksum = true);

    // Feed incoming SysEx data (called from MIDI input handler)
    void processIncoming(const uint8_t* data, size_t length);

    // Set the function used to actually send SysEx bytes out
    void setSendFunction(std::function<void(const std::vector<uint8_t>&)> fn);

    bool isWaitingForReply() const { return waitingForReply; }

    static constexpr int timeoutMs = 3000;
    static constexpr int heartbeatIntervalMs = 50;

private:
    void timerCallback() override;
    void flushSendQueue();
    void dispatchMessage(const SysEx::DecodedMessage& msg);

    struct PendingMessage
    {
        std::vector<uint8_t> encoded;
        bool expectsReply = false;
    };

    std::deque<PendingMessage> sendQueue;
    bool waitingForReply = false;
    juce::int64 lastSendTime = 0;

    std::function<void(const std::vector<uint8_t>&)> sendFn;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NmProtocol)
};
