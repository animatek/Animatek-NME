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

TEST_CASE("decode reports whether the checksum byte is there and matches")
{
    // PatchHandling (cc 0x17) carries a checksum: header bit 4 is set
    auto framed = SysEx::encode(0x17, 2, { 0x40, 0x56, 0x01 }, true);
    auto good = SysEx::decode(framed.data(), framed.size());
    CHECK(good.checksumPresent);
    CHECK(good.checksumValid);

    // One payload byte damaged in transit: still decoded, flagged as bad
    auto damaged = framed;
    damaged[5] ^= 0x01;
    auto bad = SysEx::decode(damaged.data(), damaged.size());
    CHECK(bad.valid);
    CHECK(bad.checksumPresent);
    CHECK_FALSE(bad.checksumValid);

    // cc 0x13 (ParameterChange, ParamFocus) has header bit 4 clear: the
    // original editor sends these without a checksum, so decode must not take
    // the last payload byte for one
    auto param = SysEx::encode(0x13, 0, { 0x01, 0x02, 0x03, 0x04 }, false);
    auto plain = SysEx::decode(param.data(), param.size());
    CHECK_FALSE(plain.checksumPresent);
    CHECK(plain.checksumValid);

    // The checksum-carrying cc values have the flag set; IAm and cc 0x13 do not
    for (int cc : { 0x14, 0x16, 0x17, 0x1c, 0x1d, 0x1e, 0x1f })
        CHECK((SysEx::encode(cc, 0, {}, true)[2] & 0x10) != 0);
    for (int cc : { 0x00, 0x13 })
        CHECK((SysEx::encode(cc, 0, {}, false)[2] & 0x10) == 0);
}

#include "midi/ConnectionManager.h"

TEST_CASE("synth error codes read as the original editor words them")
{
    CHECK(std::string(synthErrorName(4)) == "Down link checksum error");
    // 5 is not "no slot focused": the original tells the user to power-cycle
    CHECK(std::string(synthErrorName(5)).find("turn it off and on") != std::string::npos);
    CHECK(std::string(synthErrorName(3)) == "Error in synth");
    CHECK(std::string(synthErrorName(6)) == "Unfinished bubble error");
    CHECK(std::string(synthErrorName(42)) == "unknown");
}

#include "midi/NmMessages.h"

TEST_CASE("ParamFocus is byte for byte the frame the original editor sends")
{
    // Captured from NME303.exe grabbing a knob (pid 1, slot A, section 0,
    // module 2, parameter 0): F0 33 4C 06 01 2F 00 00 02 00 F7
    ParameterFocusMessage focus;
    focus.pid = 1;
    focus.section = 0;
    focus.module = 2;
    focus.parameter = 0;
    const auto frame = SysEx::encode(0x13, 0, focus.encode(), false);
    const std::vector<uint8_t> captured = { 0xF0, 0x33, 0x4C, 0x06, 0x01, 0x2F, 0x00, 0x00, 0x02, 0x00, 0xF7 };
    CHECK(frame == captured);

    // A morph knob: section 2, module 1, as captured (2F 00 02 01 00)
    focus.section = 2; focus.module = 1; focus.parameter = 0;
    const auto morph = SysEx::encode(0x13, 0, focus.encode(), false);
    CHECK(morph[6] == 0x00);
    CHECK(morph[7] == 0x02);
    CHECK(morph[8] == 0x01);
}
