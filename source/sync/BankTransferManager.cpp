#include "BankTransferManager.h"
#include "../midi/ConnectionManager.h"
#include "../model/PatchParser.h"
#include "../model/PchFileIO.h"

#include <algorithm>
#include <iostream>

BankTransferManager::BankTransferManager(ConnectionManager& cm, ModuleDescriptions& descs)
    : connection(&cm), moduleDescs(descs)
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
        for (const auto& old : live.findChildFiles(juce::File::findFiles, false, "*.pch;*.pchList"))
            if (!old.deleteFile())
                allMoved = false;

        for (const auto& file : staged.findChildFiles(juce::File::findFiles, false, "*.pch;*.pchList"))
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
    connection->cancelFlashDump();
    connection->setBankUploadResultCallback(nullptr);

    if (savingToDisk && singleBankFolder != juce::File())
        writePchLists(singleBankFolder);

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
        else
        {
            writePchLists(stagingRoot);
            if (!publishStagedMirror())
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

    // A send changed the bank names; a save changed nothing on the synth.
    if (!savingToDisk && connection->isConnected())
    {
        auto aliveFlag = alive;
        auto* conn = connection;
        juce::Timer::callAfterDelay(500, [aliveFlag, conn]() {
            if (*aliveFlag && conn->isConnected())
                conn->requestPatchList();
        });
    }
}

// One BankN.pchList per bank folder that got patches, naming them by position.
// `rootOrFolder` is the single-bank folder, or the root holding Bank1..Bank9.
void BankTransferManager::writePchLists(const juce::File& rootOrFolder)
{
    for (int section = 0; section < 9; ++section)
    {
        const auto& entries = savedEntries[static_cast<size_t>(section)];
        if (std::all_of(entries.begin(), entries.end(), [](const juce::String& e) { return e.isEmpty(); }))
            continue;

        const auto folder = rootOrFolder == singleBankFolder
                              ? rootOrFolder
                              : rootOrFolder.getChildFile("Bank" + juce::String(section + 1));
        const auto list = folder.getChildFile("Bank" + juce::String(section + 1) + ".pchList");
        if (!list.replaceWithText(PchList::write(entries), false, false, nullptr))
            progress.failures.add(list.getFileName() + " (could not write)");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Save bank → disk

void BankTransferManager::saveBankToDisk(int section, const juce::File& folder, ProgressCallback cb)
{
    if (busy || !connection->isConnected())
        return;

    savingToDisk = true;
    bankSection = juce::jlimit(0, 8, section);
    progressCallback = std::move(cb);
    // Saving one bank adds files to the folder the user picked and deletes
    // nothing, so there is no staged mirror to publish or roll back.
    mirrorRoot = juce::File();
    stagingRoot = juce::File();
    singleBankFolder = folder;

    saveItems.clear();
    const bool listLoaded = connection->isPatchListLoaded();
    const auto& list = connection->getPatchList();
    for (int pos = 0; pos < 99; ++pos)
    {
        const size_t idx = static_cast<size_t>(bankSection * 99 + pos);
        const juce::String name(idx < list.size() ? list[idx] : std::string());
        // Without the list nothing says which positions are empty, so all are
        // asked for: an empty one answers in a few ms.
        if (!listLoaded || name.isNotEmpty())
            saveItems.push_back({ bankSection, pos, name, name, folder });
    }

    folder.createDirectory();

    std::cout << "[BANKXFER] Saving bank " << (bankSection + 1) << " ("
              << saveItems.size() << " positions) to "
              << folder.getFullPathName() << std::endl;

    beginSaveTransfer();
}

void BankTransferManager::saveAllBanksToDisk(const juce::File& banksRoot, ProgressCallback cb)
{
    if (busy || !connection->isConnected())
        return;

    // Mirror semantics need to know which positions are genuinely empty. An
    // unloaded list reads as all-empty, which would publish an empty backup
    // over a good one.
    if (!connection->isPatchListLoaded())
    {
        std::cout << "[BANKXFER] Refusing all-banks backup: patch list not loaded"
                  << std::endl;
        return;
    }

    savingToDisk = true;
    progressCallback = std::move(cb);
    singleBankFolder = juce::File();

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
    const auto& list = connection->getPatchList();
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
    for (auto& entries : savedEntries)
        entries.fill({});

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
        || !connection->isConnected())
    {
        finishTransfer(cancelRequested);
        return;
    }

    const auto item = saveItems[static_cast<size_t>(itemIndex)];
    reportProgress(item.label.isNotEmpty() ? item.label
                                           : "Position " + juce::String(item.position + 1));

    const int gen = ++generation;
    auto aliveFlag = alive;

    // The answer arrives on the MIDI thread — bounce first, then validate the
    // generation so a late answer after its watchdog is ignored.
    connection->requestFlashDump(item.section, item.position,
        [this, aliveFlag, gen, item](int, int, const std::vector<uint8_t>& patchData) {
            juce::MessageManager::callAsync([this, aliveFlag, gen, item, patchData]() {
                if (!*aliveFlag || gen != generation || !busy)
                    return;
                ++generation;  // disarm the watchdog
                onFlashDump(item, patchData);
            });
        });

    juce::Timer::callAfterDelay(fetchWatchdogMs, [this, aliveFlag, gen, item]() {
        if (!*aliveFlag || gen != generation || !busy)
            return;
        ++generation;
        connection->cancelFlashDump();

        std::cout << "[BANKXFER] No answer for bank " << (item.section + 1)
                  << ", position " << (item.position + 1) << std::endl;

        progress.failures.add((item.label.isNotEmpty() ? item.label
                                                       : "Position " + juce::String(item.position + 1))
                              + " (timeout)");
        advanceSave();
    });
}

void BankTransferManager::onFlashDump(const SaveItem& item, const std::vector<uint8_t>& patchData)
{
    // An empty position: nothing to write, and not a failure (the list may be
    // stale, or a full-bank scan is asking every position).
    if (!patchData.empty())
    {
        PatchParser parser(moduleDescs);
        auto patch = parser.parse({ patchData });

        bool ok = false;
        juce::String name = item.name;
        if (patch)
        {
            if (patch->getName().isNotEmpty())
                name = patch->getName();
            const auto fileName = juce::String(item.position + 1).paddedLeft('0', 2)
                                  + " - " + sanitizeFileName(name) + ".pch";
            PchFileIO io(moduleDescs);
            ok = io.writeFile(*patch, item.folder.getChildFile(fileName));
            if (ok)
                savedEntries[static_cast<size_t>(item.section)][static_cast<size_t>(item.position)] = fileName;
        }

        if (!ok)
            progress.failures.add(item.label.isNotEmpty() ? item.label : name);
    }

    advanceSave();
}

void BankTransferManager::advanceSave()
{
    ++itemIndex;
    ++progress.current;
    reportProgress({});

    // Straight on: the synth has answered, so the wire is free.
    auto aliveFlag = alive;
    juce::MessageManager::callAsync([this, aliveFlag]() {
        if (*aliveFlag && busy)
            saveNextItem();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Send disk → bank

std::vector<BankTransferManager::SendItem>
BankTransferManager::itemsFrom(const juce::File& source, juce::StringArray& problems,
                               juce::File& listUsed)
{
    std::vector<SendItem> items;
    listUsed = juce::File();

    juce::File list, folder = source;
    if (source.existsAsFile())
    {
        list = source;
        folder = source.getParentDirectory();
    }
    else
    {
        const auto lists = PchList::findIn(folder);
        if (lists.size() > 1)
        {
            juce::StringArray names;
            for (const auto& l : lists)
                names.add(l.getFileName());
            problems.add("This folder has " + juce::String(lists.size()) + " lists ("
                         + names.joinIntoString(", ") + "): choose the one to send");
            return items;
        }
        if (lists.size() == 1)
            list = lists.getFirst();
    }

    if (list.existsAsFile())
    {
        PchList::Entries entries;
        juce::String error;
        if (!PchList::parse(list.loadFileAsString(), entries, error))
        {
            problems.add(list.getFileName() + ": " + error);
            return items;
        }

        listUsed = list;
        for (int pos = 0; pos < PchList::positions; ++pos)
        {
            const auto& name = entries[static_cast<size_t>(pos)];
            if (name.isEmpty())
                continue;
            const auto file = folder.getChildFile(name);
            if (file.existsAsFile())
                items.push_back({ pos, file });
            else
                problems.add(name + " (not in the folder)");
        }
        return items;
    }

    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.pch");
    files.sort();

    // An NME backup from before the lists: every name starts "NN - ", and NN is
    // the position.
    auto prefixPosition = [](const juce::File& f) {
        const auto name = f.getFileName();
        const bool prefixed = name.length() > 5 && name.substring(0, 2).containsOnly("0123456789")
                              && name.substring(2, 5) == " - ";
        const int n = prefixed ? name.substring(0, 2).getIntValue() : 0;
        return n >= 1 && n <= PchList::positions ? n - 1 : -1;
    };
    if (!files.isEmpty()
        && std::all_of(files.begin(), files.end(), [&](const juce::File& f) { return prefixPosition(f) >= 0; }))
    {
        for (const auto& f : files)
            items.push_back({ prefixPosition(f), f });
        return items;
    }

    for (int i = 0; i < files.size(); ++i)
    {
        if (i < PchList::positions)
            items.push_back({ i, files[i] });
        else
            problems.add(files[i].getFileName() + " (bank full)");
    }
    return items;
}

void BankTransferManager::sendBankToSynth(const std::vector<SendItem>& items, int section,
                                          ProgressCallback cb)
{
    if (busy || !connection->isConnected())
        return;

    savingToDisk = false;
    bankSection = juce::jlimit(0, 8, section);
    progressCallback = std::move(cb);
    mirrorRoot = juce::File();
    stagingRoot = juce::File();
    singleBankFolder = juce::File();

    sendItems = items;
    progress = {};
    progress.total = static_cast<int>(sendItems.size());

    if (sendItems.empty())
    {
        finishTransfer(false);
        return;
    }

    busy = true;
    cancelRequested = false;
    itemIndex = 0;

    std::cout << "[BANKXFER] Sending " << sendItems.size() << " patches to bank "
              << (bankSection + 1) << std::endl;

    sendNextFile();
}

void BankTransferManager::sendNextFile()
{
    if (cancelRequested || itemIndex >= static_cast<int>(sendItems.size())
        || !connection->isConnected())
    {
        finishTransfer(cancelRequested);
        return;
    }

    // Let a queued edit drain first: an upload clears the ACK queue, so
    // starting early would silently drop it. The queue's own 3 s ACK timeout
    // bounds this wait.
    if (!connection->isAckedQueueIdle())
    {
        auto aliveWait = alive;
        juce::Timer::callAfterDelay(150, [this, aliveWait]() {
            if (*aliveWait && busy)
                sendNextFile();
        });
        return;
    }

    const auto item = sendItems[static_cast<size_t>(itemIndex)];
    const auto file = item.file;
    reportProgress(file.getFileNameWithoutExtension());

    // The "NN - " prefix a bank backup's file name carries is stripped by
    // PchFileIO::patchNameFromFileName, so the synth gets the patch's own name.
    PchFileIO io(moduleDescs);
    std::shared_ptr<Patch> patch = io.readFile(file);

    if (!patch)
    {
        progress.failures.add(file.getFileName() + " (parse failed)");
        ++itemIndex;
        ++progress.current;
        auto aliveFlag = alive;
        juce::MessageManager::callAsync([this, aliveFlag]() {
            if (*aliveFlag && busy)
                sendNextFile();
        });
        return;
    }

    const int gen = ++generation;
    auto aliveFlag = alive;

    // Fires on the message thread: true once every packet is ACKed, false on
    // an ACK timeout. A failure stops the transfer.
    connection->setBankUploadResultCallback([this, aliveFlag, gen, file](bool success) {
        if (!*aliveFlag || gen != generation || !busy)
            return;
        ++generation;

        if (!success)
        {
            progress.failures.add(file.getFileName() + " (upload failed)");
            finishTransfer(false);
            return;
        }

        ++itemIndex;
        ++progress.current;
        reportProgress(file.getFileNameWithoutExtension());

        juce::Timer::callAfterDelay(postWriteDelayMs, [this, aliveFlag]() {
            if (*aliveFlag && busy)
                sendNextFile();
        });
    });

    connection->uploadPatchToBank(bankSection, item.position, *patch);
}
