#include <doctest.h>
#include "midi/SynthHub.h"

// Slots are numbered globally, synth * 4 + slot, and only SynthHub knows how to turn
// one back into a synth and its own local slot.
TEST_CASE("a global slot splits into its synth and its local slot and back")
{
    for (int slot = 0; slot < kTotalSlots; ++slot)
    {
        CHECK(SynthSlot::valid(slot));
        CHECK(SynthSlot::global(SynthSlot::synthOf(slot), SynthSlot::localOf(slot)) == slot);
        CHECK(SynthSlot::localOf(slot) >= 0);
        CHECK(SynthSlot::localOf(slot) < kSlotsPerSynth);
        CHECK(SynthSlot::synthOf(slot) < kMaxSynths);
    }
    CHECK_FALSE(SynthSlot::valid(-1));
    CHECK_FALSE(SynthSlot::valid(kTotalSlots));
}

TEST_CASE("the second synth's slots follow the first's")
{
    static_assert(kMaxSynths >= 2, "this test is about several synths");
    CHECK(SynthSlot::global(0, 3) == 3);
    CHECK(SynthSlot::global(1, 0) == 4);
    CHECK(SynthSlot::synthOf(5) == 1);
    CHECK(SynthSlot::localOf(5) == 1);
}

TEST_CASE("slots are named for the user by synth and letter")
{
    CHECK(SynthSlot::label(0) == "1A");
    CHECK(SynthSlot::label(5) == "2B");
}

TEST_CASE("the first synth keeps the bare letter in saved layouts")
{
    // The window layout saved before there were several synths is stored under
    // mdiSlotA .. mdiSlotD, and the first synth must still find it.
    CHECK(SynthSlot::key(0) == "A");
    CHECK(SynthSlot::key(3) == "D");
    CHECK(SynthSlot::key(4) == "2A");
}
