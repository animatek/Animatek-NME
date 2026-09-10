#include <doctest.h>
#include "protocol/NewModuleMessage.h"
#include "model/BitStream.h"

static const std::vector<int> oscADefaults {64, 64, 64, 64, 0, 0, 0, 0, 0, 0};

TEST_CASE("OscA NewModule matches the original jnmprotocol reference packet")
{
    // nmedit/libs/jnmprotocol2/test/.../ProtocolTester.java:testNewModuleMessage
    // Includes OscA's custom frequency-display value, even when it is zero.
    const std::vector<uint8_t> expected {
        0xf0, 0x33, 0x7c, 0x06, 0x05, 0x18, 0x01, 0x60, 0x10, 0x50, 0x08,
        0x12, 0x4f, 0x39, 0x58, 0x68, 0x13, 0x10, 0x01, 0x25, 0x00, 0x00,
        0x13, 0x30, 0x11, 0x20, 0x78, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x5b, 0x40, 0x45, 0x00, 0x20, 0x02, 0x6a, 0x02,
        0x0a, 0x27, 0x5c, 0x6c, 0x34, 0x09, 0x48, 0x00, 0x1e, 0xf7
    };
    NewModuleMessageProto msg(5, 7, 1, 10, 2, 9, "OscA2", oscADefaults, {0});
    CHECK(msg.toSysEx(0) == expected);
}

TEST_CASE("NewModule preserves eight-bit fields and section boundaries")
{
    // Sixteen-character names have no extra NUL in PDL2. Custom values are
    // eight bits before MIDI packing, including adjacent non-zero values.
    for (const std::string name : {"OscA", "1234567890123456", "12345678901234567890"})
    {
        CAPTURE(name);
        NewModuleMessageProto msg(5, 7, 1, 10, 2, 200, name,
                                  oscADefaults, {0x81, 0xfe, 0x55});
        const auto bytes = msg.toSysEx(2);
        CHECK(bytes[2] == 0x7e);
        unsigned sum = 0;
        for (size_t i = 0; i < bytes.size() - 2; ++i)
            sum += bytes[i];
        CHECK(bytes[bytes.size() - 2] == (sum & 0x7f));
        for (size_t i = 1; i + 1 < bytes.size(); ++i)
            CHECK(bytes[i] < 128);

        BitStream bs({bytes.begin() + 5, bytes.end() - 2});
        CHECK(bs.readBits(8) == 48);
        CHECK(bs.readBits(8) == 7);
        CHECK(bs.readBits(8) == 1);
        CHECK(bs.readBits(8) == 10);
        CHECK(bs.readBits(8) == 2);
        CHECK(bs.readBits(8) == 200);
        CHECK(bs.readString16() == name.substr(0, 16));
        CHECK(bs.readBits(8) == 82);
        CHECK(bs.readBits(1) == 1);
        CHECK(bs.readBits(15) == 0);
        CHECK(bs.readBits(8) == 77);
        CHECK(bs.readBits(1) == 1);
        CHECK(bs.readBits(7) == 1);
        CHECK(bs.readBits(7) == 10);
        CHECK(bs.readBits(7) == 7);
        for (int i = 0; i < 4; ++i)
            CHECK(bs.readBits(7) == 64);
        CHECK(bs.readBits(2) == 0);
        for (int i = 0; i < 4; ++i)
            CHECK(bs.readBits(7) == 0);
        CHECK(bs.readBits(1) == 0);
        bs.alignToByte();
        CHECK(bs.readBits(8) == 91);
        CHECK(bs.readBits(1) == 1);
        CHECK(bs.readBits(7) == 1);
        CHECK(bs.readBits(7) == 10);
        CHECK(bs.readBits(8) == 3);
        CHECK(bs.readBits(8) == 0x81);
        CHECK(bs.readBits(8) == 0xfe);
        CHECK(bs.readBits(8) == 0x55);
        bs.alignToByte();
        CHECK(bs.readBits(8) == 90);
        CHECK(bs.readBits(1) == 1);
        CHECK(bs.readBits(7) == 1);
        CHECK(bs.readBits(8) == 10);
        CHECK(bs.readString16() == name.substr(0, 16));
        CHECK(bs.remaining() == 0);
    }
}

TEST_CASE("NewModule parameter values 90 and 91 are not section markers")
{
    auto params = oscADefaults;
    params[0] = 90;
    params[1] = 91;
    NewModuleMessageProto msg(5, 7, 1, 10, 2, 9, "OscA", params, {1});
    const auto bytes = msg.toSysEx(0);
    BitStream bs({bytes.begin() + 5, bytes.end() - 2});
    // SingleModule: 6 fields + 5 name bytes. CableDump: 3 bytes.
    bs.setPosition((11 + 3) * 8);
    CHECK(bs.readBits(8) == 77);
    CHECK(bs.readBits(1) == 1);
    CHECK(bs.readBits(7) == 1);
    CHECK(bs.readBits(7) == 10);
    CHECK(bs.readBits(7) == 7);
    CHECK(bs.readBits(7) == 90);
    CHECK(bs.readBits(7) == 91);
}

TEST_CASE("NewModule keeps the PatchPacket command flag clear at any pid")
{
    // PatchPacket := 0:1 command:1 pid:6. NewModule is the only edit message on
    // cc=0x1f, so it is the only one whose pid is six bits wide; bit 6 is the
    // command flag the bulk upload sets, and an edit must leave it at 0. A pid
    // of 64 or more must not raise it.
    for (int pid : {0, 5, 63, 64, 100, 127})
    {
        CAPTURE(pid);
        NewModuleMessageProto msg(pid, 7, 1, 10, 2, 9, "OscA", oscADefaults, {0});
        const auto bytes = msg.toSysEx(0);
        CHECK((bytes[4] & 0x40) == 0);
        CHECK((bytes[4] & 0x3F) == (pid & 0x3F));
    }
}
