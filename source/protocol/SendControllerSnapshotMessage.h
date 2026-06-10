#pragma once

#include "SysExMessage.h"

/**
 * SendControllerSnapshot message (cc=0x17, sc=0x55)
 *
 * Asks the synth to transmit, on its MIDI out, the current value of every
 * parameter with a MIDI controller assignment in the slot's active patch —
 * one CC message per assignment. Typically recorded at the start of a
 * sequencer track so playback initializes the patch state. Read-only:
 * nothing on the synth changes.
 *
 * PDL2 spec (midi.pdl2):
 *   SendControllerSnapshot := ;   (empty body, pid + sc only)
 */
class SendControllerSnapshotMessage : public SysExMessage
{
public:
    explicit SendControllerSnapshotMessage(int pid);

    std::vector<uint8_t> toSysEx(int slot = 0) const override;

private:
    int pid_ = 0;
};
