#include <doctest.h>
#include "midi/ConnectionManager.h"
#include "model/Patch.h"
#include "model/PatchParser.h"
#include "model/PatchSerializer.h"
#include "model/PchFileIO.h"
#include "model/ModuleDescriptions.h"
#include "sync/PchList.h"
#include "sync/BankTransferManager.h"

// Banks go to and from flash the way Clavia's editor 3.03 does it, without a
// slot. The frames marked "wire" are copied from a capture of that editor
// against a real G1 (G1originaleditor/captures/*.banco*.txt, 2026-10-18).

using Frames = std::vector<std::vector<uint8_t>>;

namespace
{
void feed(ConnectionManager& connection, const std::vector<uint8_t>& frame)
{
    connection.getProtocol().processIncoming(frame.data(), frame.size());
}

void connectRecorder(ConnectionManager& connection, Frames& frames)
{
    connection.getProtocol().setSendFunction(
        [&frames](const std::vector<uint8_t>& frame) { frames.push_back(frame); });
    const auto iam = SysEx::encode(NmCmd::IAm, 0, {1, 3, 3}, false);
    feed(connection, iam);
    REQUIRE(connection.isConnected());
}

// The lower 7 bits of each byte, as one stream cut to whole bytes.
std::vector<uint8_t> unpack7(const uint8_t* data, size_t count)
{
    std::vector<uint8_t> out;
    uint32_t buffer = 0;
    int held = 0;
    for (size_t i = 0; i < count; ++i)
    {
        buffer = (buffer << 7) | (data[i] & 0x7F);
        held += 7;
        if (held >= 8)
        {
            held -= 8;
            out.push_back(static_cast<uint8_t>((buffer >> held) & 0xFF));
        }
    }
    return out;
}

// What the synth sends for an occupied position: one PatchPacket (cc 0x1e)
// whose stream is the 0x18 section and then the patch's sections.
std::vector<uint8_t> flashDumpReply(int section, int position, const std::string& name,
                                    const Patch& patch)
{
    std::vector<uint8_t> raw { 0x18, 0x02, static_cast<uint8_t>(section),
                               static_cast<uint8_t>(position) };
    raw.insert(raw.end(), name.begin(), name.end());
    raw.push_back(0);
    PatchSerializer serializer;
    const auto sections = serializer.serializeForUpload(patch);
    for (size_t i = 1; i < sections.size(); ++i)   // [0] is the slot's PatchName
        raw.insert(raw.end(), sections[i].begin(), sections[i].end());

    std::vector<uint8_t> payload { 0x28 };
    const auto packed = UploadPacketizer::pack7Bit(raw.data(), raw.size());
    payload.insert(payload.end(), packed.begin(), packed.end());
    return SysEx::encode(0x1e, 0, payload, true);
}
}  // namespace

TEST_CASE("A flash dump request is the frame Clavia's editor sends")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    frames.clear();

    connection.requestFlashDump(7, 0, [](int, int, const std::vector<uint8_t>&) {});
    REQUIRE(frames.size() == 1);
    CHECK(frames.front() == std::vector<uint8_t>{ 0xF0, 0x33, 0x5C, 0x06, 0x41, 0x17,
                                                  0x07, 0x00, 0x64, 0xF7 });  // wire
}

TEST_CASE("An empty bank position answers with an empty dump")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);

    int calls = 0;
    std::vector<uint8_t> got { 1 };
    connection.requestFlashDump(7, 5, [&](int section, int position, const std::vector<uint8_t>& data) {
        ++calls;
        CHECK(section == 7);
        CHECK(position == 5);
        got = data;
    });

    // An empty answer for another position belongs to nobody here.
    feed(connection, SysEx::encode(NmCmd::ACK, 0, {0x28, 0x18, 0x04, 0x07, 0x06}, true));
    CHECK(calls == 0);

    feed(connection, { 0xF0, 0x33, 0x58, 0x06, 0x28, 0x18, 0x04, 0x07, 0x05, 0x51, 0xF7 });  // wire
    CHECK(calls == 1);
    CHECK(got.empty());
}

TEST_CASE("An occupied bank position answers with a patch the parser reads")
{
    static ModuleDescriptions descs;
    static const bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);

    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);

    std::vector<uint8_t> got;
    int calls = 0;
    connection.requestFlashDump(7, 0, [&](int, int, const std::vector<uint8_t>& data) {
        ++calls;
        got = data;
    });

    feed(connection, flashDumpReply(7, 0, "Fellowship", Patch{}));
    REQUIRE(calls == 1);
    REQUIRE_FALSE(got.empty());

    FlashDumpReply reply;
    REQUIRE(FlashDumpReply::peek(got, reply));
    CHECK(reply.section == 7);
    CHECK(reply.position == 0);

    PatchParser parser(descs);
    const auto patch = parser.parse({ got });
    REQUIRE(patch != nullptr);
    CHECK(patch->getName() == "Fellowship");

    // Once answered, a repeat is ordinary traffic again.
    feed(connection, flashDumpReply(7, 0, "Fellowship", Patch{}));
    CHECK(calls == 1);
}

TEST_CASE("A patch sent to a bank names its position in the PatchName section")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    frames.clear();

    Patch patch;
    patch.setName("Control");
    connection.uploadPatchToBank(0, 0, patch);
    REQUIRE(frames.size() == 1);
    const auto& frame = frames.front();
    CHECK((frame[2] & 0x03) == 0);   // slot 0 in the header, as the original
    const auto raw = unpack7(frame.data() + 5, frame.size() - 7);

    // wire: 37 01 00 00 "Control" 00 21 ...
    const std::vector<uint8_t> opening { 0x37, 0x01, 0x00, 0x00, 'C', 'o', 'n', 't',
                                         'r', 'o', 'l', 0x00, 0x21 };
    REQUIRE(raw.size() > opening.size());
    CHECK(std::equal(opening.begin(), opening.end(), raw.begin()));
}

TEST_CASE("A patch sent to a slot keeps the three zeros")
{
    PatchSerializer serializer;
    Patch patch;
    patch.setName("A");
    const auto name = serializer.serializeForUpload(patch).front();
    CHECK(name == std::vector<uint8_t>{ 0x37, 0x00, 0x00, 0x00, 'A', 0x00 });
}

TEST_CASE("A .pchList reads like Clavia's editor reads it")
{
    PchList::Entries entries;
    juce::String error;

    SUBCASE("the original's own layout")
    {
        REQUIRE(PchList::parse("1 \r\nControl.pch\r\nDRUM VICIUS.pch\r\n", entries, error));
        CHECK(entries[0] == "Control.pch");
        CHECK(entries[1] == "DRUM VICIUS.pch");
        CHECK(entries[2].isEmpty());
    }
    SUBCASE("a number moves to a position, empty lines do not")
    {
        REQUIRE(PchList::parse("\r\nA.pch\r\n\r\n10\r\nB.pch\r\n99\r\nC.pch\r\n", entries, error));
        CHECK(entries[0] == "A.pch");
        CHECK(entries[1].isEmpty());
        CHECK(entries[9] == "B.pch");
        CHECK(entries[98] == "C.pch");
    }
    SUBCASE("corrupt lists")
    {
        CHECK_FALSE(PchList::parse("100\r\nA.pch\r\n", entries, error));
        CHECK_FALSE(PchList::parse("99\r\nA.pch\r\nB.pch\r\n", entries, error));
        CHECK_FALSE(PchList::parse("notes.txt\r\n", entries, error));
    }
}

TEST_CASE("A written .pchList reads back to the same positions")
{
    PchList::Entries entries;
    entries[0] = "01 - Bass.pch";
    entries[1] = "02 - Lead.pch";
    entries[9] = "10 - Pad.pch";
    entries[98] = "99 - Last.pch";

    const auto text = PchList::write(entries);
    CHECK(text.startsWith("1 \r\n01 - Bass.pch\r\n02 - Lead.pch\r\n10 \r\n"));

    PchList::Entries back;
    juce::String error;
    REQUIRE(PchList::parse(text, back, error));
    CHECK(back == entries);
}

// Real answers from a G1, when a capture is at hand (aseqdump text, as written by
// G1originaleditor/scripts/capture-real.sh). Not in the repo: the patches are
// someone's. Run with NME_FLASH_CAPTURE=<synth-to-editor.*.txt>.
TEST_CASE("Flash dumps captured from a G1 parse into patches")
{
    const char* path = std::getenv("NME_FLASH_CAPTURE");
    if (path == nullptr)
        return;

    static ModuleDescriptions descs;
    static const bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);

    // aseqdump splits a long SysEx over several lines; glue them back.
    std::vector<std::vector<uint8_t>> messages;
    for (const auto& line : juce::StringArray::fromLines(juce::File(path).loadFileAsString()))
    {
        const int at = line.indexOf("System exclusive");
        if (at < 0)
            continue;
        juce::MemoryBlock bytes;
        bytes.loadFromHexString(line.substring(at + 16));
        const auto* p = static_cast<const uint8_t*>(bytes.getData());
        if (bytes.getSize() > 0 && p[0] == 0xF0)
            messages.emplace_back(p, p + bytes.getSize());
        else if (!messages.empty())
            messages.back().insert(messages.back().end(), p, p + bytes.getSize());
    }

    int parsed = 0;
    for (const auto& m : messages)
    {
        if (m.size() < 8 || ((m[2] >> 2) & 0x1F) != 0x1e)
            continue;
        const std::vector<uint8_t> data(m.begin() + 5, m.end() - 2);
        FlashDumpReply reply;
        REQUIRE(FlashDumpReply::peek(data, reply));
        PatchParser parser(descs);
        const auto patch = parser.parse({ data });
        REQUIRE(patch != nullptr);
        CHECK(patch->getName().isNotEmpty());
        MESSAGE("bank " << reply.section + 1 << " pos " << reply.position + 1 << ": \""
                << patch->getName() << "\", "
                << patch->getPolyVoiceArea().getModules().size() << " poly + "
                << patch->getCommonArea().getModules().size() << " common modules, "
                << patch->getPolyVoiceArea().getConnections().size()
                       + patch->getCommonArea().getConnections().size() << " cables");
        // NME_FLASH_OUT=<folder> also writes them, to set against the .pch files
        // Clavia's editor saved from the same bank.
        if (const char* out = std::getenv("NME_FLASH_OUT"))
            PchFileIO(descs).writeFile(*patch, juce::File(out).getChildFile(patch->getName() + ".pch"));
        ++parsed;
    }
    CHECK(parsed > 0);
}

TEST_CASE("What a folder or a list sends to a bank")
{
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("nme-bank-send-" + juce::Uuid().toString());
    root.createDirectory();
    for (auto name : { "B.pch", "A.pch", "C.pch" })
        root.getChildFile(name).replaceWithText("x");

    juce::StringArray problems;
    juce::File list;

    SUBCASE("no list: files by name from position 1")
    {
        const auto items = BankTransferManager::itemsFrom(root, problems, list);
        REQUIRE(items.size() == 3);
        CHECK(items[0].file.getFileName() == "A.pch");
        CHECK(items[2].position == 2);
        CHECK(list == juce::File());
    }
    SUBCASE("an NME backup without a list: the NN of each name")
    {
        for (auto name : { "A.pch", "B.pch", "C.pch" })
            root.getChildFile(name).deleteFile();
        root.getChildFile("05 - Lead.pch").replaceWithText("x");
        root.getChildFile("01 - Bass.pch").replaceWithText("x");
        const auto items = BankTransferManager::itemsFrom(root, problems, list);
        REQUIRE(items.size() == 2);
        CHECK(items[0].position == 0);
        CHECK(items[1].position == 4);
        CHECK(items[1].file.getFileName() == "05 - Lead.pch");
    }
    SUBCASE("one list: its positions, and a missing file is named")
    {
        root.getChildFile("Bank8.pchList").replaceWithText("1 \r\nC.pch\r\n10 \r\nA.pch\r\nGone.pch\r\n");
        const auto items = BankTransferManager::itemsFrom(root, problems, list);
        REQUIRE(items.size() == 2);
        CHECK(items[0].position == 0);
        CHECK(items[0].file.getFileName() == "C.pch");
        CHECK(items[1].position == 9);
        CHECK(problems.size() == 1);
        CHECK(list.getFileName() == "Bank8.pchList");
    }
    SUBCASE("two lists: nothing until one is chosen")
    {
        root.getChildFile("Bank1.pchList").replaceWithText("1 \r\nA.pch\r\n");
        root.getChildFile("Bank8.pchList").replaceWithText("5 \r\nB.pch\r\n");
        CHECK(BankTransferManager::itemsFrom(root, problems, list).empty());
        CHECK(problems.size() == 1);

        problems.clear();
        const auto items = BankTransferManager::itemsFrom(root.getChildFile("Bank8.pchList"),
                                                          problems, list);
        REQUIRE(items.size() == 1);
        CHECK(items[0].position == 4);
        CHECK(items[0].file.getFileName() == "B.pch");
    }

    root.deleteRecursively();
}

// The answers G1-Emu (Clavia's OS) gave to a bank write, 2026-10-18: a plain ACK
// per packet, then 18 00 with position and name once the flash is written.
TEST_CASE("A bank write completes on the store answer, and fails on a refusal")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);

    int ok = 0, failed = 0;
    connection.setBankUploadResultCallback([&](bool success) { success ? ++ok : ++failed; });
    Patch patch;
    patch.setName("Control");

    SUBCASE("stored")
    {
        connection.uploadPatchToBank(0, 0, patch);
        feed(connection, { 0xF0, 0x33, 0x58, 0x06, 0x00, 0x18, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00,
                           0x16, 0x00, 0x03, 0x00, 0x00, 'C', 'o', 'n', 't', 'r', 'o', 'l',
                           0x00, 0x05, 0x2B, 0xF7 });  // G1-Emu
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        CHECK(ok == 1);
        CHECK(failed == 0);
    }
    SUBCASE("refused")
    {
        connection.uploadPatchToBank(0, 0, patch);
        feed(connection, SysEx::encode(NmCmd::ACK, 0, {0x00, 0x18, 0x02, 0x00, 0x00}, true));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        CHECK(ok == 0);
        CHECK(failed == 1);
        CHECK_FALSE(connection.isUploadingPatch());
    }
}

// End to end against a running G1-Emu (or a G1) on real MIDI ports: send the
// .pchList given in NME_BANK_LIST to bank 9, read bank 9 back, and compare.
// Writes to that synth's bank 9, so it only runs when asked:
//   NME_BANK_PORT="PC Port" NME_BANK_LIST=<Bank8.pchList> nme_tests -tc="*end to end*"
TEST_CASE("A bank goes to a synth and comes back, end to end")
{
    const char* portName = std::getenv("NME_BANK_PORT");
    const char* listPath = std::getenv("NME_BANK_LIST");
    if (portName == nullptr || listPath == nullptr)
        return;

    static ModuleDescriptions descs;
    static const bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);

    auto find = [&](const juce::Array<juce::MidiDeviceInfo>& devices) {
        for (const auto& d : devices)
            if (d.name.contains(portName))
                return d.identifier;
        return juce::String();
    };
    const auto in = find(juce::MidiInput::getAvailableDevices());
    const auto out = find(juce::MidiOutput::getAvailableDevices());
    REQUIRE(in.isNotEmpty());
    REQUIRE(out.isNotEmpty());

    ConnectionManager connection;
    REQUIRE(connection.connect(in, out));
    for (int i = 0; i < 100 && !connection.isConnected(); ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    REQUIRE(connection.isConnected());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1500);   // let the connect traffic settle

    BankTransferManager transfer(connection, descs);
    auto run = [&](auto start) {
        BankTransferManager::Progress last;
        bool done = false;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        start([&](const BankTransferManager::Progress& p) { last = p; done = p.finished; });
        while (!done && juce::Time::getMillisecondCounterHiRes() - t0 < 300000)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        MESSAGE(last.current << "/" << last.total << " in "
                << juce::String((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1)
                << " s, failures: " << last.failures.joinIntoString(", "));
        CHECK(done);
        CHECK(last.failures.isEmpty());
    };

    juce::StringArray problems;
    juce::File list;
    const auto items = BankTransferManager::itemsFrom(juce::File(listPath), problems, list);
    REQUIRE_FALSE(items.empty());
    run([&](auto cb) { transfer.sendBankToSynth(items, 8, cb); });

    const auto back = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("nme-bank-back-" + juce::Uuid().toString());
    run([&](auto cb) { transfer.saveBankToDisk(8, back, cb); });

    // Same positions, same patches.
    PchList::Entries sent, got;
    juce::String error;
    REQUIRE(PchList::parse(list.loadFileAsString(), sent, error));
    REQUIRE(PchList::parse(back.getChildFile("Bank9.pchList").loadFileAsString(), got, error));
    PchFileIO io(descs);
    for (int pos = 0; pos < PchList::positions; ++pos)
    {
        const auto& s = sent[static_cast<size_t>(pos)];
        const auto& g = got[static_cast<size_t>(pos)];
        CHECK_MESSAGE(s.isEmpty() == g.isEmpty(), "position " << pos + 1);
        if (s.isEmpty() || g.isEmpty())
            continue;
        const auto a = io.readFile(list.getParentDirectory().getChildFile(s));
        const auto b = io.readFile(back.getChildFile(g));
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        CHECK(a->getPolyVoiceArea().getModules().size() == b->getPolyVoiceArea().getModules().size());
        CHECK(a->getCommonArea().getModules().size() == b->getCommonArea().getModules().size());
        CHECK(a->getPolyVoiceArea().getConnections().size() == b->getPolyVoiceArea().getConnections().size());
        MESSAGE("position " << pos + 1 << ": " << s << " -> " << g);
    }
    back.deleteRecursively();
    connection.disconnect();
}
