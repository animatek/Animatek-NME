#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "midi/ConnectionManager.h"
#include "sync/BankTransferManager.h"

// An all-banks backup used to delete the previous mirror before fetching the
// new one, so a run that failed halfway destroyed the only copy the user had.
// These tests pin the cheap half of that: a backup that never starts must not
// touch a single existing file.
//
// Not covered here, and deliberately not claimed: cancellation after the first
// item, a disconnect mid-download, a partial fetch and a publication failure.
// Those need a patch list on the connection, which today can only arrive by
// hand-building a PatchListResponse ACK. That seam is T1 work in
// docs/POST_0180_PLAN.md; until it exists those paths rest on the staging
// design and on hardware testing.

namespace
{
ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

// A previous, good backup: one patch in Bank1 and one in Bank3.
struct ExistingMirror
{
    juce::File root;
    juce::Array<juce::File> files;

    ExistingMirror()
        : root(juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("nme-bank-backup-" + juce::Uuid().toString()))
    {
        for (const auto& [bank, name] : { std::pair<int, const char*>{1, "01 - Bass.pch"},
                                          std::pair<int, const char*>{3, "07 - Lead.pch"} })
        {
            const auto folder = root.getChildFile("Bank" + juce::String(bank));
            folder.createDirectory();
            const auto file = folder.getChildFile(name);
            // No newline: replaceWithText() rewrites line endings to CRLF by default,
            // which would make the comparison below about JUCE, not about the backup.
            file.replaceWithText("previous backup");
            files.add(file);
        }
    }

    ~ExistingMirror() { root.deleteRecursively(); }

    bool intact() const
    {
        for (const auto& f : files)
            if (!f.existsAsFile() || f.loadFileAsString() != "previous backup")
                return false;
        return true;
    }

    bool hasStagingLeftovers() const
    {
        return root.getChildFile(".nme-backup-staging").exists();
    }
};

// The connection, minus the MIDI ports. Connected, but no patch list has been
// fetched, which is the state the guard is about.
struct Recorder
{
    ConnectionManager connection;
    std::vector<std::vector<uint8_t>> frames;

    void connect()
    {
        connection.getProtocol().setSendFunction(
            [this](const std::vector<uint8_t>& frame) { frames.push_back(frame); });
        const auto iam = SysEx::encode(NmCmd::IAm, 0, {1, 3, 3}, false);
        connection.getProtocol().processIncoming(iam.data(), iam.size());
    }
};
}  // namespace

TEST_CASE("An all-banks backup with no patch list leaves the previous one alone")
{
    ExistingMirror mirror;
    REQUIRE(mirror.intact());

    Recorder rec;
    rec.connect();
    REQUIRE(rec.connection.isConnected());
    // Nothing has fetched the list, so every position reads as empty. Mirroring
    // that would publish an empty backup over a good one.
    REQUIRE_FALSE(rec.connection.isPatchListLoaded());

    BankTransferManager transfer(rec.connection, descriptions());
    bool reported = false;
    transfer.saveAllBanksToDisk(mirror.root, 0, [&](const BankTransferManager::Progress&) {
        reported = true;
    });

    CHECK_FALSE(transfer.isBusy());
    CHECK_FALSE(reported);
    CHECK(mirror.intact());
    CHECK_FALSE(mirror.hasStagingLeftovers());
}

TEST_CASE("An all-banks backup with no connection leaves the previous one alone")
{
    ExistingMirror mirror;
    REQUIRE(mirror.intact());

    ConnectionManager connection;
    REQUIRE_FALSE(connection.isConnected());

    BankTransferManager transfer(connection, descriptions());
    transfer.saveAllBanksToDisk(mirror.root, 0, nullptr);

    CHECK_FALSE(transfer.isBusy());
    CHECK(mirror.intact());
    CHECK_FALSE(mirror.hasStagingLeftovers());
}
