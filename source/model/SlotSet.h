#pragma once

#include <juce_core/juce_core.h>
#include <array>

// A slot set: what each of the four slots should hold, saved together and
// recalled together. Layered patches (a drone in A, its counterpoint in B...)
// only make the sound as a group, and the G1 has nowhere to say so: it stores
// patches one bank position at a time, and performances only arrived with the G2.
//
// So a set lives on disk, as a folder in the library's Sets/ holding ordinary
// .pch files (and their .var sidecars) plus one manifest naming them. The
// patches stay standard files that open on their own; the manifest only adds
// what a folder cannot say: which slot each one goes to, which slots to switch
// off, and which to leave alone.
struct SlotSet
{
    enum class Mode
    {
        Keep,     // leave the slot exactly as it is
        Patch,    // load this set's patch into the slot
        Disable   // switch the slot off on the synth, whatever it holds
    };

    struct Slot
    {
        Mode mode = Mode::Keep;
        juce::String file;        // Patch: a bare file name inside the set folder
        juce::String patchName;   // Patch: informational, shown before loading
        bool enabled = true;      // Patch: whether the slot plays once loaded
        int bankSection = -1;     // Patch: bank it was stored in when saved (0-8), -1 unknown
        int bankPosition = -1;    // Patch: position in that bank (0-98)
    };

    juce::String name;
    juce::String notes;
    int focusSlot = -1;           // the slot to focus after loading, -1 = leave focus alone
    std::array<Slot, 4> slots;

    static constexpr int kFormatVersion = 1;

    bool changesAnything() const;
    int numPatches() const;
};

// The manifest's extension. It sits inside the set's folder and is named after
// the set, so the folder still reads sensibly in a file manager.
inline constexpr const char* kSlotSetExtension = ".nmset";

juce::String slotSetToJson(const SlotSet& set);
// Rejects anything this editor cannot honour faithfully rather than loading
// half of it: an unknown mode, a newer format, or a file name reaching outside
// the set's folder.
bool slotSetFromJson(const juce::String& text, SlotSet& set, juce::String& error);

// A patch file named in a manifest must be a plain .pch inside the set folder:
// no separators and nothing hidden. A manifest is a file people pass around.
bool isSafeSlotSetFileName(const juce::String& fileName);
// "A Nucleo Duro.pch" for slot A, without doubling a letter the patch name
// already starts with.
juce::String slotSetPatchFileName(int slot, const juce::String& patchName);
// The folder (and manifest) name for a set, or empty when nothing legal is left.
juce::String slotSetFolderName(const juce::String& setName);

// A bank location as the synth browser shows it (414 = bank 4, position 14),
// and back. -1 / false when there is no valid location.
int slotSetBankNumber(int section, int position);
bool slotSetBankFromNumber(int number, int& section, int& position);

// Which slots end up enabled, given which are enabled now.
std::array<bool, 4> slotSetEnableMask(const SlotSet& set, const std::array<bool, 4>& current);
// The slot to focus once the set is in. The synth always keeps its focused slot
// enabled, so focus has to land on a slot the set leaves on, or a slot the set
// switches off would stay on.
int slotSetFocusAfterLoad(const SlotSet& set, const std::array<bool, 4>& mask, int currentFocus);
