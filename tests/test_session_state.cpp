#include <doctest.h>
#include "model/SessionState.h"

// A DAW project keeps the patches of the slots and the one on screen (#94). Whatever goes in
// has to come out byte for byte, because it is a .pch that gets read again.

static juce::MemoryBlock bytes(const juce::String& text)
{
    return juce::MemoryBlock(text.toRawUTF8(), text.getNumBytesAsUTF8());
}

TEST_CASE("a session comes back as it went in")
{
    SessionState in;
    in.activeSlot = 2;
    // Line endings, a header-like section and non-ASCII text: the things a .pch is made of.
    in.slots.push_back({ 0, juce::String::fromUTF8("Bass \xC3\xB1"), bytes("[Header]\r\nVersion=Nord Modular patch 3.0\r\n"), {} });
    in.slots.push_back({ 2, "Lead", bytes("[ModuleDump]\n1 2 3\n"), bytes("variations") });

    const auto block = in.toBlock();
    SessionState out;
    REQUIRE(SessionState::fromBlock(block.getData(), block.getSize(), out));

    CHECK(out.activeSlot == 2);
    REQUIRE(out.slots.size() == 2);
    CHECK(out.slots[0].slot == 0);
    CHECK(out.slots[0].name == in.slots[0].name);
    CHECK(out.slots[0].pch == in.slots[0].pch);
    CHECK(out.slots[0].var.getSize() == 0);
    CHECK(out.slots[1].slot == 2);
    CHECK(out.slots[1].pch == in.slots[1].pch);
    CHECK(out.slots[1].var == in.slots[1].var);
}

TEST_CASE("an empty session is a session")
{
    SessionState in;
    const auto block = in.toBlock();
    SessionState out;
    CHECK(SessionState::fromBlock(block.getData(), block.getSize(), out));
    CHECK(out.slots.empty());
}

TEST_CASE("things that are not a session are refused")
{
    SessionState out;
    CHECK_FALSE(SessionState::fromBlock(nullptr, 0, out));

    const auto junk = bytes("not xml at all");
    CHECK_FALSE(SessionState::fromBlock(junk.getData(), junk.getSize(), out));

    const auto other = bytes("<Other version=\"1\"/>");
    CHECK_FALSE(SessionState::fromBlock(other.getData(), other.getSize(), out));

    // A project saved by a newer editor is not half read.
    const auto newer = bytes("<NMESession version=\"99\" activeSlot=\"0\"/>");
    CHECK_FALSE(SessionState::fromBlock(newer.getData(), newer.getSize(), out));
}

TEST_CASE("a slot with no patch in it, or no number, is dropped")
{
    const auto text = bytes("<NMESession version=\"1\" activeSlot=\"1\">"
                            "<Slot index=\"0\" name=\"Empty\"><Pch></Pch></Slot>"
                            "<Slot name=\"NoIndex\"><Pch>QQ==</Pch></Slot>"
                            "<Slot index=\"3\" name=\"Good\"><Pch>QQ==</Pch></Slot>"
                            "</NMESession>");
    SessionState out;
    REQUIRE(SessionState::fromBlock(text.getData(), text.getSize(), out));
    REQUIRE(out.slots.size() == 1);
    CHECK(out.slots[0].slot == 3);
    CHECK(out.slots[0].name == "Good");
    CHECK(out.slots[0].pch == bytes("A"));
}
