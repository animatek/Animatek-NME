#include <doctest.h>
#include "midi/OsUpdate.h"

#include <numeric>
#include <random>

// The stream has to be exactly what the G1's update utility reads: a header it
// checks, data it unscrambles, and a checksum it compares. None of it can be
// tried against Clavia's OS here, so an image of our own stands in, the size
// of the rack's 3.03b, whose header is known from a capture of the updater.

static std::vector<uint8_t> fakeImage(size_t size = 0x5E000)
{
    std::vector<uint8_t> image(size);
    std::mt19937 rng(1234);
    for (auto& b : image)
        b = static_cast<uint8_t>(rng());
    image[0] = 0x46; image[1] = 0xFC; image[2] = 0x27; image[3] = 0x00;
    return image;
}

static uint32_t word(const std::vector<uint8_t>& unpacked)
{
    return uint32_t(unpacked[0]) << 24 | uint32_t(unpacked[1]) << 16 | uint32_t(unpacked[2]) << 8 | unpacked[3];
}

TEST_CASE("OS update: packing and scrambling come back")
{
    std::vector<uint8_t> raw(256);
    std::iota(raw.begin(), raw.end(), 0);
    for (size_t n : { size_t(1), size_t(7), size_t(8), size_t(112), size_t(256) })
    {
        const auto packed = OsUpdate::pack(raw.data(), n);
        CHECK(packed.size() == n + (n + 6) / 7);
        for (auto b : packed)
            CHECK(b < 0x80);
        CHECK(OsUpdate::unpack(packed.data(), n) == std::vector<uint8_t>(raw.begin(), raw.begin() + long(n)));
    }
    CHECK(OsUpdate::unscramble(OsUpdate::scramble(raw)) == raw);
    CHECK(OsUpdate::scramble(raw) != raw);
}

TEST_CASE("OS update: the header is the updater's")
{
    const auto msgs = OsUpdate::buildMessages(fakeImage());
    const std::vector<uint8_t> header { 0xF0, 0x33, 0x7F, 0x06, 0x70,
        0x00, 0x00, 0x18, 0x00, 0x10,    // $300020
        0x02, 0x00, 0x02, 0x70, 0x00,    // $5E000
        0x03, 0x03, 0x22, 0xF7 };
    CHECK(msgs.front() == header);
}

TEST_CASE("OS update: every byte arrives and the checksum covers them")
{
    const auto image = fakeImage();
    const auto msgs = OsUpdate::buildMessages(image);
    REQUIRE(msgs.size() == 1 + (image.size() + 111) / 112 + 1);    // 3,438 data messages

    std::vector<uint8_t> received;
    for (size_t i = 1; i + 1 < msgs.size(); ++i)
    {
        const auto& m = msgs[i];
        REQUIRE(m.size() == 134);
        CHECK(m[4] == 0x71);
        const auto block = OsUpdate::unpack(m.data() + 5, 112);
        const size_t want = std::min<size_t>(112, image.size() - received.size());
        received.insert(received.end(), block.begin(), block.begin() + long(want));
    }
    uint32_t sum = 0;
    for (auto b : received)
        sum += b;
    CHECK(OsUpdate::unscramble(received) == image);

    const auto& tail = msgs.back();
    CHECK(tail[4] == 0x73);
    CHECK(word(OsUpdate::unpack(tail.data() + 5, 4)) == sum);
}

TEST_CASE("OS update: only an image that can be a G1 OS goes")
{
    CHECK(OsUpdate::checkImage(fakeImage()).empty());
    CHECK(! OsUpdate::checkImage(fakeImage(1024)).empty());
    CHECK(! OsUpdate::checkImage(fakeImage(0x70000)).empty());
    CHECK(! OsUpdate::checkImage(fakeImage(0x5E001)).empty());
    auto notCode = fakeImage();
    notCode[0] = 0;
    CHECK(! OsUpdate::checkImage(notCode).empty());
}

TEST_CASE("OS update: the synth's answers")
{
    // A G1 in its update utility, and G1-Emu's normal OS (no serial in its flash)
    const uint8_t update[] = { 0x01, 0x03, 0x03, 0x7F, 0x01, 0x08, 0x01 };
    const uint8_t normal[] = { 0x01, 0x03, 0x03, 0x3F, 0x7F, 0x7F, 0x00 };
    CHECK(OsUpdate::isUpdateModeIAm(update, sizeof(update)));
    CHECK(! OsUpdate::isUpdateModeIAm(normal, sizeof(normal)));

    const uint8_t ok[] = { 0xF0, 0x33, 0x00, 0x06, 0x7F, 0xF7 };
    CHECK(OsUpdate::isChecksumAccepted(ok, sizeof(ok)));
    CHECK(! OsUpdate::isChecksumAccepted(update, sizeof(update)));
}
