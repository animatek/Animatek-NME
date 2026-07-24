#include "MoveModuleMessage.h"

MoveModuleMessage::MoveModuleMessage(int pid, int section, int moduleIndex, int xpos, int ypos)
    : pid_(pid)
    , section_(section)
    , moduleIndex_(moduleIndex)
    , xpos_(xpos)
    , ypos_(ypos)
{
}

std::vector<uint8_t> MoveModuleMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // PatchModification prefix: 0:1 pid:7 0:1 sc:7
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x34);  // sc = ModuleMove

    // Payload (4 bytes) per PDL2 spec:
    // ModuleMove := 0:1 section:7 0:1 module:7 0:1 xpos:7 0:1 ypos:7
    msg.push_back(static_cast<uint8_t>(section_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(moduleIndex_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(xpos_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(ypos_ & 0x7F));

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
