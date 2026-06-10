#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class ConnectionManager;
class ModuleDescriptions;

// Orchestrates bulk patch transfers between a synth bank and a disk folder.
//
// Save direction: each non-empty bank position is loaded into a temp slot,
// fetched, parsed, and written to disk as "NN - Name.pch".
// Send direction: each .pch file is parsed, uploaded to the temp slot, and
// stored to the destination bank at sequential positions.
//
// One transfer runs at a time; all progress callbacks fire on the message
// thread. The temp slot's synth-side content is overwritten by the transfer —
// the editor model for that slot is re-fetched when the transfer ends.
class BankTransferManager
{
public:
    struct Progress
    {
        int current = 0;               // items completed (including failures)
        int total = 0;
        juce::String itemName;         // patch/file currently transferring
        bool finished = false;
        bool cancelled = false;
        juce::StringArray failures;    // names of items that failed
    };
    using ProgressCallback = std::function<void(const Progress&)>;

    BankTransferManager(ConnectionManager& cm, ModuleDescriptions& descs);
    ~BankTransferManager();

    bool isBusy() const { return busy; }

    // Save every non-empty patch of bank `section` (0-8) into destFolder.
    // Requires the synth patch list to be loaded (to skip empty positions).
    void saveBankToDisk(int section, const juce::File& destFolder, int tempSlot,
                        ProgressCallback cb);

    // Mirror-backup all 9 banks into banksRoot/Bank1..Bank9. Existing .pch
    // files inside those subfolders are deleted first so each folder exactly
    // reflects the synth bank, including positions deleted on the synth.
    void saveAllBanksToDisk(const juce::File& banksRoot, int tempSlot,
                            ProgressCallback cb);

    // Upload .pch files (sorted by filename) to bank `section`, filling
    // positions 1..N in order. Existing patches at those positions are
    // overwritten. Stops cleanly on the first upload failure.
    void sendBankToSynth(const juce::Array<juce::File>& files, int section,
                         int tempSlot, ProgressCallback cb);

    // Request the running transfer to stop after the current item.
    void cancel() { cancelRequested = true; }

private:
    struct SaveItem
    {
        int section = 0;
        int position = 0;
        juce::String name;     // patch name from the synth patch list
        juce::String label;    // progress display ("B3: name" in all-banks mode)
        juce::File folder;     // destination folder for this item
    };

    void beginSaveTransfer();

    void saveNextItem();
    void sendNextFile();
    void finishTransfer(bool cancelled);
    void reportProgress(const juce::String& itemName);
    static juce::String sanitizeFileName(juce::String name);

    ConnectionManager& connection;
    ModuleDescriptions& moduleDescs;

    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };
    bool busy = false;
    bool cancelRequested = false;
    bool savingToDisk = false;
    int bankSection = 0;
    int tempSlot = 0;
    int itemIndex = 0;
    int generation = 0;   // invalidates stale fetch callbacks and watchdogs
    std::vector<SaveItem> saveItems;
    juce::Array<juce::File> sendFiles;
    Progress progress;
    ProgressCallback progressCallback;

    static constexpr int fetchWatchdogMs = 12000;   // per-item fetch timeout
    static constexpr int interItemDelayMs = 150;    // breather between fetches
    static constexpr int postStoreDelayMs = 500;    // let StorePatch settle
    static constexpr int slotFocusDelayMs = 200;    // selectSlot → uploadPatch settle
};
