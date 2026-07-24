#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>

// Converts 7-bit MIDI-encoded bytes into a continuous bitstream.
// Each input byte contributes its lower 7 bits. The total bit count
// is then truncated to a byte boundary (multiple of 8) per the PDL2
// "Section % 8" alignment rule.
class BitStream
{
public:
    explicit BitStream(const std::vector<uint8_t>& midiBytes)
    {
        // Pack 7 bits from each MIDI byte into a flat bit buffer
        bits.reserve(midiBytes.size() * 7);
        for (auto b : midiBytes)
        {
            for (int i = 6; i >= 0; --i)
                bits.push_back((b >> i) & 1);
        }

        // Truncate to byte boundary
        totalBits = (bits.size() / 8) * 8;
    }

    // Read n bits (1-32) as an unsigned integer, MSB first
    uint32_t readBits(int n)
    {
        if (n < 1 || n > 32)
            throw std::runtime_error("BitStream::readBits: invalid width " + std::to_string(n));
        if (position + static_cast<size_t>(n) > totalBits)
            throw std::runtime_error("BitStream::readBits: read past end (pos="
                + std::to_string(position) + " n=" + std::to_string(n)
                + " total=" + std::to_string(totalBits) + ")");

        uint32_t value = 0;
        for (int i = 0; i < n; ++i)
            value = (value << 1) | bits[position++];
        return value;
    }

    // Read a null-terminated string, up to 16 chars max.
    // PDL2: String := 16*chars:8/0 — the /0 means stop at null terminator.
    // Non-printable characters are replaced with '?' to avoid JUCE assertion failures.
    std::string readString16()
    {
        std::string result;
        result.reserve(16);
        for (int i = 0; i < 16; ++i)
        {
            if (remaining() < 8)
                break;  // Not enough data
            auto val = readBits(8);
            if (val == 0)
                break;  // Null terminator — stop reading per PDL2 /0 rule
            if (val >= 0x20 && val <= 0x7E)
                result += static_cast<char>(val);
            else
                result += '?';
        }
        return result;
    }

    // Advance position to next 8-bit boundary
    void alignToByte()
    {
        size_t remainder = position % 8;
        if (remainder != 0)
            position += (8 - remainder);
    }

    size_t remaining() const { return (position < totalBits) ? (totalBits - position) : 0; }
    size_t getPosition() const { return position; }
    void setPosition(size_t pos) { position = pos; }
    size_t getTotalBits() const { return totalBits; }

private:
    std::vector<uint8_t> bits;  // each element is 0 or 1
    size_t position = 0;
    size_t totalBits = 0;
};
