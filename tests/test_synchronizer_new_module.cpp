#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "midi/ConnectionManager.h"
#include "protocol/NewModuleMessage.h"
#include "sync/PatchSynchronizer.h"

// Adding a module over the wire has to carry that module's custom-class values.
// The encoder has always been able to (test_new_module_message.cpp checks it
// byte for byte against jnmprotocol's reference OscA packet), but the
// synchronizer passed an empty vector, so the synth never heard about OscA's
// frequency-display unit or a sequencer's steps and invented its own defaults.
// The gap was in the production path, so these tests drive the production path:
// a real Patch, the real descriptors and the real synchronizer, with the frames
// recorded off the protocol instead of going out of a MIDI port.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

namespace
{
// The connection, minus the MIDI ports: the protocol's send function appends to
// a vector, and a synthetic IAm completes the handshake.
struct Recorder
{
    ConnectionManager connection;
    std::vector<std::vector<uint8_t>> frames;

    Recorder()
    {
        connection.getProtocol().setSendFunction(
            [this](const std::vector<uint8_t>& frame) { frames.push_back(frame); });
        const auto iam = SysEx::encode(NmCmd::IAm, 0, {1, 3, 3}, false);
        connection.getProtocol().processIncoming(iam.data(), iam.size());
        REQUIRE(connection.isConnected());
        frames.clear();   // drop the handshake traffic
    }
};

// What the synchronizer should have built for the module it just announced.
std::vector<uint8_t> expectedFrame(ConnectionManager& connection, int slot, int section,
                                   Module& module, const std::vector<int>& customValues)
{
    std::vector<int> paramValues;
    for (auto& param : module.getParameters())
        if (param.getDescriptor()->paramClass == "parameter")
            paramValues.push_back(param.getValue());

    NewModuleMessageProto msg(connection.getPatchId(slot), module.getDescriptor()->index,
                              section, module.getContainerIndex(),
                              module.getPosition().x, module.getPosition().y,
                              module.getTitle().toStdString(), paramValues, customValues);
    return msg.toSysEx(slot);
}
}  // namespace

TEST_CASE("Adding a module announces its custom values, not an empty CustomDump")
{
    constexpr int slot = 0;
    constexpr int section = 1;   // poly area

    // OscA carries one custom value (its frequency-display unit) and NoteSeqB
    // two, which is what pins the count: the CustomDump is positional, so a
    // dropped value shifts every later one.
    juce::String moduleName;
    size_t expectedCustomCount = 0;
    SUBCASE("OscA, one custom value")   { moduleName = "OscA";     expectedCustomCount = 1; }
    SUBCASE("NoteSeqB, two custom values") { moduleName = "NoteSeqB"; expectedCustomCount = 2; }

    const auto* desc = descriptions().getModuleByName(moduleName);
    REQUIRE(desc != nullptr);

    Recorder rec;
    Patch patch;
    PatchSynchronizer sync(patch, rec.connection, slot);
    sync.enable();

    auto* module = patch.createModule(section, desc->index, 2, 9, moduleName, descriptions());
    REQUIRE(module != nullptr);

    std::vector<int> customValues;
    for (auto& param : module->getParameters())
        if (param.getDescriptor()->paramClass == "custom")
            customValues.push_back(param.getValue());
    REQUIRE(customValues.size() == expectedCustomCount);

    REQUIRE(rec.frames.size() == 1);
    CHECK(rec.frames.front() == expectedFrame(rec.connection, slot, section, *module, customValues));

    // The regression this guards. A default custom value is usually zero, and
    // an omitted zero is not the same packet as a sent zero: jnmprotocol's
    // reference OscA packet carries it. Leaving the collection loop out again
    // fails here rather than on the hardware.
    CHECK(rec.frames.front() != expectedFrame(rec.connection, slot, section, *module, {}));
}
