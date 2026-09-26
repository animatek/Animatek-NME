#pragma once

#include <vector>

// The G1's OS takes the editor's note messages (cc 0x17, sc 0x56) as ONE key,
// not a keyboard: a press stores its note and sets the key down, a release sets
// it up without looking at which note it names, and a panel-scan routine turns
// only the key's up/down changes into note-on/off of the stored note. Two editor
// notes that overlap therefore leave one sounding for ever: the second press only
// swaps the stored note, and the releases name the wrong one. Checked on a real
// G1 and reproduced in G1-Emu (see G1-Emu/NOTES.md, "The editor's keyboard is
// one key"). Notes on the regular MIDI input are not affected.
//
// EditorKey keeps the editor on the same terms: one note down at a time. It
// turns the editor's presses and releases into the messages that are safe to
// send - a new press releases the note that is down first, and a release of a
// note that is not the one down sends nothing (it would lift the OS's key under
// the note that is still sounding).
class EditorKey
{
public:
    struct Event
    {
        int note;
        bool on;
        bool operator==(const Event& o) const { return note == o.note && on == o.on; }
    };

    std::vector<Event> press(int note)
    {
        std::vector<Event> out;
        if (down >= 0 && down != note)
            out.push_back({ down, false });
        out.push_back({ note, true });
        down = note;
        return out;
    }

    std::vector<Event> release(int note)
    {
        if (note != down)
            return {};
        down = -1;
        return { { note, false } };
    }

    // The synth's key is up again (disconnected, or its slot reloaded): forget ours.
    void reset() { down = -1; }

    int noteDown() const { return down; }

private:
    int down = -1;
};
