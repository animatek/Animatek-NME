#pragma once

#include "Patch.h"
#include "BitStream.h"
#include "ModuleDescriptions.h"
#include <memory>
#include <vector>

class PatchParser
{
public:
    explicit PatchParser(const ModuleDescriptions& moduleDescs);

    // Parse 7-bit MIDI section data into a Patch.
    // Each entry in sections is one PatchPacket response (independently 7-bit encoded).
    // A single entry may contain multiple PDL2 sections (e.g. Header + PatchName2).
    std::unique_ptr<Patch> parse(const std::vector<std::vector<uint8_t>>& sections);

private:
    void parseSection(BitStream& bs, Patch& patch);
    void parsePatchName(BitStream& bs, Patch& patch);
    void parsePatchName2(BitStream& bs, Patch& patch);
    void parseHeader(BitStream& bs, Patch& patch);
    void parseModuleDump(BitStream& bs, Patch& patch);
    void parseCableDump(BitStream& bs, Patch& patch);
    void parseParameterDump(BitStream& bs, Patch& patch);
    void parseMorphMap(BitStream& bs, Patch& patch);
    void parseKnobMapDump(BitStream& bs, Patch& patch);
    void parseControlMapDump(BitStream& bs, Patch& patch);
    void parseCustomDump(BitStream& bs, Patch& patch);
    void parseNameDump(BitStream& bs, Patch& patch);
    void parseNoteDump(BitStream& bs, Patch& patch);

    static int bitWidth(int maxValue);

    const ModuleDescriptions& descs;
};
