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

void BankTransferManager::finishTransfer(bool cancelled)
{
    ++generation;
    busy = false;
    connection.setBankFetchCallback(nullptr);
    connection.setBankUploadResultCallback(nullptr);

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

    savingToDisk = true;
    tempSlot = juce::jlimit(0, 3, slot);
    progressCallback = std::move(cb);

    saveItems.clear();
    const auto& list = connection.getPatchList();
    for (int section = 0; section < 9; ++section)
    {
        const auto folder = banksRoot.getChildFile("Bank" + juce::String(section + 1));
        folder.createDirectory();

        // Mirror semantics: the folder must reflect the bank exactly, so
        // patches deleted on the synth do not linger from older backups.
        for (const auto& old : folder.findChildFiles(juce::File::findFiles, false, "*.pch"))
            old.deleteFile();

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

    // Classic .pch files carry no internal name, so the reader falls back to
    // the filename. Our bank backups are named "NN - Name.pch" — strip the
    // position prefix or the synth stores names like "86 - DoubleSawPa".
    if (patch)
    {
        const auto name = patch->getName();
        if (name.length() > 5
            && juce::CharacterFunctions::isDigit(name[0])
            && juce::CharacterFunctions::isDigit(name[1])
            && name.substring(2, 5) == " - ")
        {
            patch->setName(name.substring(5));
        }
    }

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
