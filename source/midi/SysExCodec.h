#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>

namespace SysEx
{
    constexpr uint8_t SYSEX_START   = 0xF0;
    constexpr uint8_t SYSEX_END     = 0xF7;
    constexpr uint8_t MANUFACTURER  = 0x33;  // Clavia
    constexpr uint8_t DEVICE        = 0x06;  // Nord Modular

    // Encode cc (5 bits) and slot (2 bits) into the header byte
    // Format: 0:1 cc:5 slot:2
    inline uint8_t encodeHeader(int cc, int slot)
    {
        return static_cast<uint8_t>(((cc & 0x1F) << 2) | (slot & 0x03));
    }

    // Decode header byte into cc and slot
    inline void decodeHeader(uint8_t header, int& cc, int& slot)
    {
        cc   = (header >> 2) & 0x1F;
        slot = header & 0x03;
    }

    // Checksum: sum of bytes % 128
    inline uint8_t checksum(const uint8_t* data, size_t length)
    {
        int sum = 0;
        for (size_t i = 0; i < length; ++i)
            sum += data[i];
        return static_cast<uint8_t>(sum & 0x7F);
    }

    // Wrap a payload into a full SysEx message
    // Wire format: F0 33 [0:1 cc:5 slot:2] 06 [payload] [checksum if addChecksum] F7
    std::vector<uint8_t> encode(int cc, int slot, const std::vector<uint8_t>& payload, bool addChecksum = true);

    struct DecodedMessage
    {
        int cc = 0;
        int slot = 0;
        std::vector<uint8_t> payload;
        bool checksumPresent = false;
        bool valid = false;
    };

    // Strip SysEx envelope and decode
    DecodedMessage decode(const uint8_t* data, size_t length);
}
