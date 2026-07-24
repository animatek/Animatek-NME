#include "DeleteModuleMessage.h"

DeleteModuleMessage::DeleteModuleMessage(int pid, int section, int moduleIndex)
    : pid_(pid)
    , section_(section)
    , moduleIndex_(moduleIndex)
{
}

std::vector<uint8_t> DeleteModuleMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // PatchModification prefix: 0:1 pid:7 0:1 sc:7
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x32);  // sc = ModuleDeletion

    // Payload (2 bytes) per PDL2 spec:
    // ModuleDeletion := 0:1 section:7 0:1 module:7
    msg.push_back(static_cast<uint8_t>(section_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(moduleIndex_ & 0x7F));

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
