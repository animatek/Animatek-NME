#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"

// A 2.10 file is converted the way Clavia's editor 3.03 converts it (G1-Emu
// issue #65: most factory patches came out silent). The rules are in
// PchFileIO.cpp; the values below are what the 3.0 copies of real patches hold.

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
    // OscSlvFM: P4 meant "on", 3.0's mute is the inverse
    "[Module 1]\r\nType=85\r\nName=FM\r\nCol=0\r\nRow=0\r\n"
    "P0=64\r\nP1=64\r\nP2=0\r\nP3=0\r\nP4=1\r\n"
    "\r\n"
    // OscB: waveform 0 was off, 1-4 the waves; 3.0 adds the mute after P7
    "[Module 2]\r\nType=8\r\nName=Saw\r\nCol=0\r\nRow=3\r\n"
    "P0=64\r\nP1=64\r\nP2=64\r\nP3=3\r\nP4=0\r\nP5=0\r\nP6=0\r\nP7=0\r\n"
    "\r\n"
    "[Module 3]\r\nType=8\r\nName=Off\r\nCol=0\r\nRow=6\r\n"
    "P0=64\r\nP1=64\r\nP2=64\r\nP3=0\r\nP4=0\r\nP5=0\r\nP6=0\r\nP7=0\r\n"
    "\r\n"
    // FilterE: type 0 was a bypassed filter
    "[Module 4]\r\nType=51\r\nName=Filter\r\nCol=0\r\nRow=9\r\n"
    "P0=0\r\nP1=1\r\nP2=0\r\nP3=60\r\nP4=64\r\nP5=0\r\nP6=0\r\nP7=1\r\nP8=0\r\n"
    "\r\n"
    // 2Output: destination 1 is outputs 1/2
    "[Module 5]\r\nType=4\r\nName=Out\r\nCol=0\r\nRow=12\r\n"
    "P0=100\r\nP1=1\r\n"
    "Im0=1\r\nIh0=64\r\nIc0=0\r\n"
    "\r\n"
    // EventSeq: its 32 steps packed in BP0 (Thumper's)
    "[Module 6]\r\nType=17\r\nName=Seq\r\nCol=1\r\nRow=0\r\n"
    "P0=15\r\nP1=1\r\nP2=0\r\nP3=0\r\n"
    "BP0=1151602961\r\n"
    "\r\n"
    // The morph knobs, a module in 2.10
    "[Module 7]\r\nType=6\r\nName=Morph\r\nCol=1\r\nRow=4\r\n"
    "P0=10\r\nP1=0\r\nP2=0\r\nP3=90\r\n"
    "\r\n"
    "[Controllers]\r\n"
    "M1=7\r\nP1=1\r\n"          // mod wheel on morph group 2
    "M7=5\r\nP7=0\r\n"          // CC 7 on the output level
    "M127=7\r\nP127=2\r\n"      // velocity on morph group 3
    "\r\n"
    "[Morphs]\r\n"
    "M0=1\r\nP0=0\r\nG0=0\r\nD0=-38\r\n"
    "\r\n"
    "[Knobs]\r\n"
    "M0=7\r\nP0=0\r\n"          // knob 1 on morph group 1
    "M20=1\r\nP20=1\r\n"
    "\r\n"
    "[Notes]\r\n"
    "Count=2\r\nL0=First line\r\nL1=Second = line\r\n";

static int value(Patch& patch, int module, int param)
{
    auto* m = patch.getPolyVoiceArea().getModuleByIndex(module);
    REQUIRE(m != nullptr);
    return m->getParameter(param)->getValue();
}

static std::unique_ptr<Patch> load(const char* text)
{
    const auto file = juce::File::createTempFile(".pch");
    file.replaceWithText(text, false, false, nullptr);
    PchFileIO io(descriptions());
    auto patch = io.readFile(file);
    file.deleteFile();
    REQUIRE(patch != nullptr);
    return patch;
}

TEST_CASE("#65: a 2.10 patch's parameters are converted as Clavia's editor does")
{
    auto patch = load(kLegacy);

    CHECK(value(*patch, 1, 4) == 0);    // OscSlvFM sounds
    CHECK(value(*patch, 1, 0) == 64);

    CHECK(value(*patch, 2, 3) == 2);    // saw
    CHECK(value(*patch, 2, 8) == 0);
    CHECK(value(*patch, 3, 3) == 0);    // off: muted
    CHECK(value(*patch, 3, 8) == 1);

    CHECK(value(*patch, 4, 0) == 0);    // bypassed
    CHECK(value(*patch, 4, 9) == 1);
    CHECK(value(*patch, 4, 3) == 60);

    CHECK(value(*patch, 5, 1) == 0);    // outputs 1/2
    CHECK(value(*patch, 5, 2) == 0);
    CHECK(value(*patch, 5, 0) == 100);

    // What the 3.0 Thumper has: steps 1, 5, 9, 13, then 3, 6, 8, 11, 15.
    const int steps[32] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
                            0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0 };
    for (int i = 0; i < 32; ++i)
        CHECK(value(*patch, 6, 4 + i) == steps[i]);
    CHECK(value(*patch, 6, 0) == 15);
}

TEST_CASE("#65: a 2.10 patch's morph module and assignments carry over")
{
    auto patch = load(kLegacy);

    CHECK(patch->getPolyVoiceArea().getModuleByIndex(7) == nullptr);
    CHECK(patch->morphValues == std::array<int, 4> { 10, 0, 0, 90 });
    CHECK(patch->morphKeyboard == std::array<int, 4> { 0, 0, 1, 0 });

    REQUIRE(patch->morphAssignments.size() == 1);
    CHECK(patch->morphAssignments[0].section == 1);
    CHECK(patch->morphAssignments[0].module == 1);
    CHECK(patch->morphAssignments[0].param == 0);
    CHECK(patch->morphAssignments[0].morph == 0);
    CHECK(patch->morphAssignments[0].range == -38);

    REQUIRE(patch->ctrlAssignments.size() == 2);
    CHECK(patch->ctrlAssignments[0].control == 1);
    CHECK(patch->ctrlAssignments[0].section == 2);
    CHECK(patch->ctrlAssignments[0].module == 1);
    CHECK(patch->ctrlAssignments[0].param == 1);
    CHECK(patch->ctrlAssignments[1].control == 7);
    CHECK(patch->ctrlAssignments[1].section == 1);
    CHECK(patch->ctrlAssignments[1].module == 5);

    CHECK(patch->knobAssignments[0].assigned);
    CHECK(patch->knobAssignments[0].section == 2);
    CHECK(patch->knobAssignments[0].module == 1);
    CHECK(patch->knobAssignments[0].param == 0);
    CHECK(patch->knobAssignments[20].assigned);
    CHECK(patch->knobAssignments[20].section == 1);
    CHECK(patch->knobAssignments[20].module == 1);
    CHECK(patch->knobAssignments[20].param == 1);
    CHECK_FALSE(patch->knobAssignments[1].assigned);

    CHECK(patch->patchNotes == "First line\nSecond = line");
}
