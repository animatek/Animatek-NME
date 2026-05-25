#pragma once

#include "NmProtocol.h"
#include "MidiDeviceManager.h"
#include <atomic>
#include <functional>

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
    void sendParameter(int section, int moduleId, int parameterId, int value);
    void sendPatchTitle(const juce::String& title);  // Change patch name in current slot (not saved to flash)
    void sendRawSysEx(const std::vector<uint8_t>& sysex);       // Fire-and-forget (no ACK needed)
    void sendAckedSysEx(const std::vector<uint8_t>& sysex,
                        bool allowNewPatchInSlotReply = false); // Queued, waits for ACK before next

    // Bank operations (high-level)
    void copyPatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition, int tempSlot);
    void movePatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition, int tempSlot);
    void deletePatchInBank(int section, int position);

    int getCurrentSlot() const;
    void selectSlot(int slot);  // Tell synth to switch active slot
    int getCurrentPatchId() const { return currentPatchId; }

    // Bank location of the last loaded patch (-1 = unknown, e.g. synth-side change)
    int getLastLoadedSection() const { return lastLoadedSection; }
    int getLastLoadedPosition() const { return lastLoadedPosition; }

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

    // Called when synth ACKs an uploadPatch() — safe to send StorePatch now
    using UploadCompleteCallback = std::function<void()>;
    void setUploadCompleteCallback(UploadCompleteCallback cb) { uploadCompleteCallback = std::move(cb); }

    // Called when synth sends real-time light/meter data (sc=0x39/0x3A)
    // lights: 128 LED values (0-3), meters: 128 meter values (0-127)
    using LightMeterCallback = std::function<void(const int lights[128], const int meters[128])>;
    void setLightMeterCallback(LightMeterCallback cb) { lightMeterCallback = std::move(cb); }

    using SynthSettingsCallback = std::function<void(const SynthSettings& settings)>;
    void setSynthSettingsCallback(SynthSettingsCallback cb) { synthSettingsCallback = std::move(cb); }

    // Patch list management
    using PatchListCallback = std::function<void(const std::vector<std::string>& names)>;
    void setPatchListCallback(PatchListCallback cb) { patchListCallback = std::move(cb); }
    void requestPatchList();  // Start loading all 891 patch names from synth
    const std::vector<std::string>& getPatchList() const { return patchListNames; }
    bool isPatchListLoaded() const { return patchListLoaded; }

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
    void onParameterChanged(const ParameterChangeMessage& msg) override;
    void onAckReceived(const AckMessage& msg) override;
    void onPatchListReceived(const AckMessage& msg) override;
    void onNMInfoReceived(const NMInfoMessage& msg) override;
    void onPatchPacketReceived(const PatchPacketMessage& msg) override;
    void onError(const ErrorMessage& msg) override;

    void setStatus(State state, const juce::String& message);
    void startHandshakeTimeout();
    void cancelHandshakeTimeout();
    void startSlotDetectionFallback();
    void sendGetPatchMessages(int patchId, int slot);
    void startPatchTimeout();
    void startSectionStaleTimeout();
    void finalizePatch();
    void storeLoadedSlotToBank(int slot, int section, int position, std::function<void()> afterStoreQueued = {});

    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };
    NmProtocol protocol;
    std::unique_ptr<MidiDeviceManager> midiDevice;
    Status status;
    StatusCallback statusCallback;
    VoiceCountCallback voiceCountCallback;
    PatchDataCallback patchDataCallback;
    ParameterChangeCallback parameterChangeCallback;
    SynthErrorCallback synthErrorCallback;
    SlotChangedCallback slotChangedCallback;
    UploadCompleteCallback uploadCompleteCallback;
    LightMeterCallback lightMeterCallback;
    SynthSettingsCallback synthSettingsCallback;

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
    // Sequential upload state: send one section at a time, wait for ACK between each
    std::vector<std::vector<uint8_t>> uploadSections;  // serialized PDL2 sections
    std::vector<uint8_t> buildUploadSysEx(int sectionIndex, int numSections, int slot);
    void sendNextUploadSection();
    int uploadSlot = 0;
    int uploadSectionIndex = 0;
    int uploadAckGeneration = 0;
    static constexpr int uploadAckTimeoutMs = 5000;
    static constexpr int uploadInterSectionDelayMs = 40;
    int pendingPatchSlot = 0;
    int currentSlot = 0;  // Track which slot is currently loaded
    int pendingBankLoadSlot = -1;
    int pendingBankLoadGeneration = 0;
    int lastLoadedSection = -1;   // Bank section of last loadPatchFromBank (-1=unknown)
    int lastLoadedPosition = -1;  // Bank position of last loadPatchFromBank (-1=unknown)
    bool suppressNextLocationClear = false;  // Set by loadPatchFromBank, cleared on NewPatchInSlot
    int currentPatchId = 0;  // Track the patch ID from ACK (used in parameter changes)
    int patchPacketsReceived = 0;
    std::function<void(int slot)> patchFetchCompleteCallback;

    // Accumulate PatchPacket stream — each completed section stored separately
    std::vector<uint8_t> sectionAccumulator;              // current section being assembled
    std::vector<std::vector<uint8_t>> patchSections;      // completed sections
    int sectionsReceived = 0;
    static constexpr int totalSections = 13;
    // patchTimeoutMs: absolute upper bound for the full 13-section fetch.
    // 8 s chosen empirically — a slow USB-MIDI round trip for 13 sections is ~2-3 s;
    // 8 s gives headroom for sluggish hosts without hanging the UI indefinitely.
    static constexpr int patchTimeoutMs = 8000;
    // sectionStaleMs: if no new section arrives within this window, the transfer
    // is considered stalled (synth dropped a packet).  2 s > worst observed gap.
    static constexpr int sectionStaleMs = 2000;
    int patchTimeoutGeneration = 0;  // Incremented on each new request to invalidate old timeouts

    // Slot detection: synth sends SlotActivated after handshake
    bool slotDetected = false;
    int slotDetectGeneration = 0;  // Invalidate fallback timer when slot is detected

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

    // Outgoing message queue with ACK-wait (mirrors Java NmProtocol send queue).
    // Messages sent via sendAckedSysEx() are queued and sent one at a time;
    // each waits for an ACK from the synth before the next is dispatched.
    // This prevents the synth from freezing/dropping operations when multiple
    // edit messages (DeleteModule, NewCable, etc.) are sent in rapid succession.
    struct QueuedSysEx
    {
        std::vector<uint8_t> bytes;
        bool allowNewPatchInSlotReply = false;
    };

    std::deque<QueuedSysEx> ackedQueue;
    bool ackedQueueWaiting = false;       // True while awaiting ACK for in-flight message
    bool ackedQueueWaitingAllowsNewPatchInSlot = false;
    int ackedQueueGeneration = 0;         // Incremented each time a message is sent; invalidates old timeouts
    static constexpr int ackedTimeoutMs = 3000;  // 3 s timeout per message
    void drainAckedQueue();              // Send next from queue if not already waiting

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionManager)
};
