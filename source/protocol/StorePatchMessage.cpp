#include "StorePatchMessage.h"

StorePatchMessage::StorePatchMessage(int slot, int section, int position)
    : slot_(slot)
    , section_(section)
    , position_(position)
{
}

std::vector<uint8_t> StorePatchMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // Subcommand prefix: pp=0x41
    msg.push_back(0x41);

    // Subcommand: StorePatch (0x0b)
    msg.push_back(0x0b);

    // Payload (3 bytes) per PDL2 spec:
    // 0:1 slot:7 0:1 section:7 0:1 position:7

    // Byte 0: 0:1 slot:7
    int byte0 = (slot_ & 0x7F);

    // Byte 1: 0:1 section:7
    int byte1 = (section_ & 0x7F);

    // Byte 2: 0:1 position:7
    int byte2 = (position_ & 0x7F);

    msg.push_back(byte0);
    msg.push_back(byte1);
    msg.push_back(byte2);

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
