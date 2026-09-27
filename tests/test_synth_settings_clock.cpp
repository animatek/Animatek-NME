#include <doctest.h>
#include "midi/NmMessages.h"

// The synth settings' MIDI clock source bit is 1 for Internal and 0 for External.
// NME used to show it the other way round: a real G1 set to Internal appeared as
// External in the Synth Settings dialog, and choosing Internal there stopped the
// synth's master clock (G1-Emu NOTES.md, "The master clock").

// The patchData of a G1's reply to RequestSynthSettings (a new G1-Emu flash, whose
// clock is External): section type 3, then the settings bits.
static const std::vector<uint8_t> kExternalReply = {
    0x01, 0x40, 0x1f, 0x77, 0x44, 0x0c, 0x01, 0x40, 0x26, 0x5b, 0x6c, 0x47, 0x2b, 0x31,
    0x42, 0x72, 0x00, 0x00, 0x03, 0x30, 0x08, 0x6c, 0x04, 0x1b, 0x01, 0x46, 0x60, 0x70,
    0x40, 0x24, 0x00, 0x05, 0x00, 0x40, 0x00, 0x00, 0x00
};

TEST_CASE("a synth whose clock source bit is 0 reads as External")
{
    SynthSettings s;
    REQUIRE(SynthSettingsMessage::decode(kExternalReply, s));
    CHECK(s.midiClockSource == 0);
    CHECK_FALSE(s.clockIsInternal());
    CHECK(s.midiClockBpm == 120);
}

TEST_CASE("choosing Internal sends 1, and it survives a round trip")
{
    for (const bool internal : { true, false })
    {
        SynthSettings s;
        REQUIRE(SynthSettingsMessage::decode(kExternalReply, s));
        s.setClockInternal(internal);
        CHECK(s.midiClockSource == (internal ? 1 : 0));

        SynthSettingsMessage m;
        m.settings = s;
        const auto payload = m.encode(0);
        REQUIRE(payload.size() > 1);
        // encode() yields the pid byte, then the packed section that decode() takes.
        const std::vector<uint8_t> packed(payload.begin() + 1, payload.end());
        SynthSettings back;
        REQUIRE(SynthSettingsMessage::decode(packed, back));
        CHECK(back.clockIsInternal() == internal);
        CHECK(back.midiClockBpm == 120);
    }
}

TEST_CASE("settings not yet read from the synth default to Internal")
{
    CHECK(SynthSettings{}.clockIsInternal());
}

// The master tune is a signed byte, 0 = in tune (nmedit reads it the same way); NME
// used to take 64 as the centre and showed a G1 at 0 as -64 cents.
TEST_CASE("the master tune is a signed byte centred on 0")
{
    SynthSettings s;
    REQUIRE(SynthSettingsMessage::decode(kExternalReply, s));
    CHECK(s.masterTuneCents() == 0);

    for (const int cents : { -127, -10, 0, 25, 127 })
    {
        s.setMasterTuneCents(cents);
        SynthSettingsMessage m;
        m.settings = s;
        const auto payload = m.encode(0);
        const std::vector<uint8_t> packed(payload.begin() + 1, payload.end());
        SynthSettings back;
        REQUIRE(SynthSettingsMessage::decode(packed, back));
        CHECK(back.masterTuneCents() == cents);
    }
    s.setMasterTuneCents(-500);
    CHECK(s.masterTuneCents() == -127);
    CHECK(SynthSettings{}.masterTuneCents() == 0);
}
