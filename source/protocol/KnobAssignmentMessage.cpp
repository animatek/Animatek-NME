#include "KnobAssignmentMessage.h"

static std::vector<uint8_t> buildSysEx(int cc, int slot, int pid, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> msg;
    msg.push_back(0xF0);
    msg.push_back(0x33);
    msg.push_back(static_cast<uint8_t>(((cc & 0x1F) << 2) | (slot & 0x03)));
    msg.push_back(0x06);

    // pid byte: 0:1 pid:7
    msg.push_back(static_cast<uint8_t>(pid & 0x7F));

    msg.insert(msg.end(), payload.begin(), payload.end());

    // Checksum: sum of all bytes % 128
    uint32_t sum = 0;
    for (auto b : msg) sum += b;
    msg.push_back(static_cast<uint8_t>(sum % 128));
    msg.push_back(0xF7);
    return msg;
}

// sc=0x25: KnobAssignment := 0:1 module:7 0:1 parameter:7 0:1 section:2 knob:5
static std::vector<uint8_t> encodeKnobAssignment(int sc, int module, int param, int section, int knob)
{
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(sc & 0x7F));
    payload.push_back(static_cast<uint8_t>(module & 0x7F));
    payload.push_back(static_cast<uint8_t>(param & 0x7F));
    payload.push_back(static_cast<uint8_t>(((section & 0x03) << 5) | (knob & 0x1F)));
    return payload;
}

std::vector<uint8_t> KnobAssignmentMessage::assign(int pid, int knob, int section, int module, int param, int slot)
{
    auto payload = encodeKnobAssignment(0x25, module, param, section, knob);
    return buildSysEx(0x17, slot, pid, payload);
}

std::vector<uint8_t> KnobAssignmentMessage::reassign(int pid, int prevKnob, int newKnob, int section, int module, int param, int slot)
{
    // sc=0x26: 0:1 prevknob:7 + NewKnobAssignmentPacket(0:1 0x25:7 + KnobAssignment)
    std::vector<uint8_t> payload;
    payload.push_back(0x26);  // sc
    payload.push_back(static_cast<uint8_t>(prevKnob & 0x7F));
    // NewKnobAssignmentPacket
    payload.push_back(0x25);  // marker
    payload.push_back(static_cast<uint8_t>(module & 0x7F));
    payload.push_back(static_cast<uint8_t>(param & 0x7F));
    payload.push_back(static_cast<uint8_t>(((section & 0x03) << 5) | (newKnob & 0x1F)));
    return buildSysEx(0x17, slot, pid, payload);
}

std::vector<uint8_t> KnobAssignmentMessage::deassign(int pid, int prevKnob, int slot)
{
    // sc=0x26: 0:1 prevknob:7 (no NewKnobAssignmentPacket)
    std::vector<uint8_t> payload;
    payload.push_back(0x26);  // sc
    payload.push_back(static_cast<uint8_t>(prevKnob & 0x7F));
    return buildSysEx(0x17, slot, pid, payload);
}

static const char* kKnobNames[23] = {
    "Knob 1",  "Knob 2",  "Knob 3",  "Knob 4",  "Knob 5",  "Knob 6",
    "Knob 7",  "Knob 8",  "Knob 9",  "Knob 10", "Knob 11", "Knob 12",
    "Knob 13", "Knob 14", "Knob 15", "Knob 16", "Knob 17", "Knob 18",
    "(unused)", "Pedal", "After touch", "(unused)", "On/Off switch"
};

const char* KnobAssignmentMessage::getKnobName(int knobIndex)
{
    if (knobIndex >= 0 && knobIndex < 23)
        return kKnobNames[knobIndex];
    return "?";
}
