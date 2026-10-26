#include <doctest.h>
#include "midi/NmMessages.h"

// What a synth says it is, in the last byte of its IAm: 0 keyboard, 1 rack, 2 Micro Modular. The two
// payloads are what G1-Emu's rack OS and the Micro's OS answer (F0 33 00 06 ... F7).

static IAmMessage iamWithDevice(uint8_t device)
{
    const uint8_t payload[] = { 0x01, 0x03, 0x03, 0x3f, 0x7f, 0x7f, device };
    return IAmMessage::decode(payload, sizeof(payload));
}

TEST_CASE("IAm: the device byte says which Nord Modular answered")
{
    CHECK(iamWithDevice(0).model() == SynthModel::Keyboard);
    CHECK(iamWithDevice(1).model() == SynthModel::Rack);
    CHECK(iamWithDevice(2).model() == SynthModel::Micro);
    CHECK(iamWithDevice(9).model() == SynthModel::Unknown);
    CHECK(std::string(IAmMessage::modelName(iamWithDevice(2).model())) == "Micro");
}

TEST_CASE("IAm: the editor's own greeting and a short one name no model")
{
    IAmMessage mine;   // sender 0, what the editor sends
    CHECK(mine.model() == SynthModel::Unknown);
    const uint8_t shortIam[] = { 0x01, 0x03, 0x03 };
    CHECK(IAmMessage::decode(shortIam, sizeof(shortIam)).model() == SynthModel::Unknown);
}
