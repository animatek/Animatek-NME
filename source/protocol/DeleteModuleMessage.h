#pragma once

#include "SysExMessage.h"
#include <vector>
#include <cstdint>

/**
 * DeleteModuleMessage - Protocol message to delete a module from a patch
 *
 * MIDI Protocol:
 *   cc = 0x17 (PatchHandling)
 *   sc = 0x32 (ModuleDeletion)
 *   payload: pid + section + module
 *
 * PDL2: ModuleDeletion := 0:1 section:7 0:1 module:7
 */
class DeleteModuleMessage : public SysExMessage
{
public:
    DeleteModuleMessage(int pid, int section, int moduleIndex);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int section_;
    int moduleIndex_;
};
