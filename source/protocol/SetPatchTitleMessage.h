#pragma once

#include "SysExMessage.h"
#include <juce_core/juce_core.h>

/**
 * SetPatchTitle message (cc=0x17, pp=0x41, sc=0x27)
 * Changes the patch name in the synthesizer's current slot.
 *
 * This change is immediate but NOT permanently saved to flash.
 * Use StorePatch to save permanently.
 *
 * PDL2 spec (midi.pdl2):
 *   SetPatchTitle := String$name
 */
class SetPatchTitleMessage : public SysExMessage
{
public:
    /**
     * @param slot Device slot (0-3)
     * @param pid Patch ID (from ACK message)
     * @param title New patch name (max 15 characters, 16 causes synth to hang!)
     */
    SetPatchTitleMessage(int slot, int pid, const juce::String& title);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int slot_;
    int pid_;
    juce::String title_;
};
