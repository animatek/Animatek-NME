#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include <vector>

// The editor's end of G1-Emu's direct link (G1-Emu issue #8, NME #83): the PC Port's bytes over a
// local TCP socket, with no MIDI port in between. Each emulator instance listens on 127.0.0.1 at
// kBasePort + n and greets an editor with one line, "G1-Emu <version> <name>\n"; after that the
// wire is raw MIDI, both ways. This is what makes the emulator reachable where no virtual MIDI port
// can be made (the plugin on Windows), and without choosing ports by hand anywhere else.
namespace DirectLink
{
    constexpr int kBasePort = 47310;
    constexpr int kMaxInstances = 8;

    // How a link shows up among the MIDI devices: the same id in the input and output lists.
    inline juce::String deviceId (int port)        { return "g1link:" + juce::String (port); }
    inline bool isDeviceId (const juce::String& id) { return id.startsWith ("g1link:"); }
    inline int portOf (const juce::String& id)      { return id.fromFirstOccurrenceOf ("g1link:", false, false).getIntValue(); }

    // Raw bytes in, whole SysEx messages out: TCP keeps the order but not the message boundaries.
    // Bytes outside F0..F7 (running status, clock) are dropped, as the editor only speaks SysEx.
    class SysExSplitter
    {
    public:
        template <typename Fn>
        void feed (const uint8_t* data, size_t size, Fn&& onMessage)
        {
            for (size_t i = 0; i < size; ++i)
            {
                const uint8_t b = data[i];
                if (b == 0xf0)
                {
                    current.assign (1, b);
                    inside = true;
                }
                else if (inside)
                {
                    current.push_back (b);
                    if (b == 0xf7)
                    {
                        inside = false;
                        onMessage (current);
                        current.clear();
                    }
                    else if (current.size() > kMaxMessage)
                    {
                        inside = false;   // no end in sight: a broken stream, not a patch
                        current.clear();
                    }
                }
            }
        }

    private:
        static constexpr size_t kMaxMessage = 64 * 1024;
        std::vector<uint8_t> current;
        bool inside = false;
    };

    // An emulator found listening: its port, and its name from the greeting (empty when another
    // editor already has it, which the emulator says by closing at once).
    struct Instance
    {
        int port = 0;
        juce::String name;
        bool busy = false;
        // The ids its MIDI PC Port has in this computer's MIDI lists, from the greeting: an editor
        // already on one of them is already talking to this G1.
        juce::StringArray pcPortIds;
        // Its name in the MIDI lists, for when there are no ids (JUCE's virtual ports on Linux).
        juce::String pcPortName;
    };

    // The greeting, "G1-Emu <version> <name>[\tpcport=<id>,<id>][\tpcname=<name>]", without its newline. False when
    // it is not G1-Emu's.
    bool parseGreeting (const juce::String& line, Instance& out);

    // Knocks on every port, briefly. Each knock costs one local connection, so it is not for every frame.
    std::vector<Instance> discover (int timeoutMs = 150);

    // One open link. Incoming SysEx is delivered on a reader thread; the owner hops threads.
    class Client
    {
    public:
        Client();
        ~Client();

        // False when nothing answers, or the emulator already has an editor.
        bool open (int port, std::function<void (std::vector<uint8_t>)> onSysEx, std::function<void()> onClosed);
        void close();
        bool isOpen() const;
        void send (const std::vector<uint8_t>& bytes);
        const juce::String& name() const { return instanceName; }
        int port() const { return linkPort; }

    private:
        class Reader;
        std::unique_ptr<juce::StreamingSocket> socket;
        std::unique_ptr<Reader> reader;
        juce::String instanceName;
        int linkPort = 0;
        juce::CriticalSection writeLock;
    };
}
