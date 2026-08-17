#include <doctest.h>
#include "model/PchFileIO.h"

// A classic .pch carries no name inside it, so a patch opened from disk is
// named after its file. Bank backups are written as "NN - Name.pch" so a bank
// folder sorts by position, and that prefix is ours: left on, it goes into the
// patch and from there onto the synth, which is how a bank ended up holding
// names like "86 - DoubleSawPa".

static juce::String nameFor(const juce::String& fileName)
{
    return PchFileIO::patchNameFromFileName(juce::File::getCurrentWorkingDirectory()
                                                .getChildFile(fileName));
}

TEST_CASE("a bank backup loses the position prefix we gave it")
{
    CHECK(nameFor("35 - BELLS++.pch") == "BELLS++");
    CHECK(nameFor("01 - A.pch")       == "A");
    CHECK(nameFor("99 - Zed.pch")     == "Zed");
}

TEST_CASE("an ordinary patch file keeps its whole name")
{
    CHECK(nameFor("BELLS++.pch")        == "BELLS++");
    CHECK(nameFor("My Bass Patch.pch")  == "My Bass Patch");
    // A name that merely starts with digits is not a prefix: the separator is
    // what makes one.
    CHECK(nameFor("303 Acid.pch")       == "303 Acid");
    CHECK(nameFor("12-Bar Blues.pch")   == "12-Bar Blues");
    CHECK(nameFor("1 - One.pch")        == "1 - One");   // one digit, not two
}

TEST_CASE("the prefix has to have something after it")
{
    // "35 - .pch" is five characters of prefix and nothing else; stripping it
    // would leave the patch nameless, so it is left alone.
    CHECK(nameFor("35 - .pch") == "35 - ");
    CHECK(nameFor("35 - x.pch") == "x");
}

TEST_CASE("the extension is gone whatever the name")
{
    CHECK(nameFor("Plain.pch").containsIgnoreCase(".pch") == false);
    CHECK(nameFor("07 - Dots.and.more.pch") == "Dots.and.more");
}
