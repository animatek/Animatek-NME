#include "ConnectionManager.h"
#include "../protocol/StorePatchMessage.h"
#include "../protocol/DeletePatchMessage.h"
#include "../protocol/SetPatchTitleMessage.h"
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

    // Send IAm handshake (sender=0 means PC, version 3.3)
    // IAm has no checksum per the PDL2 spec
    IAmMessage iam;
    iam.sender = 0;
    iam.versionHigh = 3;
    iam.versionLow = 3;
    auto payload = iam.encode();
    protocol.sendMessage(NmCmd::IAm, 0, payload, /*expectsReply=*/true, /*addChecksum=*/false);

    startHandshakeTimeout();
    return true;
}

void ConnectionManager::disconnect()
{
    cancelHandshakeTimeout();
    collectingSections = false;
    pendingBankLoadSlot = -1;
    pendingBankLoadGeneration++;
    slotDetected = false;
    slotDetectGeneration++;

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
    if (!isConnected())
        return;

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
    const int gen = patchTimeoutGeneration;
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(3000, [this, gen, slot, aliveFlag]() {
        if (!*aliveFlag) return;
        if (gen == patchTimeoutGeneration && waitingForPatchAck && !collectingSections)
        {
            std::cout << "[PATCH] No ACK for patch request (slot " << slot
                      << ") after 3s - resetting fetch state" << std::endl;
            waitingForPatchAck = false;
        }
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

    // libnmProtocol sends slot-management commands with SysEx header slot 0.
    // The target slot lives in the payload.
    std::vector<uint8_t> selectedPayload;
    selectedPayload.push_back(0x41);  // pid = PatchManagerCommand
    selectedPayload.push_back(0x07);  // sc = SlotsSelected
    // PDL: SlotsSelected := 0:4 slot0:1 slot1:1 slot2:1 slot3:1
    selectedPayload.push_back(static_cast<uint8_t>(1 << (3 - slot)));
    protocol.sendMessage(NmCmd::PatchHandling, 0, selectedPayload,
                         /*expectsReply=*/true, /*addChecksum=*/true);

    std::vector<uint8_t> activePayload;
    activePayload.push_back(0x41);  // pid = PatchManagerCommand
    activePayload.push_back(0x09);  // sc = SlotActivated
    activePayload.push_back(static_cast<uint8_t>(slot & 0x7F));
    protocol.sendMessage(NmCmd::PatchHandling, 0, activePayload,
                         /*expectsReply=*/true, /*addChecksum=*/true);

    currentSlot = slot;

    std::cout << "[SLOT] Sent SlotsSelected + SlotActivated: " << slot << std::endl;
}

void ConnectionManager::loadPatchFromBank(int section, int position, int targetSlot)
{
    if (!isConnected())
        return;

    // Loading while the patch list stream is in flight interleaves two
    // request/response streams and freezes the G1 — cancel the list first.
    cancelPatchListFetch("bank load supersedes it");

    int slot = (targetSlot >= 0) ? targetSlot : currentSlot;
    currentSlot = slot;
    pendingPatchSlot = slot;

    // A browser load supersedes any patch fetch already in flight. Without this,
    // late packets from the previous request can finish after the double-click
    // and briefly install the previously requested patch into the target slot.
    waitingForPatchAck = false;
    collectingSections = false;
    patchPacketsReceived = 0;
    sectionAccumulator.clear();
    patchSections.clear();
    sectionsReceived = 0;
    patchTimeoutGeneration++;

    lastLoadedSection = section;
    lastLoadedPosition = position;
    suppressNextLocationClear = true;

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

std::vector<uint8_t> ConnectionManager::buildUploadSysEx(int sectionIndex, int numSections, int slot)
{
    bool isFirst = (sectionIndex == 0);
    bool isLast  = (sectionIndex == numSections - 1);
    int  cc      = 0x1c | (isFirst ? 1 : 0) | (isLast ? 2 : 0);
    int  sectionsEnded = sectionIndex + 1;

    // payload[0]: 0:1 command:1 pid:6
    //   MSB=0, command=1 (bulk upload), pid=sectionsEnded
    uint8_t cmdPidByte = static_cast<uint8_t>(0x40 | (sectionsEnded & 0x3F));

    std::vector<uint8_t> msg;
    msg.push_back(0xF0);
    msg.push_back(0x33);
    msg.push_back(static_cast<uint8_t>(((cc & 0x1F) << 2) | (slot & 0x03)));
    msg.push_back(0x06);
    msg.push_back(cmdPidByte);
    msg.insert(msg.end(), uploadSections[static_cast<size_t>(sectionIndex)].begin(),
                          uploadSections[static_cast<size_t>(sectionIndex)].end());
    // Checksum: sum of all bytes (F0 through last payload byte) % 128
    uint32_t sum = 0;
    for (auto b : msg)
        sum += b;
    msg.push_back(static_cast<uint8_t>(sum % 128));
    msg.push_back(0xF7);
    return msg;
}

void ConnectionManager::sendNextUploadSection()
{
    int total = static_cast<int>(uploadSections.size());
    if (uploadSectionIndex >= total)
    {
        // All sections sent and ACKed — done
        std::cout << "[UPLOAD] All " << total << " sections sent and ACKed." << std::endl;
        waitingForUploadAck = false;
        // Notify the bank transfer (if one is running) or MainComponent
        if (bankUploadResultCallback)
        {
            auto cb = std::move(bankUploadResultCallback);
            bankUploadResultCallback = nullptr;
            juce::MessageManager::callAsync([cb]() { cb(true); });
        }
        else if (uploadCompleteCallback)
        {
            auto cb = uploadCompleteCallback;
            juce::MessageManager::callAsync([cb]() { cb(); });
        }
        // Suppress the next auto-fetch triggered by NewPatchInSlot (sc=0x38).
        // currentPatch is already authoritative — it IS the patch we just uploaded.
        // Re-fetching would replace it with a synth copy that may not include
        // morph assignments (the synth's working-slot memory may strip them).
        suppressNextAutoFetch = true;
        return;
    }

    auto msg = buildUploadSysEx(uploadSectionIndex, total, uploadSlot);
    const int sentSection = uploadSectionIndex;
    const int ackGeneration = ++uploadAckGeneration;

    // Log full SysEx for debugging
    std::cout << "[UPLOAD]   section " << uploadSectionIndex
              << "/" << total << " size=" << msg.size()
              << " " << describePdlSection(uploadSections[static_cast<size_t>(uploadSectionIndex)])
              << " hex:";
    for (size_t k = 0; k < msg.size(); ++k)
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)msg[k];
    std::cout << std::dec << std::endl;

    sendRawSysEx(msg);
    // waitingForUploadAck stays true — onAckReceived will call sendNextUploadSection
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(uploadAckTimeoutMs, [this, sentSection, ackGeneration, aliveFlag]() {
        if (!*aliveFlag) return;
        if (waitingForUploadAck && uploadAckGeneration == ackGeneration)
        {
            std::cout << "[UPLOAD] ACK timeout at section " << sentSection
                      << "/" << uploadSections.size()
                      << " " << (uploadSections.size() > static_cast<size_t>(sentSection)
                          ? describePdlSection(uploadSections[static_cast<size_t>(sentSection)])
                          : "type=-1 (Unknown)")
                      << " — aborting upload" << std::endl;
            waitingForUploadAck = false;
            uploadSections.clear();
            uploadSectionIndex = 0;
            setStatus(State::Connected, "Upload timeout at section " + juce::String(sentSection));

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

    // Serialize the patch into individual PDL2 sections in the Java upload order.
    PatchSerializer serializer;
    uploadSections = serializer.serializeForUpload(patch);
    uploadSlot = slot;
    uploadSectionIndex = 0;
    ++uploadAckGeneration;

    std::cout << "[UPLOAD] Uploading patch \"" << patch.getName().toStdString()
              << "\" to slot " << slot << " (" << uploadSections.size() << " sections)" << std::endl;

    // Send sections one at a time, waiting for ACK between each (like Java protocol)
    waitingForUploadAck = true;
    sendNextUploadSection();
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

void ConnectionManager::sendParameter(int section, int moduleId, int parameterId, int value)
{
    if (!isConnected())
    {
        DBG("sendParameter: NOT CONNECTED");
        return;
    }

    ParameterChangeMessage msg;
    msg.pid = currentPatchId;
    msg.section = section;
    msg.module = moduleId;
    msg.parameter = parameterId;
    msg.value = value;

    auto payload = msg.encode();

    DBG("sendParameter: slot=" + juce::String(currentSlot)
        + " pid=" + juce::String(currentPatchId)
        + " section=" + juce::String(section)
        + " module=" + juce::String(moduleId)
        + " param=" + juce::String(parameterId)
        + " value=" + juce::String(value));

    // Parameter messages use cc=0x13, have checksum, no reply expected
    // IMPORTANT: Use currentSlot, not 0!
    protocol.sendMessage(NmCmd::ParameterChange, currentSlot, payload, /*expectsReply=*/false, /*addChecksum=*/true);
}

void ConnectionManager::sendPatchTitle(const juce::String& title)
{
    if (!isConnected())
    {
        DBG("sendPatchTitle: NOT CONNECTED");
        return;
    }

    SetPatchTitleMessage msg(currentSlot, currentPatchId, title);
    auto sysex = msg.toSysEx(currentSlot);
    sendRawSysEx(sysex);

    DBG("sendPatchTitle: slot=" + juce::String(currentSlot)
        + " pid=" + juce::String(currentPatchId)
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

void ConnectionManager::sendAckedSysEx(const std::vector<uint8_t>& sysex, bool allowNewPatchInSlotReply)
{
    if (!isConnected() || !midiDevice)
        return;

    ackedQueue.push_back({ sysex, allowNewPatchInSlotReply });
    drainAckedQueue();
}

void ConnectionManager::drainAckedQueue()
{
    if (ackedQueueWaiting || ackedQueue.empty())
        return;

    auto msg = ackedQueue.front();
    ackedQueue.pop_front();
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
            std::cout << "[QUEUE] ACK timeout (gen=" << generation << ") — unblocking queue ("
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

    // Unblock the acked queue — any pending edit messages can now be sent
    if (ackedQueueWaiting)
    {
        ackedQueueWaiting = false;
        ackedQueueWaitingAllowsNewPatchInSlot = false;
        std::cout << "[QUEUE] ACK received, unblocking queue ("
                  << ackedQueue.size() << " pending)" << std::endl;
        drainAckedQueue();
    }

    if (waitingForUploadAck)
    {
        // Synth ACK for current upload section — advance to next
        currentPatchId = msg.pid1;
        ++uploadAckGeneration;  // invalidate timeout for the section just ACKed
        uploadSectionIndex++;
        std::cout << "[UPLOAD] ACK for section " << (uploadSectionIndex - 1)
                  << ", patchId=" << currentPatchId << std::endl;
        auto aliveFlag = alive;
        juce::Timer::callAfterDelay(uploadInterSectionDelayMs, [this, aliveFlag]() {
            if (!*aliveFlag) return;
            if (waitingForUploadAck)
                sendNextUploadSection();  // sends next or completes if all done
        });
        return;
    }

    if (waitingForPatchAck)
    {
        waitingForPatchAck = false;
        collectingSections = true;
        currentPatchId = msg.pid1;  // Store for use in parameter changes
        DBG("Patch ACK for slot " + juce::String(pendingPatchSlot)
            + ", patchId=" + juce::String(currentPatchId) + " — sending GetPatch for all 13 sections");
        sendGetPatchMessages(currentPatchId, pendingPatchSlot);
        startPatchTimeout();
    }
}

void ConnectionManager::cancelPatchListFetch(const char* reason)
{
    if (!fetchingPatchList)
        return;

    fetchingPatchList = false;
    patchListGeneration++;  // Invalidate the in-flight request chain
    lastListCancelMs = juce::Time::getMillisecondCounter();
    std::cout << "[PATCHLIST] Fetch cancelled: " << reason << std::endl;

    // Deliver what we have so any UI waiting on the list leaves its
    // loading state. The user can refresh for a complete list later.
    if (patchListCallback)
        patchListCallback(patchListNames);
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
    auto msgs = GetPatchMessage::forAllSections(patchId);
    for (auto& m : msgs)
    {
        auto payload = m.encode();
        // GetPatch uses PatchModification format (same cc=0x17).
        // Responses come as PatchPacket (cc=0x1c-0x1f), not ACK,
        // so don't set expectsReply — let them all queue and send freely.
        protocol.sendMessage(NmCmd::PatchHandling, slot, payload,
                             /*expectsReply=*/false, /*addChecksum=*/true);
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
                + " sections — parsing partial data");
            finalizePatch();
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
            finalizePatch();
        }
    });
}

void ConnectionManager::finalizePatch()
{
    collectingSections = false;

    // If we have a partial accumulator (section in progress), discard it
    sectionAccumulator.clear();

    if (patchSections.empty())
    {
        DBG("No patch sections to parse");
        return;
    }

    DBG(juce::String(sectionsReceived < totalSections ? "Partial" : "All") + " "
        + juce::String(patchSections.size()) + " sections — invoking parser");

    const int completedSlot = pendingPatchSlot;

    // Mark this slot as the current one
    currentSlot = completedSlot;

    if (bankFetchCallback)
    {
        auto cb = std::move(bankFetchCallback);
        bankFetchCallback = nullptr;
        cb(patchSections, completedSlot);
    }
    else if (patchDataCallback)
        patchDataCallback(patchSections, completedSlot);

    if (patchFetchCompleteCallback)
    {
        auto cb = std::move(patchFetchCompleteCallback);
        patchFetchCompleteCallback = nullptr;
        cb(completedSlot);
    }

    patchSections.clear();
    sectionsReceived = 0;
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
        int base = msg.lightStartIndex;
        for (int i = 0; i < 20 && (base + i) < 128; ++i)
            globalLightValues[base + i] = msg.lightValues[i];
        if (lightMeterCallback)
            lightMeterCallback(globalLightValues, globalMeterValues);
    }

    if (msg.sc == 0x3a && msg.meterStartIndex >= 0)  // MeterMessage
    {
        int base = msg.meterStartIndex;
        for (int i = 0; i < 5; ++i)
        {
            if ((base + i*2)   < 128) globalMeterValues[base + i*2]   = msg.meterValuesB[i];
            if ((base + i*2+1) < 128) globalMeterValues[base + i*2+1] = msg.meterValuesA[i];
        }
        if (lightMeterCallback)
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

        // Update the patch ID from the synth's notification
        currentPatchId = msg.newPatchPid;

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
            }
            requestPatch(msg.newPatchSlot);
        }
        else if (waitingForUploadAck)
        {
            std::cout << "[UPLOAD] Ignoring NewPatchInSlot during upload" << std::endl;
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
            uploadSections.clear();
            uploadSectionIndex = 0;
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

    if (msg.sc == 0x09 && !msg.data.empty())  // SlotActivated
    {
        int activeSlot = msg.data[0] & 0x03;
        std::cout << "[SLOT] Active slot changed to " << activeSlot << std::endl;

        currentSlot = activeSlot;
        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        // Slot changed on synth — we don't know the bank location
        lastLoadedSection = -1;
        lastLoadedPosition = -1;
        suppressNextLocationClear = false;

        // Notify UI of slot change
        if (slotChangedCallback)
            slotChangedCallback(activeSlot);

        // Auto-load patch from the active slot (unless already loading or uploading)
        if (isConnected() && !waitingForPatchAck && !collectingSections && !waitingForUploadAck)
            requestPatch(activeSlot);
    }

    // Silently handle high-frequency messages (Lights, Meters, SlotsSelected)
    // sc=0x39 (Lights), sc=0x3a (Meters), sc=0x07 (SlotsSelected)
}

void ConnectionManager::onPatchPacketReceived(const PatchPacketMessage& msg)
{
    // In Java protocol, PatchMessage.isreply = true — patch packets unblock the send queue.
    // The synth may respond to some edit commands (NewModule) with a patch confirmation packet.
    if (ackedQueueWaiting)
    {
        ackedQueueWaiting = false;
        std::cout << "[QUEUE] PatchPacket received — unblocking queue ("
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
                  << ") — unblocking queue (" << ackedQueue.size() << " pending)" << std::endl;
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
            std::cout << "[SLOT] No SlotActivated received — defaulting to slot 0" << std::endl;
            requestPatch(0);
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
