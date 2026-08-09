#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

/**
 * BitStreamWriter - Writes bits and converts to 7-bit MIDI encoding.
 * Inverse of BitStream (which reads 7-bit MIDI into bits).
 */
class BitStreamWriter
{
public:
    BitStreamWriter() = default;

    // Write n bits (1-32) from value, MSB first
    void writeBits(uint32_t value, int n)
    {
        if (n < 1 || n > 32)
            throw std::runtime_error("BitStreamWriter::writeBits: invalid width " + std::to_string(n));

        for (int i = n - 1; i >= 0; --i)
            bits.push_back((value >> i) & 1);
    }

    // Write a null-terminated string (up to 16 chars)
    // PDL2: String := 16*chars:8/0
    void writeString16(const std::string& str)
    {
        size_t len = str.length();
        if (len > 16)
            len = 16;

        for (size_t i = 0; i < len; ++i)
        {
            uint8_t ch = static_cast<uint8_t>(str[i]);
            writeBits(ch, 8);
        }

        // Null terminator only if the string didn't fill all 16 slots
        if (len < 16)
            writeBits(0, 8);
    }

    // Align to next 8-bit boundary
    void alignToByte()
    {
        size_t remainder = bits.size() % 8;
        if (remainder != 0)
        {
            size_t padding = 8 - remainder;
            for (size_t i = 0; i < padding; ++i)
                bits.push_back(0);
        }
    }

    // Convert bitstream to 7-bit MIDI encoding
    // Per PDL2 spec: pack bits into 7-bit bytes
    std::vector<uint8_t> toMidiBytes()
    {
        // Align to byte boundary first (PDL2 "Section % 8" rule)
        alignToByte();

        std::vector<uint8_t> result;
        result.reserve((bits.size() + 6) / 7);

        // Pack 7 bits at a time into MIDI bytes
        for (size_t i = 0; i < bits.size(); i += 7)
        {
            uint8_t byte = 0;
            for (int j = 0; j < 7 && (i + j) < bits.size(); ++j)
            {
                byte = (byte << 1) | bits[i + j];
            }
            // If less than 7 bits remain, left-justify them
            if (bits.size() - i < 7)
            {
                byte <<= (7 - (bits.size() - i));
            }
            result.push_back(byte);
        }

        return result;
    }

    // Convert the bitstream to plain 8-bit bytes.
    //
    // The upload path needs this rather than toMidiBytes(): the synth is fed one
    // continuous byte stream chopped into fixed-size packets, and each packet is
    // 7-bit encoded on its own, so the 7-bit packing cannot happen per section.
    std::vector<uint8_t> toBytes()
    {
        alignToByte();

        std::vector<uint8_t> result;
        result.reserve(bits.size() / 8);

        for (size_t i = 0; i < bits.size(); i += 8)
        {
            uint8_t byte = 0;
            for (int j = 0; j < 8; ++j)
                byte = static_cast<uint8_t>((byte << 1) | bits[i + static_cast<size_t>(j)]);
            result.push_back(byte);
        }

        return result;
    }

    size_t bitCount() const { return bits.size(); }
    void clear() { bits.clear(); }

private:
    std::vector<uint8_t> bits;  // each element is 0 or 1
};
