#pragma once

#include "SysExMessage.h"

/**
 * ModuleMove message (cc=0x17, PatchModification sc=0x34)
 * Updates a module's position in the patch layout.
 *
 * PDL2 spec (midi.pdl2):
 *   PatchModification := 0:1 pid:7 0:1 sc:7 ...
 *   ModuleMove :=
 *     0:1 section:7 0:1 module:7 0:1 xpos:7 0:1 ypos:7
 */
class MoveModuleMessage : public SysExMessage
{
public:
    /**
     * @param pid Patch ID (from ACK response)
     * @param section Voice area (0=common, 1=poly)
     * @param moduleIndex Module index within section
     * @param xpos X position (grid units)
     * @param ypos Y position (grid units)
     */
    MoveModuleMessage(int pid, int section, int moduleIndex, int xpos, int ypos);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int section_;
    int moduleIndex_;
    int xpos_;
    int ypos_;
};
