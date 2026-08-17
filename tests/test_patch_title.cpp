#include <doctest.h>
#include "protocol/SetPatchTitleMessage.h"

// The patch title is the one message with a hard hazard behind it: fifteen
// characters is the limit and sixteen hangs the synth, which is why the message
// truncates rather than trusting its callers. Two callers reach it, a patch
// rename and the caption the editor puts on the synth's display, and only the
// first of those takes text a user typed.

static std::vector<uint8_t> payloadOf(const std::vector<uint8_t>& sysex)
{
    // F0 33 cc|slot 06 pid sc <name...> 00 checksum F7
    REQUIRE(sysex.size() > 8);
    return { sysex.begin() + 6, sysex.end() - 3 };   // the name, without its NUL
}

TEST_CASE("a patch title is framed as the protocol expects")
{
    SetPatchTitleMessage msg(2, 0x11, "Bass");
    const auto bytes = msg.toSysEx(2);

    CHECK(bytes.front() == 0xF0);
    CHECK(bytes[1] == 0x33);
    CHECK(bytes[2] == ((0x17 << 2) | 2));   // cc:5 slot:2
    CHECK(bytes[3] == 0x06);                // device
    CHECK(bytes[4] == 0x11);                // pid
    CHECK(bytes[5] == 0x27);                // SetPatchTitle subcommand
    CHECK(bytes.back() == 0xF7);

    const auto name = payloadOf(bytes);
    CHECK(std::string(name.begin(), name.end()) == "Bass");

    // Null-terminated, then checksum, then F7.
    CHECK(bytes[bytes.size() - 3] == 0x00);
}

TEST_CASE("a title longer than fifteen characters is cut, not sent")
{
    // Sixteen is the number that hangs the synth, so this is the test that
    // matters: whatever a caller passes, at most fifteen go on the wire.
    SetPatchTitleMessage msg(0, 1, "ANME 017 - About");   // exactly sixteen
    const auto name = payloadOf(msg.toSysEx(0));

    CHECK(name.size() == 15);
    CHECK(std::string(name.begin(), name.end()) == "ANME 017 - Abou");
}

TEST_CASE("fifteen characters go through untouched")
{
    SetPatchTitleMessage msg(0, 1, "Fifteen chars!!");   // exactly fifteen
    const auto name = payloadOf(msg.toSysEx(0));

    CHECK(name.size() == 15);
    CHECK(std::string(name.begin(), name.end()) == "Fifteen chars!!");
}

TEST_CASE("the editor's own display caption is well clear of the limit")
{
    // "ANME 0.16v": fixed, ten characters, and not assembled from anything a
    // user can lengthen. The cut above is the backstop; this is why it never
    // has to fire on this path.
    SetPatchTitleMessage msg(0, 1, "ANME 0.16v");
    const auto name = payloadOf(msg.toSysEx(0));

    CHECK(name.size() == 10);
    CHECK(std::string(name.begin(), name.end()) == "ANME 0.16v");
}

TEST_CASE("every byte of a title stays inside seven bits")
{
    // The wire is 7-bit; a stray high bit would end the SysEx early and leave
    // the synth waiting for a terminator that never comes.
    SetPatchTitleMessage msg(0, 1, juce::String::fromUTF8("Caf\xc3\xa9 \xc3\xbc"));
    const auto bytes = msg.toSysEx(0);

    for (size_t i = 1; i + 1 < bytes.size(); ++i)
        CHECK((bytes[i] & 0x80) == 0);
}

TEST_CASE("an empty title is still a well-formed message")
{
    SetPatchTitleMessage msg(1, 5, "");
    const auto bytes = msg.toSysEx(1);

    // F0 33 hdr 06 pid sc NUL checksum F7
    CHECK(bytes.size() == 9);
    CHECK(bytes[6] == 0x00);
    CHECK(bytes.back() == 0xF7);
}
