#include <doctest.h>
#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"

// The G1 keeps 16 characters of a module name. A longer one in the editor is a
// name the synth does not have: two patches in the maintainer's bank 9 came
// back from the synth with their names cut and no longer matched their files.

static ModuleDescriptions& descriptions()
{
    static ModuleDescriptions descs;
    static bool loaded = descs.loadFromFile(
        juce::File(NME_TEST_DATA_DIR).getChildFile("modules.xml"));
    REQUIRE(loaded);
    return descs;
}

TEST_CASE("a module title is held to the G1's 16 characters")
{
    const auto* desc = descriptions().getModuleByName("FilterD");
    REQUIRE(desc != nullptr);
    auto module = Module::createFromDescriptor(*desc);
    module->setTitle("12dB Dynamic Multimode Filter");
    CHECK(module->getTitle() == "12dB Dynamic Mul");
    module->setTitle("Short");
    CHECK(module->getTitle() == "Short");
    module->setTitle("Exactly16chars!!");
    CHECK(module->getTitle() == "Exactly16chars!!");
}

TEST_CASE("a .pch with a longer module name loads with the name the synth keeps")
{
    const auto file = juce::File::createTempFile(".pch");
    file.replaceWithText(
        "[Header]\r\n"
        "Version=Nord Modular patch 3.0\r\n"
        "0 127 0 127 2 0 0 1 909 0 1 1 0 0 15 0 0 0 0 0 0 0 1 \r\n"
        "[/Header]\r\n"
        "[ModuleDump]\r\n"
        "1 \r\n"
        "1 49 2 5 \r\n"
        "[/ModuleDump]\r\n"
        "[ModuleDump]\r\n"
        "0 \r\n"
        "[/ModuleDump]\r\n"
        "[NameDump]\r\n"
        "1 \r\n"
        "1 12dB Dynamic Multimode Filter\r\n"
        "[/NameDump]\r\n"
        "[NameDump]\r\n"
        "0 \r\n"
        "[/NameDump]\r\n");

    PchFileIO io(descriptions());
    auto patch = io.readFile(file);
    file.deleteFile();
    REQUIRE(patch != nullptr);
    const auto* module = patch->getContainer(1).getModuleByIndex(1);
    REQUIRE(module != nullptr);
    CHECK(module->getTitle() == "12dB Dynamic Mul");
}
