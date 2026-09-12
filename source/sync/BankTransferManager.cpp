#include "BankTransferManager.h"
#include "../midi/ConnectionManager.h"
#include "../model/PatchParser.h"
#include "../model/PchFileIO.h"
#include "../protocol/StorePatchMessage.h"

#include <iostream>

BankTransferManager::BankTransferManager(ConnectionManager& cm, ModuleDescriptions& descs)
    : connection(cm), moduleDescs(descs)
{
}

BankTransferManager::~BankTransferManager()
{
    *alive = false;
}

juce::String BankTransferManager::sanitizeFileName(juce::String name)
{
    name = name.trim();
    for (auto c : juce::String("\\/:*?\"<>|"))
        name = name.replaceCharacter(c, '_');
    return name.isEmpty() ? juce::String("Untitled") : name;
}

void BankTransferManager::reportProgress(const juce::String& itemName)
{
    progress.itemName = itemName;
    if (progressCallback)
        progressCallback(progress);
}

void BankTransferManager::discardStagedMirror()
{
    if (stagingRoot != juce::File() && stagingRoot.exists())
        stagingRoot.deleteRecursively();
    stagingRoot = juce::File();
    mirrorRoot = juce::File();
}

bool BankTransferManager::publishStagedMirror()
{
    bool allMoved = true;

    for (int section = 0; section < 9; ++section)
    {
        const auto name = "Bank" + juce::String(section + 1);
        const auto staged = stagingRoot.getChildFile(name);
        const auto live = mirrorRoot.getChildFile(name);
        live.createDirectory();

        // Mirror semantics, applied here rather than before the download: the
        // bank folder ends up holding exactly what this completed backup
        // fetched, so patches deleted on the synth do not linger. Positions the
        // synth no longer has were never staged.
        for (const auto& old : live.findChildFiles(juce::File::findFiles, false, "*.pch"))
            if (!old.deleteFile())
                allMoved = false;

        for (const auto& file : staged.findChildFiles(juce::File::findFiles, false, "*.pch"))
            if (!file.moveFileTo(live.getChildFile(file.getFileName())))
                allMoved = false;
    }

    if (allMoved)
    {
        discardStagedMirror();
        return true;
    }

    // Publication is local file moves, so this is rare: a locked file, a full
    // disk, permissions. Keep the staged copy rather than delete it — it is the
    // complete backup, and the alternative is throwing it away.
    std::cout << "[BANKXFER] Could not publish the backup. The complete copy is at "
              << stagingRoot.getFullPathName() << std::endl;
    stagingRoot = juce::File();
    mirrorRoot = juce::File();
    return false;
}

void BankTransferManager::finishTransfer(bool cancelled)
{
    ++generation;
    busy = false;
    connection.setBankFetchCallback(nullptr);
    connection.setBankUploadResultCallback(nullptr);

    if (savingToDisk && stagingRoot != juce::File())
    {
        // Every item fetched, parsed and written, nothing cancelled, and the
        // run reached the end of the list. A disconnect stops saveNextItem()
        // early without recording a failure, so the count is what catches it.
        const bool everyItemDone = itemIndex >= static_cast<int>(saveItems.size());
        const bool complete = !cancelled && progress.failures.isEmpty() && everyItemDone;

        if (!complete)
        {
            const int missing = static_cast<int>(saveItems.size()) - itemIndex;
            std::cout << "[BANKXFER] Incomplete backup ("
                      << progress.failures.size() << " failures, "
                      << juce::jmax(0, missing) << " never fetched"
                      << (cancelled ? ", cancelled" : "")
                      << "): keeping the previous backup untouched" << std::endl;
            discardStagedMirror();
            progress.failures.add("Backup incomplete: the previous one was kept");
        }
        else if (!publishStagedMirror())
        {
            progress.failures.add("Backup complete but could not replace the old files");
        }
    }

    progress.finished = true;
    progress.cancelled = cancelled;
    progress.itemName.clear();
    if (progressCallback)
        progressCallback(progress);
    progressCallback = nullptr;

    std::cout << "[BANKXFER] Finished: " << progress.current << "/" << progress.total
              << " items, " << progress.failures.size() << " failures"
              << (cancelled ? " (cancelled)" : "") << std::endl;

    // The synth's temp slot now holds the last transferred patch — re-fetch it
    // so the editor model matches, then refresh bank names after a send.
    if (connection.isConnected())
    {
        connection.requestPatch(tempSlot);

        if (!savingToDisk)
        {
            auto aliveFlag = alive;
            auto* conn = &connection;
            juce::Timer::callAfterDelay(1500, [aliveFlag, conn]() {
                if (*aliveFlag && conn->isConnected())
                    conn->requestPatchList();
            });
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Save bank → disk

void BankTransferManager::saveBankToDisk(int section, const juce::File& folder,
                                         int slot, ProgressCallback cb)
{
    if (busy || !connection.isConnected())
        return;

    savingToDisk = true;
    bankSection = juce::jlimit(0, 8, section);
    tempSlot = juce::jlimit(0, 3, slot);
    progressCallback = std::move(cb);
    // Saving one bank adds files to the folder the user picked and deletes
    // nothing, so there is no staged mirror to publish or roll back.
    mirrorRoot = juce::File();
    stagingRoot = juce::File();

    saveItems.clear();
    const auto& list = connection.getPatchList();
    for (int pos = 0; pos < 99; ++pos)
    {
        const size_t idx = static_cast<size_t>(bankSection * 99 + pos);
        if (idx < list.size() && !list[idx].empty())
        {
            const juce::String name(list[idx]);
            saveItems.push_back({ bankSection, pos, name, name, folder });
        }
    }

    folder.createDirectory();

    std::cout << "[BANKXFER] Saving bank " << (bankSection + 1) << " ("
              << saveItems.size() << " patches) to "
              << folder.getFullPathName() << std::endl;

    beginSaveTransfer();
}

void BankTransferManager::saveAllBanksToDisk(const juce::File& banksRoot, int slot,
                                             ProgressCallback cb)
{
    if (busy || !connection.isConnected())
        return;

    // Mirror semantics need to know which positions are genuinely empty. An
    // unloaded list reads as all-empty, which would publish an empty backup
    // over a good one.
    if (!connection.isPatchListLoaded())
    {
        std::cout << "[BANKXFER] Refusing all-banks backup: patch list not loaded"
                  << std::endl;
        return;
    }

    savingToDisk = true;
    tempSlot = juce::jlimit(0, 3, slot);
    progressCallback = std::move(cb);

    // Download into a staging folder beside the mirror. Nothing under
    // Bank1..Bank9 is touched until the whole backup is on disk: a backup that
    // fails halfway used to delete the previous one on its way in, so a bad
    // cable or a synth that stopped answering cost you the only copy you had.
    mirrorRoot = banksRoot;
    stagingRoot = banksRoot.getChildFile(".nme-backup-staging");
    if (stagingRoot.exists())
        stagingRoot.deleteRecursively();   // leftovers from an interrupted run
    stagingRoot.createDirectory();

    saveItems.clear();
    const auto& list = connection.getPatchList();
    for (int section = 0; section < 9; ++section)
    {
        const auto folder = stagingRoot.getChildFile("Bank" + juce::String(section + 1));
        folder.createDirectory();

        for (int pos = 0; pos < 99; ++pos)
        {
            const size_t idx = static_cast<size_t>(section * 99 + pos);
            if (idx < list.size() && !list[idx].empty())
            {
                const juce::String name(list[idx]);
                saveItems.push_back({ section, pos, name,
                                      "B" + juce::String(section + 1) + ": " + name,
                                      folder });
            }
        }
    }

    std::cout << "[BANKXFER] Backing up all banks (" << saveItems.size()
              << " patches) to " << banksRoot.getFullPathName() << std::endl;

    beginSaveTransfer();
}

void BankTransferManager::beginSaveTransfer()
{
    progress = {};
    progress.total = static_cast<int>(saveItems.size());

    if (saveItems.empty())
    {
        finishTransfer(false);
        return;
    }

    busy = true;
    cancelRequested = false;
    itemIndex = 0;
    saveNextItem();
}

void BankTransferManager::saveNextItem()
{
    if (cancelRequested || itemIndex >= static_cast<int>(saveItems.size())
        || !connection.isConnected())
    {
        finishTransfer(cancelRequested);
        return;
    }

    const auto item = saveItems[static_cast<size_t>(itemIndex)];
    reportProgress(item.label);

    const int gen = ++generation;
    auto aliveFlag = alive;

    // finalizePatch() may fire this off the message thread — bounce first,
    // then validate the generation so stale fetches and watchdogs are ignored.
    connection.setBankFetchCallback(
        [this, aliveFlag, gen, item](const std::vector<std::vector<uint8_t>>& sections, int) {
            auto sectionsCopy = sections;
            juce::MessageManager::callAsync(
                [this, aliveFlag, gen, item, sectionsCopy = std::move(sectionsCopy)]() {
                    if (!*aliveFlag || gen != generation || !busy)
                        return;
                    ++generation;  // disarm the watchdog

                    PatchParser parser(moduleDescs);
                    auto patch = parser.parse(sectionsCopy);

                    bool ok = false;
                    if (patch)
                    {
                        const auto file = item.folder.getChildFile(
                            juce::String(item.position + 1).paddedLeft('0', 2)
                            + " - " + sanitizeFileName(item.name) + ".pch");
                        PchFileIO io(moduleDescs);
                        ok = io.writeFile(*patch, file);
                    }

                    if (!ok)
                        progress.failures.add(item.label);

                    ++itemIndex;
                    ++progress.current;
                    reportProgress(item.label);

                    juce::Timer::callAfterDelay(interItemDelayMs, [this, aliveFlag]() {
                        if (*aliveFlag && busy)
                            saveNextItem();
                    });
                });
        });

    connection.loadPatchFromBank(item.section, item.position, tempSlot);

    juce::Timer::callAfterDelay(fetchWatchdogMs, [this, aliveFlag, gen, item]() {
        if (!*aliveFlag || gen != generation || !busy)
            return;
        ++generation;
        connection.setBankFetchCallback(nullptr);

        std::cout << "[BANKXFER] Fetch timeout for \"" << item.name.toStdString()
                  << "\" (bank " << (item.section + 1) << ", pos "
                  << (item.position + 1) << ")" << std::endl;

        progress.failures.add(item.label + " (timeout)");
        ++itemIndex;
        ++progress.current;
        saveNextItem();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Send disk → bank

void BankTransferManager::sendBankToSynth(const juce::Array<juce::File>& files,
                                          int section, int slot, ProgressCallback cb)
{
    if (busy || !connection.isConnected())
        return;

    savingToDisk = false;
    bankSection = juce::jlimit(0, 8, section);
    tempSlot = juce::jlimit(0, 3, slot);
    progressCallback = std::move(cb);
    mirrorRoot = juce::File();
    stagingRoot = juce::File();

    sendFiles = files;
    sendFiles.sort();

    progress = {};

    // A bank holds 99 positions — anything beyond that can never be stored.
    while (sendFiles.size() > 99)
    {
        progress.failures.add(sendFiles.getLast().getFileName() + " (bank full)");
        sendFiles.removeLast();
    }

    progress.total = sendFiles.size();

    if (sendFiles.isEmpty())
    {
        finishTransfer(false);
        return;
    }

    busy = true;
    cancelRequested = false;
    itemIndex = 0;

    std::cout << "[BANKXFER] Sending " << sendFiles.size() << " patches to bank "
              << (bankSection + 1) << std::endl;

    sendNextFile();
}

void BankTransferManager::sendNextFile()
{
    if (cancelRequested || itemIndex >= sendFiles.size() || !connection.isConnected())
    {
        finishTransfer(cancelRequested);
        return;
    }

    // Let the previous StorePatch drain first: uploadPatch() clears the ACK
    // queue, so starting the next upload early would silently drop the store.
    // The queue's own 3 s ACK timeout bounds this wait.
    if (!connection.isAckedQueueIdle())
    {
        auto aliveWait = alive;
        juce::Timer::callAfterDelay(150, [this, aliveWait]() {
            if (*aliveWait && busy)
                sendNextFile();
        });
        return;
    }

    const auto file = sendFiles[itemIndex];
    const int position = itemIndex;
    reportProgress(file.getFileNameWithoutExtension());

    PchFileIO io(moduleDescs);
    std::shared_ptr<Patch> patch = io.readFile(file);

    // The "NN - " prefix a bank backup's file name carries is stripped by
    // PchFileIO::patchNameFromFileName now, so every way of opening one lands on
    // the same name rather than only this path getting it right.

    if (!patch)
    {
        progress.failures.add(file.getFileName() + " (parse failed)");
        ++itemIndex;
        ++progress.current;
        auto aliveFlag = alive;
        juce::Timer::callAfterDelay(interItemDelayMs, [this, aliveFlag]() {
            if (*aliveFlag && busy)
                sendNextFile();
        });
        return;
    }

    const int gen = ++generation;
    auto aliveFlag = alive;

    // Fires on the message thread: true once every section is ACKed, false on
    // an ACK timeout. The roadmap calls for a clean stop on failure.
    connection.setBankUploadResultCallback([this, aliveFlag, gen, file, position](bool success) {
        if (!*aliveFlag || gen != generation || !busy)
            return;
        ++generation;

        if (!success)
        {
            progress.failures.add(file.getFileName() + " (upload failed)");
            finishTransfer(false);
            return;
        }

        StorePatchMessage store(tempSlot, bankSection, position);
        connection.sendAckedSysEx(store.toSysEx(tempSlot));

        ++itemIndex;
        ++progress.current;
        reportProgress(file.getFileNameWithoutExtension());

        juce::Timer::callAfterDelay(postStoreDelayMs, [this, aliveFlag]() {
            if (*aliveFlag && busy)
                sendNextFile();
        });
    });

    // The G1 only accepts working-memory uploads reliably into the focused
    // slot (issue #1) — focus it and give the synth a moment to settle,
    // mirroring the proven single-file disk upload path.
    connection.selectSlot(tempSlot);
    juce::Timer::callAfterDelay(slotFocusDelayMs, [this, aliveFlag, gen, patch]() {
        if (!*aliveFlag || !busy || gen != generation)
            return;
        if (!connection.isConnected())
        {
            finishTransfer(false);
            return;
        }
        connection.uploadPatch(tempSlot, *patch);
    });
}
