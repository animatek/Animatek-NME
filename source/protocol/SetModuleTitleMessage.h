#pragma once

#include "SysExMessage.h"
#include <juce_core/juce_core.h>

/**
 * SetModuleTitle message (cc=0x17, PatchModification sc=0x33)
 * Renames a module in the synth's working slot.
 *
 * Without this the new name only lives in the editor's patch: it would reach
 * the synth on a full patch upload (NameDump section) and nowhere else, so a
 * Store to Bank right after a rename saved the old name.
 *
 * The change is immediate but NOT saved to flash — use StorePatch for that.
 *
 * PDL2 spec (midi.pdl2):
 *   PatchModification := 0:1 pid:7 0:1 sc:7 ...
 *   SetModuleTitle := 0:1 section:7 0:1 module:7 String$name
 */
class SetModuleTitleMessage : public SysExMessage
{
public:
    /**
     * @param pid Patch ID (from ACK response)
     * @param section Voice area (0=common, 1=poly)
     * @param moduleIndex Module index within section
     * @param title New module name (truncated to 16 characters)
     */
    SetModuleTitleMessage(int pid, int section, int moduleIndex, const juce::String& title);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int section_;
    int moduleIndex_;
    juce::String title_;
};
