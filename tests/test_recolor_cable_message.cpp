#include "doctest.h"
#include "protocol/NewCableMessage.h"
#include "protocol/RecolorCableMessage.h"
#include "midi/SysExCodec.h"

// Layout read from the original editor's CMCableRecolor serializer
// (G1originaleditor/notes/07). Not yet checked against a real synth.

TEST_CASE("recolor names the cable the way insert does and puts the new colour in the low bits")
{
    NewCableMessage insert(5, 1, SignalType::Audio, 3, true, 2, 9, false, 4);
    RecolorCableMessage recolor(5, 1, SignalType::Logic, 3, true, 2, 9, false, 4);

    const auto a = insert.toSysEx(2);
    const auto b = recolor.toSysEx(2);

    REQUIRE(a.size() == b.size());
    // F0 33 hdr 06 pid sc | byte0 | m1 t1c1 m2 t2c2 | chk F7
    CHECK(b[2] == a[2]);          // same cc 0x17 and slot
    CHECK(b[4] == 5);             // pid
    CHECK(b[5] == 0x54);          // CableRecolor
    CHECK(b[6] == ((1 << 3) | 2)); // section 1, Logic
    for (size_t i = 7; i <= 10; ++i)
        CHECK(b[i] == a[i]);      // both connectors identical to the insert
}

TEST_CASE("recolor carries a valid checksum and no colour leaks into the section bit")
{
    for (int section : { 0, 1 })
        for (int color = 0; color < 7; ++color)
        {
            RecolorCableMessage m(0x7F, section, static_cast<SignalType>(color),
                                  1, false, 0, 2, true, 1);
            const auto bytes = m.toSysEx(0);
            const auto decoded = SysEx::decode(bytes.data(), bytes.size());
            CHECK(decoded.valid);
            CHECK(decoded.checksumPresent);
            CHECK(decoded.checksumValid);
            CHECK(bytes[6] == ((section << 3) | color));
        }
}
