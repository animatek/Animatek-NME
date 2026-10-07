#include <doctest.h>
#include "midi/DirectLink.h"
#include <atomic>
#include <cstdlib>
#include <thread>

// TCP keeps the order of the bytes but not where one message ends: the splitter puts the SysEx
// messages back together however the stream was cut.
TEST_CASE("the direct link's byte stream comes back as whole SysEx messages")
{
    const std::vector<uint8_t> a { 0xf0, 0x33, 0x00, 0x06, 0x01, 0x03, 0x03, 0xf7 };
    const std::vector<uint8_t> b { 0xf0, 0x33, 0x4c, 0x06, 0x12, 0x40, 0x01, 0x02, 0x03, 0x7f, 0xf7 };
    std::vector<uint8_t> stream { 0xfe };   // noise before the first message is dropped
    stream.insert(stream.end(), a.begin(), a.end());
    stream.insert(stream.end(), b.begin(), b.end());

    for (size_t cut = 1; cut < stream.size(); ++cut)
    {
        DirectLink::SysExSplitter splitter;
        std::vector<std::vector<uint8_t>> out;
        auto collect = [&out](const std::vector<uint8_t>& m) { out.push_back(m); };
        splitter.feed(stream.data(), cut, collect);
        splitter.feed(stream.data() + cut, stream.size() - cut, collect);
        REQUIRE(out.size() == 2);
        CHECK(out[0] == a);
        CHECK(out[1] == b);
    }
}

TEST_CASE("the greeting names the instance and the ids of its MIDI PC Port")
{
    DirectLink::Instance inst;
    REQUIRE(DirectLink::parseGreeting("G1-Emu 1 G1-Emu plugin 2\tpcport=129-0,129-1", inst));
    CHECK(inst.name == "G1-Emu plugin 2");
    CHECK(inst.pcPortIds == juce::StringArray { "129-0", "129-1" });

    REQUIRE(DirectLink::parseGreeting("G1-Emu 1 G1-Emu", inst));   // an emulator that knows no ids
    CHECK(inst.name == "G1-Emu");
    CHECK(inst.pcPortIds.isEmpty());

    REQUIRE(DirectLink::parseGreeting("G1-Emu 1 G1-Emu plugin 1\tpcport=,\tpcname=G1-Emu PC Port", inst));
    CHECK(inst.name == "G1-Emu plugin 1");
    CHECK(inst.pcPortIds.isEmpty());   // JUCE's virtual ports on Linux have no id
    CHECK(inst.pcPortName == "G1-Emu PC Port");

    CHECK_FALSE(DirectLink::parseGreeting("SSH-2.0-OpenSSH", inst));
}

TEST_CASE("a link is listed among the MIDI devices by an id that names its port")
{
    CHECK(DirectLink::deviceId(47311) == "g1link:47311");
    CHECK(DirectLink::isDeviceId("g1link:47311"));
    CHECK_FALSE(DirectLink::isDeviceId("hw:2,0,0"));
    CHECK(DirectLink::portOf("g1link:47311") == 47311);
}

// Against a running emulator, only when asked: NME_G1EMU_LINK_TEST=1 with G1-Emu listening.
TEST_CASE("the editor's IAm over the direct link is answered by G1-Emu")
{
    if (std::getenv("NME_G1EMU_LINK_TEST") == nullptr)
        return;

    const auto found = DirectLink::discover();
    REQUIRE_FALSE(found.empty());
    CHECK_FALSE(found[0].busy);
    MESSAGE("found " << found[0].name << " on port " << found[0].port);

    DirectLink::Client client;
    std::atomic<bool> answered { false };
    REQUIRE(client.open(found[0].port, [&answered](std::vector<uint8_t> m) {
        // IAm from the synth: F0 33 00 06 01 (sender 1 = the G1) ...
        if (m.size() > 5 && m[1] == 0x33 && m[3] == 0x06 && m[4] == 0x01)
            answered = true;
    }, {}));
    client.send({ 0xf0, 0x33, 0x00, 0x06, 0x00, 0x03, 0x03, 0xf7 });
    for (int i = 0; i < 300 && !answered; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(answered);

    // While this editor holds it, the emulator shows as busy to anyone else.
    const auto again = DirectLink::discover();
    REQUIRE_FALSE(again.empty());
    CHECK(again[0].busy);
    client.close();
}
