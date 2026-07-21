#pragma once

#include <juce_core/juce_core.h>  // juce::StreamingSocket lives here in this JUCE version
#include <memory>

class MainComponent;
class McpRequestHandler;

// Tiny embedded control server for the local MCP bridge (see mcp-bridge/ at
// the repo root): listens on 127.0.0.1 for newline-delimited JSON requests
// and dispatches each one, on the JUCE message thread, to McpRequestHandler -
// so an external AI agent can add modules / connect cables in the live patch
// exactly as if a user had done it via the canvas.
//
// Deliberately localhost-only and unauthenticated: this is a local dev/
// control channel, not a network service. Gated at build time by the
// NME_MCP_BRIDGE CMake option.
class McpBridgeServer : private juce::Thread
{
public:
    // Fixed default port for mcp-bridge/server.py to target. Arbitrarily
    // chosen from the dynamic/ephemeral range to keep collisions unlikely;
    // not configurable yet since there's only ever one AnimatekNME instance
    // running at a time (moreThanOneInstanceAllowed() == false).
    static constexpr int defaultPort = 51027;

    McpBridgeServer(MainComponent& owner, int port = defaultPort);
    ~McpBridgeServer() override;

    void start();
    void stop();

    bool isRunning() const { return isThreadRunning(); }
    bool isListening() const { return bound_; }
    int getPort() const { return port_; }

private:
    void run() override;
    void handleConnection(juce::StreamingSocket& socket);

    // Runs `request` through McpRequestHandler on the message thread and
    // blocks the calling (connection-handling) thread until either the
    // result is ready or a watchdog timeout elapses.
    juce::var dispatch(const juce::var& request);

    MainComponent& owner_;
    int port_;
    juce::StreamingSocket listener_;
    std::unique_ptr<McpRequestHandler> handler_;
    std::atomic<bool> bound_{false};  // true once createListener() actually succeeded

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(McpBridgeServer)
};
