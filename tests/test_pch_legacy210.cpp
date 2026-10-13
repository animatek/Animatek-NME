#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"

// A 2.10 file stores some parameters differently from 3.0 (G1-Emu issue #65:
// most factory patches came out silent). Found by converting 800 patches the
// community archive holds in both formats and comparing them value by value.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static const char* kLegacy =
    "[Header]\r\n"
    "Info=Clavia Nord Modular patch file\r\n"
    "Version=Nord Modular patch 2.10\r\n"
    "Name=Legacy\r\n"
    "Voices=1\r\n"
    "\r\n"
    "[Module 1]\r\n"
    "Type=85\r\n"          // OscSlvFM: P4 is the mute, stored inverted
    "Name=FM\r\n"
    "Col=0\r\n"
    "Row=0\r\n"
    "P0=64\r\nP1=64\r\nP2=0\r\nP3=0\r\nP4=1\r\n"
    "\r\n"
    "[Module 2]\r\n"
    "Type=8\r\n"           // OscB: P3 is the waveform, one-based
    "Name=Saw\r\n"
    "Col=0\r\n"
    "Row=3\r\n"
    "P3=2\r\nP8=0\r\n"     // P8, its mute, is verbatim
    "\r\n"
    "[Module 3]\r\n"
    "Type=8\r\n"
    "Name=Top\r\n"
    "Col=0\r\n"
    "Row=6\r\n"
    "P3=3\r\n"             // the top value is ambiguous and kept
    "\r\n"
    "[Module 4]\r\n"
    "Type=26\r\n"          // LFOC: P2 is the waveform, one-based
    "Name=LFO\r\n"
    "Col=0\r\n"
    "Row=9\r\n"
    "P1=1\r\nP2=1\r\n"     // P1, its range, is verbatim
    "\r\n"
    "[Module 5]\r\n"
    "Type=4\r\n"           // 2Output: destination 1-based, as before
    "Name=Out\r\n"
    "Col=0\r\n"
    "Row=12\r\n"
    "P0=100\r\nP1=1\r\n"
    "Im0=1\r\nIh0=64\r\nIc0=0\r\n"
    "\r\n"
    "[Module 6]\r\n"
    "Type=17\r\n"          // EventSeq: its 32 steps packed in BP0 (Thumper's)
    "Name=Seq\r\n"
    "Col=1\r\n"
    "Row=0\r\n"
    "P0=15\r\nP1=1\r\nP2=0\r\nP3=0\r\n"
    "BP0=1151602961\r\n";

static int value(Patch& patch, int module, int param)
{
    auto* m = patch.getPolyVoiceArea().getModuleByIndex(module);
    REQUIRE(m != nullptr);
    return m->getParameter(param)->getValue();
}

TEST_CASE("#65: a 2.10 patch's mutes and selectors are read as 3.0 has them")
{
    const auto file = juce::File::createTempFile(".pch");
    file.replaceWithText(kLegacy, false, false, nullptr);
    PchFileIO io(descriptions());
    auto patch = io.readFile(file);
    file.deleteFile();
    REQUIRE(patch != nullptr);

    CHECK(value(*patch, 1, 4) == 0);    // OscSlvFM sounds, not muted
    CHECK(value(*patch, 1, 0) == 64);
    CHECK(value(*patch, 2, 3) == 1);    // saw
    CHECK(value(*patch, 2, 8) == 0);
    CHECK(value(*patch, 3, 3) == 3);
    CHECK(value(*patch, 4, 2) == 0);
    CHECK(value(*patch, 4, 1) == 1);
    CHECK(value(*patch, 5, 1) == 0);    // outputs 1/2
    CHECK(value(*patch, 5, 0) == 100);

    // What the 3.0 Thumper has: steps 1, 5, 9, 13, then 3, 6, 8, 11, 15.
    const int steps[32] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
                            0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0 };
    for (int i = 0; i < 32; ++i)
        CHECK(value(*patch, 6, 4 + i) == steps[i]);
    CHECK(value(*patch, 6, 0) == 15);
}
