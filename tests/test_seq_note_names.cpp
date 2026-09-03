#include <doctest.h>
#include "format/ValueFormatters.h"
#include "model/ModuleDescriptions.h"

// The note sequencers can read their steps as note names instead of as the
// numbers the original editor shows (issue #76). Two things have to hold for
// that to be trustworthy: the names themselves, and the fact that the setting
// only ever reaches the parameters that carry an alternative reading.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

static const ParameterDescriptor& paramOf(const juce::String& moduleName, const juce::String& paramName)
{
    const auto* m = descriptions().getModuleByName(moduleName);
    REQUIRE(m != nullptr);
    for (const auto& p : m->parameters)
        if (p.name == paramName)
            return p;
    FAIL("no parameter " << paramName << " on " << moduleName);
    static ParameterDescriptor dummy;
    return dummy;
}

TEST_CASE("NoteSeqB steps name absolute pitches around 60 = C4")
{
    CHECK(ValueFormatters::format("fmtSeqNote", 60) == "C4");
    CHECK(ValueFormatters::format("fmtSeqNote", 61) == "C#4");
    CHECK(ValueFormatters::format("fmtSeqNote", 62) == "D4");
    CHECK(ValueFormatters::format("fmtSeqNote", 59) == "B3");

    // The octave floors rather than rounding, which is where the original's
    // fmtNote goes wrong: MIDI 66 is F#4, not F#5.
    CHECK(ValueFormatters::format("fmtSeqNote", 66) == "F#4");
    CHECK(ValueFormatters::format("fmtSeqNote", 0)  == "C-1");
    CHECK(ValueFormatters::format("fmtSeqNote", 127) == "G9");
}

TEST_CASE("NoteSeqA steps name the interval they transpose by")
{
    CHECK(ValueFormatters::format("fmtSeqInterval", 64) == "0");
    CHECK(ValueFormatters::format("fmtSeqInterval", 66) == "+2 (D)");
    CHECK(ValueFormatters::format("fmtSeqInterval", 71) == "+7 (G)");
    CHECK(ValueFormatters::format("fmtSeqInterval", 65) == "+1 (C#)");
    CHECK(ValueFormatters::format("fmtSeqInterval", 63) == "-1 (B)");
    CHECK(ValueFormatters::format("fmtSeqInterval", 52) == "-12 (C)");
}

TEST_CASE("Only the sequencer steps answer to the note-name setting")
{
    const auto& noteB = paramOf("NoteSeqB", "note 1");
    const auto& stepA = paramOf("NoteSeqA", "step 1");
    const auto& loop  = paramOf("NoteSeqB", "loop");

    CHECK(noteB.displayFormatter(false) == "");
    CHECK(noteB.displayFormatter(true)  == "fmtSeqNote");

    CHECK(stepA.displayFormatter(false) == "value-64");
    CHECK(stepA.displayFormatter(true)  == "fmtSeqInterval");

    // A switch has one honest reading, so the setting must not touch it.
    CHECK(loop.displayFormatter(false) == "fmtOffOn");
    CHECK(loop.displayFormatter(true)  == "fmtOffOn");
}

TEST_CASE("Every step of both sequencers carries the alternative reading")
{
    for (int i = 1; i <= 16; ++i)
    {
        CHECK(paramOf("NoteSeqB", "note " + juce::String(i)).noteFormatter == "fmtSeqNote");
        CHECK(paramOf("NoteSeqA", "step " + juce::String(i)).noteFormatter == "fmtSeqInterval");
    }
}
