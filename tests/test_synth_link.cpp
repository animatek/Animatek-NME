#include <doctest.h>
#include "midi/SynthLink.h"

TEST_CASE("the emulator's ports are told from a real G1's")
{
    CHECK(SynthLink::isEmulatorPort("G1Emu"));
    CHECK(SynthLink::isEmulatorPort("G1Emu 2 PC Port"));
    CHECK(SynthLink::isEmulatorPort("g1-emu"));
    CHECK_FALSE(SynthLink::isEmulatorPort("UM-ONE"));
    CHECK_FALSE(SynthLink::isEmulatorPort("Nord Modular"));
}

TEST_CASE("the status bar says where the editor is connected")
{
    CHECK(SynthLink::describe(true, "UM-ONE") == "Real G1 (UM-ONE)");
    CHECK(SynthLink::describe(true, "G1Emu 2") == "G1-Emu (G1Emu 2)");
    CHECK(SynthLink::describe(false, "UM-ONE") == "Not connected");
}
