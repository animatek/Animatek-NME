#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PatchParser.h"
#include "model/PatchSerializer.h"
#include "midi/UploadPacketizer.h"

// The round trip under test is the real wire path: serializeForUpload()
// produces the raw 8-bit sections the upload cuts into packets, and the
// download side hands PatchParser 7-bit MIDI bytes. Any field that does not
// survive serialize -> parse -> serialize is a field the synth would lose.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static std::unique_ptr<Patch> parseRaw(const std::vector<std::vector<uint8_t>>& rawSections)
{
    // Each section is independently 7-bit encoded on the wire.
    std::vector<std::vector<uint8_t>> midi;
    midi.reserve(rawSections.size());
    for (const auto& s : rawSections)
        midi.push_back(UploadPacketizer::pack7Bit(s.data(), s.size()));

    PatchParser parser(descriptions());
    return parser.parse(midi);
}

static std::unique_ptr<Patch> buildTestPatch()
{
    auto& descs = descriptions();
    auto patch = std::make_unique<Patch>();
    patch->setName("RTTEST");

    auto& header = patch->getHeader();
    header.voices = 4;
    header.bendRange = 12;
    header.portamento = true;
    header.portamentoTime = 42;
    header.octaveShift = 1;

    const auto* constant = descs.getModuleByName("Constant");
    const auto* oscA     = descs.getModuleByName("OscA");
    const auto* out2     = descs.getModuleByName("2Output");
    REQUIRE(constant != nullptr);
    REQUIRE(oscA != nullptr);
    REQUIRE(out2 != nullptr);

    auto* mConst = patch->createModule(1, constant->index, 3, 5,  "Const1", descs);
    auto* mOsc   = patch->createModule(1, oscA->index,     2, 10, "Osc",    descs);
    auto* mOut   = patch->createModule(1, out2->index,     2, 40, "Out",    descs);
    REQUIRE(mConst != nullptr);
    REQUIRE(mOsc != nullptr);
    REQUIRE(mOut != nullptr);

    // One module in the common area too, so both ModuleDump sections carry data
    auto* mCommonConst = patch->createModule(0, constant->index, 0, 0, "CC", descs);
    REQUIRE(mCommonConst != nullptr);

    // A cable: OscA's audio out into the output module's left input
    auto* srcConn = mOsc->getConnector(0, /*isOutput=*/true);
    auto* dstConn = mOut->getConnector(0, /*isOutput=*/false);
    REQUIRE(srcConn != nullptr);
    REQUIRE(dstConn != nullptr);
    patch->getPolyVoiceArea().addConnection(srcConn, dstConn);

    // Distinctive parameter values (setValue clamps to the legal range)
    if (!mConst->getParameters().empty())
        mConst->getParameters()[0].setValue(33);
    if (!mOsc->getParameters().empty())
        mOsc->getParameters()[0].setValue(7);

    // One assignment of each kind
    patch->morphValues = { 10, 20, 30, 40 };
    patch->morphAssignments.push_back({ 1, mConst->getContainerIndex(), 0, 2, 15 });

    patch->knobAssignments[5].assigned = true;
    patch->knobAssignments[5].section = 1;
    patch->knobAssignments[5].module = mConst->getContainerIndex();
    patch->knobAssignments[5].param = 0;

    patch->ctrlAssignments.push_back({ 7, 1, mConst->getContainerIndex(), 0 });

    return patch;
}

TEST_CASE("a patch survives serialize -> parse with its structure intact")
{
    auto original = buildTestPatch();

    PatchSerializer serializer;
    auto sections = serializer.serializeForUpload(*original);
    REQUIRE(sections.size() == 16);

    auto parsed = parseRaw(sections);
    REQUIRE(parsed != nullptr);

    CHECK(parsed->getName() == "RTTEST");
    CHECK(parsed->getHeader().voices == original->getHeader().voices);
    CHECK(parsed->getHeader().bendRange == original->getHeader().bendRange);
    CHECK(parsed->getHeader().portamento == original->getHeader().portamento);
    CHECK(parsed->getHeader().portamentoTime == original->getHeader().portamentoTime);

    REQUIRE(parsed->getPolyVoiceArea().getModules().size() == 3);
    REQUIRE(parsed->getCommonArea().getModules().size() == 1);
    CHECK(parsed->getPolyVoiceArea().getConnections().size() == 1);

    // The Constant keeps its spot, its title and its value
    const auto* origConst = original->getPolyVoiceArea().getModules()[0].get();
    const auto* rtConst = parsed->getPolyVoiceArea().getModuleByIndex(
        origConst->getContainerIndex());
    REQUIRE(rtConst != nullptr);
    CHECK(rtConst->getDescriptor()->index == origConst->getDescriptor()->index);
    CHECK(rtConst->getPosition() == origConst->getPosition());
    CHECK(rtConst->getTitle() == origConst->getTitle());
    REQUIRE_FALSE(rtConst->getParameters().empty());
    CHECK(rtConst->getParameters()[0].getValue() == 33);

    CHECK(parsed->morphValues == original->morphValues);
    REQUIRE(parsed->morphAssignments.size() == 1);
    CHECK(parsed->morphAssignments[0].module == original->morphAssignments[0].module);
    CHECK(parsed->morphAssignments[0].morph == original->morphAssignments[0].morph);

    CHECK(parsed->knobAssignments[5].assigned);
    CHECK(parsed->knobAssignments[5].module == original->knobAssignments[5].module);

    REQUIRE(parsed->ctrlAssignments.size() == 1);
    CHECK(parsed->ctrlAssignments[0].control == 7);
}

TEST_CASE("serialize -> parse -> serialize is byte-identical")
{
    auto original = buildTestPatch();

    PatchSerializer serializer;
    auto gen1 = serializer.serializeForUpload(*original);
    auto parsed = parseRaw(gen1);
    REQUIRE(parsed != nullptr);
    auto gen2 = serializer.serializeForUpload(*parsed);

    REQUIRE(gen1.size() == gen2.size());
    for (size_t i = 0; i < gen1.size(); ++i)
    {
        INFO("section ", i);
        CHECK(gen1[i] == gen2[i]);
    }
}

TEST_CASE("an init patch round-trips too")
{
    Patch empty;

    PatchSerializer serializer;
    auto gen1 = serializer.serializeForUpload(empty);
    auto parsed = parseRaw(gen1);
    REQUIRE(parsed != nullptr);

    CHECK(parsed->getPolyVoiceArea().getModules().empty());
    CHECK(parsed->getCommonArea().getModules().empty());

    auto gen2 = serializer.serializeForUpload(*parsed);
    REQUIRE(gen1.size() == gen2.size());
    for (size_t i = 0; i < gen1.size(); ++i)
    {
        INFO("section ", i);
        CHECK(gen1[i] == gen2[i]);
    }
}
