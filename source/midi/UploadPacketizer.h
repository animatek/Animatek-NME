#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The G1's bulk upload is ONE continuous byte stream cut into packets, exactly
// as the original C libnmprotocol does: `first` marks only the very first
// packet of the transfer, `last` only the final one, and each packet carries
// how many sections end inside it. A packet bigger than the synth accepts gets
// answered with a bogus checksum error and the upload dies (issue #39), and a
// transfer that stops without a `last` packet leaves the synth deaf to all
// MIDI (issue #40) — which is why this logic lives here as pure functions,
// where the tests can hold it to those rules without a synth on the wire.
namespace UploadPacketizer
{
    // Largest raw (8-bit) payload the synth accepts per packet.
    constexpr int kPacketBytes = 166;

    struct Packet
    {
        std::vector<uint8_t> data;   // raw 8-bit bytes, at most kPacketBytes
        int sectionsEnded = 0;       // sections completed inside this packet
        std::string label;           // section descriptions, for the log line
    };

    /** Lays the sections end to end and cuts the stream into packets of at
        most `packetBytes` raw bytes. A section ends inside whichever packet
        took its last byte, which is what the pid field of that packet
        reports. `labels`, when given, must parallel `sections`. */
    std::vector<Packet> cut(const std::vector<std::vector<uint8_t>>& sections,
                            const std::vector<std::string>& labels = {},
                            int packetBytes = kPacketBytes);

    /** Packs raw 8-bit bytes into 7-bit MIDI data bytes, MSB first, the
        trailing partial group left-justified — the same encoding
        BitStreamWriter::toMidiBytes() produces, applied per packet rather
        than per section. */
    std::vector<uint8_t> pack7Bit(const uint8_t* raw, size_t count);

    /** Frames one packet as the SysEx the synth accepts:
        F0 33 [0:1 cc:5 slot:2] 06 [0:1 1:1 sectionsEnded:6] [7-bit data] sum F7
        with cc = 0x1c | first | last<<1. */
    std::vector<uint8_t> frame(const Packet& packet, bool isFirst, bool isLast,
                               int slot);

    /** The empty `last` packet that gets the synth out of bulk-receive state
        when a transfer has to stop early (issue #40). */
    std::vector<uint8_t> closeTransferFrame(int slot);
}
