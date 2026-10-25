#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Installing an OS into a G1's flash over MIDI, as Clavia's OS updater does.
// The synth has to be in its boot ROM's update utility first (Shift + Store
// held at power-on; G1-Emu: Settings > OS... > Update mode), which answers the
// editor's greeting with 7F where the normal OS puts its serial's top bits.
//
// The stream (read from the update utility, checked against a capture of
// Clavia's 3.03b updater: same messages byte for byte but the last one's
// padding), every message F0 33 7F 06 <cmd> ... F7:
//   70  header: flash address, length (5 packed bytes each), the OS version
//       (two bytes), and the sum of those 12 bytes & 7F
//   71  112 data bytes, packed into 128. Nothing answers each one: the synth
//       takes them as they come, so the sender paces them
//   73  the 32-bit sum of every data byte as sent. Right, the synth answers
//       F0 33 00 06 7F F7 and asks "Update flash?": Store writes it, any other
//       key keeps the old OS
// Packing: groups of 8, the first byte carrying the low bit of each of the
// next 7 (LSB first), each byte being (b << 1) | bit. Before packing, every
// byte has its odd and even bits swapped and is XORed with a key that starts
// at 17 and grows by 17.
//
// The image is the OS as it runs from RAM at $100000 (385,024 bytes for the
// rack's 3.03b). Nothing of Clavia's is here: the user brings it.
namespace OsUpdate
{
    constexpr uint32_t kFlashAddress = 0x300020;   // the OS, past the flash's 32-byte header
    constexpr int kBlockBytes = 112;
    // The flash keeps the OS only up to $70000, where the patch banks start;
    // the loader also copies one long word past the image.
    constexpr size_t kMaxImageBytes = 0x70000 - 0x20 - 4;
    constexpr size_t kMinImageBytes = 0x10000;

    struct Version { int major = 3; int minor = 3; };

    // Why an image cannot go, or empty when it can.
    std::string checkImage(const std::vector<uint8_t>& image);

    // The whole update, header to checksum, without the greeting.
    std::vector<std::vector<uint8_t>> buildMessages(const std::vector<uint8_t>& image,
                                                    Version version = {});

    // The synth's answer to the final checksum message.
    bool isChecksumAccepted(const uint8_t* data, size_t length);

    // An IAm from a synth in its update utility (bytes after F0 33 00 06).
    bool isUpdateModeIAm(const uint8_t* payload, size_t length);

    // Exposed for the tests.
    std::vector<uint8_t> pack(const uint8_t* raw, size_t count);
    std::vector<uint8_t> unpack(const uint8_t* packed, size_t count);
    std::vector<uint8_t> scramble(const std::vector<uint8_t>& image);
    std::vector<uint8_t> unscramble(const std::vector<uint8_t>& data);
}
