#include <doctest.h>
#include "model/SlotSet.h"

// A slot set's manifest is the only thing that says which patch goes to which
// slot, and it is a file people share, so what goes in has to come back out and
// anything odd has to be refused rather than half loaded.

static SlotSet fullSet()
{
    SlotSet set;
    set.name = "Fractura";
    set.notes = "Start with A and C.";
    set.focusSlot = 2;

    auto& a = set.slots[0];
    a.mode = SlotSet::Mode::Patch;
    a.file = "A Nucleo Duro.pch";
    a.patchName = "A Nucleo Duro";
    a.bankSection = 3;
    a.bankPosition = 13;

    set.slots[1].mode = SlotSet::Mode::Disable;

    auto& d = set.slots[3];
    d.mode = SlotSet::Mode::Patch;
    d.file = "D Vector Acido.pch";
    d.patchName = "D Vector Acido";
    d.enabled = false;
    return set;
}

TEST_CASE("a slot set survives the trip through its manifest")
{
    const auto original = fullSet();
    SlotSet back;
    juce::String error;
    REQUIRE(slotSetFromJson(slotSetToJson(original), back, error));

    CHECK(back.name == "Fractura");
    CHECK(back.notes == "Start with A and C.");
    CHECK(back.focusSlot == 2);

    CHECK(back.slots[0].mode == SlotSet::Mode::Patch);
    CHECK(back.slots[0].file == "A Nucleo Duro.pch");
    CHECK(back.slots[0].patchName == "A Nucleo Duro");
    CHECK(back.slots[0].enabled);
    CHECK(back.slots[0].bankSection == 3);
    CHECK(back.slots[0].bankPosition == 13);

    CHECK(back.slots[1].mode == SlotSet::Mode::Disable);
    CHECK(back.slots[2].mode == SlotSet::Mode::Keep);

    CHECK(back.slots[3].mode == SlotSet::Mode::Patch);
    CHECK_FALSE(back.slots[3].enabled);
    CHECK(back.slots[3].bankSection == -1);

    CHECK(back.changesAnything());
    CHECK(back.numPatches() == 2);
}

TEST_CASE("a slot the manifest leaves out is left alone")
{
    const juce::String json = R"({"format":"animatek-nme-slot-set","version":1,"name":"Two",
        "slots":{"B":{"mode":"patch","file":"B Hilo.pch"}}})";
    SlotSet set;
    juce::String error;
    REQUIRE(slotSetFromJson(json, set, error));
    CHECK(set.slots[0].mode == SlotSet::Mode::Keep);
    CHECK(set.slots[1].mode == SlotSet::Mode::Patch);
    CHECK(set.slots[1].enabled);          // absent means on
    CHECK(set.slots[3].mode == SlotSet::Mode::Keep);
    CHECK(set.focusSlot == -1);
}

TEST_CASE("a manifest this editor cannot honour is refused")
{
    auto refused = [](const juce::String& json) {
        SlotSet set;
        juce::String error;
        const bool ok = slotSetFromJson(json, set, error);
        CHECK(error.isNotEmpty());
        return !ok;
    };

    CHECK(refused("not json at all"));
    CHECK(refused(R"({"format":"something-else","version":1,"slots":{}})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","slots":{}})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","version":2,"slots":{}})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","version":1})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","version":1,"slots":{"E":{"mode":"keep"}}})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","version":1,"slots":{"A":{"mode":"mute"}}})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","version":1,"focus":"Z","slots":{}})"));
    CHECK(refused(R"({"format":"animatek-nme-slot-set","version":1,"slots":{"A":{"mode":"patch","file":"../x.pch"}}})"));
}

TEST_CASE("a manifest can only name patches inside its own folder")
{
    CHECK(isSafeSlotSetFileName("A Nucleo Duro.pch"));
    CHECK(isSafeSlotSetFileName("B Drone... Long.PCH"));

    CHECK_FALSE(isSafeSlotSetFileName(""));
    CHECK_FALSE(isSafeSlotSetFileName(".pch"));
    CHECK_FALSE(isSafeSlotSetFileName("../A.pch"));
    CHECK_FALSE(isSafeSlotSetFileName("sub/A.pch"));
    CHECK_FALSE(isSafeSlotSetFileName("sub\\A.pch"));
    CHECK_FALSE(isSafeSlotSetFileName("C:A.pch"));
    CHECK_FALSE(isSafeSlotSetFileName(".hidden.pch"));
    CHECK_FALSE(isSafeSlotSetFileName("A.var"));
    CHECK_FALSE(isSafeSlotSetFileName(" A.pch"));
}

TEST_CASE("a set's patch files carry their slot letter once")
{
    CHECK(slotSetPatchFileName(0, "Nucleo Duro")   == "A Nucleo Duro.pch");
    CHECK(slotSetPatchFileName(0, "A Nucleo Duro") == "A Nucleo Duro.pch");
    CHECK(slotSetPatchFileName(1, "A Nucleo Duro") == "B A Nucleo Duro.pch");
    CHECK(slotSetPatchFileName(3, "D")             == "D.pch");
    CHECK(slotSetPatchFileName(2, "   ")           == "C Untitled.pch");
    CHECK(slotSetPatchFileName(2, "Bass/Lead")     == "C BassLead.pch");
    CHECK(slotSetPatchFileName(1, ".hidden")       == "B hidden.pch");
}

TEST_CASE("a set's folder name is what is left of its name once made legal")
{
    CHECK(slotSetFolderName("  Fractura  ") == "Fractura");
    CHECK(slotSetFolderName("Horizontes ABCD") == "Horizontes ABCD");
    CHECK(slotSetFolderName("...") == "");
    CHECK(slotSetFolderName("") == "");
}

TEST_CASE("bank locations read the way the synth browser shows them")
{
    CHECK(slotSetBankNumber(3, 13) == 414);
    CHECK(slotSetBankNumber(1, 0)  == 201);
    CHECK(slotSetBankNumber(8, 98) == 999);
    CHECK(slotSetBankNumber(-1, 0) == -1);
    CHECK(slotSetBankNumber(9, 0)  == -1);
    CHECK(slotSetBankNumber(0, 99) == -1);

    int section = -1, position = -1;
    CHECK(slotSetBankFromNumber(414, section, position));
    CHECK(section == 3);
    CHECK(position == 13);

    for (int bad : { 0, 100, 200, 1000, 1001, -414 })
    {
        section = position = -7;
        CHECK_FALSE(slotSetBankFromNumber(bad, section, position));
        CHECK(section == -7);
    }
}

TEST_CASE("loading a set switches slots the way it says and leaves the rest")
{
    SlotSet set = fullSet();   // A on, B off, C left, D loaded but off
    const std::array<bool, 4> now { false, true, true, true };
    const auto mask = slotSetEnableMask(set, now);
    CHECK(mask[0]);
    CHECK_FALSE(mask[1]);
    CHECK(mask[2]);
    CHECK_FALSE(mask[3]);

    // The set asks for C, which stays on.
    CHECK(slotSetFocusAfterLoad(set, mask, 3) == 2);

    // Asked for a slot the set switches off: focus must not stay there either,
    // or the synth would keep it on.
    set.focusSlot = 1;
    CHECK(slotSetFocusAfterLoad(set, mask, 3) == 0);
    CHECK(slotSetFocusAfterLoad(set, mask, 2) == 2);

    const std::array<bool, 4> allOff {};
    SlotSet off;
    for (auto& s : off.slots)
        s.mode = SlotSet::Mode::Disable;
    CHECK(slotSetFocusAfterLoad(off, slotSetEnableMask(off, allOff), 1) == 1);
    CHECK_FALSE(SlotSet{}.changesAnything());
}
