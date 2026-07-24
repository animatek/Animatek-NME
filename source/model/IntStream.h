#pragma once

#include <vector>
#include <cstdint>
#include <string>

/**
 * IntStream - Simple stream of 7-bit integers.
 * Mimics Java IntStream used in NewModuleMessage.
 * Each value is 0-127 (7 bits).
 */
class IntStream
{
public:
    IntStream() = default;

    // Append a 7-bit value (0-127)
    void append(int value)
    {
        values.push_back(value & 0x7F);
    }

    // Append a null-terminated string (max 16 chars)
    void appendString(const std::string& str)
    {
        size_t len = str.length();
        if (len > 16)
            len = 16;

        for (size_t i = 0; i < len; ++i)
            append(static_cast<uint8_t>(str[i]));

        append(0);  // null terminator
    }

    const std::vector<int>& getValues() const { return values; }
    size_t size() const { return values.size(); }
    bool empty() const { return values.empty(); }

private:
    std::vector<int> values;
};
