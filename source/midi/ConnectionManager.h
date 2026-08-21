#pragma once

#include "NmProtocol.h"
#include "MidiDeviceManager.h"
#include "UploadPacketizer.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

class Patch;

class ConnectionManager : public NmProtocol::Listener
{
public:
    ConnectionManager();
    ~ConnectionManager() override;

    enum class State
    {
        Disconnected,
        Connecting,
        Connected
    };

    struct Status
    {
        State state = State::Disconnected;
        juce::String message = "Disconnected";
        int synthVersionHigh = 0;
        int synthVersionLow = 0;
    };

    // Connection management
    bool connect(const juce::String& inputId, const juce::String& outputId);
    void disconnect();

    bool isConnected() const { return status.state == State::Connected; }
    const Status& getStatus() const { return status; }

    // Synth commands
    void requestPatch(int slot = 0);
    void loadPatchFromBank(int section, int position, int targetSlot = -1);  // -1 = use current slot
    void uploadPatch(int slot, const Patch& patch);  // Upload full patch to synth working slot
    void requestSynthSettings();
    void sendSynthSettings(const SynthSettings& settings);
    // slot is explicit (not the hardware-focused slot) — confirmed on real
    // hardware 2026-07-20 that the G1 applies a ParameterChange addressed to
    // a slot other than the one with front-panel focus, which is what makes
    // simultaneous multi-window editing viable (see the multi-window plan).
    void sendParameter(int slot, int section, int moduleId, int parameterId, int value);

    // Throttled + coalesced parameter delivery. Use this for bulk changes
    // (Mutator/Randomize snapshots) so a large patch does not flood the synth
    // with hundreds of simultaneous messages and drop the connection.
    void queueParameter(int slot, int section, int moduleId, int parameterId, int value);
    void sendPatchTitle(int slot, const juce::String& title);  // Change patch name (not saved to flash)
    // Change a module's name on the synth (not saved to flash). Without this the
    // rename would only exist in the editor until a full patch upload.
    void sendModuleTitle(int slot, int section, int moduleIndex, const juce::String& title);
    void sendControllerSnapshot();  // Ask synth to emit current values of assigned MIDI CCs (read-only)
    // Play notes on the current slot via the editor protocol (Note, cc=0x17 sc=0x56).
    // The editor talks to the synth's PC port, which ignores regular MIDI notes.
    void sendNoteOn(int note, int velocity);
    void sendNoteOff(int note);
    void sendRawSysEx(const std::vector<uint8_t>& sysex);       // Fire-and-forget (no ACK needed)
    void sendAckedSysEx(const std::vector<uint8_t>& sysex,
                        bool allowNewPatchInSlotReply = false); // Queued, waits for ACK before next
    bool isAckedQueueIdle() const { return ackedQueue.empty() && !ackedQueueWaiting; }
    bool isFetchingPatch() const { return waitingForPatchAck || collectingSections; }
    bool isUploadingPatch() const { return waitingForUploadAck; }

    // Bank operations (high-level)
    void copyPatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition, int tempSlot);
    void movePatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition, int tempSlot);
    void deletePatchInBank(int section, int position);

    int getCurrentSlot() const;
    void selectSlot(int slot);  // Give a slot focus (blinking LED on hardware)
    int getCurrentPatchId() const { return currentPatchId; }
    // Pid for a specific slot, independent of hardware focus — use this (not
    // getCurrentPatchId) when addressing an edit at a particular slot.
    int getPatchId(int slot) const { return slotPatchIds[static_cast<size_t>(slot & 0x03)]; }

    // Slot enable state (fixed LED on hardware; several slots can be enabled
    // at once, independent of which one has focus)
    void setSlotEnabled(int slot, bool enabled);  // Send full mask to synth
    bool isSlotEnabled(int slot) const
    {
        return slot >= 0 && slot < 4 && slotEnabled[static_cast<size_t>(slot)];
    }

    // Bank location of the last loaded patch (-1 = unknown, e.g. synth-side change)
    int getLastLoadedSection() const { return lastLoadedSection; }
    int getLastLoadedPosition() const { return lastLoadedPosition; }

    // Where each slot's patch lives in the synth's banks: set by a bank load and
    // by a store, cleared when the synth puts something else in the slot behind
    // our back. -1 = unknown (a new patch, or one opened from a file). This is
    // per slot on purpose — one editor holds four patches from four locations.
    int getSlotBankSection(int slot) const
    {
        return (slot >= 0 && slot < 4) ? slotBankSection[static_cast<size_t>(slot)] : -1;
    }
    int getSlotBankPosition(int slot) const
    {
        return (slot >= 0 && slot < 4) ? slotBankPosition[static_cast<size_t>(slot)] : -1;
    }
    void setSlotBankLocation(int slot, int section, int position)
    {
        if (slot < 0 || slot >= 4) return;
        slotBankSection[static_cast<size_t>(slot)] = section;
        slotBankPosition[static_cast<size_t>(slot)] = position;
    }
    void clearSlotBankLocation(int slot) { setSlotBankLocation(slot, -1, -1); }

    // Fired when the synth tells us where a slot's patch came from.
    using BankLocationCallback = std::function<void(int slot)>;
    void setBankLocationCallback(BankLocationCallback cb) { bankLocationCallback = std::move(cb); }

    // Every bank position carrying this name, as (section, position) pairs. One
    // hit is an answer; several are candidates the user has to choose between,
    // because the G1 never says where a front-panel load came from.
    std::vector<std::pair<int, int>> findPatchLocations(const juce::String& name) const;

    // Device enumeration (delegates to MidiDeviceManager)
    static juce::Array<juce::MidiDeviceInfo> getAvailableInputDevices();
    static juce::Array<juce::MidiDeviceInfo> getAvailableOutputDevices();

    // Callbacks
    using StatusCallback = std::function<void(const Status&)>;
    void setStatusCallback(StatusCallback cb) { statusCallback = std::move(cb); }

    using VoiceCountCallback = std::function<void(const int voiceCounts[4])>;
    void setVoiceCountCallback(VoiceCountCallback cb) { voiceCountCallback = std::move(cb); }

    using PatchDataCallback = std::function<void(const std::vector<std::vector<uint8_t>>& sections, int slot)>;
    void setPatchDataCallback(PatchDataCallback cb) { patchDataCallback = std::move(cb); }

    // Called when synth sends a parameter change (knob turned on hardware)
    using ParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int value)>;
    void setParameterChangeCallback(ParameterChangeCallback cb) { parameterChangeCallback = std::move(cb); }

    // Called when synth sends an error notification (sc=0x7e)
    using SynthErrorCallback = std::function<void(int errorCode)>;
    void setSynthErrorCallback(SynthErrorCallback cb) { synthErrorCallback = std::move(cb); }

    // Called when synth changes active slot (user pressed slot button on hardware)
    using SlotChangedCallback = std::function<void(int slot)>;
    void setSlotChangedCallback(SlotChangedCallback cb) { slotChangedCallback = std::move(cb); }

    // Called when the synth reports which slots are enabled (SlotsSelected mask)
    using SlotsEnabledCallback = std::function<void(const std::array<bool, 4>&)>;
    void setSlotsEnabledCallback(SlotsEnabledCallback cb) { slotsEnabledCallback = std::move(cb); }

    // Called when synth ACKs an uploadPatch() — safe to send StorePatch now
    using UploadCompleteCallback = std::function<void()>;
    void setUploadCompleteCallback(UploadCompleteCallback cb) { uploadCompleteCallback = std::move(cb); }

    // Called when synth sends real-time light/meter data (sc=0x39/0x3A)
    // lights: 128 LED values (0-3), meters: 128 meter values (0-127).
    // Like every listener callback here, it arrives on the message thread:
    // MidiDeviceManager bounces incoming SysEx there before the protocol
    // decodes it, so nothing downstream needs a hop of its own. The arrays
    // belong to this object and are only valid for the duration of the call.
    using LightMeterCallback = std::function<void(const int lights[128], const int meters[128])>;
    void setLightMeterCallback(LightMeterCallback cb) { lightMeterCallback = std::move(cb); }

    using SynthSettingsCallback = std::function<void(const SynthSettings& settings)>;
    void setSynthSettingsCallback(SynthSettingsCallback cb) { synthSettingsCallback = std::move(cb); }

    // Patch fetch progress: fired when a fetch starts (0/N) and per completed
    // section.
    using PatchLoadProgressCallback = std::function<void(int sectionsDone, int totalSections)>;
    void setPatchLoadProgressCallback(PatchLoadProgressCallback cb) { patchLoadProgressCallback = std::move(cb); }

    // Fired when a patch fetch finalizes without all sections despite retries
    // (synth overloaded or connection flaky). The partial patch is still
    // delivered, but it may be missing cables/parameters — the user must be
    // warned before editing or saving it.
    using PatchLoadIncompleteCallback = std::function<void(int slot, int sectionsReceived, int totalSections)>;
    void setPatchLoadIncompleteCallback(PatchLoadIncompleteCallback cb) { patchLoadIncompleteCallback = std::move(cb); }

    // Patch list management
    using PatchListCallback = std::function<void(const std::vector<std::string>& names)>;
    void setPatchListCallback(PatchListCallback cb) { patchListCallback = std::move(cb); }
    void requestPatchList();  // Start loading all 891 patch names from synth
    void cancelPatchListFetch(const char* reason);  // Abort in-flight list fetch (delivers partial names)
    void resumePatchListIfInterrupted();  // Restart the list fetch if a patch op aborted it earlier
    const std::vector<std::string>& getPatchList() const { return patchListNames; }
    bool isPatchListLoaded() const { return patchListLoaded; }

    // Bank transfer hooks (one-shot, set by BankTransferManager before each item).
    // While bankFetchCallback is set, a completed patch fetch is delivered to it
    // INSTEAD of patchDataCallback, keeping the editor UI out of bulk transfers.
    using BankFetchCallback = std::function<void(const std::vector<std::vector<uint8_t>>& sections, int slot)>;
    void setBankFetchCallback(BankFetchCallback cb) { bankFetchCallback = std::move(cb); }

    // While set, upload completion (true) or ACK-timeout abort (false) is
    // delivered here instead of uploadCompleteCallback.
    using BankUploadResultCallback = std::function<void(bool success)>;
    void setBankUploadResultCallback(BankUploadResultCallback cb) { bankUploadResultCallback = std::move(cb); }

    NmProtocol& getProtocol() { return protocol; }

    // Call after sending a structural edit (module/cable add/delete) that will
    // cause the synth to respond with NewPatchInSlot.  The next N NewPatchInSlot
    // messages will be treated as echoes and suppressed.
    void expectSyncEcho() { pendingSyncEchoes_++; }

    // Legacy boolean suppress (for upload-in-progress protection).
    void setSuppressNewPatchInSlot(bool s) { suppressNewPatchInSlot_ = s; }

private:
    // NmProtocol::Listener
    void onIAmReceived(const IAmMessage& msg) override;
    void onSynthMessage(int cc) override;
    void onParameterChanged(const ParameterChangeMessage& msg) override;
    void onAckReceived(const AckMessage& msg) override;
    void onPatchListReceived(const AckMessage& msg) override;
    void onNMInfoReceived(const NMInfoMessage& msg) override;
    void onPatchPacketReceived(const PatchPacketMessage& msg) override;
    void onError(const ErrorMessage& msg) override;

    void setStatus(State state, const juce::String& message);
    void sendNoteEvent(int note, int velocity, bool on);
    void startHandshakeTimeout();
    void sendHandshake();          // IAm, sender=0: the editor introducing itself
    juce::uint32 lastHandshakeMs = 0;  // rate-limits the re-hello in onSynthMessage
    void cancelHandshakeTimeout();
    void startSlotDetectionFallback();
    void sendGetPatchMessages(int patchId, int slot);
    void startPatchTimeout();
    void startSectionStaleTimeout();
    void retryMissingSections();
    void finalizePatch();
    void storeLoadedSlotToBank(int slot, int section, int position, std::function<void()> afterStoreQueued = {});
    void startEnabledSlotPrefetch();   // Queue every enabled-but-unfocused slot after connect
    void continueSlotPrefetchQueue();  // Fetch the next queued slot once the wire is free
    void sendPatchRequest(int slot);   // One RequestPatch attempt, under the retry budget
    void serviceDeferredAutoFetch();   // Run a fetch the wire was too busy to take

    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };
    NmProtocol protocol;

    // Coalesced, throttled outgoing parameter queue. Keyed by (slot,section,module,param)
    // so repeated changes to the same parameter (e.g. auditioning Mutator children
    // quickly) collapse to the latest value, and two different slots' queued edits
    // (e.g. from two open slot windows) can never collide or overwrite each other.
    // Drained a few at a time by one timer, so bulk snapshot applies never overlap
    // or flood the synth. A context generation prevents queued values from crossing
    // a slot switch or full patch replacement.
    struct ParamKey
    {
        int slot, section, module, param;
        bool operator<(const ParamKey& o) const
        {
            if (slot    != o.slot)    return slot    < o.slot;
            if (section != o.section) return section < o.section;
            if (module  != o.module)  return module  < o.module;
            return param < o.param;
        }
    };
    std::map<ParamKey, int> paramQueue_;
    struct ParamQueueTimer : juce::Timer
    {
        explicit ParamQueueTimer(ConnectionManager& o) : owner(o) {}
        void timerCallback() override { owner.drainParamQueue(); }
        ConnectionManager& owner;
    };
    ParamQueueTimer paramQueueTimer_ { *this };
    void drainParamQueue();
    void clearParamQueue();
    // slot < 0 (default) discards the whole queue, for the truly global cases
    // (disconnect). Any other value discards only that slot's entries — a
    // fetch/upload for one slot must not silently drop another slot's
    // unrelated queued edits (e.g. made in a background slot's sub-window).
    void invalidateParamQueue(const char* reason, int slot = -1);
    std::uint64_t paramContextGeneration_ = 0;
    std::uint64_t queuedParamGeneration_ = 0;
    // Drain rate, user-selectable via Editor Options (Send speed). With no
    // overlapping chains the G1 tolerates a higher rate than the old 4/20ms.
    int paramDrainBatch_      = 8;   // params per tick
    int paramDrainIntervalMs_ = 20;  // tick period  -> 400/s default

public:
    /** Set the throttled parameter delivery rate (params per tick / tick ms). */
    void setParamSendRate(int batchPerTick, int intervalMs);

private:

    std::unique_ptr<MidiDeviceManager> midiDevice;
    Status status;
    StatusCallback statusCallback;
    VoiceCountCallback voiceCountCallback;
    PatchDataCallback patchDataCallback;
    ParameterChangeCallback parameterChangeCallback;
    SynthErrorCallback synthErrorCallback;
    SlotChangedCallback slotChangedCallback;
    SlotsEnabledCallback slotsEnabledCallback;
    UploadCompleteCallback uploadCompleteCallback;
    BankFetchCallback bankFetchCallback;
    BankUploadResultCallback bankUploadResultCallback;
    LightMeterCallback lightMeterCallback;
    SynthSettingsCallback synthSettingsCallback;
    PatchLoadProgressCallback patchLoadProgressCallback;
    PatchLoadIncompleteCallback patchLoadIncompleteCallback;

    // Global light/meter arrays updated by synth messages
    int globalLightValues[128] = {};
    int globalMeterValues[128] = {};

    // Patch request state
    bool waitingForPatchAck = false;
    bool collectingSections = false;
    bool waitingForUploadAck = false;      // True while waiting for synth ACK after uploadPatch
    bool suppressNextAutoFetch = false;    // Set after upload completes; clears on next NewPatchInSlot
    bool suppressNewPatchInSlot_ = false;  // Set during upload-in-progress
    int pendingSyncEchoes_ = 0;            // Count of expected NewPatchInSlot echoes from sync edits
    // Sequential upload state: one packet at a time, waiting for an ACK between
    // each. The cutting and framing rules live in UploadPacketizer (pure, unit
    // tested); this class only walks the packet list against the ACK stream.
    std::vector<UploadPacketizer::Packet> uploadPackets;
    void sendNextUploadPacket();
    // An upload that just stops leaves the synth parked in bulk-receive state,
    // deaf to all MIDI until something closes the transfer (issue #40).
    void closeUploadTransfer(const char* reason);
    int uploadSlot = 0;
    int uploadPacketIndex = 0;
    int uploadAckGeneration = 0;
    static constexpr int uploadAckTimeoutMs = 5000;
    static constexpr int uploadInterSectionDelayMs = 40;
    int pendingPatchSlot = 0;
    int currentSlot = 0;  // Slot with focus (blinking LED on hardware)
    std::array<bool, 4> slotEnabled { false, false, false, false };  // Enable mask as reported by the synth
    // Slots the user pinned (Ctrl+click here / Shift+button on the panel).
    // Pinned slots stay enabled when the selection moves away; an unpinned
    // slot is only enabled while selected. The mask we send is always
    // pinned + selected. Synced from synth notifications in updateSlotPins().
    std::array<bool, 4> slotPinned { false, false, false, false };
    // True once the synth has told us its real enable mask (SlotsSelected or
    // extended settings). Until then we must never send a mask of our own —
    // a wrong guess disables slots on the synth.
    bool slotEnableMaskKnown = false;
    void updateSlotPinsFromMask(int selectedSlot);
    void sendSlotMask();
    int pendingBankLoadSlot = -1;
    int pendingBankLoadGeneration = 0;
    int lastLoadedSection = -1;   // Bank section of last loadPatchFromBank (-1=unknown)
    int lastLoadedPosition = -1;  // Bank position of last loadPatchFromBank (-1=unknown)
    int slotBankSection[4] = { -1, -1, -1, -1 };   // Per-slot bank origin (-1=unknown)
    int slotBankPosition[4] = { -1, -1, -1, -1 };
    BankLocationCallback bankLocationCallback;
    bool suppressNextLocationClear = false;  // Set by loadPatchFromBank, cleared on NewPatchInSlot
    int currentPatchId = 0;  // Pid of the focused slot's patch (used in parameter changes)
    // Pid per slot: patch generations advance independently per slot, and a
    // background slot's fetch/NewPatchInSlot must not contaminate the pid
    // used for the focused slot (or vice versa for queued structural edits).
    std::array<int, 4> slotPatchIds { 0, 0, 0, 0 };
    // True when the editor holds a model for the slot that matches the synth
    // (set by a complete fetch delivery or a finished editor upload). While
    // set, SlotActivated skips its auto-fetch so switching slots reuses the
    // in-memory model instead of re-downloading all 13 sections. Invalidated
    // by a genuine NewPatchInSlot, by starting a fetch/upload, and on
    // disconnect (the synth can change while we're away).
    std::array<bool, 4> slotModelDelivered {};
    int patchPacketsReceived = 0;
    std::function<void(int slot)> patchFetchCompleteCallback;

    // Accumulate PatchPacket stream — each completed section stored separately
    std::vector<uint8_t> sectionAccumulator;              // current section being assembled
    std::vector<std::vector<uint8_t>> patchSections;      // completed sections
    int sectionsReceived = 0;
    static constexpr int totalSections = 13;
    // Which of the 13 GetPatch requests have been answered, keyed by
    // GetPatchMessage::Section. Lets a stalled fetch re-request only the
    // missing sections, and drops the duplicate when a retried request
    // races its slow original reply (parsing a section twice would
    // duplicate cables/parameters in the model).
    std::array<bool, 13> sectionSeen {};
    int fetchPatchId = -1;         // pid the in-flight GetPatch burst was sent with
    int sectionRetriesLeft = 0;
    // A synth at 99-100% DSP load answers GetPatch slowly and can stall for
    // seconds mid-fetch (issue #15). Re-requesting the missing sections
    // recovers the fetch instead of installing a partial patch.
    static constexpr int maxSectionRetries = 2;
    // patchTimeoutMs: upper bound for one fetch attempt (restarted per retry).
    // 8 s chosen empirically — a slow USB-MIDI round trip for 13 sections is ~2-3 s;
    // 8 s gives headroom for sluggish hosts without hanging the UI indefinitely.
    static constexpr int patchTimeoutMs = 8000;
    // sectionStaleMs: if no new section arrives within this window, the transfer
    // is considered stalled (synth dropped a packet).  2 s > worst observed gap.
    static constexpr int sectionStaleMs = 2000;
    int patchTimeoutGeneration = 0;  // Incremented on each new request to invalidate old timeouts
    // A synth busy writing a large patch into a slot never answers the
    // RequestPatch that follows its own NewPatchInSlot, and the editor was left
    // showing the previous patch (issue #41). Ask again a few times before
    // giving up, and keep hold of notifications that arrive while another
    // transfer owns the wire.
    static constexpr int maxPatchRequestAttempts = 3;
    int patchRequestAttemptsLeft = 0;
    std::array<bool, 4> autoFetchPending {};

    // Slot detection: synth sends SlotActivated after handshake
    bool slotDetected = false;
    int slotDetectGeneration = 0;  // Invalidate fallback timer when slot is detected

    // Phase 2: prefetch every enabled-but-not-focused slot right after
    // connecting (mirrors the original Nomad editor's `if (slot.isEnabled())
    // slot.requestPatch()` loop), so the first switch to any of them is also
    // instant instead of incurring a 13-section fetch. One fetch at a time —
    // the G1 can't handle overlapping GetPatch streams. backgroundPrefetchSlot
    // marks the slot the CURRENTLY in-flight fetch is for, if any, so a real
    // SlotActivated for a different slot can tell "safe to abort and requeue"
    // (this) apart from "a manual reload/bank op is running" (must not abort).
    std::vector<int> slotPrefetchQueue;
    int backgroundPrefetchSlot = -1;

    // Patch list retrieval state
    bool fetchingPatchList = false;
    bool patchListLoaded = false;
    int patchListSection = 0;      // Current section (0-8) being requested
    int patchListPosition = 0;     // Current position (0-98) being requested
    int patchListGeneration = 0;   // Invalidate old timeouts
    std::vector<std::string> patchListNames;  // 891 entries (9 banks × 99 positions)
    PatchListCallback patchListCallback;
    // patchListTimeoutMs: 891 patches × one request/response each.
    // Measured at ~8-9 s on a real G1; 10 s allows for occasional retransmits.
    static constexpr int patchListTimeoutMs = 10000;
    // After cancelPatchListFetch, in-flight responses from the old stream must
    // drain before a new fetch starts, or they get filed at wrong positions.
    static constexpr juce::uint32 listRestartCooldownMs = 400;
    juce::uint32 lastListCancelMs = 0;
    // Set when a patch fetch/upload cancels an in-flight list fetch (most
    // commonly: connecting starts the list fetch, then the synth's
    // SlotActivated notification triggers the initial patch load a moment
    // later, aborting the list partway through — e.g. only banks 1-6 show up
    // until the user manually hits refresh). Cleared once the interrupting
    // operation finishes and the list fetch is restarted automatically.
    bool patchListInterruptedByFetch = false;

    // Outgoing message queue with ACK-wait (mirrors Java NmProtocol send queue).
    // Messages sent via sendAckedSysEx() are queued and sent one at a time;
    // each waits for an ACK from the synth before the next is dispatched.
    // This prevents the synth from freezing/dropping operations when multiple
    // edit messages (DeleteModule, NewCable, etc.) are sent in rapid succession.
    struct QueuedSysEx
    {
        std::vector<uint8_t> bytes;
        bool allowNewPatchInSlotReply = false;
        int slot = 0;  // From the SysEx header at enqueue time; selects which slot's pid is patched in at send time
    };

    std::deque<QueuedSysEx> ackedQueue;
    bool ackedQueueWaiting = false;       // True while awaiting ACK for in-flight message
    bool ackedQueueWaitingAllowsNewPatchInSlot = false;
    int ackedQueueGeneration = 0;         // Incremented each time a message is sent; invalidates old timeouts
    static constexpr int ackedTimeoutMs = 3000;  // 3 s timeout per message
    void drainAckedQueue();              // Send next from queue if not already waiting

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionManager)
};
