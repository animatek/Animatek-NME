#include <doctest.h>
#include "midi/ConnectionManager.h"
#include "model/Patch.h"
#include "model/PatchSerializer.h"

using Frames = std::vector<std::vector<uint8_t>>;

static void receive(NmProtocol& protocol, int cc, const std::vector<uint8_t>& payload)
{
    const auto frame = SysEx::encode(cc, 0, payload, cc != NmCmd::IAm);
    protocol.processIncoming(frame.data(), frame.size());
}

// The real protocol and connection logic, but no MIDI device is opened.
static void connectRecorder(ConnectionManager& connection, Frames& frames)
{
    auto& protocol = connection.getProtocol();
    protocol.setSendFunction([&frames](const std::vector<uint8_t>& frame) {
        frames.push_back(frame);
    });
    receive(protocol, NmCmd::IAm, {1, 3, 3});
    REQUIRE(connection.isConnected());
}

static void dispatchFor(int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}

TEST_CASE("Replacing a protocol sender discards the previous queue and reply wait")
{
    Frames previous, next;
    NmProtocol protocol;
    protocol.setSendFunction([&previous](const auto& frame) { previous.push_back(frame); });
    protocol.sendMessage(NmCmd::IAm, 0, {0, 3, 3}, true, false);
    protocol.sendMessage(NmCmd::PatchHandling, 2, {0x41, 0x35}, true);
    REQUIRE(previous.size() == 1);
    REQUIRE(protocol.isWaitingForReply());

    protocol.setSendFunction([&next](const auto& frame) { next.push_back(frame); });
    CHECK_FALSE(protocol.isWaitingForReply());
    protocol.sendMessage(NmCmd::IAm, 0, {0, 3, 3}, true, false);
    REQUIRE(next.size() == 1);
    CHECK(next.front() == previous.front());
    receive(protocol, NmCmd::ACK, {5, 0x7f, 5});
    CHECK(next.size() == 1);

    protocol.setSendFunction({});
    protocol.sendMessage(NmCmd::PatchHandling, 3, {0x41, 0x35}, true);
    protocol.setSendFunction([&next](const auto& frame) { next.push_back(frame); });
    receive(protocol, NmCmd::ACK, {5, 0x7f, 5});
    CHECK(next.size() == 1);
}

TEST_CASE("MIDI device disconnection and destruction detach the pending sender")
{
    NmProtocol protocol;
    auto device = std::make_unique<MidiDeviceManager>(protocol);
    protocol.sendMessage(NmCmd::IAm, 0, {0, 3, 3}, true, false);
    protocol.sendMessage(NmCmd::PatchHandling, 2, {0x41, 0x35}, true);
    REQUIRE(protocol.isWaitingForReply());

    SUBCASE("disconnect while the owner stays alive") { device->disconnect(); }
    SUBCASE("destroy the owner before the protocol timeout")
    {
        device.reset();
        dispatchFor(NmProtocol::timeoutMs + 100);
    }

    REQUIRE_FALSE(protocol.isWaitingForReply());
    // A late reply or a subsequent send must not invoke the freed device.
    receive(protocol, NmCmd::ACK, {5, 0x7f, 5});
    protocol.sendMessage(NmCmd::IAm, 0, {0, 3, 3}, true, false);
    CHECK_FALSE(protocol.isWaitingForReply());
    dispatchFor(NmProtocol::heartbeatIntervalMs + 20);
}

TEST_CASE("Disconnect resets a pending patch fetch and its saved bank location")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    connection.setSlotBankLocation(2, 4, 20);
    connection.requestPatch(2);
    REQUIRE(connection.isFetchingPatch());
    REQUIRE(connection.getProtocol().isWaitingForReply());

    connection.disconnect();
    CHECK_FALSE(connection.isConnected());
    CHECK_FALSE(connection.isFetchingPatch());
    CHECK_FALSE(connection.getProtocol().isWaitingForReply());
    CHECK(connection.getSlotBankSection(2) == -1);
    CHECK(connection.getSlotBankPosition(2) == -1);
    const auto sentBefore = frames.size();
    // Let both the request watchdog and the protocol timeout expire.
    dispatchFor(NmProtocol::timeoutMs + 100);
    CHECK_FALSE(connection.isConnected());
    CHECK(frames.size() == sentBefore);
}

TEST_CASE("Deferred settings and list requests cannot cross a reconnect")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);

    SUBCASE("settings readback") { connection.sendSynthSettings(SynthSettings{}); }
    SUBCASE("interrupted patch list resume")
    {
        connection.requestPatchList();
        connection.cancelPatchListFetch("test interruption");
        connection.resumePatchListIfInterrupted();
    }

    connection.disconnect();
    connectRecorder(connection, frames);
    frames.clear();
    dispatchFor(600);
    CHECK(frames.empty());
    CHECK(connection.isConnected());

    connection.sendParameter(3, 1, 2, 0, 99);
    REQUIRE(frames.size() == 1);
    const auto decoded = SysEx::decode(frames.front().data(), frames.front().size());
    CHECK(decoded.slot == 3);
    CHECK(decoded.cc == NmCmd::ParameterChange);
}

TEST_CASE("Destroying a connection cancels deferred settings readback")
{
    Frames frames;
    auto connection = std::make_unique<ConnectionManager>();
    connectRecorder(*connection, frames);
    connection->sendSynthSettings(SynthSettings{});
    connection.reset();
    const auto sentBefore = frames.size();
    dispatchFor(350);
    CHECK(frames.size() == sentBefore);
}

TEST_CASE("Disconnect clears structural and parameter queues before reconnect")
{
    Frames frames;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    const auto first = SysEx::encode(NmCmd::PatchHandling, 1, {0, 0x31, 1, 1, 4, 4});
    const auto second = SysEx::encode(NmCmd::PatchHandling, 2, {0, 0x31, 1, 2, 8, 8});
    connection.sendAckedSysEx(first);
    connection.sendAckedSysEx(second);
    connection.queueParameter(2, 1, 2, 0, 99);
    REQUIRE_FALSE(connection.isAckedQueueIdle());
    REQUIRE(frames.size() == 1);

    connection.disconnect();
    CHECK(connection.isAckedQueueIdle());
    connectRecorder(connection, frames);
    frames.clear();
    connection.sendAckedSysEx(second);
    REQUIRE(frames.size() == 1);
    CHECK(frames.front() == second);
    receive(connection.getProtocol(), NmCmd::ACK, {0, 0x7f, 0});
    dispatchFor(100);
    CHECK(connection.isAckedQueueIdle());
    CHECK(frames.size() == 1);
}

TEST_CASE("Disconnect closes an upload before detaching the transport")
{
    Frames frames;
    int completions = 0;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    connection.setUploadCompleteCallback([&completions]() { ++completions; });
    connection.uploadPatch(2, Patch{});
    REQUIRE(connection.isUploadingPatch());
    REQUIRE(frames.size() == 1);
    connection.queueParameter(2, 1, 1, 0, 42);
    // An ACK schedules the next packet/completion for later, on the old session.
    receive(connection.getProtocol(), NmCmd::ACK, {5, 0x7f, 5});
    connection.disconnect();
    REQUIRE(frames.size() == 2);
    CHECK(frames.back() == UploadPacketizer::closeTransferFrame(2));
    CHECK_FALSE(connection.isUploadingPatch());
    connectRecorder(connection, frames);
    frames.clear();
    dispatchFor(100);
    CHECK(frames.empty());
    CHECK(completions == 0);
}

TEST_CASE("Disconnect reports an unfinished bank upload as failed once")
{
    Frames frames;
    int failures = 0;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    connection.setBankUploadResultCallback([&failures](bool success) {
        CHECK_FALSE(success);
        ++failures;
    });
    connection.uploadPatch(1, Patch{});
    connection.disconnect();
    connection.disconnect();
    CHECK(failures == 1);
}

TEST_CASE("A bank upload still completes once when its connection remains alive")
{
    Frames frames;
    int completions = 0;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    connection.setBankUploadResultCallback([&completions](bool success) {
        CHECK(success);
        ++completions;
    });
    connection.uploadPatch(1, Patch{});
    REQUIRE(frames.size() == 1);
    REQUIRE((frames.front()[2] & 0x08) != 0);  // The init patch fits in one last packet.
    receive(connection.getProtocol(), NmCmd::ACK, {5, 0x7f, 5});
    dispatchFor(150);
    REQUIRE_FALSE(connection.isUploadingPatch());
    REQUIRE(completions == 1);
    connection.disconnect();
    CHECK(completions == 1);
}

TEST_CASE("Disconnect cancels an upload completion already posted to the message thread")
{
    Frames frames;
    int completions = 0, bankFailures = 0;
    bool bankUpload = false;
    ConnectionManager connection;
    connectRecorder(connection, frames);
    SUBCASE("editor completion")
    {
        connection.setUploadCompleteCallback([&completions]() { ++completions; });
    }
    SUBCASE("bank result")
    {
        bankUpload = true;
        connection.setBankUploadResultCallback([&](bool success) {
            if (success) ++completions;
            else ++bankFailures;
        });
    }
    Patch patch;
    PatchSerializer serializer;
    const auto packets = UploadPacketizer::cut(serializer.serializeForUpload(patch));
    connection.uploadPatch(0, patch);
    for (size_t i = 0; i < packets.size(); ++i)
    {
        receive(connection.getProtocol(), NmCmd::ACK, {5, 0x7f, 5});
        // Pump timers without dispatching the callAsync completion they post.
        // The timer thread may still be waiting for a previous timer delivery.
        for (int attempt = 0; attempt < 200 && connection.isUploadingPatch()
             && frames.size() == i + 1; ++attempt)
        {
            juce::Thread::sleep(5);
            juce::Timer::callPendingTimersSynchronously();
        }
        REQUIRE((!connection.isUploadingPatch() || frames.size() > i + 1));
    }
    REQUIRE_FALSE(connection.isUploadingPatch());
    REQUIRE(completions == 0);
    connection.disconnect();
    dispatchFor(50);
    CHECK(completions == 0);
    CHECK(bankFailures == (bankUpload ? 1 : 0));
}
