#pragma once

#include "SysExMessage.h"
#include "../model/SignalType.h"

/**
 * CableDelete message (cc=0x17, PatchModification sc=0x51)
 * Removes a cable connection between two module connectors.
 *
 * PDL2 spec (midi.pdl2):
 *   PatchModification := 0:1 pid:7 0:1 sc:7 ...
 *   CableDelete :=
 *     0:1 1:6 section:1
 *     0:1 module1:7 0:1 type1:1 connector1:6
 *     0:1 module2:7 0:1 type2:1 connector2:6
 */
class DeleteCableMessage : public SysExMessage
{
public:
    /**
     * @param pid Patch ID (from ACK response)
     * @param section Voice area (0=common, 1=poly)
     * @param color Signal type/cable color
     * @param module1 First module index
     * @param isOutput1 True if connector1 is an output
     * @param connector1 Connector index within module1
     * @param module2 Second module index
     * @param isOutput2 True if connector2 is an output
     * @param connector2 Connector index within module2
     */
    DeleteCableMessage(int pid, int section, SignalType color,
                      int module1, bool isOutput1, int connector1,
                      int module2, bool isOutput2, int connector2);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int section_;
    SignalType color_;
    int module1_, module2_;
    bool isOutput1_, isOutput2_;
    int connector1_, connector2_;
};
