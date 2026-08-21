#include "ConnectionManager.h"
#include "../protocol/StorePatchMessage.h"
#include "../protocol/DeletePatchMessage.h"
#include "../protocol/SetPatchTitleMessage.h"
#include "../protocol/SetModuleTitleMessage.h"
#include "../protocol/SendControllerSnapshotMessage.h"
#include "../model/PatchSerializer.h"
#include "../model/Patch.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>

namespace
{
class Midi7BitReader
{
public:
    explicit Midi7BitReader(const std::vector<uint8_t>& bytesIn) : bytes(bytesIn) {}

    bool readBits(int count, int& value)
    {
        value = 0;
        for (int i = 0; i < count; ++i)
        {
            if (byteIndex >= bytes.size())
                return false;

            value = (value << 1) | ((bytes[byteIndex] >> bitIndex) & 0x01);
            if (--bitIndex < 0)
            {
                bitIndex = 6;
                ++byteIndex;
            }
        }
        return true;
    }

private:
    const std::vector<uint8_t>& bytes;
    size_t byteIndex = 0;
    int bitIndex = 6;
};

const char* pdlSectionName(int type)
{
    switch (type)
    {
        case 33:  return "Header";
        case 55:  return "PatchName";
        case 74:  return "ModuleDump";
        case 77:  return "ParameterDump";
        case 82:  return "CableDump";
        case 90:  return "NameDump";
        case 91:  return "CustomDump";
        case 96:  return "ControlMapDump";
        case 98:  return "KnobMapDump";
        case 101: return "MorphMap";
        case 105: return "NoteDump";
        default:  return "Unknown";
    }
}

const char* synthErrorName(int code)
{
    switch (code)
    {
        case 3:  return "non-fatal patch message warning";
        case 4:  return "checksum error";
        case 5:  return "no slot focused";
        case 6:  return "non-fatal patch message warning";
        default: return "unknown";
    }
}

std::string describePdlSection(const std::vector<uint8_t>& section)
{
    Midi7BitReader reader(section);
    int type = -1;
    if (!reader.readBits(8, type))
        return "type=-1 (Unknown)";

    std::ostringstream out;
    out << "type=" << type << " (" << pdlSectionName(type) << ")";

    if (type == 77)
    {
        int pdlSection = 0;
        int moduleCount = 0;
        int moduleIndex = 0;
        int moduleType = 0;
        if (reader.readBits(1, pdlSection) && reader.readBits(7, moduleCount))
        {
            out << " pdlSection=" << pdlSection << " modules=" << moduleCount;
            if (moduleCount > 0 && reader.readBits(7, moduleIndex) && reader.readBits(7, moduleType))
                out << " firstModule=" << moduleIndex << " firstType=" << moduleType;
        }
    }

    return out.str();
}

// Same description, read from a section that has not been 7-bit encoded yet —
// what the upload path works with, since it encodes per packet rather than per
// section.
std::string describeRawSection(const std::vector<uint8_t>& section)
{
    auto bitsAt = [&section](size_t bitPos, int width) -> int
    {
        int value = 0;
        for (int i = 0; i < width; ++i)
        {
            size_t p = bitPos + static_cast<size_t>(i);
            size_t byteIndex = p / 8;
            if (byteIndex >= section.size())
                return -1;
            int bit = (section[byteIndex] >> (7 - static_cast<int>(p % 8))) & 1;
            value = (value << 1) | bit;
        }
        return value;
    };

    if (section.empty())
        return "type=-1 (Unknown)";

    const int type = section[0];
    std::ostringstream out;
    out << "type=" << type << " (" << pdlSectionName(type) << ")";

    if (type == 77)
    {
        const int pdlSection  = bitsAt(8, 1);
        const int moduleCount = bitsAt(9, 7);
        if (pdlSection >= 0 && moduleCount >= 0)
        {
            out << " pdlSection=" << pdlSection << " modules=" << moduleCount;
            const int moduleIndex = bitsAt(16, 7);
            const int moduleType  = bitsAt(23, 7);
            if (moduleCount > 0 && moduleIndex >= 0 && moduleType >= 0)
                out << " firstModule=" << moduleIndex << " firstType=" << moduleType;
        }
    }

    return out.str();
}

// Identify which GetPatch request a completed PatchPacket entry answers, so a
// stalled fetch can re-request exactly the missing sections. Entries are
// 7-bit encoded and start at the PDL2 type field; area-scoped dumps carry a
// poly/common bit right after the type. Returns a GetPatchMessage::Section
// index, or -1 if the entry cannot be identified.
int classifyGetPatchReply(const std::vector<uint8_t>& entry)
{
    Midi7BitReader reader(entry);
    int type = -1;
    if (!reader.readBits(8, type))
        return -1;

    auto areaSplit = [&reader](GetPatchMessage::Section poly, GetPatchMessage::Section common)
    {
        int area = 0;
        if (!reader.readBits(1, area))
            return -1;
        return static_cast<int>(area != 0 ? poly : common);
    };

    switch (type)
    {
        case 33: case 55: case 39:  // Header, PatchName, PatchName2
            return GetPatchMessage::Header;
        case 74:  return areaSplit(GetPatchMessage::PolyModule,    GetPatchMessage::CommonModule);
        case 82:  return areaSplit(GetPatchMessage::PolyCable,     GetPatchMessage::CommonCable);
        case 77:  return areaSplit(GetPatchMessage::PolyParameter, GetPatchMessage::CommonParameter);
        case 101: return GetPatchMessage::MorphMap;
        case 98:  return GetPatchMessage::KnobMap;
        case 96:  return GetPatchMessage::ControlMap;
        case 91: case 90:  // CustomDump rides in front of NameDump in the same reply
            return areaSplit(GetPatchMessage::PolyNameDump, GetPatchMessage::CommonNameDump);
        case 105: return GetPatchMessage::Note;
        default:  return -1;
    }
}
}

ConnectionManager::ConnectionManager()
{
    protocol.addListener(this);
}

ConnectionManager::~ConnectionManager()
{
    *alive = false;   // Cancel any pending Timer::callAfterDelay lambdas
    protocol.stopTimer();
    protocol.removeListener(this);
    disconnect();
}

bool ConnectionManager::connect(const juce::String& inputId, const juce::String& outputId)
{
    disconnect();

    midiDevice = std::make_unique<MidiDeviceManager>(protocol);

    if (!midiDevice->connect(inputId, outputId))
    {
        midiDevice.reset();
        setStatus(State::Disconnected, "Failed to open MIDI ports");
        return false;
    }

    setStatus(State::Connecting, "Connecting...");
    sendHandshake();
    startHandshakeTimeout();
    return true;
}

// Say hello: sender=0 means PC, version 3.3. IAm carries no checksum per the
// PDL2 spec.
void ConnectionManager::sendHandshake()
{
    IAmMessage iam;
    iam.sender = 0;
    iam.versionHigh = 3;
    iam.versionLow = 3;
    lastHandshakeMs = juce::Time::getMillisecondCounter();
    protocol.sendMessage(NmCmd::IAm, 0, iam.encode(), /*expectsReply=*/true, /*addChecksum=*/false);
}

// The synth said something while we are not connected to it.
//
// The only route from Disconnected to Connected without the user opening MIDI
// Settings is an IAm from the synth, and a G1 does not reliably volunteer one:
// it answers the hello it is sent. Start the editor with the synth switched
// off and that hello goes out into a dead cable, so switching the synth on
// afterwards left the editor sitting at "No response from synth" with the
// synth's own chatter arriving all the while (issue #73).
//
// Anything it sends of its own accord - a light frame, the slot mask, the
// voice counts - is proof that it is there and listening, so say hello again.
// Rate-limited to one attempt per reply timeout so a stream of light frames
// cannot turn into a stream of handshakes, and stopped for good once the
// connection is up, where the IAm answer itself is the thing that changes the
// state.
void ConnectionManager::onSynthMessage(int cc)
{
    if (cc == NmCmd::IAm || status.state == State::Connected || midiDevice == nullptr)
        return;

    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastHandshakeMs < static_cast<juce::uint32>(NmProtocol::timeoutMs))
        return;

    std::cout << "[SYNTH] Heard the synth while disconnected, saying hello again" << std::endl;
    setStatus(State::Connecting, "Synth detected, connecting...");
    sendHandshake();
    startHandshakeTimeout();
}

void ConnectionManager::disconnect()
{
    cancelHandshakeTimeout();
    invalidateParamQueue("disconnect");
    collectingSections = false;
    pendingBankLoadSlot = -1;
    pendingBankLoadGeneration++;
    slotDetected = false;
    slotDetectGeneration++;
    // Slot state can change while we're away — relearn it on reconnect
    slotEnableMaskKnown = false;
    slotEnabled.fill(false);
    slotPinned.fill(false);
    slotPatchIds.fill(0);
    slotModelDelivered.fill(false);
    patchListInterruptedByFetch = false;
    slotPrefetchQueue.clear();
    backgroundPrefetchSlot = -1;

    // An upload still in flight has to be closed before the port goes, or the
    // synth stays parked waiting for the rest of a transfer that will never
    // arrive and answers no MIDI at all afterwards (issue #40).
    if (waitingForUploadAck)
    {
        ++uploadAckGeneration;
        closeUploadTransfer("disconnect");
        waitingForUploadAck = false;
        uploadPackets.clear();
        uploadPacketIndex = 0;
    }

    if (midiDevice)
    {
        midiDevice->disconnect();
        midiDevice.reset();
    }

    setStatus(State::Disconnected, "Disconnected");
}

juce::Array<juce::MidiDeviceInfo> ConnectionManager::getAvailableInputDevices()
{
    return MidiDeviceManager::getAvailableInputDevices();
}

juce::Array<juce::MidiDeviceInfo> ConnectionManager::getAvailableOutputDevices()
{
    return MidiDeviceManager::getAvailableOutputDevices();
}

void ConnectionManager::onIAmReceived(const IAmMessage& msg)
{
    // sender=1 means the synth is responding
    if (msg.sender == 1)
    {
        // The G1 does not only answer an IAm, it announces itself, over and
        // over: the dumps on issue #73 settle into one every three seconds for
        // as long as the synth is on, with nothing asked of it. Every one of
        // them used to be read as a connection that had just come up and re-ran
        // the whole opening sequence: patch list, synth settings, and a patch
        // fetch for the slot on screen. That is the "loading patch 1/13" loop
        // in the report, and reloading the patch under the user is what threw
        // the canvas back to its top-left corner while they were working in it.
        //
        // The original editor ignores this message outright outside its own
        // connect(). We keep listening while disconnected, so a synth switched
        // on after the editor still connects on its own, and ignore it once
        // there is nothing left to learn from it.
        // Printed rather than DBG'd: the announcements are the one thing that
        // tells a unit doing this apart from one that is not, and the console
        // is where a bug report's evidence comes from.
        if (status.state == State::Connected)
        {
            std::cout << "[SYNTH] IAm announcement from a synth already connected"
                         " (v" << msg.versionHigh << "." << msg.versionLow
                      << ", serial " << msg.serial << "), ignoring" << std::endl;
            return;
        }

        cancelHandshakeTimeout();

        status.synthVersionHigh = msg.versionHigh;
        status.synthVersionLow = msg.versionLow;

        setStatus(State::Connected,
                  "Connected: Nord Modular v" +
                  juce::String(msg.versionHigh) + "." +
                  juce::String(msg.versionLow));

        // Start fallback timer: if synth doesn't send SlotActivated within 3s,
        // default to slot 0
        startSlotDetectionFallback();
    }
}

void ConnectionManager::onParameterChanged(const ParameterChangeMessage& msg)
{
    // Synth notifies us of a parameter change (user turned a knob on the hardware)
    if (parameterChangeCallback)
        parameterChangeCallback(msg.section, msg.module, msg.parameter, msg.value);
}

void ConnectionManager::requestPatch(int slot)
{
    // A request the user or a slot switch asked for starts with a full retry
    // budget; retryPatchRequest() re-enters below it.
    patchRequestAttemptsLeft = maxPatchRequestAttempts;
    sendPatchRequest(slot);
}

void ConnectionManager::sendPatchRequest(int slot)
{
    if (!isConnected())
        return;

    --patchRequestAttemptsLeft;
    if (slot >= 0 && slot < 4)
        autoFetchPending[static_cast<size_t>(slot)] = false;

    // Unconditional (not gated behind DBG/JUCE_DEBUG) so it always marks a
    // fetch boundary in the console — makes it easy to isolate one patch
    // load's log lines when copying a session's output for a bug report.
    std::cout << "===== LOAD PATCH: slot=" << static_cast<char>('A' + (slot & 0x03))
              << " source=synth-fetch =====" << std::endl;

    // Until this fetch delivers, the editor's model for the slot (if any)
    // can no longer be assumed to match the synth.
    if (slot >= 0 && slot < 4)
        slotModelDelivered[static_cast<size_t>(slot)] = false;

    // If this fetch isn't the background-prefetch queue continuing itself
    // (continueSlotPrefetchQueue sets backgroundPrefetchSlot to this exact
    // slot right before calling requestPatch), some other caller — a manual
    // reload, a genuine slot activation — has taken over the fetch pipeline.
    // Leaving the stale flag set let a later SlotActivated mistake this
    // unrelated fetch for our own abortable prefetch and corrupt it (found
    // in code review).
    if (backgroundPrefetchSlot != slot)
        backgroundPrefetchSlot = -1;

    // Any queued values describe the patch that was active before this fetch.
    // They must never land on the patch that is about to replace it — but
    // only for THIS slot, other slots' queued edits are unaffected.
    invalidateParamQueue("patch request", slot);

    // A patch fetch must not interleave with the 891-message patch list
    // stream — the G1 firmware freezes on colliding request streams.
    cancelPatchListFetch("patch request supersedes it");

    // Cancel any pending edit queue — we're about to reload from synth
    if (!ackedQueue.empty() || ackedQueueWaiting)
    {
            ackedQueue.clear();
            ackedQueueWaiting = false;
            ackedQueueWaitingAllowsNewPatchInSlot = false;
        ++ackedQueueGeneration;
    }

    // Reset any in-progress request (new request supersedes old one)
    waitingForPatchAck = true;
    collectingSections = false;
    pendingPatchSlot = slot;
    patchPacketsReceived = 0;
    sectionAccumulator.clear();
    patchSections.clear();
    sectionsReceived = 0;
    sectionSeen.fill(false);
    fetchPatchId = -1;
    sectionRetriesLeft = maxSectionRetries;
    patchTimeoutGeneration++;  // Invalidate any pending timeout

    RequestPatchMessage req;
    req.slot = slot;
    auto payload = req.encode();
    protocol.sendMessage(NmCmd::PatchHandling, slot, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    if (patchLoadProgressCallback)
        patchLoadProgressCallback(0, totalSections);

    // Self-heal if the synth never ACKs (busy loading a patch from the front
    // panel, for example). Without this, waitingForPatchAck stays true forever
    // and every future NewPatchInSlot auto-fetch is skipped — the editor goes
    // deaf and looks disconnected even though MIDI is fine.
    //
    // Resetting the flag alone was not enough: a synth still writing a large
    // patch into a slot drops the request, and the editor then sat on the
    // previous patch with nothing to prompt it to ask again — which is exactly
    // what a bank load of a big patch looked like (issue #41). Ask again
    // instead, a few times, spaced far enough apart for the synth to finish.
    const int gen = patchTimeoutGeneration;
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(3000, [this, gen, slot, aliveFlag]() {
        if (!*aliveFlag) return;
        if (gen != patchTimeoutGeneration || !waitingForPatchAck || collectingSections)
            return;

        waitingForPatchAck = false;

        if (patchRequestAttemptsLeft > 0)
        {
            std::cout << "[PATCH] No ACK for patch request (slot " << slot
                      << ") after 3s - retrying (" << patchRequestAttemptsLeft
                      << " attempts left)" << std::endl;
            sendPatchRequest(slot);
            return;
        }

        std::cout << "[PATCH] No ACK for patch request (slot " << slot
                  << ") after " << maxPatchRequestAttempts
                  << " attempts - giving up" << std::endl;
        setStatus(State::Connected,
                  "Synth did not answer the patch request for slot "
                      + juce::String::charToString(static_cast<juce::juce_wchar>('A' + (slot & 0x03))));
    });

    DBG("Requesting patch from slot " + juce::String(slot));
}

int ConnectionManager::getCurrentSlot() const
{
    return currentSlot;
}

void ConnectionManager::selectSlot(int slot)
{
    if (!isConnected() || slot < 0 || slot > 3)
        return;

    // Selecting a slot invalidates nothing. Queued edits are keyed by slot,
    // drainParamQueue holds per slot, and every send carries its slot in the
    // SysEx envelope, so an edit queued for A stays valid while the synth shows
    // B. This used to discard the whole queue, which silently threw away edits
    // that were still in flight. What does invalidate a slot's queue is its
    // patch being replaced, and those calls already pass the slot ("patch
    // request", "bank patch load").

    // Emulate the panel rule for a plain slot-button press: enable state is
    // sticky for pinned slots and follows the selection otherwise, so the
    // mask is always pinned + newly selected slot. The slot being left drops
    // out only if it wasn't pinned. Never guess: without the synth's real
    // state we'd disable pinned slots, so in that case send only the
    // selection and ask for the state.
    // libnmProtocol sends slot-management commands with SysEx header slot 0;
    // the target slot lives in the payload.
    currentSlot = slot;
    currentPatchId = slotPatchIds[static_cast<size_t>(slot)];

    if (slotEnableMaskKnown)
        sendSlotMask();
    else
        requestSynthSettings();

    std::vector<uint8_t> activePayload;
    activePayload.push_back(0x41);  // pid = PatchManagerCommand
    activePayload.push_back(0x09);  // sc = SlotActivated
    activePayload.push_back(static_cast<uint8_t>(slot & 0x7F));
    protocol.sendMessage(NmCmd::PatchHandling, 0, activePayload,
                         /*expectsReply=*/true, /*addChecksum=*/true);

    std::cout << "[SLOT] Sent " << (slotEnableMaskKnown ? "SlotsSelected + " : "")
              << "SlotActivated: " << slot << std::endl;
}

void ConnectionManager::sendSlotMask()
{
    // Enable mask = pinned slots + the selected slot (always enabled)
    uint8_t mask = 0;
    for (int i = 0; i < 4; ++i)
        if (slotPinned[static_cast<size_t>(i)] || i == currentSlot)
            mask |= static_cast<uint8_t>(1 << (3 - i));

    std::vector<uint8_t> payload;
    payload.push_back(0x41);  // pid = PatchManagerCommand
    payload.push_back(0x07);  // sc = SlotsSelected
    payload.push_back(mask);
    protocol.sendMessage(NmCmd::PatchHandling, 0, payload,
                         /*expectsReply=*/true, /*addChecksum=*/true);
}

void ConnectionManager::updateSlotPinsFromMask(int selectedSlot)
{
    // Derive pin state from a synth-reported enable mask. An enabled,
    // non-selected slot is necessarily pinned; a disabled slot necessarily
    // isn't. For the selected slot the mask can't tell pinned from
    // enabled-by-selection, so our current belief is kept — it converges as
    // soon as the selection moves.
    for (int i = 0; i < 4; ++i)
    {
        if (!slotEnabled[static_cast<size_t>(i)])
            slotPinned[static_cast<size_t>(i)] = false;
        else if (i != selectedSlot)
            slotPinned[static_cast<size_t>(i)] = true;
    }
}

void ConnectionManager::setSlotEnabled(int slot, bool enabled)
{
    if (!isConnected() || slot < 0 || slot > 3)
        return;

    // Never guess the mask: with unknown state a toggle would send a
    // single-bit mask and disable every other enabled slot on the synth.
    if (!slotEnableMaskKnown)
    {
        std::cout << "[SLOT] Enable mask unknown, requesting synth settings first" << std::endl;
        requestSynthSettings();
        return;
    }

    // Ctrl+click pins/unpins the slot (Shift+button on the panel): pinned
    // slots stay enabled when the selection moves elsewhere. Send the full
    // resulting mask; the local enabled state is updated when the synth
    // confirms with its own SlotsSelected notification.
    slotPinned[static_cast<size_t>(slot)] = enabled;
    sendSlotMask();

    std::cout << "[SLOT] Sent SlotsSelected (slot " << slot
              << (enabled ? " pinned" : " unpinned") << ")" << std::endl;
}

void ConnectionManager::loadPatchFromBank(int section, int position, int targetSlot)
{
    if (!isConnected())
        return;

    std::cout << "===== LOAD PATCH: slot=" << static_cast<char>('A' + ((targetSlot >= 0 ? targetSlot : currentSlot) & 0x03))
              << " source=bank(section=" << (section + 1) << ",pos=" << (position + 1)
              << ") =====" << std::endl;

    // Loading while the patch list stream is in flight interleaves two
    // request/response streams and freezes the G1 — cancel the list first.
    cancelPatchListFetch("bank load supersedes it");

    int slot = (targetSlot >= 0) ? targetSlot : currentSlot;
    invalidateParamQueue("bank patch load", slot);
    currentSlot = slot;
    currentPatchId = slotPatchIds[static_cast<size_t>(slot & 0x03)];
    pendingPatchSlot = slot;

    // A bank load is never part of the background-prefetch queue (that only
    // ever calls plain requestPatch) — it always takes over the fetch
    // pipeline from whatever the stale flag claimed (found in code review,
    // same class of bug as requestPatch's guard above).
    backgroundPrefetchSlot = -1;

    // A browser load supersedes any patch fetch already in flight. Without this,
    // late packets from the previous request can finish after the double-click
    // and briefly install the previously requested patch into the target slot.
    waitingForPatchAck = false;
    collectingSections = false;
    patchPacketsReceived = 0;
    sectionAccumulator.clear();
    patchSections.clear();
    sectionsReceived = 0;
    sectionSeen.fill(false);
    fetchPatchId = -1;
    sectionRetriesLeft = maxSectionRetries;
    patchTimeoutGeneration++;

    lastLoadedSection = section;
    lastLoadedPosition = position;
    suppressNextLocationClear = true;
    // The slot now holds the patch that lives here, which is where Store to Bank
    // offers to put it back.
    setSlotBankLocation(slot, section, position);

    std::cout << "[LOAD] Loading patch from bank: section=" << section
              << " position=" << position << " to slot=" << slot << std::endl;

    LoadPatchMessage msg;
    msg.slot = slot;
    msg.section = section;
    msg.position = position;
    auto payload = msg.encode();
    protocol.sendMessage(NmCmd::PatchHandling, slot, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    const int loadGeneration = ++pendingBankLoadGeneration;
    pendingBankLoadSlot = slot;

    // Prefer the synth's NewPatchInSlot notification. This fallback covers
    // devices/firmware paths that ACK the load but do not emit sc=0x38.
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(1200, [this, slot, loadGeneration, aliveFlag]() {
        if (!*aliveFlag) return;
        if (!isConnected())
            return;

        if (pendingBankLoadGeneration == loadGeneration && pendingBankLoadSlot == slot)
        {
            std::cout << "[LOAD] NewPatchInSlot fallback for slot=" << slot << std::endl;
            pendingBankLoadSlot = -1;
            if (!waitingForPatchAck && !collectingSections && !waitingForUploadAck)
                requestPatch(slot);
        }
    });
}

// The synth stays in bulk-receive state until it sees a packet flagged `last`.
// An upload that simply stops — rejected section, ACK timeout, disconnect —
// leaves it there, and from then on it answers nothing at all: no ACKs, no
// reply to IAm, not even its idle Lights/VoiceCount stream, so the editor's
// handshake fails on every restart and the synth looks dead (issue #40).
// One empty terminating packet gets it out, no power cycle needed.
void ConnectionManager::closeUploadTransfer(const char* reason)
{
    std::cout << "[UPLOAD] Closing transfer (" << reason
              << ") so the synth leaves bulk-receive state" << std::endl;

    sendRawSysEx(UploadPacketizer::closeTransferFrame(uploadSlot));
}

void ConnectionManager::sendNextUploadPacket()
{
    int total = static_cast<int>(uploadPackets.size());
    if (uploadPacketIndex >= total)
    {
        // All packets sent and ACKed — done
        std::cout << "[UPLOAD] All " << total << " packets sent and ACKed." << std::endl;
        waitingForUploadAck = false;
        // Notify the bank transfer (if one is running) or MainComponent
        if (bankUploadResultCallback)
        {
            auto cb = std::move(bankUploadResultCallback);
            bankUploadResultCallback = nullptr;
            juce::MessageManager::callAsync([cb]() { cb(true); });
        }
        else
        {
            // The editor model IS the patch we just uploaded — slot switches
            // can reuse it without re-fetching. (Bank uploads excluded above:
            // they push disk files, not the editor model.)
            slotModelDelivered[static_cast<size_t>(uploadSlot & 0x03)] = true;

            if (uploadCompleteCallback)
            {
                auto cb = uploadCompleteCallback;
                juce::MessageManager::callAsync([cb]() { cb(); });
            }

            resumePatchListIfInterrupted();
            if (backgroundPrefetchSlot == (uploadSlot & 0x03))
                backgroundPrefetchSlot = -1;
            continueSlotPrefetchQueue();
            serviceDeferredAutoFetch();
        }
        // Suppress the next auto-fetch triggered by NewPatchInSlot (sc=0x38).
        // currentPatch is already authoritative — it IS the patch we just uploaded.
        // Re-fetching would replace it with a synth copy that may not include
        // morph assignments (the synth's working-slot memory may strip them).
        suppressNextAutoFetch = true;
        return;
    }

    auto msg = UploadPacketizer::frame(uploadPackets[static_cast<size_t>(uploadPacketIndex)],
                                       /*isFirst=*/uploadPacketIndex == 0,
                                       /*isLast=*/uploadPacketIndex == total - 1,
                                       uploadSlot);
    const int sentPacket = uploadPacketIndex;
    const int ackGeneration = ++uploadAckGeneration;

    // Log full SysEx for debugging
    std::cout << "[UPLOAD]   packet " << uploadPacketIndex
              << "/" << total << " size=" << msg.size()
              << " raw=" << uploadPackets[static_cast<size_t>(uploadPacketIndex)].data.size()
              << " " << uploadPackets[static_cast<size_t>(uploadPacketIndex)].label
              << " hex:";
    for (size_t k = 0; k < msg.size(); ++k)
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)msg[k];
    std::cout << std::dec << std::endl;

    sendRawSysEx(msg);
    // waitingForUploadAck stays true — onAckReceived will call sendNextUploadPacket
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(uploadAckTimeoutMs, [this, sentPacket, ackGeneration, aliveFlag]() {
        if (!*aliveFlag) return;
        if (waitingForUploadAck && uploadAckGeneration == ackGeneration)
        {
            std::cout << "[UPLOAD] ACK timeout at packet " << sentPacket
                      << "/" << uploadPackets.size()
                      << " " << (uploadPackets.size() > static_cast<size_t>(sentPacket)
                          ? uploadPackets[static_cast<size_t>(sentPacket)].label
                          : "(unknown)")
                      << ", aborting upload" << std::endl;
            waitingForUploadAck = false;
            invalidateParamQueue("upload timeout", uploadSlot);
            closeUploadTransfer("ACK timeout");
            uploadPackets.clear();
            uploadPacketIndex = 0;
            setStatus(State::Connected, "Upload timeout at packet " + juce::String(sentPacket));

            if (bankUploadResultCallback)
            {
                auto cb = std::move(bankUploadResultCallback);
                bankUploadResultCallback = nullptr;
                juce::MessageManager::callAsync([cb]() { cb(false); });
            }
        }
    });
}

void ConnectionManager::uploadPatch(int slot, const Patch& patch)
{
    if (!isConnected())
        return;

    // Not authoritative again until every section is ACKed.
    if (slot >= 0 && slot < 4)
        slotModelDelivered[static_cast<size_t>(slot)] = false;

    // The full upload supersedes all parameter deltas for the previous synth
    // patch — for this slot only, other slots' queued edits are unaffected.
    invalidateParamQueue("full patch upload", slot);

    // Don't interleave the upload with the patch list request stream.
    cancelPatchListFetch("patch upload supersedes it");

    // Cancel any pending edit operations in the ACK queue.
    // The upload sends the complete patch state, making individual
    // DeleteModule/DeleteCable/NewModule messages redundant and potentially
    // conflicting with the upload sequence on the synth.
    if (!ackedQueue.empty() || ackedQueueWaiting)
    {
        std::cout << "[UPLOAD] Clearing edit queue (" << ackedQueue.size()
                  << " pending messages discarded)" << std::endl;
        ackedQueue.clear();
        ackedQueueWaiting = false;
        // Bump generation so any in-flight timeout lambda is invalidated
        ++ackedQueueGeneration;
    }

    // Serialize the patch into individual PDL2 sections in the Java upload order,
    // then lay them end to end and chop the result into packets the synth accepts.
    PatchSerializer serializer;
    auto sections = serializer.serializeForUpload(patch);

    std::vector<std::string> labels;
    labels.reserve(sections.size());
    for (const auto& section : sections)
        labels.push_back(describeRawSection(section));

    uploadPackets = UploadPacketizer::cut(sections, labels);

    uploadSlot = slot;
    uploadPacketIndex = 0;
    ++uploadAckGeneration;

    std::cout << "[UPLOAD] Uploading patch \"" << patch.getName().toStdString()
              << "\" to slot " << slot << " (" << sections.size() << " sections in "
              << uploadPackets.size() << " packets)" << std::endl;

    if (uploadPackets.empty())
        return;

    // Send packets one at a time, waiting for ACK between each (like Java protocol)
    waitingForUploadAck = true;
    sendNextUploadPacket();
}

void ConnectionManager::requestSynthSettings()
{
    if (!isConnected())
        return;

    RequestSynthSettingsMessage msg;
    auto payload = msg.encode();
    protocol.sendMessage(NmCmd::PatchHandling, 0, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    std::cout << "[SYNTH] Requesting synth settings" << std::endl;
}

void ConnectionManager::sendSynthSettings(const SynthSettings& settings)
{
    if (!isConnected())
    {
        DBG("sendSynthSettings: NOT CONNECTED");
        return;
    }

    SynthSettingsMessage msg;
    msg.settings = settings;
    auto payload = msg.encode(currentPatchId);

    const auto slot = juce::jlimit(0, 3, currentSlot);
    auto sysex = SysEx::encode(0x1f, slot, payload, /*addChecksum=*/true);
    sendRawSysEx(sysex);

    std::cout << "[SYNTH] Sent synth settings: name=\"" << settings.name << "\""
              << " slot=" << slot
              << " pid=" << currentPatchId
              << " midiChannels="
              << (settings.midiChannelSlot[0] + 1) << ","
              << (settings.midiChannelSlot[1] + 1) << ","
              << (settings.midiChannelSlot[2] + 1) << ","
              << (settings.midiChannelSlot[3] + 1)
              << std::endl;
    std::cout << "[SYNTH] Sent synth settings sysex:";
    for (auto b : sysex)
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int) b;
    std::cout << std::dec << std::endl;

    juce::Timer::callAfterDelay(250, [this]() {
        if (isConnected())
            requestSynthSettings();
    });
}

void ConnectionManager::sendParameter(int slot, int section, int moduleId, int parameterId, int value)
{
    if (!isConnected())
    {
        DBG("sendParameter: NOT CONNECTED");
        return;
    }

    // Edits made while a full upload is in flight belong to the authoritative
    // editor patch, but must wait until the section stream has completed.
    if (waitingForUploadAck && slot == uploadSlot)
    {
        queueParameter(slot, section, moduleId, parameterId, value);
        return;
    }

    // A fetched patch is about to replace the editor model. Do not interleave a
    // parameter message with that request/response stream or apply an edit to an
    // uncertain patch context.
    if ((waitingForPatchAck || collectingSections) && slot == pendingPatchSlot)
        return;

    ParameterChangeMessage msg;
    msg.pid = getPatchId(slot);
    msg.section = section;
    msg.module = moduleId;
    msg.parameter = parameterId;
    msg.value = value;

    auto payload = msg.encode();

    DBG("sendParameter: slot=" + juce::String(slot)
        + " pid=" + juce::String(msg.pid)
        + " section=" + juce::String(section)
        + " module=" + juce::String(moduleId)
        + " param=" + juce::String(parameterId)
        + " value=" + juce::String(value));

    // Parameter messages use cc=0x13, have checksum, no reply expected.
    // Addressed to the owning slot, not necessarily the hardware-focused one
    // (confirmed on real hardware that the G1 applies it regardless).
    protocol.sendMessage(NmCmd::ParameterChange, slot, payload, /*expectsReply=*/false, /*addChecksum=*/true);
}

void ConnectionManager::queueParameter(int slot, int section, int moduleId, int parameterId, int value)
{
    if (!isConnected()) return;

    // Only the fetched slot's own edits are meaningless (its model is about to be
    // replaced). Another slot's edits must still be queued, matching the slot-scoped
    // guards in sendParameter and the busySlot hold in drainParamQueue.
    if ((waitingForPatchAck || collectingSections) && slot == pendingPatchSlot)
        return;

    // The first item binds this batch to the current patch context. Invalidation
    // clears the map, but the generation check in drainParamQueue is a second line
    // of defence against a transition path forgetting to clear it explicitly.
    if (paramQueue_.empty())
        queuedParamGeneration_ = paramContextGeneration_;

    // Coalesce: a later change to the same parameter overwrites the pending one,
    // so rapid re-auditioning never builds an unbounded backlog. Keying by slot
    // too means two different slots' queued edits (e.g. two open slot windows)
    // can never collide.
    paramQueue_[{ slot, section, moduleId, parameterId }] = value;

    if (!paramQueueTimer_.isTimerRunning())
        paramQueueTimer_.startTimer(paramDrainIntervalMs_);
}

void ConnectionManager::drainParamQueue()
{
    if (!isConnected()) { clearParamQueue(); return; }

    // A fetch/upload in flight must not be interleaved with a parameter send
    // for the SAME slot (the G1 can't handle that), but it says nothing about
    // OTHER slots' queued edits (e.g. from a background sub-window) — those
    // still drain normally. Held entries just wait for the next tick instead
    // of being dropped (found in code review: this used to clear the whole
    // queue for any in-flight fetch/upload, regardless of slot).
    const int busySlot = (waitingForPatchAck || collectingSections) ? pendingPatchSlot
                        : waitingForUploadAck                       ? uploadSlot
                                                                     : -1;

    if (queuedParamGeneration_ != paramContextGeneration_)
    {
        clearParamQueue();
        return;
    }

    int sent = 0;
    for (auto it = paramQueue_.begin(); it != paramQueue_.end() && sent < paramDrainBatch_; )
    {
        if (it->first.slot == busySlot) { ++it; continue; }  // hold, retry next tick
        sendParameter(it->first.slot, it->first.section, it->first.module, it->first.param, it->second);
        it = paramQueue_.erase(it);
        ++sent;
    }

    if (paramQueue_.empty())
        paramQueueTimer_.stopTimer();
}

void ConnectionManager::clearParamQueue()
{
    paramQueue_.clear();
    paramQueueTimer_.stopTimer();
}

void ConnectionManager::invalidateParamQueue(const char* reason, int slot)
{
    if (slot < 0 || slot > 3)
    {
        ++paramContextGeneration_;
        if (!paramQueue_.empty())
            std::cout << "[PARAM] Discarding " << paramQueue_.size()
                      << " queued changes (all slots): " << reason << std::endl;
        clearParamQueue();
        return;
    }

    size_t before = paramQueue_.size();
    for (auto it = paramQueue_.begin(); it != paramQueue_.end(); )
        it = (it->first.slot == slot) ? paramQueue_.erase(it) : std::next(it);

    if (paramQueue_.size() != before)
        std::cout << "[PARAM] Discarding " << (before - paramQueue_.size())
                  << " queued changes for slot " << slot << ": " << reason << std::endl;
    if (paramQueue_.empty())
        paramQueueTimer_.stopTimer();
}

void ConnectionManager::setParamSendRate(int batchPerTick, int intervalMs)
{
    paramDrainBatch_      = juce::jmax(1, batchPerTick);
    paramDrainIntervalMs_ = juce::jlimit(5, 100, intervalMs);

    // Apply the new period immediately if a drain is already in flight.
    if (paramQueueTimer_.isTimerRunning())
        paramQueueTimer_.startTimer(paramDrainIntervalMs_);
}

void ConnectionManager::sendPatchTitle(int slot, const juce::String& title)
{
    if (!isConnected())
    {
        DBG("sendPatchTitle: NOT CONNECTED");
        return;
    }

    const int pid = getPatchId(slot);
    SetPatchTitleMessage msg(slot, pid, title);
    auto sysex = msg.toSysEx(slot);
    sendRawSysEx(sysex);

    DBG("sendPatchTitle: slot=" + juce::String(slot)
        + " pid=" + juce::String(pid)
        + " title=\"" + title + "\"");
}

void ConnectionManager::sendModuleTitle(int slot, int section, int moduleIndex,
                                        const juce::String& title)
{
    if (!isConnected())
    {
        DBG("sendModuleTitle: NOT CONNECTED");
        return;
    }

    const int pid = getPatchId(slot);
    SetModuleTitleMessage msg(pid, section, moduleIndex, title);
    // Queued like the other patch modifications (move/delete) so a rename can't
    // overtake a structural edit that is still waiting for its ACK.
    sendAckedSysEx(msg.toSysEx(slot));

    DBG("sendModuleTitle: slot=" + juce::String(slot)
        + " pid=" + juce::String(pid)
        + " section=" + juce::String(section)
        + " module=" + juce::String(moduleIndex)
        + " title=\"" + title + "\"");
}

void ConnectionManager::sendControllerSnapshot()
{
    if (!isConnected())
        return;

    // Fire-and-forget, matching jnmprotocol (the message does not expect a
    // reply) — the synth answers with a stream of CC messages on its MIDI out.
    SendControllerSnapshotMessage msg(currentPatchId);
    sendRawSysEx(msg.toSysEx(currentSlot));

    std::cout << "[SNAPSHOT] Requested controller snapshot for slot "
              << currentSlot << " (pid=" << currentPatchId << ")" << std::endl;
}

void ConnectionManager::sendRawSysEx(const std::vector<uint8_t>& sysex)
{
    if (!isConnected() || !midiDevice)
        return;

    midiDevice->sendSysEx(sysex);
}

void ConnectionManager::sendNoteOn(int note, int velocity)
{
    sendNoteEvent(note, velocity, true);
}

void ConnectionManager::sendNoteOff(int note)
{
    sendNoteEvent(note, 0, false);
}

void ConnectionManager::sendNoteEvent(int note, int velocity, bool on)
{
    // The editor talks to the synth's PC port, which ignores regular MIDI notes.
    // Note (cc=0x17, sc=0x56) is {onOff, note} with onOff 0=on, 1=off, as
    // captured from the original Clavia editor's keyboard floater (ALSA seq
    // sniff of its Wine MIDI output). No velocity on the wire. NoteEvent
    // (sc=0x41) is incoming-only and rejected with synth error 5.
    juce::ignoreUnused(velocity);

    if (!isConnected() || !midiDevice)
    {
        std::cout << "[KEYS] note event skipped: not connected" << std::endl;
        return;
    }

    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(currentPatchId & 0x7F),
        0x56,
        static_cast<uint8_t>(on ? 0x00 : 0x01),
        static_cast<uint8_t>(note & 0x7F)
    };
    midiDevice->sendSysEx(SysEx::encode(0x17, currentSlot, payload, /*addChecksum=*/true));
}

void ConnectionManager::sendAckedSysEx(const std::vector<uint8_t>& sysex, bool allowNewPatchInSlotReply)
{
    if (!isConnected() || !midiDevice)
        return;

    // Remember which slot the message was built for (header byte 2 carries
    // cc:5|slot:2) so the pid patched in at send time is that slot's pid.
    const int slot = sysex.size() > 2 ? (sysex[2] & 0x03) : currentSlot;
    ackedQueue.push_back({ sysex, allowNewPatchInSlotReply, slot });
    drainAckedQueue();
}

void ConnectionManager::drainAckedQueue()
{
    if (ackedQueueWaiting || ackedQueue.empty())
        return;

    auto msg = ackedQueue.front();
    ackedQueue.pop_front();

    if (msg.allowNewPatchInSlotReply && msg.bytes.size() > 4)
    {
        msg.bytes[4] = static_cast<uint8_t>(slotPatchIds[static_cast<size_t>(msg.slot & 0x03)] & 0x7F);

        if (msg.bytes.size() > 6 && msg.bytes.front() == 0xF0 && msg.bytes.back() == 0xF7)
            msg.bytes[msg.bytes.size() - 2] = SysEx::checksum(msg.bytes.data(), msg.bytes.size() - 2);
    }

    ackedQueueWaiting = true;
    ackedQueueWaitingAllowsNewPatchInSlot = msg.allowNewPatchInSlotReply;
    int generation = ++ackedQueueGeneration;
    midiDevice->sendSysEx(msg.bytes);

    std::cout << "[QUEUE] Sent queued message (gen=" << generation
              << ", " << ackedQueue.size() << " remaining), waiting for ACK" << std::endl;

    // 3-second timeout: if no ACK arrives, unblock the queue.
    // The generation check ensures only the timeout for the *current* message fires.
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(ackedTimeoutMs, [this, generation, aliveFlag]() {
        if (!*aliveFlag) return;
        if (ackedQueueWaiting && ackedQueueGeneration == generation)
        {
            std::cout << "[QUEUE] ACK timeout (gen=" << generation << "), unblocking queue ("
                      << ackedQueue.size() << " pending)" << std::endl;
            ackedQueueWaiting = false;
            ackedQueueWaitingAllowsNewPatchInSlot = false;
            drainAckedQueue();
        }
    });
}

void ConnectionManager::onAckReceived(const AckMessage& msg)
{
    DBG("ACK received: pid1=" + juce::String(msg.pid1)
        + " type=0x" + juce::String::toHexString(msg.type)
        + " pid2=" + juce::String(msg.pid2));

    // PatchLoadResponse (ACK type 0x38): the synth says which bank location it
    // just put into which slot — including loads started from the front panel,
    // which is the only way the editor can learn about those.
    //   pid2 = slot, payload = pid3, unknown, section, position, ...
    if (msg.type == 0x38 && msg.payload.size() >= 4)
    {
        const int slot = msg.pid2 & 0x03;
        const int section = msg.payload[2] & 0x7F;
        const int position = msg.payload[3] & 0x7F;
        if (section < 9 && position < 99)
        {
            setSlotBankLocation(slot, section, position);
            std::cout << "[LOAD] Synth reports slot " << slot << " holds bank location "
                      << ((section + 1) * 100 + position + 1) << std::endl;
            if (bankLocationCallback)
                bankLocationCallback(slot);
        }
    }

    if (waitingForUploadAck)
    {
        // Upload ACKs belong to uploadSlot, which may differ from the focused
        // slot. Classify them before generic focused-slot PID resynchronization.
        slotPatchIds[static_cast<size_t>(uploadSlot & 0x03)] = msg.pid1;
        if (uploadSlot == currentSlot)
            currentPatchId = msg.pid1;
        ++uploadAckGeneration;  // invalidate timeout for the packet just ACKed
        uploadPacketIndex++;
        std::cout << "[UPLOAD] ACK for packet " << (uploadPacketIndex - 1)
                  << ", patchId=" << msg.pid1 << std::endl;
        auto aliveFlag = alive;
        juce::Timer::callAfterDelay(uploadInterSectionDelayMs, [this, aliveFlag]() {
            if (!*aliveFlag) return;
            if (waitingForUploadAck)
                sendNextUploadPacket();  // sends next or completes if all done
        });
        return;
    }

    if (waitingForPatchAck)
    {
        waitingForPatchAck = false;
        collectingSections = true;
        // The pid belongs to the fetched slot, which may differ from the
        // focused slot. Classify it before generic PID resynchronization.
        slotPatchIds[static_cast<size_t>(pendingPatchSlot & 0x03)] = msg.pid1;
        if (pendingPatchSlot == currentSlot)
            currentPatchId = msg.pid1;
        DBG("Patch ACK for slot " + juce::String(pendingPatchSlot)
            + ", patchId=" + juce::String(msg.pid1) + ", sending GetPatch for all 13 sections");
        sendGetPatchMessages(msg.pid1, pendingPatchSlot);
        startPatchTimeout();
        return;
    }

    // Plain ACKs carry the synth's pid for the focused patch. Resync before
    // unblocking queued structural edits, otherwise the next queued message may
    // be sent with the stale pid that caused the previous patch generation.
    if (msg.type == 0x7f && msg.pid1 != currentPatchId)
    {
        std::cout << "[SYNC] Patch id resynced from ACK: " << currentPatchId
                  << " -> " << msg.pid1 << std::endl;
        currentPatchId = msg.pid1;
        slotPatchIds[static_cast<size_t>(currentSlot & 0x03)] = msg.pid1;
    }

    // Unblock the acked queue — any pending edit messages can now be sent
    if (ackedQueueWaiting)
    {
        ackedQueueWaiting = false;
        ackedQueueWaitingAllowsNewPatchInSlot = false;
        std::cout << "[QUEUE] ACK received, unblocking queue ("
                  << ackedQueue.size() << " pending)" << std::endl;
        drainAckedQueue();
    }

}

std::vector<std::pair<int, int>> ConnectionManager::findPatchLocations(const juce::String& name) const
{
    std::vector<std::pair<int, int>> found;
    if (name.isEmpty() || !patchListLoaded)
        return found;

    const auto wanted = name.trim();
    for (size_t i = 0; i < patchListNames.size(); ++i)
    {
        if (patchListNames[i].empty())
            continue;
        if (!juce::String(patchListNames[i]).trim().equalsIgnoreCase(wanted))
            continue;
        found.emplace_back(static_cast<int>(i) / 99, static_cast<int>(i) % 99);
    }
    return found;
}

void ConnectionManager::cancelPatchListFetch(const char* reason)
{
    if (!fetchingPatchList)
        return;

    fetchingPatchList = false;
    patchListGeneration++;  // Invalidate the in-flight request chain
    lastListCancelMs = juce::Time::getMillisecondCounter();
    patchListInterruptedByFetch = true;
    std::cout << "[PATCHLIST] Fetch cancelled: " << reason << std::endl;

    // Deliver what we have so any UI waiting on the list leaves its
    // loading state — resumePatchListIfInterrupted() replaces it with the
    // full list once the interrupting operation finishes.
    if (patchListCallback)
        patchListCallback(patchListNames);
}

void ConnectionManager::resumePatchListIfInterrupted()
{
    if (!patchListInterruptedByFetch)
        return;
    patchListInterruptedByFetch = false;

    if (!isConnected() || fetchingPatchList)
        return;

    std::cout << "[PATCHLIST] Resuming list fetch aborted by a patch operation" << std::endl;
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(listRestartCooldownMs, [this, aliveFlag]() {
        if (*aliveFlag && isConnected() && !fetchingPatchList)
            requestPatchList();
    });
}

void ConnectionManager::startEnabledSlotPrefetch()
{
    slotPrefetchQueue.clear();
    for (int i = 0; i < 4; ++i)
        if (slotEnabled[static_cast<size_t>(i)] && i != currentSlot
            && !slotModelDelivered[static_cast<size_t>(i)])
            slotPrefetchQueue.push_back(i);

    if (slotPrefetchQueue.empty())
        return;

    std::cout << "[SLOT] Background-prefetching " << slotPrefetchQueue.size()
              << " enabled slot(s) after connect" << std::endl;
    continueSlotPrefetchQueue();
}

void ConnectionManager::continueSlotPrefetchQueue()
{
    if (slotPrefetchQueue.empty() || !isConnected())
    {
        slotPrefetchQueue.clear();
        backgroundPrefetchSlot = -1;
        return;
    }

    // Never start over something already using the wire — a focused-slot
    // activation, a manual reload, a bank operation, or the patch-list fetch.
    // Retry shortly instead of colliding with it.
    if (waitingForPatchAck || collectingSections || waitingForUploadAck || fetchingPatchList)
    {
        auto aliveFlag = alive;
        juce::Timer::callAfterDelay(300, [this, aliveFlag]() {
            if (*aliveFlag) continueSlotPrefetchQueue();
        });
        return;
    }

    int slot = slotPrefetchQueue.front();
    slotPrefetchQueue.erase(slotPrefetchQueue.begin());

    if (slot == currentSlot || slotModelDelivered[static_cast<size_t>(slot)])
    {
        // Became the focused slot, or got delivered by something else
        // (a manual browser load, say) while it sat in the queue.
        continueSlotPrefetchQueue();
        return;
    }

    std::cout << "[SLOT] Background-prefetching slot " << static_cast<char>('A' + slot) << std::endl;
    backgroundPrefetchSlot = slot;
    requestPatch(slot);
}

void ConnectionManager::requestPatchList()
{
    std::cout << "[PATCHLIST] requestPatchList called, connected=" << isConnected() << std::endl;

    if (!isConnected())
        return;

    // A fetch is already streaming — starting a second one would interleave
    // two response streams and file names at the wrong bank positions.
    if (fetchingPatchList)
    {
        std::cout << "[PATCHLIST] Already fetching - ignoring duplicate request" << std::endl;
        return;
    }

    // After a cancellation, responses from the old stream may still be in
    // flight. Wait briefly so they land while fetchingPatchList is false and
    // get dropped, instead of being filed under the new fetch's cursor.
    const auto sinceCancel = juce::Time::getMillisecondCounter() - lastListCancelMs;
    if (lastListCancelMs != 0 && sinceCancel < listRestartCooldownMs)
    {
        const int waitMs = static_cast<int>(listRestartCooldownMs - sinceCancel);
        std::cout << "[PATCHLIST] Deferring restart " << waitMs
                  << "ms after cancellation" << std::endl;
        auto aliveFlag = alive;
        juce::Timer::callAfterDelay(waitMs, [this, aliveFlag]() {
            if (*aliveFlag && isConnected() && !fetchingPatchList)
                requestPatchList();
        });
        return;
    }

    // Initialize patch list to 891 empty entries (9 banks × 99 positions)
    patchListNames.clear();
    patchListNames.resize(9 * 99, "");  // All initially empty

    fetchingPatchList = true;
    patchListLoaded = false;
    patchListSection = 0;
    patchListPosition = 0;
    patchListGeneration++;  // Invalidate old timeouts

    std::cout << "[PATCHLIST] Starting request: section=0 position=0" << std::endl;
    DBG("Requesting patch list from synth (891 patches)...");

    // Send first request: section 0, position 0
    GetPatchListMessage msg;
    msg.section = patchListSection;
    msg.position = patchListPosition;
    auto payload = msg.encode();

    std::cout << "[PATCHLIST] Payload (hex): ";
    for (auto byte : payload)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;

    protocol.sendMessage(NmCmd::PatchHandling, 0, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    // Start timeout
    int generation = patchListGeneration;
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(patchListTimeoutMs, [this, generation, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (generation == patchListGeneration && fetchingPatchList)
        {
            std::cout << "[PATCHLIST] TIMEOUT - delivering partial results" << std::endl;
            DBG("Patch list timeout - delivering partial results");
            fetchingPatchList = false;
            patchListLoaded = true;
            if (patchListCallback)
                patchListCallback(patchListNames);
        }
    });
}

void ConnectionManager::onPatchListReceived(const AckMessage& msg)
{
    // The list fetch handles up to 891 responses — per-response logging is
    // expensive enough to slow the whole transfer. Opt in with NME_MIDI_LOG=1.
    static const bool verboseLog = (std::getenv("NME_MIDI_LOG") != nullptr);

    if (!fetchingPatchList)
        return;

    if (verboseLog)
    {
        std::cout << "[PATCHLIST] ACK payload size: " << msg.payload.size() << std::endl;
        std::cout << "[PATCHLIST] ACK payload (hex): ";
        for (size_t i = 0; i < std::min(msg.payload.size(), size_t(20)); ++i)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)msg.payload[i] << " ";
        std::cout << std::dec << std::endl;
    }

    // Parse the PatchListResponse from the ACK payload
    auto response = PatchListResponseMessage::decode(
        msg.payload.data(), msg.payload.size(),
        patchListSection, patchListPosition);

    if (verboseLog)
        std::cout << "[PATCHLIST] Parsed " << response.entries.size()
                  << " entries, nextSection=" << response.nextSection
                  << " nextPosition=" << response.nextPosition << std::endl;

    const int requestLinearIndex = patchListSection * 99 + patchListPosition;
    int nextLinearIndex = requestLinearIndex;

    // Store entries in the flat array
    for (const auto& entry : response.entries)
    {
        int index = entry.section * 99 + entry.position;
        if (index >= 0 && index < static_cast<int>(patchListNames.size()))
        {
            patchListNames[static_cast<size_t>(index)] = entry.name.empty() ? "" : entry.name;
            nextLinearIndex = std::max(nextLinearIndex, index + 1);
            if (verboseLog)
                std::cout << "[PATCHLIST]   Entry: section=" << entry.section
                          << " pos=" << entry.position
                          << " name=\"" << entry.name << "\"" << std::endl;
        }
    }

    DBG("Patch list: received " + juce::String(response.entries.size())
        + " entries from section " + juce::String(patchListSection)
        + " position " + juce::String(patchListPosition));

    // Check if we're done
    if (response.nextSection < 0 || nextLinearIndex >= static_cast<int>(patchListNames.size()))
    {
        std::cout << "[PATCHLIST] COMPLETE! Total entries in array: " << patchListNames.size() << std::endl;
        DBG("Patch list complete!");
        fetchingPatchList = false;
        patchListLoaded = true;
        if (patchListCallback)
            patchListCallback(patchListNames);
        return;
    }

    // Continue with next request. Advance from the highest explicit entry we
    // accepted, matching Nomad's worker behavior and avoiding duplicate page
    // starts when sparse/empty bank positions are encoded in the response.
    if (nextLinearIndex <= requestLinearIndex)
    {
        nextLinearIndex = response.nextSection >= 0
            ? response.nextSection * 99 + response.nextPosition
            : static_cast<int>(patchListNames.size());
    }

    patchListSection = nextLinearIndex / 99;
    patchListPosition = nextLinearIndex % 99;

    if (verboseLog)
        std::cout << "[PATCHLIST] Requesting next: section=" << patchListSection
                  << " position=" << patchListPosition << std::endl;

    GetPatchListMessage nextMsg;
    nextMsg.section = patchListSection;
    nextMsg.position = patchListPosition;
    auto payload = nextMsg.encode();
    protocol.sendMessage(NmCmd::PatchHandling, 0, payload, /*expectsReply=*/true, /*addChecksum=*/true);
}

void ConnectionManager::sendGetPatchMessages(int patchId, int slot)
{
    fetchPatchId = patchId;  // kept for per-section re-requests on a stalled fetch
    auto msgs = GetPatchMessage::forAllSections(patchId);
    for (auto& m : msgs)
    {
        auto payload = m.encode();
        // GetPatch uses PatchModification format (same cc=0x17). Responses
        // come as PatchPacket (cc=0x1c-0x1f). Java GetPatchMessage sets
        // expectsreply=true: each section request must wait for its reply,
        // otherwise the burst overruns the synth and sections are dropped.
        protocol.sendMessage(NmCmd::PatchHandling, slot, payload,
                             /*expectsReply=*/true, /*addChecksum=*/true);
    }
}

void ConnectionManager::startPatchTimeout()
{
    int generation = patchTimeoutGeneration;

    // Hard timeout: absolute max wait for entire patch
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(patchTimeoutMs, [this, generation, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (generation == patchTimeoutGeneration && collectingSections && sectionsReceived < totalSections)
        {
            DBG("Patch hard timeout: received " + juce::String(sectionsReceived) + "/" + juce::String(totalSections)
                + " sections");
            retryMissingSections();
        }
    });
}

void ConnectionManager::startSectionStaleTimeout()
{
    int generation = patchTimeoutGeneration;
    int currentCount = sectionsReceived;

    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(sectionStaleMs, [this, generation, currentCount, aliveFlag]()
    {
        if (!*aliveFlag) return;
        // Fire if no new sections arrived since this timer was started
        if (generation == patchTimeoutGeneration && collectingSections
            && sectionsReceived == currentCount && sectionsReceived > 0
            && sectionsReceived < totalSections)
        {
            DBG("Patch stale timeout: no new sections for " + juce::String(sectionStaleMs) + "ms"
                + " (have " + juce::String(sectionsReceived) + "/" + juce::String(totalSections) + ")");
            retryMissingSections();
        }
    });
}

void ConnectionManager::retryMissingSections()
{
    // A synth running at full DSP load answers GetPatch slowly and can stall
    // mid-fetch. Re-request only the sections that never arrived; installing a
    // partial patch here would silently drop cables/parameters (issue #15).
    std::vector<int> missing;
    for (int i = 0; i < totalSections; ++i)
        if (!sectionSeen[static_cast<size_t>(i)])
            missing.push_back(i);

    if (missing.empty() || fetchPatchId < 0 || sectionRetriesLeft <= 0)
    {
        finalizePatch();
        return;
    }

    --sectionRetriesLeft;
    patchTimeoutGeneration++;  // invalidate the timers of the stalled attempt

    std::cout << "[PATCH] Fetch stalled at " << sectionsReceived << "/" << totalSections
              << ", re-requesting " << missing.size() << " missing sections ("
              << (maxSectionRetries - sectionRetriesLeft) << "/" << maxSectionRetries
              << " retries)" << std::endl;

    for (int s : missing)
    {
        GetPatchMessage m;
        m.pid = fetchPatchId;
        m.section = static_cast<GetPatchMessage::Section>(s);
        protocol.sendMessage(NmCmd::PatchHandling, pendingPatchSlot, m.encode(),
                             /*expectsReply=*/true, /*addChecksum=*/true);
    }

    startPatchTimeout();
    startSectionStaleTimeout();
}

void ConnectionManager::finalizePatch()
{
    collectingSections = false;

    // If we have a partial accumulator (section in progress), discard it
    sectionAccumulator.clear();

    // Retries exhausted and sections still missing: the delivered patch may
    // lack cables/parameters. Warn the user before they edit or save it.
    if (sectionsReceived < totalSections && patchLoadIncompleteCallback)
        patchLoadIncompleteCallback(pendingPatchSlot, sectionsReceived, totalSections);

    if (patchSections.empty())
    {
        DBG("No patch sections to parse");
        return;
    }

    DBG(juce::String(sectionsReceived < totalSections ? "Partial" : "All") + " "
        + juce::String(patchSections.size()) + " sections, invoking parser");

    const int completedSlot = pendingPatchSlot;

    // Note: currentSlot (focus) is deliberately NOT updated here. A patch can
    // arrive for a background slot (NewPatchInSlot on a non-focused slot) and
    // must not hijack where parameter edits are sent. Focus only moves via
    // selectSlot() and the synth's SlotActivated notification.

    if (bankFetchCallback)
    {
        // Bank transfer: sections go to disk, the editor model is untouched.
        auto cb = std::move(bankFetchCallback);
        bankFetchCallback = nullptr;
        cb(patchSections, completedSlot);
    }
    else if (patchDataCallback)
    {
        patchDataCallback(patchSections, completedSlot);

        // A complete delivery means the editor model now mirrors the synth,
        // so slot switches can reuse it without re-fetching. Partial loads
        // stay invalid — the next switch to that slot retries the download.
        if (sectionsReceived >= totalSections && completedSlot >= 0 && completedSlot < 4)
            slotModelDelivered[static_cast<size_t>(completedSlot)] = true;

        resumePatchListIfInterrupted();
        if (backgroundPrefetchSlot == completedSlot)
            backgroundPrefetchSlot = -1;
        continueSlotPrefetchQueue();
    }

    if (patchFetchCompleteCallback)
    {
        auto cb = std::move(patchFetchCompleteCallback);
        patchFetchCompleteCallback = nullptr;
        cb(completedSlot);
    }

    patchSections.clear();
    sectionsReceived = 0;

    serviceDeferredAutoFetch();
}

// Fetch a patch whose NewPatchInSlot notification arrived while the wire was
// already busy. Nothing else would ever ask for it: the notification is not
// repeated, and the editor stays on the slot, so no slot switch comes along to
// notice the model is stale (issue #41).
void ConnectionManager::serviceDeferredAutoFetch()
{
    if (!isConnected() || waitingForPatchAck || collectingSections || waitingForUploadAck)
        return;

    for (int slot = 0; slot < 4; ++slot)
    {
        if (!autoFetchPending[static_cast<size_t>(slot)])
            continue;

        autoFetchPending[static_cast<size_t>(slot)] = false;
        std::cout << "[PATCH] Running deferred fetch for slot "
                  << static_cast<char>('A' + slot) << std::endl;
        requestPatch(slot);
        return;  // one fetch at a time; the next runs when this one finishes
    }
}

void ConnectionManager::onNMInfoReceived(const NMInfoMessage& msg)
{
    // Debug: log all NMInfo subcommands we receive
    if (msg.sc != 0x39 && msg.sc != 0x3a)  // skip Lights and Meters (too spammy)
    {
        DBG("[NMInfo] sc=0x" + juce::String::toHexString(msg.sc) + " data=" + juce::String(static_cast<int>(msg.data.size())) + " bytes");
    }

    if (msg.sc == 0x39 && msg.lightStartIndex >= 0)  // LightMessage
    {
        // The stream mostly re-sends unchanged values; don't wake the UI for those.
        int base = msg.lightStartIndex;
        bool changed = false;
        for (int i = 0; i < 20 && (base + i) < 128; ++i)
        {
            changed = changed || globalLightValues[base + i] != msg.lightValues[i];
            globalLightValues[base + i] = msg.lightValues[i];
        }
        if (changed && lightMeterCallback)
            lightMeterCallback(globalLightValues, globalMeterValues);
    }

    if (msg.sc == 0x3a && msg.meterStartIndex >= 0)  // MeterMessage
    {
        // Store in wire order, matching NOMAD's LightProcessor slot layout:
        // even slot = channel B (first byte of each pair), odd slot = channel A.
        // Which channel a given light reads is decided at the consumer
        // (PatchCanvas::paintLights), same as the Java pair semantics.
        int base = msg.meterStartIndex;
        bool changed = false;
        for (int i = 0; i < 5; ++i)
        {
            if ((base + i*2) < 128)
            {
                changed = changed || globalMeterValues[base + i*2] != msg.meterValuesB[i];
                globalMeterValues[base + i*2] = msg.meterValuesB[i];
            }
            if ((base + i*2+1) < 128)
            {
                changed = changed || globalMeterValues[base + i*2+1] != msg.meterValuesA[i];
                globalMeterValues[base + i*2+1] = msg.meterValuesA[i];
            }
        }
        if (changed && lightMeterCallback)
            lightMeterCallback(globalLightValues, globalMeterValues);
    }

    if (msg.sc == 0x05)  // VoiceCount
    {
        DBG("[DSP] VoiceCount received: " + juce::String(msg.voiceCount[0]) + " "
            + juce::String(msg.voiceCount[1]) + " " + juce::String(msg.voiceCount[2]) + " "
            + juce::String(msg.voiceCount[3]));
        if (voiceCountCallback)
            voiceCountCallback(msg.voiceCount);
    }

    if (msg.sc == 0x38)  // NewPatchInSlot
    {
        DBG("New patch in slot " + juce::String(msg.newPatchSlot) + " pid=" + juce::String(msg.newPatchPid));

        // Update before unblocking queued structural edits; drainAckedQueue()
        // patches each outgoing message's pid from its slot's entry. Only a
        // notification for the focused slot may touch currentPatchId.
        if (msg.newPatchSlot >= 0 && msg.newPatchSlot <= 3)
        {
            slotPatchIds[static_cast<size_t>(msg.newPatchSlot)] = msg.newPatchPid;
            if (msg.newPatchSlot == currentSlot)
                currentPatchId = msg.newPatchPid;

            // A genuine patch change on the synth invalidates our model for
            // that slot. Echoes of our own edits/uploads (the suppress flags
            // below) don't — the model already reflects them.
            const bool ownEcho = pendingSyncEchoes_ > 0 || suppressNewPatchInSlot_
                              || suppressNextAutoFetch || waitingForUploadAck;
            if (!ownEcho)
                slotModelDelivered[static_cast<size_t>(msg.newPatchSlot)] = false;
        }
        else
        {
            currentPatchId = msg.newPatchPid;
        }

        // Structural edit messages such as CableInsert/ModuleInsert are
        // confirmed by NewPatchInSlot on some firmware paths instead of a
        // regular ACK. Treat it as a queue reply so subsequent edit messages
        // are not held until the 3-second timeout.
        if (ackedQueueWaiting && ackedQueueWaitingAllowsNewPatchInSlot)
        {
            ackedQueueWaiting = false;
            ackedQueueWaitingAllowsNewPatchInSlot = false;
            std::cout << "[QUEUE] NewPatchInSlot received, unblocking queue ("
                      << ackedQueue.size() << " pending)" << std::endl;
            drainAckedQueue();
        }

        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        if (msg.newPatchSlot >= 0
            && pendingBankLoadSlot == msg.newPatchSlot
            && isConnected()
            && !waitingForPatchAck
            && !collectingSections
            && !waitingForUploadAck)
        {
            pendingBankLoadSlot = -1;
            pendingBankLoadGeneration++;
            suppressNextLocationClear = false;
            std::cout << "[LOAD] NewPatchInSlot confirmed bank load for slot="
                      << msg.newPatchSlot << std::endl;
            requestPatch(msg.newPatchSlot);
            return;
        }

        // Auto-request the new patch data from the synth — unless suppressed.
        // pendingSyncEchoes_: decremented for each echo from a structural edit we sent.
        // suppressNewPatchInSlot_: set during upload-in-progress.
        // suppressNextAutoFetch: one-shot flag set after upload completes.
        // waitingForUploadAck: upload in progress — don't re-fetch.
        if (pendingSyncEchoes_ > 0)
        {
            pendingSyncEchoes_--;
            std::cout << "[SYNC] Consuming sync echo (remaining: " << pendingSyncEchoes_ << ")" << std::endl;
        }
        else if (suppressNewPatchInSlot_)
        {
            std::cout << "[UPLOAD] Ignoring NewPatchInSlot (upload suppress active)" << std::endl;
        }
        else if (suppressNextAutoFetch)
        {
            suppressNextAutoFetch = false;
            std::cout << "[UPLOAD] Skipping auto-fetch after upload (NewPatchInSlot)" << std::endl;
        }
        else if (isConnected() && !waitingForPatchAck && !collectingSections
                 && !waitingForUploadAck && msg.newPatchSlot >= 0)
        {
            if (suppressNextLocationClear)
                suppressNextLocationClear = false;
            else
            {
                lastLoadedSection = -1;
                lastLoadedPosition = -1;
                // Something else was put in this slot from the front panel, so
                // whatever bank location we had recorded for it is now a lie.
                clearSlotBankLocation(msg.newPatchSlot);
            }
            requestPatch(msg.newPatchSlot);
        }
        else if (waitingForUploadAck)
        {
            std::cout << "[UPLOAD] Ignoring NewPatchInSlot during upload" << std::endl;
        }
        else if (isConnected() && msg.newPatchSlot >= 0)
        {
            // The wire was busy with another fetch. Dropping the notification
            // here left the editor showing the previous patch with nothing to
            // make it ask again (issue #41), so remember it and fetch as soon
            // as the current transfer finishes.
            autoFetchPending[static_cast<size_t>(msg.newPatchSlot & 0x03)] = true;
            std::cout << "[PATCH] NewPatchInSlot for slot "
                      << static_cast<char>('A' + (msg.newPatchSlot & 0x03))
                      << " arrived mid-transfer - fetch deferred" << std::endl;
        }
    }

    if (msg.sc == 0x40 && msg.data.size() >= 4)  // KnobChange: physical knob turned on synth
    {
        // Payload identical to ParameterChange: section, module, parameter, value
        if (parameterChangeCallback)
            parameterChangeCallback(msg.data[0], msg.data[1], msg.data[2], msg.data[3]);
    }

    if (msg.sc == 0x7e)  // Error notification from synth
    {
        int errorCode = msg.data.empty() ? -1 : msg.data[0];
        std::cout << "*** SYNTH ERROR: sc=0x7e code=" << errorCode
                  << " (" << synthErrorName(errorCode) << ")"
                  << " (pid=" << msg.pid << ")" << std::endl;

        if (waitingForUploadAck)
        {
            ++uploadAckGeneration;
            waitingForUploadAck = false;
            invalidateParamQueue("upload rejected", uploadSlot);
            closeUploadTransfer("rejected by synth");
            uploadPackets.clear();
            uploadPacketIndex = 0;
            setStatus(State::Connected,
                      "Upload rejected by synth (code " + juce::String(errorCode)
                          + ": " + juce::String(synthErrorName(errorCode)) + ")");
        }

        if (synthErrorCallback)
        {
            // Capture callback by value so it's safe even if ConnectionManager
            // is destroyed before the async fires.
            auto cb = synthErrorCallback;
            juce::MessageManager::callAsync([cb, errorCode]() { cb(errorCode); });
        }
    }

    if (msg.sc == 0x07 && !msg.data.empty())  // SlotsSelected: enable mask
    {
        // PDL: SlotsSelected := 0:4 slot0:1 slot1:1 slot2:1 slot3:1
        int mask = msg.data[0] & 0x0f;
        for (int i = 0; i < 4; ++i)
            slotEnabled[static_cast<size_t>(i)] = (mask & (1 << (3 - i))) != 0;
        slotEnableMaskKnown = true;
        updateSlotPinsFromMask(currentSlot);

        std::cout << "[SLOT] Enabled slots:";
        for (int i = 0; i < 4; ++i)
            if (slotEnabled[static_cast<size_t>(i)])
                std::cout << " " << static_cast<char>('A' + i);
        if (mask == 0)
            std::cout << " (none)";
        std::cout << std::endl;

        if (slotsEnabledCallback)
            slotsEnabledCallback(slotEnabled);
    }

    if (msg.sc == 0x09 && !msg.data.empty())  // SlotActivated
    {
        int activeSlot = msg.data[0] & 0x03;
        std::cout << "[SLOT] Active slot changed to " << activeSlot << std::endl;

        // Same as selectSlot: the synth moving its own focus says nothing about
        // edits queued for any slot, all of which are slot-addressed.
        currentSlot = activeSlot;
        currentPatchId = slotPatchIds[static_cast<size_t>(activeSlot & 0x03)];
        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        // Slot changed on synth — we don't know the bank location
        lastLoadedSection = -1;
        lastLoadedPosition = -1;
        suppressNextLocationClear = false;

        // Notify UI of slot change
        if (slotChangedCallback)
            slotChangedCallback(activeSlot);

        // Auto-load patch from the active slot (unless already loading or
        // uploading). If the editor already holds a model that matches the
        // synth-side patch, reuse it: switching slots stays instant instead
        // of re-downloading all 13 sections (the NewPatchInSlot handler
        // invalidates the flag when the slot's content genuinely changes).
        if (isConnected() && !waitingForUploadAck)
        {
            const bool needsFetch = !slotModelDelivered[static_cast<size_t>(activeSlot)];

            if (needsFetch && (waitingForPatchAck || collectingSections) && backgroundPrefetchSlot >= 0)
            {
                // The wire is occupied by our own background prefetch (see
                // startEnabledSlotPrefetch), not a manual reload or bank
                // operation — safe to abort it and requeue it: a genuine
                // slot activation always takes priority.
                int interrupted = backgroundPrefetchSlot;
                backgroundPrefetchSlot = -1;
                std::cout << "[SLOT] Focused-slot activation pre-empts background prefetch of slot "
                          << static_cast<char>('A' + interrupted) << std::endl;
                if (std::find(slotPrefetchQueue.begin(), slotPrefetchQueue.end(), interrupted)
                    == slotPrefetchQueue.end())
                    slotPrefetchQueue.push_back(interrupted);
                requestPatch(activeSlot);
            }
            else if (!waitingForPatchAck && !collectingSections)
            {
                if (needsFetch)
                    requestPatch(activeSlot);
                else
                    std::cout << "[SLOT] Reusing in-memory patch for slot " << activeSlot
                              << ", skipping re-fetch" << std::endl;
            }
            // else: busy with something else (manual reload, bank op) — skip,
            // same as before phase 2's background prefetch existed.
        }
    }

    // High-frequency messages sc=0x39 (Lights) and sc=0x3a (Meters) are
    // handled above without logging.
}

void ConnectionManager::onPatchPacketReceived(const PatchPacketMessage& msg)
{
    // In Java protocol, PatchMessage.isreply = true — patch packets unblock the send queue.
    // The synth may respond to some edit commands (NewModule) with a patch confirmation packet.
    if (ackedQueueWaiting)
    {
        ackedQueueWaiting = false;
        std::cout << "[QUEUE] PatchPacket received, unblocking queue ("
                  << ackedQueue.size() << " pending)" << std::endl;
        drainAckedQueue();
    }

    if (!collectingSections && !waitingForPatchAck)
    {
        // Not fetching a patch — this may be the reply to requestSynthSettings().
        // The settings decode is heuristic (matches on a 0x03 type byte), so it
        // must NEVER run while a patch fetch is streaming: a patch section can
        // false-positive, showing a garbled synth name and swallowing the
        // section, which stalls the fetch until the stale timeout.
        SynthSettings settings;
        if (SynthSettingsMessage::decode(msg.patchData, settings))
        {
            std::cout << "[SYNTH] Received synth settings: name=\"" << settings.name << "\"" << std::endl;

            // The extended settings block carries the initial slot state:
            // enable mask (fixed LEDs) and the focused slot (blinking LED).
            if (settings.hasExtended)
            {
                for (int i = 0; i < 4; ++i)
                    slotEnabled[static_cast<size_t>(i)] = settings.slotSelected[i] != 0;
                slotEnableMaskKnown = true;
                updateSlotPinsFromMask(settings.activeSlot);
                if (slotsEnabledCallback)
                    slotsEnabledCallback(slotEnabled);

                // Track focus, but leave slotDetected alone: the initial
                // patch fetch is still driven by SlotActivated/NewPatchInSlot
                // or by the detection fallback (which uses currentSlot).
                if (settings.activeSlot >= 0 && settings.activeSlot <= 3
                    && settings.activeSlot != currentSlot)
                {
                    currentSlot = settings.activeSlot;
                    std::cout << "[SLOT] Focused slot from synth settings: "
                              << static_cast<char>('A' + currentSlot) << std::endl;
                    if (slotChangedCallback)
                        slotChangedCallback(currentSlot);
                }

                // Now that we know which slots are actually populated on the
                // synth, background-fetch the ones we're not focused on too
                // — mirrors the original Nomad editor's connect-time
                // behaviour, so the first switch to any of them is instant.
                startEnabledSlotPrefetch();
            }

            if (synthSettingsCallback)
            {
                auto cb = synthSettingsCallback;
                juce::MessageManager::callAsync([cb, settings]() { cb(settings); });
            }
            return;
        }

        std::cout << "[MIDI] PatchPacket outside patch fetch: first=" << msg.isFirst
                  << " last=" << msg.isLast
                  << " command=" << msg.command
                  << " pid=" << msg.pid
                  << " dataSize=" << msg.patchData.size()
                  << " data:";
        for (size_t i = 0; i < msg.patchData.size(); ++i)
            std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int) msg.patchData[i];
        std::cout << std::dec << std::endl;
        return;  // Not expecting patch data
    }

    if (msg.isFirst)
        sectionAccumulator.clear();

    sectionAccumulator.insert(sectionAccumulator.end(), msg.patchData.begin(), msg.patchData.end());

    if (msg.isLast)
    {
        // A re-requested section can arrive twice when the original reply was
        // merely slow rather than lost — parsing the duplicate would double
        // every cable and parameter in the model, so drop it.
        const int sectionKind = classifyGetPatchReply(sectionAccumulator);
        if (sectionKind >= 0)
        {
            if (sectionSeen[static_cast<size_t>(sectionKind)])
            {
                DBG("Duplicate patch section (kind " + juce::String(sectionKind) + "), dropped");
                sectionAccumulator.clear();
                startSectionStaleTimeout();
                return;
            }
            sectionSeen[static_cast<size_t>(sectionKind)] = true;
        }

        // Store completed section separately (each has independent 7-bit encoding)
        patchSections.push_back(std::move(sectionAccumulator));
        sectionAccumulator.clear();
        sectionsReceived++;

        if (patchLoadProgressCallback)
            patchLoadProgressCallback(sectionsReceived, totalSections);

        DBG("Received section " + juce::String(sectionsReceived) + "/" + juce::String(totalSections)
            + " (" + juce::String(patchSections.back().size()) + " bytes)");

        if (sectionsReceived >= totalSections)
        {
            DBG("All " + juce::String(totalSections) + " sections received");
            finalizePatch();
        }
        else
        {
            // Reset the stale timer — if no more sections arrive within sectionStaleMs, finalize
            startSectionStaleTimeout();
        }
    }
}

void ConnectionManager::onError(const ErrorMessage& msg)
{
    // ErrorMessage.isreply = true in Java — also unblocks the send queue
    if (ackedQueueWaiting)
    {
        std::cout << "[QUEUE] Error from synth (code " << msg.errorCode
                  << "), unblocking queue (" << ackedQueue.size() << " pending)" << std::endl;
        ackedQueueWaiting = false;
        drainAckedQueue();
    }
    setStatus(State::Disconnected,
              "Error from synth (code " + juce::String(msg.errorCode) + ")");
}

void ConnectionManager::setStatus(State state, const juce::String& message)
{
    status.state = state;
    status.message = message;

    if (statusCallback)
        statusCallback(status);
}

void ConnectionManager::startHandshakeTimeout()
{
    // Use a Timer via callAfterDelay for the 3-second handshake timeout
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(NmProtocol::timeoutMs, [this, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (status.state == State::Connecting)
        {
            DBG("ConnectionManager: handshake timeout");
            setStatus(State::Disconnected, "No response from synth (timeout)");
        }
    });
}

void ConnectionManager::cancelHandshakeTimeout()
{
    // The callAfterDelay lambda checks state, so transitioning out of
    // Connecting effectively cancels it.
}

void ConnectionManager::startSlotDetectionFallback()
{
    int generation = slotDetectGeneration;

    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(3000, [this, generation, aliveFlag]()
    {
        if (!*aliveFlag) return;
        // Only fire if no SlotActivated/NewPatchInSlot arrived and we're still connected
        if (generation == slotDetectGeneration && !slotDetected && isConnected()
            && !waitingForPatchAck && !collectingSections)
        {
            // currentSlot may already hold the real focus (seeded from the
            // extended synth settings); fall back to it rather than slot 0.
            std::cout << "[SLOT] No SlotActivated received, fetching slot "
                      << static_cast<char>('A' + currentSlot) << std::endl;
            requestPatch(currentSlot);
        }
    });
}

void ConnectionManager::copyPatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition, int tempSlot)
{
    if (!isConnected())
        return;

    tempSlot = juce::jlimit(0, 3, tempSlot);

    std::cout << "[COPY] Copying patch from (" << srcSection << "," << srcPosition
              << ") to (" << dstSection << "," << dstPosition
              << ") via slot " << tempSlot << std::endl;

    patchFetchCompleteCallback = [this, tempSlot, dstSection, dstPosition](int completedSlot) {
        if (completedSlot != tempSlot || !isConnected())
            return;

        storeLoadedSlotToBank(tempSlot, dstSection, dstPosition, [this]() {
            std::cout << "[COPY] Copy complete!" << std::endl;
            auto aliveFlag = alive;
            juce::Timer::callAfterDelay(300, [this, aliveFlag]() {
                if (!*aliveFlag) return;
                if (isConnected())
                    requestPatchList();
            });
        });
    };

    // Load source patch to temp slot. Store happens after requestPatch(tempSlot)
    // has completed, via patchFetchCompleteCallback above.
    loadPatchFromBank(srcSection, srcPosition, tempSlot);
}

void ConnectionManager::movePatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition, int tempSlot)
{
    if (!isConnected())
        return;

    tempSlot = juce::jlimit(0, 3, tempSlot);

    std::cout << "[MOVE] Moving patch from (" << srcSection << "," << srcPosition
              << ") to (" << dstSection << "," << dstPosition
              << ") via slot " << tempSlot << std::endl;

    patchFetchCompleteCallback = [this, tempSlot, srcSection, srcPosition, dstSection, dstPosition](int completedSlot) {
        if (completedSlot != tempSlot || !isConnected())
            return;

        storeLoadedSlotToBank(tempSlot, dstSection, dstPosition, [this, srcSection, srcPosition]() {
            DeletePatchMessage deleteMsg(srcSection, srcPosition);
            sendAckedSysEx(deleteMsg.toSysEx());

            std::cout << "[MOVE] Move complete; delete queued for source bank ("
                      << srcSection << "," << srcPosition << ")" << std::endl;

            auto aliveFlag = alive;
            juce::Timer::callAfterDelay(500, [this, aliveFlag]() {
                if (!*aliveFlag) return;
                if (isConnected())
                    requestPatchList();
            });
        });
    };

    loadPatchFromBank(srcSection, srcPosition, tempSlot);
}

void ConnectionManager::deletePatchInBank(int section, int position)
{
    if (!isConnected())
        return;

    std::cout << "[DELETE] Deleting patch at bank " << section
              << " position " << position << std::endl;

    DeletePatchMessage msg(section, position);
    sendAckedSysEx(msg.toSysEx());

    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(500, [this, aliveFlag]() {
        if (!*aliveFlag) return;
        if (isConnected())
            requestPatchList();
    });
}

void ConnectionManager::storeLoadedSlotToBank(int slot, int section, int position, std::function<void()> afterStoreQueued)
{
    std::cout << "[BANK] Storing from slot " << slot << " to bank ("
              << section << "," << position << ")" << std::endl;

    StorePatchMessage msg(slot, section, position);
    sendAckedSysEx(msg.toSysEx(slot));

    if (afterStoreQueued)
        afterStoreQueued();
}
