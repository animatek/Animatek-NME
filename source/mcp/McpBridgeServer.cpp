#include "McpBridgeServer.h"
#include "McpRequestHandler.h"
#include <juce_events/juce_events.h>
#include <iostream>

McpBridgeServer::McpBridgeServer(MainComponent& owner, int port)
    : juce::Thread("MCP Bridge"), owner_(owner), port_(port),
      handler_(std::make_unique<McpRequestHandler>(owner))
{
}

McpBridgeServer::~McpBridgeServer()
{
    stop();
}

void McpBridgeServer::start()
{
    startThread();
}

void McpBridgeServer::stop()
{
    signalThreadShouldExit();
    listener_.close();  // unblocks waitForNextConnection()
    stopThread(3000);
    bound_ = false;
}

void McpBridgeServer::run()
{
    if (!listener_.createListener(port_, "127.0.0.1"))
    {
        std::cerr << "[MCP] Failed to bind 127.0.0.1:" << port_ << std::endl;
        return;
    }
    bound_ = true;
    std::cout << "[MCP] Listening on 127.0.0.1:" << port_ << std::endl;

    while (!threadShouldExit())
    {
        std::unique_ptr<juce::StreamingSocket> client(listener_.waitForNextConnection());
        if (!client)
            continue;  // listener closed (shutdown) or accept failed
        handleConnection(*client);
    }
}

void McpBridgeServer::handleConnection(juce::StreamingSocket& socket)
{
    juce::String pending;

    while (!threadShouldExit() && socket.isConnected())
    {
        // read(..., false) puts the socket in non-blocking mode and does a
        // single recv() - with no data yet available that returns 0
        // immediately, indistinguishable from a closed connection unless we
        // first confirm data is actually ready. waitUntilReady also gives a
        // periodic wakeup (short timeout) to keep threadShouldExit() live
        // while idle, instead of a bare blocking read.
        int ready = socket.waitUntilReady(true, 200);
        if (ready < 0)
            break;  // socket error/closed
        if (ready == 0)
            continue;  // poll timeout, no data yet - loop back and check threadShouldExit()

        char readBuf[4096];
        int bytesRead = socket.read(readBuf, sizeof(readBuf), false);
        if (bytesRead <= 0)
            break;  // connection closed or error

        pending += juce::String::fromUTF8(readBuf, bytesRead);

        int newlinePos;
        while ((newlinePos = pending.indexOfChar('\n')) >= 0)
        {
            juce::String line = pending.substring(0, newlinePos).trim();
            pending = pending.substring(newlinePos + 1);
            if (line.isEmpty())
                continue;

            juce::var request;
            juce::var response;
            auto parseResult = juce::JSON::parse(line, request);
            if (parseResult.failed())
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("id", juce::var());
                obj->setProperty("ok", false);
                auto* err = new juce::DynamicObject();
                err->setProperty("code", "bad_json");
                err->setProperty("message", parseResult.getErrorMessage());
                obj->setProperty("error", juce::var(err));
                response = juce::var(obj);
            }
            else
            {
                response = dispatch(request);
            }

            juce::String responseLine = juce::JSON::toString(response, true) + "\n";
            socket.write(responseLine.toRawUTF8(), (int) responseLine.getNumBytesAsUTF8());
        }
    }
}

juce::var McpBridgeServer::dispatch(const juce::var& request)
{
    struct Shared
    {
        enum State { queued, running, completed, cancelled };
        McpRequestHandler& handler;
        juce::var request;
        juce::var response;
        juce::WaitableEvent done;
        std::atomic<int> state { queued };

        Shared(McpRequestHandler& h, const juce::var& r) : handler(h), request(r) {}
    };
    auto shared = std::make_shared<Shared>(*handler_, request);

    juce::MessageManager::callAsync([shared]() {
        int expected = Shared::queued;
        if (!shared->state.compare_exchange_strong(expected, Shared::running))
        {
            shared->done.signal();
            return;
        }
        shared->response = shared->handler.handle(shared->request);
        shared->state.store(Shared::completed);
        shared->done.signal();
    });

    for (int elapsedMs = 0; elapsedMs < 4000; elapsedMs += 50)
    {
        if (shared->done.wait(50))
            return shared->response;

        if (threadShouldExit())
        {
            int expected = Shared::queued;
            if (!shared->state.compare_exchange_strong(expected, Shared::cancelled))
            {
                shared->done.wait();
                return shared->response;
            }

            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", request.getProperty("id", juce::var()));
            obj->setProperty("ok", false);
            auto* err = new juce::DynamicObject();
            err->setProperty("code", "server_stopping");
            err->setProperty("message", "editor is shutting down");
            obj->setProperty("error", juce::var(err));
            return juce::var(obj);
        }
    }

    int expected = Shared::queued;
    if (!shared->state.compare_exchange_strong(expected, Shared::cancelled))
    {
        // Once execution has started it may already have mutated the model;
        // wait and return its real result rather than invite a duplicate retry.
        shared->done.wait();
        return shared->response;
    }

    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", request.getProperty("id", juce::var()));
        obj->setProperty("ok", false);
        auto* err = new juce::DynamicObject();
        err->setProperty("code", "ui_busy");
        err->setProperty("message", "editor did not respond in time");
        obj->setProperty("error", juce::var(err));
        return juce::var(obj);
    }
}
