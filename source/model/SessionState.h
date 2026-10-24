#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// What a DAW project keeps of the editor (#94): the patch each slot held and the slot
// that was on screen. The plugin hands this to the host when the project is saved and
// gets it back when the project opens, so the editor is not empty again.
//
// A slot's patch travels as the .pch (and .var, when there are variations) the editor
// would write to disk, so every way of saving and reading one already agrees on it.
// The connection is not in here: the MIDI ports and G1-Emu links are already
// remembered by the editor's own settings and found again on their own.
struct SessionSlot
{
    int slot = 0;                 // global slot, synth * 4 + slot
    juce::String name;            // the patch's name; a .pch carries none
    juce::MemoryBlock pch;
    juce::MemoryBlock var;        // empty when the patch has no variations
};

struct SessionState
{
    static constexpr int kFormatVersion = 1;

    int activeSlot = 0;
    std::vector<SessionSlot> slots;

    juce::MemoryBlock toBlock() const;
    // False for anything that is not a session of this format or an older one:
    // a project saved by a newer editor is left alone rather than half read.
    static bool fromBlock(const void* data, size_t size, SessionState& out);
};
