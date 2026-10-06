#include "DirectLink.h"

namespace DirectLink
{
namespace
{
    // The greeting line, or empty if the emulator closed without one (it already has an editor)
    // or said something that is not G1-Emu.
    juce::String readGreeting (juce::StreamingSocket& s, int timeoutMs)
    {
        juce::String line;
        const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs);
        while (juce::Time::getMillisecondCounter() < deadline && line.length() < 200)
        {
            const int ready = s.waitUntilReady (true, 20);
            if (ready < 0)
                return {};
            if (ready == 0)
                continue;
            char c = 0;
            if (s.read (&c, 1, false) != 1)
                return {};
            if (c == '\n')
                break;
            line += juce::String::charToString (static_cast<juce::juce_wchar> (static_cast<unsigned char> (c)));
        }
        if (! line.startsWith ("G1-Emu "))
            return {};
        // "G1-Emu <version> <name>"
        return line.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false).trim();
    }
}

std::vector<Instance> discover (int timeoutMs)
{
    std::vector<Instance> found;
    for (int i = 0; i < kMaxInstances; ++i)
    {
        juce::StreamingSocket s;
        const int port = kBasePort + i;
        if (! s.connect ("127.0.0.1", port, timeoutMs))
            continue;
        Instance inst;
        inst.port = port;
        inst.name = readGreeting (s, timeoutMs);
        inst.busy = inst.name.isEmpty();
        found.push_back (inst);
        s.close();
    }
    return found;
}

// Reads the socket until it closes, splitting the stream into SysEx messages.
class Client::Reader : public juce::Thread
{
public:
    Reader (juce::StreamingSocket& s, std::function<void (std::vector<uint8_t>)> m, std::function<void()> c)
        : juce::Thread ("G1-Emu link"), socket (s), onSysEx (std::move (m)), onClosed (std::move (c)) {}

    void run() override
    {
        uint8_t buf[4096];
        while (! threadShouldExit())
        {
            const int ready = socket.waitUntilReady (true, 100);
            if (ready < 0)
                break;
            if (ready == 0)
                continue;
            const int n = socket.read (buf, sizeof (buf), false);
            if (n <= 0)
                break;   // the emulator went away
            splitter.feed (buf, static_cast<size_t> (n), [this] (const std::vector<uint8_t>& m) { onSysEx (m); });
        }
        if (! threadShouldExit() && onClosed)
            onClosed();
    }

private:
    juce::StreamingSocket& socket;
    std::function<void (std::vector<uint8_t>)> onSysEx;
    std::function<void()> onClosed;
    SysExSplitter splitter;
};

Client::Client() = default;

Client::~Client()
{
    close();
}

bool Client::open (int port, std::function<void (std::vector<uint8_t>)> onSysEx, std::function<void()> onClosed)
{
    close();
    auto s = std::make_unique<juce::StreamingSocket>();
    if (! s->connect ("127.0.0.1", port, 500))
        return false;
    const auto name = readGreeting (*s, 1000);
    if (name.isEmpty())
        return false;   // another editor has it

    socket = std::move (s);
    instanceName = name;
    linkPort = port;
    reader = std::make_unique<Reader> (*socket, std::move (onSysEx), std::move (onClosed));
    reader->startThread();
    return true;
}

void Client::close()
{
    if (reader)
        reader->signalThreadShouldExit();
    if (socket)
        socket->close();   // wakes the reader
    if (reader)
        reader->stopThread (1000);
    reader.reset();
    socket.reset();
    instanceName.clear();
    linkPort = 0;
}

bool Client::isOpen() const
{
    return socket != nullptr && socket->isConnected();
}

void Client::send (const std::vector<uint8_t>& bytes)
{
    const juce::ScopedLock lock (writeLock);
    if (socket != nullptr && ! bytes.empty())
        socket->write (bytes.data(), static_cast<int> (bytes.size()));
}
}
