#include "SysExCodec.h"

namespace SysEx
{

std::vector<uint8_t> encode(int cc, int slot, const std::vector<uint8_t>& payload, bool addChecksum)
{
    std::vector<uint8_t> msg;
    msg.reserve(5 + payload.size() + (addChecksum ? 1 : 0));

    // Wire format: F0 33 [header] 06 [payload] [checksum?] F7
    msg.push_back(SYSEX_START);
    msg.push_back(MANUFACTURER);
    msg.push_back(encodeHeader(cc, slot));
    msg.push_back(DEVICE);

    for (auto b : payload)
        msg.push_back(b);

    if (addChecksum)
    {
        // Checksum covers ALL bytes from F0 through payload (verified against synth responses)
        auto chk = checksum(msg.data(), msg.size());
        msg.push_back(chk);
    }

    msg.push_back(SYSEX_END);

    return msg;
}

DecodedMessage decode(const uint8_t* data, size_t length)
{
    DecodedMessage result;

    // Minimum: F0 33 header 06 F7 = 5 bytes (no payload, no checksum)
    if (length < 5)
        return result;

    if (data[0] != SYSEX_START || data[length - 1] != SYSEX_END)
        return result;

    // Wire format: F0 33 [header] 06 [payload...] F7
    if (data[1] != MANUFACTURER)
        return result;

    decodeHeader(data[2], result.cc, result.slot);

    if (data[3] != DEVICE)
        return result;

    // Payload is everything between DEVICE byte and F7
    size_t payloadStart = 4;
    size_t payloadEnd   = length - 1;  // before F7

    if (payloadEnd > payloadStart)
        result.payload.assign(data + payloadStart, data + payloadEnd);

    result.valid = true;
    return result;
}

} // namespace SysEx
