#include <doctest.h>
#include "model/PchFileIO.h"

// Issue #87: old Mac patches carry a 128-byte MacBinary header before the text.
static juce::MemoryBlock macBinary(const juce::String& name, const juce::String& body)
{
    juce::MemoryBlock block(128, true);
    auto* h = static_cast<juce::uint8*>(block.getData());
    h[1] = (juce::uint8) name.length();
    for (int i = 0; i < name.length(); ++i)
        h[2 + i] = (juce::uint8) name[i];
    std::memcpy(h + 65, "PCH NORD", 8);
    const auto length = (juce::uint32) body.getNumBytesAsUTF8();
    h[83] = (juce::uint8) (length >> 24);
    h[84] = (juce::uint8) (length >> 16);
    h[85] = (juce::uint8) (length >> 8);
    h[86] = (juce::uint8) length;
    block.append(body.toRawUTF8(), (size_t) length);
    return block;
}

TEST_CASE("A MacBinary header is taken off the text of a patch")
{
    const juce::String body = "[Header]\r\nVersion=Nord Modular patch 3.0\r\n[/Header]\r\n";

    CHECK(PchFileIO::patchTextFromBytes(macBinary("DRYistheword", body)) == body);

    // The data fork's length stops it short of a resource fork after it.
    auto withTrailer = macBinary("DRYistheword", body);
    withTrailer.append("\0\0\0junk", 7);
    CHECK(PchFileIO::patchTextFromBytes(withTrailer) == body);

    // A plain patch is not touched.
    juce::MemoryBlock plain(body.toRawUTF8(), (size_t) body.getNumBytesAsUTF8());
    CHECK(PchFileIO::patchTextFromBytes(plain) == body);
}
