#include "NmProtocol.h"

NmProtocol::NmProtocol()
{
    startTimer(heartbeatIntervalMs);
}

NmProtocol::~NmProtocol()
{
    stopTimer();
}

void NmProtocol::addListener(Listener* listener)
{
    listeners.add(listener);
}

void NmProtocol::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void NmProtocol::sendMessage(int cc, int slot, const std::vector<uint8_t>& payload,
                             bool expectsReply, bool addChecksum)
{
    auto encoded = SysEx::encode(cc, slot, payload, addChecksum);
    sendQueue.push_back({ std::move(encoded), expectsReply });

    // Send right away instead of waiting for the next timer tick. The original
    // editor's ProtocolRunner thread drains its queue as soon as a message is
    // enqueued; pacing knob moves at one message per tick adds audible lag.
    flushSendQueue();
}

void NmProtocol::processIncoming(const uint8_t* data, size_t length)
{
    auto msg = SysEx::decode(data, length);
    if (msg.valid)
    {
        // Only genuine replies unblock the queue (jnmprotocol isReply()):
        // streamed NMInfo traffic (meters/lights/voicecount) and panel knob
        // ParameterChange messages arrive unsolicited and must not release a
        // request that is still waiting for its answer.
        if (msg.cc != NmCmd::ParameterChange && msg.cc != NmCmd::NMInfo)
            waitingForReply = false;

        dispatchMessage(msg);

        // The reply we were blocking on has arrived; resume sending without
        // waiting up to a full timer tick.
        flushSendQueue();
    }
}

void NmProtocol::setSendFunction(std::function<void(const std::vector<uint8_t>&)> fn)
{
    sendFn = std::move(fn);
}

void NmProtocol::timerCallback()
{
    // Check for timeout
    if (waitingForReply)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSendTime > timeoutMs)
        {
            DBG("NmProtocol: reply timeout");
            waitingForReply = false;
            // Could notify listeners of timeout here
        }
        else
        {
            return;
        }
    }

    flushSendQueue();
}

void NmProtocol::flushSendQueue()
{
    if (!sendFn)
        return;

    // Drain until empty or a message that blocks on a reply has been sent.
    while (!waitingForReply && !sendQueue.empty())
    {
        auto pending = std::move(sendQueue.front());
        sendQueue.pop_front();

        // Debug: log outgoing message
        juce::String hexDump;
        for (size_t i = 0; i < juce::jmin(pending.encoded.size(), size_t(20)); ++i)
            hexDump += juce::String::toHexString(pending.encoded[i]).paddedLeft('0', 2) + " ";
        if (pending.encoded.size() > 20)
            hexDump += "...";
        DBG("NmProtocol SEND: " + hexDump + " (size=" + juce::String(pending.encoded.size()) + ")");

        if (pending.expectsReply)
        {
            waitingForReply = true;
            lastSendTime = juce::Time::getMillisecondCounter();
        }

        sendFn(pending.encoded);
    }
}

void NmProtocol::dispatchMessage(const SysEx::DecodedMessage& msg)
{
    listeners.call([&](Listener& l) { l.onSynthMessage(msg.cc); });

    // Message type is identified by cc field in the SysEx header, not payload
    switch (msg.cc)
    {
        case NmCmd::IAm:
        {
            auto iam = IAmMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onIAmReceived(iam); });
            break;
        }
        case NmCmd::ParameterChange:
        {
            auto param = ParameterChangeMessage::decode(msg.payload.data(), msg.payload.size());
            if (param.pid >= 0)  // Only dispatch valid ParameterChange (sc=0x40), skip ParameterSelect etc.
                listeners.call([&](Listener& l) { l.onParameterChanged(param); });
            break;
        }
        case NmCmd::NMInfo:
        {
            auto info = NMInfoMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onNMInfoReceived(info); });
            break;
        }
        case NmCmd::ACK:
        {
            auto ack = AckMessage::decode(msg.payload.data(), msg.payload.size());

            // Check if this is a PatchListResponse (type 0x13 or 0x15)
            if (ack.type == 0x13 || ack.type == 0x15)
            {
                listeners.call([&](Listener& l) { l.onPatchListReceived(ack); });
            }
            else
            {
                listeners.call([&](Listener& l) { l.onAckReceived(ack); });
            }
            break;
        }
        case NmCmd::PatchHandling:
        {
            auto patch = PatchMessage::decode(msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onPatchReceived(patch); });
            break;
        }
        case 0x1c: case 0x1d: case 0x1e: case 0x1f:  // PatchPacket
        {
            auto pkt = PatchPacketMessage::decode(msg.cc, msg.payload.data(), msg.payload.size());
            listeners.call([&](Listener& l) { l.onPatchPacketReceived(pkt); });
            break;
        }
        default:
            DBG("NmProtocol: unknown cc 0x" + juce::String::toHexString(msg.cc));
            break;
    }
}
