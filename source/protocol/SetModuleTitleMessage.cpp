#include "SetModuleTitleMessage.h"

SetModuleTitleMessage::SetModuleTitleMessage(int pid, int section, int moduleIndex,
                                             const juce::String& title)
    : pid_(pid)
    , section_(section)
    , moduleIndex_(moduleIndex)
    , title_(title)
{
    // Module names are a 16-character PDL2 String; the Java reference truncates
    // the same way before sending.
    if (title_.length() > 16)
        title_ = title_.substring(0, 16);
}

std::vector<uint8_t> SetModuleTitleMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // PatchModification prefix: 0:1 pid:7 0:1 sc:7
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x33);  // sc = SetModuleTitle

    // SetModuleTitle := 0:1 section:7 0:1 module:7 String$name
    msg.push_back(static_cast<uint8_t>(section_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(moduleIndex_ & 0x7F));

    for (int i = 0; i < title_.length(); ++i)
        msg.push_back(static_cast<uint8_t>(title_[i]) & 0x7F);

    // Terminator only when the name is shorter than the full 16 slots
    if (title_.length() < 16)
        msg.push_back(0x00);

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
