#include "SendControllerSnapshotMessage.h"

SendControllerSnapshotMessage::SendControllerSnapshotMessage(int pid)
    : pid_(pid)
{
}

std::vector<uint8_t> SendControllerSnapshotMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    appendHeader(msg, 0x17, slot);

    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x55);  // sc = SendControllerSnapshot

    appendFooter(msg);
    return msg;
}
