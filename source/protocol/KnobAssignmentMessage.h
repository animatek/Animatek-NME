#pragma once
#include <vector>
#include <cstdint>

/**
 * KnobAssignment / KnobAssignmentChange messages (cc=0x17)
 *
 * New assignment (sc=0x25):
 *   KnobAssignment := 0:1 module:7 0:1 parameter:7 0:1 section:2 knob:5
 *
 * Change/deassign (sc=0x26):
 *   KnobAssignmentChange := 0:1 prevknob:7 ?NewKnobAssignmentPacket
 *   NewKnobAssignmentPacket := 0:1 0x25:7 KnobAssignment
 *
 * 23 knob slots (KnobMapDump order): 0-17 = Knob 1-18, 19 = Pedal,
 * 20 = After touch, 22 = On/Off switch. Indices 18 and 21 are unused
 * (see nmedit knobmap.h / KnobSet.getByID).
 */
class KnobAssignmentMessage
{
public:
    // New assignment (no previous knob assigned to this param)
    static std::vector<uint8_t> assign(int pid, int knob, int section, int module, int param, int slot);

    // Re-assignment (param was on prevKnob, move to newKnob)
    static std::vector<uint8_t> reassign(int pid, int prevKnob, int newKnob, int section, int module, int param, int slot);

    // Deassign (remove param from knob)
    static std::vector<uint8_t> deassign(int pid, int prevKnob, int slot);

    // Knob display names
    static const char* getKnobName(int knobIndex);
    static constexpr int numKnobs = 23;

    // Indices 18 and 21 are gaps in the wire format, not real knobs
    static constexpr bool isValidKnob(int k)
    {
        return k >= 0 && k < numKnobs && k != 18 && k != 21;
    }
};
