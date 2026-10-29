#include "OsUpdate.h"

namespace OsUpdate
{
namespace
{
    std::vector<uint8_t> message(uint8_t cmd, const std::vector<uint8_t>& body)
    {
        std::vector<uint8_t> m { 0xF0, 0x33, 0x7F, 0x06, cmd };
        m.insert(m.end(), body.begin(), body.end());
        m.push_back(0xF7);
        return m;
    }

    std::vector<uint8_t> packWord(uint32_t v)
    {
        const uint8_t raw[4] = { uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v) };
        return pack(raw, 4);
    }

    uint8_t swapBits(uint8_t x)
    {
        return uint8_t(((x & 0x55) << 1) | ((x & 0xAA) >> 1));
    }
}

std::vector<uint8_t> pack(const uint8_t* raw, size_t count)
{
    std::vector<uint8_t> out;
    out.reserve(count + (count + 6) / 7);
    for (size_t g = 0; g < count; g += 7)
    {
        const size_t n = count - g < 7 ? count - g : 7;
        uint8_t low = 0;
        for (size_t k = 0; k < n; ++k)
            low |= uint8_t((raw[g + k] & 1) << k);
        out.push_back(low);
        for (size_t k = 0; k < n; ++k)
            out.push_back(uint8_t(raw[g + k] >> 1));
    }
    return out;
}

std::vector<uint8_t> unpack(const uint8_t* packed, size_t count)
{
    std::vector<uint8_t> out;
    out.reserve(count);
    size_t j = 0;
    while (out.size() < count)
    {
        uint8_t low = packed[j++];
        for (int k = 0; k < 7 && out.size() < count; ++k, low >>= 1)
            out.push_back(uint8_t((packed[j++] << 1) | (low & 1)));
    }
    return out;
}

std::vector<uint8_t> scramble(const std::vector<uint8_t>& image)
{
    std::vector<uint8_t> out(image.size());
    uint8_t key = 17;
    for (size_t i = 0; i < image.size(); ++i, key = uint8_t(key + 17))
        out[i] = uint8_t(swapBits(image[i]) ^ key);
    return out;
}

std::vector<uint8_t> unscramble(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> out(data.size());
    uint8_t key = 17;
    for (size_t i = 0; i < data.size(); ++i, key = uint8_t(key + 17))
        out[i] = swapBits(uint8_t(data[i] ^ key));
    return out;
}

std::string checkImage(const std::vector<uint8_t>& image)
{
    if (image.size() < kMinImageBytes)
        return "It is too small to be a G1 OS (" + std::to_string(image.size()) + " bytes).";
    // The whole ROM of a G1 (512 KB) is what G1-Emu boots, and the web builder hands it out
    // beside the OS update image: the two get mixed up, so say which is which.
    if (image.size() == 0x80000)
        return "This is a whole 512 KB ROM, which is for G1-Emu, not an update image. For a G1 you want "
               "the OS update that comes with it (the web builder's \"OS update\" download, "
               "or tools/g1update.py fromrom).";
    if (image.size() > kMaxImageBytes)
        return "It is too big for an OS update (" + std::to_string(image.size()) + " bytes, at most "
             + std::to_string(kMaxImageBytes) + ": the G1 erases only that much of its flash).";
    if (image.size() % 4 != 0)
        return "Its size is not a whole number of long words, which no G1 OS image has.";
    // Every G1 OS starts by masking the interrupts: move.w #$2700,sr
    if (image[0] != 0x46 || image[1] != 0xFC || image[2] != 0x27 || image[3] != 0x00)
        return "It does not start like a G1 OS (the image that runs from RAM at $100000).";
    return {};
}

std::vector<std::vector<uint8_t>> buildMessages(const std::vector<uint8_t>& image, Version version)
{
    const auto data = scramble(image);

    std::vector<uint8_t> head = packWord(kFlashAddress);
    const auto length = packWord(uint32_t(data.size()));
    head.insert(head.end(), length.begin(), length.end());
    head.push_back(uint8_t(version.major & 0x7F));
    head.push_back(uint8_t(version.minor & 0x7F));
    unsigned sum = 0;
    for (auto b : head)
        sum += b;
    head.push_back(uint8_t(sum & 0x7F));

    std::vector<std::vector<uint8_t>> msgs;
    msgs.reserve(data.size() / kBlockBytes + 3);
    msgs.push_back(message(0x70, head));

    // The last block goes full, padded with zeros: the synth takes only what
    // the length still asks for, and the checksum counts only those bytes.
    uint32_t checksum = 0;
    for (size_t i = 0; i < data.size(); i += kBlockBytes)
    {
        uint8_t block[kBlockBytes] = {};
        for (size_t k = 0; k < size_t(kBlockBytes) && i + k < data.size(); ++k)
        {
            block[k] = data[i + k];
            checksum += data[i + k];
        }
        msgs.push_back(message(0x71, pack(block, kBlockBytes)));
    }
    msgs.push_back(message(0x73, packWord(checksum)));
    return msgs;
}

bool isChecksumAccepted(const uint8_t* data, size_t length)
{
    return length == 6 && data[0] == 0xF0 && data[1] == 0x33 && data[2] == 0x00
        && data[3] == 0x06 && data[4] == 0x7F && data[5] == 0xF7;
}

bool isUpdateModeIAm(const uint8_t* payload, size_t length)
{
    // sender 1, the version, then 7F where the normal OS puts its serial's top bits
    return length >= 4 && payload[0] == 1 && payload[3] == 0x7F;
}
}
