#include "NewCableMessage.h"

NewCableMessage::NewCableMessage(int pid, int section, SignalType color,
                                 int module1, bool isOutput1, int connector1,
                                 int module2, bool isOutput2, int connector2)
    : pid_(pid)
    , section_(section)
    , color_(color)
    , module1_(module1)
    , module2_(module2)
    , isOutput1_(isOutput1)
    , isOutput2_(isOutput2)
    , connector1_(connector1)
    , connector2_(connector2)
{
}

std::vector<uint8_t> NewCableMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // PatchModification prefix: 0:1 pid:7 0:1 sc:7
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x50);  // sc = CableInsert

    // Payload (5 bytes) per PDL2 spec:
    // CableInsert :=
    //   0:1 1:3 section:1 color:3
    //   0:1 module1:7 0:1 type1:1 connector1:6
    //   0:1 module2:7 0:1 type2:1 connector2:6

    // Byte 0: 0:1 1:3 section:1 color:3
    // PDL2 "1:3" = value 1 in 3 bits = 001, NOT 111
    int byte0 = (0x01 << 4) | ((section_ & 0x01) << 3) | (static_cast<int>(color_) & 0x07);
    msg.push_back(static_cast<uint8_t>(byte0));

    // Byte 1: 0:1 module1:7
    msg.push_back(static_cast<uint8_t>(module1_ & 0x7F));

    // Byte 2: 0:1 type1:1 connector1:6
    int type1 = isOutput1_ ? 1 : 0;
    msg.push_back(static_cast<uint8_t>(((type1 & 0x01) << 6) | (connector1_ & 0x3F)));

    // Byte 3: 0:1 module2:7
    msg.push_back(static_cast<uint8_t>(module2_ & 0x7F));

    // Byte 4: 0:1 type2:1 connector2:6
    int type2 = isOutput2_ ? 1 : 0;
    msg.push_back(static_cast<uint8_t>(((type2 & 0x01) << 6) | (connector2_ & 0x3F)));

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
