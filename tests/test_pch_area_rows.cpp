#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"

// The per-area sections open with the area number (1 poly, 0 common). Two ways
// real files write it broke the reader, found by running nme_pch_dump over the
// community patch archive:
//  #85  the first row glued to the area ("1 1 7 0 0"): the reader started the
//       rows at line 1 and lost that module or cable (1,344 archive patches).
//  #86  "\r\r\n" line endings: an empty line after every row, and the area read
//       from the empty first line came out as 0, so everything went to common.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

// A sine osc (OscA, type 7) cabled to a stereo output (2Output, type 4) in the
// poly area, with every per-area section's first row glued to the area number.
static const char* kGlued =
    "[Header]\r\n"
    "Version=Nord Modular patch 3.0\r\n"
    "0 127 0 127 2 0 0 1 4000 2 1 1 0 0 0 0 1 1 1 1 1 1 1 \r\n"
    "[/Header]\r\n"
    "[ModuleDump]\r\n"
    "1 1 7 0 0 \r\n"
    "2 4 1 0 \r\n"
    "[/ModuleDump]\r\n"
    "[ModuleDump]\r\n"
    "0 \r\n"
    "[/ModuleDump]\r\n"
    "[CableDump]\r\n"
    "1 0 1 0 1 2 0 0 \r\n"
    "[/CableDump]\r\n"
    "[CableDump]\r\n"
    "0 \r\n"
    "[/CableDump]\r\n"
    "[ParameterDump]\r\n"
    "1 1 7 10 52 64 0 64 0 0 0 0 0 0 \r\n"
    "2 4 3 127 0 0 \r\n"
    "[/ParameterDump]\r\n"
    "[ParameterDump]\r\n"
    "0 \r\n"
    "[/ParameterDump]\r\n"
    "[NameDump]\r\n"
    "1 1 Sine\r\n"
    "2 Out\r\n"
    "[/NameDump]\r\n"
    "[NameDump]\r\n"
    "0 \r\n"
    "[/NameDump]\r\n";

static std::unique_ptr<Patch> load(const juce::String& text)
{
    const auto file = juce::File::createTempFile(".pch");
    file.replaceWithText(text, false, false, nullptr);
    PchFileIO io(descriptions());
    auto patch = io.readFile(file);
    file.deleteFile();
    return patch;
}

static void checkSinePatch(const Patch& patch)
{
    const auto& poly = patch.getContainer(1);
    const auto& common = patch.getContainer(0);
    REQUIRE(poly.getModules().size() == 2);
    CHECK(common.getModules().empty());

    const auto* osc = poly.getModuleByIndex(1);
    REQUIRE(osc != nullptr);
    CHECK(osc->getDescriptor()->index == 7);
    CHECK(osc->getTitle() == "Sine");
    CHECK(const_cast<Module*>(osc)->getParameter(0)->getValue() == 52);
    CHECK(poly.getModuleByIndex(2)->getTitle() == "Out");

    REQUIRE(poly.getConnections().size() == 1);
}

TEST_CASE("#85: a first row glued to the area number is read, not skipped")
{
    auto patch = load(kGlued);
    REQUIRE(patch != nullptr);
    checkSinePatch(*patch);
}

TEST_CASE("#86: \\r\\r\\n line endings keep each module in its own area")
{
    auto text = juce::String(kGlued).replace("\r\n", "\r\r\n");
    auto patch = load(text);
    REQUIRE(patch != nullptr);
    checkSinePatch(*patch);
}
