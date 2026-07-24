#include "SetPatchTitleMessage.h"

SetPatchTitleMessage::SetPatchTitleMessage(int slot, int pid, const juce::String& title)
    : slot_(slot)
    , pid_(pid)
    , title_(title)
{
    // CRITICAL: Truncate to 15 characters max (16+ causes synth to hang!)
    if (title_.length() > 15)
        title_ = title_.substring(0, 15);
}

std::vector<uint8_t> SetPatchTitleMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // Payload: pid:7 sc:7 String$name

    // PID (7 bits)
    msg.push_back(pid_ & 0x7F);

    // Subcommand: 0x27
    msg.push_back(0x27);

    // String name (null-terminated, max 15 chars)
    for (int i = 0; i < title_.length(); ++i)
    {
        uint8_t ch = static_cast<uint8_t>(title_[i]);
        msg.push_back(ch & 0x7F);  // 7-bit ASCII
    }

    // Null terminator
    msg.push_back(0x00);

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
