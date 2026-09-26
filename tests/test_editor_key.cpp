#include <doctest.h>
#include "midi/EditorKey.h"

// The G1 treats the editor's notes as one key (EditorKey.h). These are the
// sequences the keyboard floater and the MCP's play_note can produce, and what
// must reach the synth so that nothing is left sounding.

using E = EditorKey::Event;

TEST_CASE("a lone note goes through unchanged")
{
    EditorKey k;
    CHECK(k.press(60) == std::vector<E>{ { 60, true } });
    CHECK(k.release(60) == std::vector<E>{ { 60, false } });
    CHECK(k.noteDown() == -1);
}

TEST_CASE("a second press releases the first note before it")
{
    // Without this the synth only swaps its stored note: 60 keeps sounding and the
    // release of 64 names a note that never started.
    EditorKey k;
    k.press(60);
    CHECK(k.press(64) == std::vector<E>{ { 60, false }, { 64, true } });
    CHECK(k.noteDown() == 64);
}

TEST_CASE("releasing a note that is no longer down sends nothing")
{
    // 60 was already released by the press of 64; sending its release now would
    // lift the synth's key while 64 is still meant to sound.
    EditorKey k;
    k.press(60);
    k.press(64);
    CHECK(k.release(60).empty());
    CHECK(k.noteDown() == 64);
    CHECK(k.release(64) == std::vector<E>{ { 64, false } });
    CHECK(k.noteDown() == -1);
}

TEST_CASE("overlapping notes in any order end with the key up")
{
    for (const bool releaseFirstNoteFirst : { true, false })
    {
        EditorKey k;
        std::vector<E> sent;
        auto add = [&](const std::vector<E>& v) { sent.insert(sent.end(), v.begin(), v.end()); };
        add(k.press(60));
        add(k.press(64));
        add(k.release(releaseFirstNoteFirst ? 60 : 64));
        add(k.release(releaseFirstNoteFirst ? 64 : 60));

        // Every on is followed by its own off, and nothing is left down.
        int down = -1;
        for (const auto& e : sent)
        {
            if (e.on) { CHECK(down == -1); down = e.note; }
            else      { CHECK(e.note == down); down = -1; }
        }
        CHECK(down == -1);
        CHECK(k.noteDown() == -1);
    }
}

TEST_CASE("pressing the note that is already down sends only the press")
{
    EditorKey k;
    k.press(60);
    CHECK(k.press(60) == std::vector<E>{ { 60, true } });
    CHECK(k.release(60) == std::vector<E>{ { 60, false } });
}

TEST_CASE("reset forgets the note that was down")
{
    EditorKey k;
    k.press(60);
    k.reset();
    CHECK(k.noteDown() == -1);
    CHECK(k.release(60).empty());
    CHECK(k.press(62) == std::vector<E>{ { 62, true } });
}
