#pragma once

#include "Patch.h"
#include "ModuleDescriptions.h"
#include <juce_core/juce_core.h>
#include <memory>

class PchFileIO
{
public:
    explicit PchFileIO(const ModuleDescriptions& descs);

    std::unique_ptr<Patch> readFile(const juce::File& file);
    bool writeFile(const Patch& patch, const juce::File& file);

    // Nord Modular 2.10 text patches declare themselves in a Version= line
    // near the top of the file. Shared by readFile dispatch and the preset
    // browser so both agree on what counts as a legacy patch.
    static bool isLegacyPatch210(const juce::File& file);
    static bool isLegacyPatch210(const juce::StringArray& lines);

    // The name a patch takes from its file when the file itself carries none,
    // which a classic .pch never does.
    //
    // Bank backups are written as "NN - Name.pch" so a bank folder sorts by
    // position, and that prefix is ours, not the patch's: left on, it travels
    // into the patch and then onto the synth, which is how names like
    // "86 - DoubleSawPa" used to end up stored in a bank. Stripped here rather
    // than at each caller, so every way of opening one agrees: the browsers,
    // File > Open, a drag onto a slot, and restoring a whole bank.
    static juce::String patchNameFromFileName(const juce::File& file);

private:
    // Reader helpers
    void parseHeader(const juce::StringArray& lines, Patch& patch);
    void parseModuleDump(const juce::StringArray& lines, Patch& patch);
    void parseCurrentNoteDump(const juce::StringArray& lines, Patch& patch);
    void parseCableDump(const juce::StringArray& lines, Patch& patch);
    void parseParameterDump(const juce::StringArray& lines, Patch& patch);
    void parseMorphMapDump(const juce::StringArray& lines, Patch& patch);
    void parseKeyboardAssignment(const juce::StringArray& lines, Patch& patch);
    void parseKnobMapDump(const juce::StringArray& lines, Patch& patch);
    void parseCtrlMapDump(const juce::StringArray& lines, Patch& patch);
    void parseCustomDump(const juce::StringArray& lines, Patch& patch);
    void parseComments(const juce::StringArray& lines, Patch& patch);
    void parseNameDump(const juce::StringArray& lines, Patch& patch);
    std::unique_ptr<Patch> readLegacyFile(const juce::StringArray& lines, const juce::File& file);
    static void normalizeLegacyModulePositions(ModuleContainer& container);
    static void connectLegacyCable(ModuleContainer& container, int sourceModule, int sourceConn,
                                   bool sourceIsOutput, int targetModule, int targetInput);

    // Writer helpers
    void writeHeader(juce::String& out, const Patch& patch);
    void writeModuleDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeCurrentNoteDump(juce::String& out, const Patch& patch);
    void writeCableDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeParameterDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeMorphMapDump(juce::String& out, const Patch& patch);
    void writeKeyboardAssignment(juce::String& out, const Patch& patch);
    void writeKnobMapDump(juce::String& out, const Patch& patch);
    void writeCtrlMapDump(juce::String& out, const Patch& patch);
    void writeCustomDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeNameDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeNotes(juce::String& out, const Patch& patch);
    void writeComments(juce::String& out, const Patch& patch);

    static juce::StringArray tokenize(const juce::String& line);
    static juce::String getLegacyValue(const juce::StringArray& lines, const juce::String& key);

    const ModuleDescriptions& descs;
};
