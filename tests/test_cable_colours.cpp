#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PatchParser.h"
#include "model/PatchSerializer.h"
#include "model/PchFileIO.h"
#include "midi/UploadPacketizer.h"

// A cable keeps the colour the patch stores for it (#100), as Clavia's editor 3.03 does: read
// from a .pch or from the synth, written back unchanged. Only a cable made in NME is coloured by
// where it starts. Derived, 8 % of the archive's cables came out another colour, and user1/user2
// (on no connector) could not come out at all.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

// An OscA (type 7) into both inputs of a 2Output (type 4): its out (audio, red) stored as user1
// (4) into input 0, and as master-slave grey (3) into input 1.
static const char* kColoured =
    "[Header]\r\n"
    "Version=Nord Modular patch 3.0\r\n"
    "0 127 0 127 2 0 0 1 4000 2 1 1 0 0 0 0 1 1 1 1 1 1 1 \r\n"
    "[/Header]\r\n"
    "[ModuleDump]\r\n"
    "1 \r\n"
    "1 7 0 0 \r\n"
    "2 4 1 0 \r\n"
    "[/ModuleDump]\r\n"
    "[ModuleDump]\r\n"
    "0 \r\n"
    "[/ModuleDump]\r\n"
    "[CableDump]\r\n"
    "1 \r\n"
    "4 2 0 0 1 0 1 \r\n"
    "3 2 1 0 1 0 1 \r\n"
    "[/CableDump]\r\n"
    "[CableDump]\r\n"
    "0 \r\n"
    "[/CableDump]\r\n"
    "[ParameterDump]\r\n"
    "1 \r\n"
    "1 7 10 52 64 0 64 0 0 0 0 0 0 \r\n"
    "2 4 3 127 0 0 \r\n"
    "[/ParameterDump]\r\n"
    "[ParameterDump]\r\n"
    "0 \r\n"
    "[/ParameterDump]\r\n"
    "[NameDump]\r\n"
    "1 \r\n"
    "1 Osc\r\n"
    "2 Out\r\n"
    "[/NameDump]\r\n"
    "[NameDump]\r\n"
    "0 \r\n"
    "[/NameDump]\r\n";

static std::unique_ptr<Patch> load(const juce::String& text)
{
    const auto file = juce::File::createTempFile(".pch");
    file.replaceWithText(text, false, false, nullptr);
    PchFileIO io(descriptions());
    auto patch = io.readFile(file);
    file.deleteFile();
    return patch;
}

// The colour of the cable into the 2Output's input `input`.
static SignalType colourInto(const Patch& patch, int input)
{
    for (const auto& c : patch.getContainer(1).getConnections())
        if (c.input != nullptr && c.input->getDescriptor()->index == input && !c.input->getDescriptor()->isOutput)
            return c.colour();
    FAIL("no cable into input " << input);
    return SignalType::None;
}

TEST_CASE("a .pch's cable colours are kept, not derived from the source")
{
    auto patch = load(kColoured);
    REQUIRE(patch != nullptr);
    REQUIRE(patch->getContainer(1).getConnections().size() == 2);
    CHECK(colourInto(*patch, 0) == SignalType::User1);
    CHECK(colourInto(*patch, 1) == SignalType::MasterSlave);
}

TEST_CASE("a .pch written back keeps its cables' colours")
{
    auto patch = load(kColoured);
    REQUIRE(patch != nullptr);
    const auto file = juce::File::createTempFile(".pch");
    PchFileIO io(descriptions());
    REQUIRE(io.writeFile(*patch, file));
    auto again = io.readFile(file);
    const auto text = file.loadFileAsString();
    file.deleteFile();
    REQUIRE(again != nullptr);
    CHECK(colourInto(*again, 0) == SignalType::User1);
    CHECK(colourInto(*again, 1) == SignalType::MasterSlave);
    CHECK(text.contains("\n4 2 0 0 1 0 1"));
    CHECK(text.contains("\n3 2 1 0 1 0 1"));
}

TEST_CASE("the colours go to the synth and come back from it as they are")
{
    auto patch = load(kColoured);
    REQUIRE(patch != nullptr);
    PatchSerializer serializer;
    std::vector<std::vector<uint8_t>> midi;
    for (const auto& s : serializer.serializeForUpload(*patch))
        midi.push_back(UploadPacketizer::pack7Bit(s.data(), s.size()));
    PatchParser parser(descriptions());
    auto back = parser.parse(midi);
    REQUIRE(back != nullptr);
    CHECK(colourInto(*back, 0) == SignalType::User1);
    CHECK(colourInto(*back, 1) == SignalType::MasterSlave);
}

TEST_CASE("a cable made in NME is coloured by where it starts")
{
    auto patch = load(kColoured);
    REQUIRE(patch != nullptr);
    auto& poly = patch->getContainer(1);
    auto* osc = poly.getModuleByIndex(1);
    auto* out = poly.getModuleByIndex(2);
    poly.removeConnection(osc->getConnector(0, true), out->getConnector(0, false));
    poly.addConnection(osc->getConnector(0, true), out->getConnector(0, false));
    CHECK(colourInto(*patch, 0) == SignalType::Audio);         // derived: OscA's out
    CHECK(colourInto(*patch, 1) == SignalType::MasterSlave);   // untouched: still stored
}
