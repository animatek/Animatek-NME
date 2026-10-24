#include <doctest.h>
#include "midi/NmMessages.h"

// What the synth answers to a Store: a one-entry patch list, which is how the editor knows
// the patch is in flash. Copied from a capture of Clavia's editor 3.03 (Patch > Save In
// Synth, "GatoNaranja" into bank 6, position 14) against G1-Emu, 2026-10-23
// (G1originaleditor/captures/emu-to-editor.store2.txt, notes/16-store-un-patch.md).

TEST_CASE("the synth's answer to a Store names the position it wrote")
{
    // After the ACK's pid/type/pid: 05 16 00, NextSection 05 0D, the name, its NUL, a
    // repeated section, the end marker and the checksum.
    std::vector<uint8_t> payload { 0x05, 0x16, 0x00, 0x03, 0x05, 0x0D };
    for (char c : std::string("GatoNaranja"))
        payload.push_back(static_cast<uint8_t>(c));
    for (uint8_t b : { 0x00, 0x05, 0x0D, 0x05, 0x0D, 0x00, 0x7F })
        payload.push_back(b);

    const auto reply = PatchListResponseMessage::decode(payload.data(), payload.size(), 0, 0);
    REQUIRE(reply.entries.size() == 1);
    CHECK(reply.entries[0].section == 5);      // bank 6
    CHECK(reply.entries[0].position == 13);    // position 14: location 614
    CHECK(reply.entries[0].name == "GatoNaranja");
}
