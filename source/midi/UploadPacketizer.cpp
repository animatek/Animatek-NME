#include "UploadPacketizer.h"

namespace UploadPacketizer
{

std::vector<Packet> cut(const std::vector<std::vector<uint8_t>>& sections,
                        const std::vector<std::string>& labels,
                        int packetBytes)
{
    std::vector<Packet> packets;
    Packet current;
    current.data.reserve(static_cast<size_t>(packetBytes));

    for (size_t s = 0; s < sections.size(); ++s)
    {
        const auto& section = sections[s];
        for (size_t i = 0; i < section.size(); ++i)
        {
            if (static_cast<int>(current.data.size()) == packetBytes)
            {
                packets.push_back(std::move(current));
                current = Packet();
                current.data.reserve(static_cast<size_t>(packetBytes));
            }
            current.data.push_back(section[i]);
        }
        // The section ends inside whichever packet took its last byte.
        current.sectionsEnded++;
        if (s < labels.size())
            current.label += (current.label.empty() ? "" : ", ") + labels[s];
    }
    if (!current.data.empty())
        packets.push_back(std::move(current));

    return packets;
}

std::vector<uint8_t> pack7Bit(const uint8_t* raw, size_t count)
{
    std::vector<uint8_t> out;
    out.reserve((count * 8 + 6) / 7);

    uint32_t buffer = 0;
    int held = 0;
    for (size_t i = 0; i < count; ++i)
    {
        buffer = (buffer << 8) | raw[i];
        held += 8;
        while (held >= 7)
        {
            held -= 7;
            out.push_back(static_cast<uint8_t>((buffer >> held) & 0x7F));
        }
    }
    if (held > 0)
        out.push_back(static_cast<uint8_t>((buffer << (7 - held)) & 0x7F));

    return out;
}

std::vector<uint8_t> frame(const Packet& packet, bool isFirst, bool isLast,
                           int slot)
{
    const int cc = 0x1c | (isFirst ? 1 : 0) | (isLast ? 2 : 0);

    // payload[0]: 0:1 command:1 pid:6
    //   MSB=0, command=1 (bulk upload), pid=sections ended in this packet
    const uint8_t cmdPidByte = static_cast<uint8_t>(0x40 | (packet.sectionsEnded & 0x3F));

    std::vector<uint8_t> msg;
    msg.push_back(0xF0);
    msg.push_back(0x33);
    msg.push_back(static_cast<uint8_t>(((cc & 0x1F) << 2) | (slot & 0x03)));
    msg.push_back(0x06);
    msg.push_back(cmdPidByte);
    auto encoded = pack7Bit(packet.data.data(), packet.data.size());
    msg.insert(msg.end(), encoded.begin(), encoded.end());
    // Checksum: sum of all bytes (F0 through last payload byte) % 128
    uint32_t sum = 0;
    for (auto b : msg)
        sum += b;
    msg.push_back(static_cast<uint8_t>(sum % 128));
    msg.push_back(0xF7);
    return msg;
}

std::vector<uint8_t> closeTransferFrame(int slot)
{
    Packet empty;
    return frame(empty, /*isFirst=*/false, /*isLast=*/true, slot);
}

}
