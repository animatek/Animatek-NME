#include <doctest.h>
#include "midi/UploadPacketizer.h"
#include "model/BitStream.h"

#include <numeric>
#include <random>

using UploadPacketizer::Packet;

// The rules under test are the ones the synth enforces the hard way: a packet
// over 166 raw bytes gets a bogus checksum error and kills the upload (issue
// #39), and a transfer that never sends a `last` packet leaves the synth deaf
// to all MIDI (issue #40).

static std::vector<std::vector<uint8_t>> makeSections(std::initializer_list<int> sizes)
{
    std::vector<std::vector<uint8_t>> sections;
    uint8_t counter = 0;
    for (int size : sizes)
    {
        std::vector<uint8_t> s(static_cast<size_t>(size));
        for (auto& b : s)
            b = counter++;
        sections.push_back(std::move(s));
    }
    return sections;
}

static std::vector<uint8_t> concatenate(const std::vector<std::vector<uint8_t>>& chunks)
{
    std::vector<uint8_t> all;
    for (const auto& c : chunks)
        all.insert(all.end(), c.begin(), c.end());
    return all;
}

TEST_CASE("cut never exceeds the packet size and loses nothing")
{
    // Shapes that have bitten before: tiny sections, one spanning several
    // packets (a ParameterDump of a ~99 module patch), boundary-exact sizes.
    const auto sections = makeSections({ 3, 166, 500, 1, 0, 332, 165 });
    const auto packets = UploadPacketizer::cut(sections);

    std::vector<std::vector<uint8_t>> datas;
    int sectionsEnded = 0;
    for (size_t i = 0; i < packets.size(); ++i)
    {
        CHECK(static_cast<int>(packets[i].data.size()) <= UploadPacketizer::kPacketBytes);
        // Cutting is greedy: every packet except the last is full.
        if (i + 1 < packets.size())
            CHECK(static_cast<int>(packets[i].data.size()) == UploadPacketizer::kPacketBytes);
        datas.push_back(packets[i].data);
        sectionsEnded += packets[i].sectionsEnded;
    }

    // The byte stream comes out exactly as it went in, and every section's
    // end is accounted for exactly once.
    CHECK(concatenate(datas) == concatenate(sections));
    CHECK(sectionsEnded == static_cast<int>(sections.size()));
}

TEST_CASE("a section larger than one packet spans packets that end no section")
{
    const auto sections = makeSections({ 500 });
    const auto packets = UploadPacketizer::cut(sections);

    REQUIRE(packets.size() == 4);   // 166+166+166+2
    CHECK(packets[0].sectionsEnded == 0);
    CHECK(packets[1].sectionsEnded == 0);
    CHECK(packets[2].sectionsEnded == 0);
    CHECK(packets[3].sectionsEnded == 1);
}

TEST_CASE("a section ending exactly on a packet boundary is counted in that packet")
{
    const auto sections = makeSections({ 166, 10 });
    const auto packets = UploadPacketizer::cut(sections);

    REQUIRE(packets.size() == 2);
    CHECK(packets[0].sectionsEnded == 1);
    CHECK(packets[0].data.size() == 166);
    CHECK(packets[1].sectionsEnded == 1);
    CHECK(packets[1].data.size() == 10);
}

TEST_CASE("labels ride with the packet their section ends in")
{
    const auto sections = makeSections({ 200, 10 });
    const auto packets = UploadPacketizer::cut(sections, { "big", "small" });

    REQUIRE(packets.size() == 2);
    CHECK(packets[0].label.empty());
    CHECK(packets[1].label == "big, small");
}

TEST_CASE("pack7Bit spreads bits MSB-first with the tail left-justified")
{
    // 0xFF = 8 bits: seven of them fill the first byte, the eighth is
    // left-justified in the second.
    const uint8_t ff = 0xFF;
    CHECK(UploadPacketizer::pack7Bit(&ff, 1) == std::vector<uint8_t> { 0x7F, 0x40 });

    const uint8_t zero = 0x00;
    CHECK(UploadPacketizer::pack7Bit(&zero, 1) == std::vector<uint8_t> { 0x00, 0x00 });

    CHECK(UploadPacketizer::pack7Bit(nullptr, 0).empty());

    // Every packed byte stays within 7 bits
    std::mt19937 rng(1234);
    std::vector<uint8_t> raw(333);
    for (auto& b : raw)
        b = static_cast<uint8_t>(rng());
    for (auto b : UploadPacketizer::pack7Bit(raw.data(), raw.size()))
        CHECK((b & 0x80) == 0);
}

TEST_CASE("pack7Bit round-trips through the BitStream the parser reads with")
{
    std::mt19937 rng(99);
    std::vector<uint8_t> raw(166);
    for (auto& b : raw)
        b = static_cast<uint8_t>(rng());

    BitStream bs(UploadPacketizer::pack7Bit(raw.data(), raw.size()));
    for (auto original : raw)
        CHECK(bs.readBits(8) == original);
}

TEST_CASE("frame produces the bulk-upload envelope the synth accepts")
{
    Packet p;
    p.data = { 0x37, 0x00, 0x01 };   // arbitrary raw bytes
    p.sectionsEnded = 2;

    SUBCASE("middle packet: neither first nor last")
    {
        auto msg = UploadPacketizer::frame(p, false, false, 1);
        CHECK(msg[2] == ((0x1c << 2) | 1));
    }
    SUBCASE("first packet")
    {
        auto msg = UploadPacketizer::frame(p, true, false, 1);
        CHECK(msg[2] == (((0x1c | 1) << 2) | 1));
    }
    SUBCASE("last packet")
    {
        auto msg = UploadPacketizer::frame(p, false, true, 1);
        CHECK(msg[2] == (((0x1c | 2) << 2) | 1));
    }

    auto msg = UploadPacketizer::frame(p, true, true, 3);
    CHECK(msg.front() == 0xF0);
    CHECK(msg[1] == 0x33);
    CHECK(msg[3] == 0x06);
    // 0:1 command:1 pid:6, with command=1 (bulk) and pid=sections ended
    CHECK(msg[4] == (0x40 | 2));
    CHECK(msg.back() == 0xF7);

    // Checksum: sum of every byte before it, modulo 128
    uint32_t sum = 0;
    for (size_t i = 0; i + 2 < msg.size(); ++i)
        sum += msg[i];
    CHECK(msg[msg.size() - 2] == (sum % 128));

    // Nothing between the envelope bytes may have the MSB set
    for (size_t i = 1; i + 1 < msg.size(); ++i)
        CHECK((msg[i] & 0x80) == 0);
}

TEST_CASE("closeTransferFrame is the empty `last` packet that frees the synth")
{
    for (int slot = 0; slot < 4; ++slot)
    {
        auto msg = UploadPacketizer::closeTransferFrame(slot);

        // Byte-for-byte what ConnectionManager::closeUploadTransfer used to
        // build by hand: F0 33 [cc=0x1e slot] 06 40 sum F7 (issue #40).
        REQUIRE(msg.size() == 7);
        CHECK(msg[0] == 0xF0);
        CHECK(msg[1] == 0x33);
        CHECK(msg[2] == (((0x1c | 2) << 2) | slot));
        CHECK(msg[3] == 0x06);
        CHECK(msg[4] == 0x40);
        CHECK(msg[5] == ((0xF0 + 0x33 + msg[2] + 0x06 + 0x40) % 128));
        CHECK(msg[6] == 0xF7);
    }
}
