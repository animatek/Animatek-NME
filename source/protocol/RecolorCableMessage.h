#pragma once

#include "SysExMessage.h"
#include "../model/SignalType.h"

/**
 * CableRecolor message (cc=0x17, PatchModification sc=0x54)
 * Changes the colour of an existing cable on the synth without removing it.
 *
 * Not in midi.pdl2. The layout comes from the original editor (NME303.exe,
 * CMCableRecolor, see G1originaleditor/notes/07): the same four connector
 * bytes as CableInsert, and a first byte with the colour in the low bits:
 *   0:1 0:4 section:1 color:3
 *   0:1 module1:7 0:1 type1:1 connector1:6
 *   0:1 module2:7 0:1 type2:1 connector2:6
 *
 * Read from the binary, not yet sent to a real synth: nothing in the editor
 * calls it.
 */
class RecolorCableMessage : public SysExMessage
{
public:
    /** @param color The new colour of the cable. Other arguments as NewCableMessage. */
    RecolorCableMessage(int pid, int section, SignalType color,
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
