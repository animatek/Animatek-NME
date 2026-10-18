#include <doctest.h>
#include "midi/UploadPacketizer.h"
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PatchParser.h"
#include "model/PatchSerializer.h"
#include "model/PchFileIO.h"

// The four header flags (#103): four bits on the wire after the retriggers,
// four numbers in a .pch's [Header], all on by default, as in Clavia's editor.

namespace
{
ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static const bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

std::unique_ptr<Patch> parseRaw(const std::vector<uint8_t>& raw)
{
    PatchParser parser(descriptions());
    return parser.parse({ UploadPacketizer::pack7Bit(raw.data(), raw.size()) });
}

juce::StringArray headerTokens(const juce::File& file)
{
    const auto lines = juce::StringArray::fromLines(file.loadFileAsString());
    const int at = lines.indexOf("[Header]");
    REQUIRE(at >= 0);
    auto tokens = juce::StringArray::fromTokens(lines[at + 2], " ", "");
    tokens.removeEmptyStrings();   // the line ends in a space, as in Clavia's files
    return tokens;
}
}  // namespace

TEST_CASE("A G1's header reads its four flags and the whole separator")
{
    // "Fellowship", bank 8 position 1 of a G1, read from flash (2026-10-18).
    const auto patch = parseRaw({ 0x21, 0x01, 0xfc, 0x07, 0xf1, 0x00, 0x7e, 0x7d, 0x02, 0xff, 0xff });
    REQUIRE(patch != nullptr);
    const auto& h = patch->getHeader();
    CHECK(h.separatorPosition == 4000);
    CHECK(h.voices == 32);
    CHECK(h.headerFlags == std::array<int, 4>{ 1, 1, 1, 1 });

    // Written to a .pch, the line is the one Clavia's editor wrote for this patch.
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("nme-header-g1-" + juce::Uuid().toString() + ".pch");
    PchFileIO io(descriptions());
    REQUIRE(io.writeFile(*patch, file));
    CHECK(headerTokens(file).joinIntoString(" ")
          == "0 127 0 127 2 0 0 32 4000 2 1 1 1 1 1 1 1 1 1 1 1 1 1");
    file.deleteFile();
}

TEST_CASE("The flags go out in the header as they came in")
{
    Patch patch;
    patch.getHeader().headerFlags = { 1, 0, 1, 0 };
    patch.getHeader().separatorPosition = 4000;
    PatchSerializer serializer;
    const auto sections = serializer.serializeForUpload(patch);
    const auto back = parseRaw(sections[1]);   // [1] is the Header
    REQUIRE(back != nullptr);
    CHECK(back->getHeader().headerFlags == std::array<int, 4>{ 1, 0, 1, 0 });
    CHECK(back->getHeader().separatorPosition == 4000);
}

TEST_CASE("A .pch writes the flags where Clavia's editor writes them")
{
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("nme-header-flags-" + juce::Uuid().toString() + ".pch");
    PchFileIO io(descriptions());

    SUBCASE("a new patch: 1 1 1 1, as Clavia's editor writes it")
    {
        Patch patch;
        REQUIRE(io.writeFile(patch, file));
        const auto tokens = headerTokens(file);
        REQUIRE(tokens.size() == 23);
        CHECK(tokens[12] == "1");
        CHECK(tokens[13] == "1");
        CHECK(tokens[14] == "1");
        CHECK(tokens[15] == "1");
    }
    SUBCASE("a file's flags survive a read and a write")
    {
        Patch patch;
        patch.getHeader().headerFlags = { 0, 1, 1, 0 };
        REQUIRE(io.writeFile(patch, file));
        const auto read = io.readFile(file);
        REQUIRE(read != nullptr);
        CHECK(read->getHeader().headerFlags == std::array<int, 4>{ 0, 1, 1, 0 });
    }
    file.deleteFile();
}

// Every .pch Clavia's editor saved in a folder keeps its [Header] line through
// NME's reader and writer. Run with NME_PCH_DIR=<folder>.
TEST_CASE("Clavia's .pch headers come out of NME unchanged")
{
    const char* dir = std::getenv("NME_PCH_DIR");
    if (dir == nullptr)
        return;

    PchFileIO io(descriptions());
    const auto out = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("nme-header-" + juce::Uuid().toString() + ".pch");
    int checked = 0;
    for (const auto& f : juce::File(dir).findChildFiles(juce::File::findFiles, false, "*.pch"))
    {
        const auto patch = io.readFile(f);
        if (patch == nullptr)
            continue;
        REQUIRE(io.writeFile(*patch, out));
        CHECK_MESSAGE(headerTokens(out) == headerTokens(f), f.getFileName());
        ++checked;
    }
    out.deleteFile();
    MESSAGE(checked << " files");
    CHECK(checked > 0);
}
