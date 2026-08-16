#include <doctest.h>
#include "midi/SysExCodec.h"

TEST_CASE("header byte packs cc and slot and unpacks them unchanged")
{
    for (int cc = 0; cc < 32; ++cc)
        for (int slot = 0; slot < 4; ++slot)
        {
            const auto header = SysEx::encodeHeader(cc, slot);
            int cc2 = -1, slot2 = -1;
            SysEx::decodeHeader(header, cc2, slot2);
            CHECK(cc2 == cc);
            CHECK(slot2 == slot);
        }
}

TEST_CASE("checksum is the byte sum modulo 128")
{
    const uint8_t data[] = { 0xF0, 0x33, 0x7F, 0x06, 0x01 };
    int expected = (0xF0 + 0x33 + 0x7F + 0x06 + 0x01) % 128;
    CHECK(SysEx::checksum(data, sizeof(data)) == expected);

    CHECK(SysEx::checksum(nullptr, 0) == 0);
}

TEST_CASE("encode produces the documented envelope")
{
    const std::vector<uint8_t> payload { 0x14, 0x02, 0x30 };
    auto msg = SysEx::encode(0x0a, 2, payload, /*addChecksum=*/true);

    REQUIRE(msg.size() == 4 + payload.size() + 2);
    CHECK(msg.front() == 0xF0);
    CHECK(msg[1] == 0x33);
    CHECK(msg[2] == SysEx::encodeHeader(0x0a, 2));
    CHECK(msg[3] == 0x06);
    CHECK(std::vector<uint8_t>(msg.begin() + 4, msg.begin() + 7) == payload);
    // Checksum covers everything from F0 through the payload
    CHECK(msg[msg.size() - 2] == SysEx::checksum(msg.data(), msg.size() - 2));
    CHECK(msg.back() == 0xF7);
}

TEST_CASE("encode without checksum omits exactly one byte")
{
    const std::vector<uint8_t> payload { 0x01 };
    auto with    = SysEx::encode(1, 0, payload, true);
    auto without = SysEx::encode(1, 0, payload, false);
    CHECK(with.size() == without.size() + 1);
    CHECK(without.back() == 0xF7);
}

TEST_CASE("decode round-trips what encode built")
{
    const std::vector<uint8_t> payload { 0x17, 0x40, 0x00, 0x7F };
    auto msg = SysEx::encode(0x1c, 3, payload, true);

    auto decoded = SysEx::decode(msg.data(), msg.size());
    REQUIRE(decoded.valid);
    CHECK(decoded.cc == 0x1c);
    CHECK(decoded.slot == 3);
    // decode keeps the trailing checksum in the payload (callers strip it)
    REQUIRE(decoded.payload.size() == payload.size() + 1);
    CHECK(std::vector<uint8_t>(decoded.payload.begin(),
                               decoded.payload.end() - 1) == payload);
}

TEST_CASE("decode rejects what is not a Clavia frame")
{
    // Too short
    const uint8_t tiny[] = { 0xF0, 0x33, 0x00 };
    CHECK_FALSE(SysEx::decode(tiny, sizeof(tiny)).valid);

    // Wrong manufacturer
    const uint8_t roland[] = { 0xF0, 0x41, 0x00, 0x06, 0xF7 };
    CHECK_FALSE(SysEx::decode(roland, sizeof(roland)).valid);

    // Wrong device
    const uint8_t wrongDev[] = { 0xF0, 0x33, 0x00, 0x05, 0xF7 };
    CHECK_FALSE(SysEx::decode(wrongDev, sizeof(wrongDev)).valid);

    // Missing terminator
    const uint8_t unterminated[] = { 0xF0, 0x33, 0x00, 0x06, 0x00 };
    CHECK_FALSE(SysEx::decode(unterminated, sizeof(unterminated)).valid);
}
