#pragma once

#include "SysExMessage.h"
#include "../model/Descriptors.h"
#include <string>
#include <vector>

/**
 * NewModule message (cc=0x1f, PatchPacket with first+last bits set)
 * Creates a new module in the synth's current patch.
 *
 * Encodes 5 PDL2 sections:
 *  - SingleModule (type 48): type, index, xpos, ypos
 *  - CableDump (type 82): section, 0 cables
 *  - ParameterDump (type 77): section, parameters
 *  - CustomDump (type 91): section, custom values (usually empty)
 *  - NameDump (type 90): section, module name
 */
class NewModuleMessageProto : public SysExMessage
{
public:
    NewModuleMessageProto(int pid, int typeId, int section, int index,
                          int xpos, int ypos, const std::string& name,
                          const std::vector<int>& paramValues,
                          const std::vector<int>& customValues = {});

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int typeId_;
    int section_;
    int index_;
    int xpos_;
    int ypos_;
    std::string name_;
    std::vector<int> paramValues_;
    std::vector<int> customValues_;
};
