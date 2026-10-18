#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include "PchList.h"

class ConnectionManager;
class ModuleDescriptions;

// Orchestrates bulk patch transfers between a synth bank and a disk folder, the
// way Clavia's editor does it: straight to and from flash, never through a slot.
//
// Save direction: each bank position is read from flash (FlashDumpRequest),
// parsed, and written to disk as "NN - Name.pch", with a BankN.pchList beside
// them that keeps the positions.
// Send direction: each .pch file is parsed and written to its bank position
// (the upload's PatchName section names the position). Positions come from the
// folder's .pchList when it has one, otherwise from the files' names in order.
//
// One transfer runs at a time; all progress callbacks fire on the message
// thread. No slot changes on the synth.
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
    // The running transfer's progress, or the last one's once it has finished.
    const Progress& getProgress() const { return progress; }
    // Follow the synth being edited. Ignored mid-transfer: a transfer finishes on the synth it began on.
    void retarget(ConnectionManager& cm) { if (!busy) connection = &cm; }

    // Save every patch of bank `section` (0-8) into destFolder, plus its
    // BankN.pchList. With the patch list loaded only its names are asked for;
    // without it all 99 positions are, and empty ones answer as such.
    void saveBankToDisk(int section, const juce::File& destFolder, ProgressCallback cb);

    // Mirror-backup all 9 banks into banksRoot/Bank1..Bank9. The download goes
    // to a staging folder and is published over the existing mirror only once
    // every patch has been fetched, parsed and written. A backup that fails,
    // times out, is cancelled or loses the connection leaves the previous
    // mirror exactly as it was: it is the only copy the user still has.
    // Mirror semantics (dropping patches deleted on the synth) therefore apply
    // at publication, and only for a backup that completed against a loaded
    // patch list.
    void saveAllBanksToDisk(const juce::File& banksRoot, ProgressCallback cb);

    struct SendItem
    {
        int position = 0;   // 0-98
        juce::File file;
    };

    // What goes to a bank from `source`: a .pchList file, each patch to its
    // position; or a folder, through its .pchList when it has exactly one, else
    // by the "NN - " each name starts with in an NME backup, else its .pch files
    // sorted by name into positions 1..99. A folder with several
    // lists sends nothing until one is chosen. Files that cannot go (missing,
    // past position 99, a corrupt list) are named in `problems`.
    static std::vector<SendItem> itemsFrom(const juce::File& source,
                                           juce::StringArray& problems,
                                           juce::File& listUsed);

    // Write each file to its position in bank `section`. Existing patches at
    // those positions are overwritten; the others are left alone. Stops
    // cleanly on the first upload failure.
    void sendBankToSynth(const std::vector<SendItem>& items, int section, ProgressCallback cb);

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
    // Move a complete staged mirror over the live one. Returns false and leaves
    // both copies on disk if any bank could not be replaced.
    bool publishStagedMirror();
    void discardStagedMirror();

    void saveNextItem();
    void onFlashDump(const SaveItem& item, const std::vector<uint8_t>& patchData);
    void advanceSave();
    void writePchLists(const juce::File& rootOrFolder);
    void sendNextFile();
    void finishTransfer(bool cancelled);
    void reportProgress(const juce::String& itemName);
    static juce::String sanitizeFileName(juce::String name);

    ConnectionManager* connection;   // the synth being edited; retargeted when that changes
    ModuleDescriptions& moduleDescs;

    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool>>(true) };
    bool busy = false;
    bool cancelRequested = false;
    bool savingToDisk = false;
    int bankSection = 0;
    int itemIndex = 0;
    int generation = 0;   // invalidates stale fetch callbacks and watchdogs
    std::vector<SaveItem> saveItems;
    // Set only for an all-banks backup: where the download is being written and
    // where it belongs once complete. Empty for a single-bank save, which
    // writes straight to the folder the user picked and adds nothing to delete.
    juce::File mirrorRoot;
    juce::File stagingRoot;
    std::vector<SendItem> sendItems;
    // File written per bank and position during a save, for the .pchList files.
    std::array<PchList::Entries, 9> savedEntries;
    juce::File singleBankFolder;   // set for a single-bank save
    Progress progress;
    ProgressCallback progressCallback;

    // A G1 answers a flash request in 35-100 ms (bank 8, 2026-10-18); this only
    // has to tell a lost answer from a slow one.
    static constexpr int fetchWatchdogMs = 3000;
    // The last ACK of a bank write already waits for the flash, so this is only
    // a breather before the next patch.
    static constexpr int postWriteDelayMs = 50;
};
