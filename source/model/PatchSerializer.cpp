#include "PatchSerializer.h"
#include <juce_core/juce_core.h>
#include <map>

// ---- Helpers ----

int PatchSerializer::bitWidth(int maxValue)
{
    if (maxValue <= 0) return 1;
    int bits = 0, v = maxValue;
    while (v > 0) { bits++; v >>= 1; }
    return bits;
}

static int pdlBitWidth(int maxValue)
{
    if (maxValue <= 0) return 1;
    int bits = 0, v = maxValue;
    while (v > 0) { bits++; v >>= 1; }
    return bits;
}

static int parameterBitWidth(const Module& module, const ParameterDescriptor& pd)
{
    // PDL2 Param97 stores MasterOsc.kbt as 7 bits even though modules.xml
    // describes the UI range as Off/On. Using the UI maxValue here misaligns
    // every following parameter in the dump and the synth rejects the patch.
    if (module.getDescriptor() != nullptr
        && module.getDescriptor()->index == 97
        && pd.index == 2
        && pd.paramClass == "parameter")
        return 7;

    return pdlBitWidth(pd.maxValue);
}

int PatchSerializer::findModuleContainerIndex(const ModuleContainer& container, const Connector* conn) const
{
    for (const auto& m : container.getModules())
        for (const auto& c : m->getConnectors())
            if (&c == conn)
                return m->getContainerIndex();
    return -1;
}

// ---- Public API ----

std::vector<std::vector<uint8_t>> PatchSerializer::serializeForUpload(const Patch& patch)
{
    std::vector<std::vector<uint8_t>> sections;
    sections.reserve(16);

    sections.push_back(serializePatchName(patch));           // [0]  PatchName (type 55)
    sections.push_back(serializeHeader(patch));              // [1]  Header (type 33)
    sections.push_back(serializeModuleDump(patch, 1));       // [2]  ModuleDump poly
    sections.push_back(serializeModuleDump(patch, 0));       // [3]  ModuleDump common
    sections.push_back(serializeNoteDump(patch));            // [4]  NoteDump
    sections.push_back(serializeCableDump(patch, 1));        // [5]  CableDump poly
    sections.push_back(serializeCableDump(patch, 0));        // [6]  CableDump common
    sections.push_back(serializeParameterDump(patch, 1));    // [7]  ParameterDump poly
    sections.push_back(serializeParameterDump(patch, 0));    // [8]  ParameterDump common
    sections.push_back(serializeMorphMap(patch));            // [9]  MorphMap
    sections.push_back(serializeKnobMap(patch));             // [10] KnobMap
    sections.push_back(serializeControlMap(patch));          // [11] ControlMap
    sections.push_back(serializeCustomDump(patch, 1));       // [12] CustomDump poly
    sections.push_back(serializeCustomDump(patch, 0));       // [13] CustomDump common
    sections.push_back(serializeNameDump(patch, 1));         // [14] NameDump poly
    sections.push_back(serializeNameDump(patch, 0));         // [15] NameDump common

    DBG("PatchSerializer: upload serialized \"" + patch.getName() + "\" to "
        + juce::String(sections.size()) + " sections");

    return sections;
}

std::vector<std::vector<uint8_t>> PatchSerializer::serialize(const Patch& patch)
{
    std::vector<std::vector<uint8_t>> sections;
    sections.reserve(13);

    sections.push_back(serializeHeaderAndName(patch));
    sections.push_back(serializeModuleDump(patch, 1));
    sections.push_back(serializeModuleDump(patch, 0));
    sections.push_back(serializeCableDump(patch, 1));
    sections.push_back(serializeCableDump(patch, 0));
    sections.push_back(serializeParameterDump(patch, 1));
    sections.push_back(serializeParameterDump(patch, 0));
    sections.push_back(serializeMorphMap(patch));
    sections.push_back(serializeKnobMap(patch));
    sections.push_back(serializeControlMap(patch));
    sections.push_back(serializeNameDump(patch, 1));
    sections.push_back(serializeNameDump(patch, 0));
    sections.push_back(serializeNoteDump(patch));

    DBG("PatchSerializer: serialized \"" + patch.getName() + "\" to "
        + juce::String(sections.size()) + " sections");

    return sections;
}

// ---- Section serializers ----

// PatchName (type 55): 0:8 0:8 0:8 String$name
// (used in .pch file format - has 3-byte padding)
std::vector<uint8_t> PatchSerializer::serializePatchName(const Patch& patch)
{
    BitStreamWriter bs;
    bs.writeBits(55, 8);  // section type
    bs.writeBits(0, 8);   // padding
    bs.writeBits(0, 8);   // padding
    bs.writeBits(0, 8);   // padding
    bs.writeString16(patch.getName().toStdString());
    bs.alignToByte();
    return bs.toMidiBytes();
}

// Header (type 33) + PatchName2 (type 39) combined — for legacy 13-section download format
std::vector<uint8_t> PatchSerializer::serializeHeaderAndName(const Patch& patch)
{
    BitStreamWriter bs;

    bs.writeBits(33, 8);  // Header section type

    const auto& h = patch.getHeader();
    bs.writeBits(h.keyRangeMin, 7);
    bs.writeBits(h.keyRangeMax, 7);
    bs.writeBits(h.velRangeMin, 7);
    bs.writeBits(h.velRangeMax, 7);
    bs.writeBits(h.bendRange, 5);
    bs.writeBits(h.portamentoTime, 7);
    bs.writeBits(h.portamento ? 1 : 0, 1);
    bs.writeBits(h.pedalMode, 1);
    bs.writeBits(h.voices - 1, 5);  // stored 0-based
    bs.writeBits(h.unknown2, 2);
    bs.writeBits(h.separatorPosition, 12);
    bs.writeBits(h.octaveShift, 3);
    bs.writeBits(h.cableVisRed ? 1 : 0, 1);
    bs.writeBits(h.cableVisBlue ? 1 : 0, 1);
    bs.writeBits(h.cableVisYellow ? 1 : 0, 1);
    bs.writeBits(h.cableVisGray ? 1 : 0, 1);
    bs.writeBits(h.cableVisGreen ? 1 : 0, 1);
    bs.writeBits(h.cableVisPurple ? 1 : 0, 1);
    bs.writeBits(h.cableVisWhite ? 1 : 0, 1);
    bs.writeBits(h.voiceRetriggerCommon, 1);
    bs.writeBits(h.voiceRetriggerPoly, 1);
    bs.writeBits(h.unknown3, 4);
    bs.writeBits(h.unknown4, 3);
    bs.alignToByte();

    // PatchName2 (type 39): no 3-byte padding, just name
    bs.writeBits(39, 8);
    bs.writeString16(patch.getName().toStdString());
    bs.alignToByte();

    return bs.toMidiBytes();
}

// Header (type 33) — standalone section for upload
std::vector<uint8_t> PatchSerializer::serializeHeader(const Patch& patch)
{
    BitStreamWriter bs;
    bs.writeBits(33, 8);  // section type

    const auto& h = patch.getHeader();
    bs.writeBits(h.keyRangeMin, 7);
    bs.writeBits(h.keyRangeMax, 7);
    bs.writeBits(h.velRangeMin, 7);
    bs.writeBits(h.velRangeMax, 7);
    bs.writeBits(h.bendRange, 5);
    bs.writeBits(h.portamentoTime, 7);
    bs.writeBits(h.portamento ? 1 : 0, 1);
    bs.writeBits(1, 1);                    // pedalMode: Java always sends 1
    bs.writeBits(h.voices - 1, 5);        // 0-based
    bs.writeBits(0, 2);                    // unknown2: Java always sends 0
    bs.writeBits(h.separatorPosition, 12);
    bs.writeBits(h.octaveShift, 3);
    bs.writeBits(h.cableVisRed ? 1 : 0, 1);
    bs.writeBits(h.cableVisBlue ? 1 : 0, 1);
    bs.writeBits(h.cableVisYellow ? 1 : 0, 1);
    bs.writeBits(h.cableVisGray ? 1 : 0, 1);
    bs.writeBits(h.cableVisGreen ? 1 : 0, 1);
    bs.writeBits(h.cableVisPurple ? 1 : 0, 1);
    bs.writeBits(h.cableVisWhite ? 1 : 0, 1);
    bs.writeBits(h.voiceRetriggerCommon, 1);
    bs.writeBits(h.voiceRetriggerPoly, 1);
    bs.writeBits(0xF, 4);                  // unknown3: Java always sends 0xF
    bs.writeBits(0, 3);                    // unknown4: Java always sends 0
    bs.alignToByte();

    return bs.toMidiBytes();
}

// ModuleDump (type 74): section:1 nmodules:7 [type:7 index:7 xpos:7 ypos:7]*nmodules
std::vector<uint8_t> PatchSerializer::serializeModuleDump(const Patch& patch, int section)
{
    BitStreamWriter bs;
    bs.writeBits(74, 8);     // section type
    bs.writeBits(section, 1);

    const auto& container = patch.getContainer(section);
    const auto& modules = container.getModules();

    bs.writeBits(static_cast<int>(modules.size()), 7);

    for (const auto& m : modules)
    {
        bs.writeBits(m->getDescriptor()->index, 7);  // module type
        bs.writeBits(m->getContainerIndex(), 7);     // index
        bs.writeBits(m->getPosition().x, 7);         // xpos
        bs.writeBits(m->getPosition().y, 7);         // ypos
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// CableDump (type 82): section:1 ncables:15 [color:3 src:7 srcConn:6 isOutput:1 dst:7 dstConn:6]*ncables
std::vector<uint8_t> PatchSerializer::serializeCableDump(const Patch& patch, int section)
{
    BitStreamWriter bs;
    bs.writeBits(82, 8);     // section type
    bs.writeBits(section, 1);

    const auto& container = patch.getContainer(section);
    const auto& connections = container.getConnections();

    bs.writeBits(static_cast<int>(connections.size()), 15);  // 15 bits per parser

    for (const auto& conn : connections)
    {
        int srcModule  = findModuleContainerIndex(container, conn.output);
        int dstModule  = findModuleContainerIndex(container, conn.input);
        int color      = static_cast<int>(conn.output->getDescriptor()->signalType);
        int srcConnIdx = conn.output->getDescriptor()->index;
        int dstConnIdx = conn.input->getDescriptor()->index;

        if (srcModule < 0 || dstModule < 0)
        {
            // Orphaned connector — write zeros to maintain count
            bs.writeBits(0, 3 + 7 + 6 + 1 + 7 + 6);
            continue;
        }

        bs.writeBits(color, 3);
        bs.writeBits(srcModule, 7);
        bs.writeBits(srcConnIdx, 6);
        bs.writeBits(1, 1);         // isOutput = 1 (source is always output)
        bs.writeBits(dstModule, 7);
        bs.writeBits(dstConnIdx, 6);
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// ParameterDump (type 77): section:1 nmodules:7 [index:7 type:7 params...]*nmodules
// Parameter bit widths are derived from descriptor maxValue (matches parser)
std::vector<uint8_t> PatchSerializer::serializeParameterDump(const Patch& patch, int section)
{
    BitStreamWriter bs;
    bs.writeBits(77, 8);     // section type
    bs.writeBits(section, 1);

    const auto& container = patch.getContainer(section);
    const auto& modules = container.getModules();

    // Count modules that have at least one "parameter" class param
    int nmodules = 0;
    for (const auto& m : modules)
        for (const auto& pd : m->getDescriptor()->parameters)
            if (pd.paramClass == "parameter") { nmodules++; break; }

    bs.writeBits(nmodules, 7);

    for (const auto& m : modules)
    {
        // Check if this module has any parameters
        bool hasParams = false;
        for (const auto& pd : m->getDescriptor()->parameters)
            if (pd.paramClass == "parameter") { hasParams = true; break; }
        if (!hasParams)
            continue;

        bs.writeBits(m->getContainerIndex(), 7);      // index
        bs.writeBits(m->getDescriptor()->index, 7);   // module type

        for (const auto& pd : m->getDescriptor()->parameters)
        {
            if (pd.paramClass != "parameter")
                continue;

            int bits = parameterBitWidth(*m, pd);
            const auto* param = m->getParameter(pd.index);
            int value = param ? param->getValue() : pd.defaultValue;
            bs.writeBits(value, bits);
        }
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// MorphMap (type 101): morph[4]:7 keyboard[4]:2 nknobs:5 [section:1 module:7 param:7 morph:2 range:8]*nknobs
std::vector<uint8_t> PatchSerializer::serializeMorphMap(const Patch& patch)
{
    BitStreamWriter bs;
    bs.writeBits(101, 8);  // section type

    // 4 morph values
    for (int i = 0; i < 4; ++i)
        bs.writeBits(patch.morphValues[i], 7);

    // 4 keyboard values
    for (int i = 0; i < 4; ++i)
        bs.writeBits(patch.morphKeyboard[i], 2);

    // Morph assignments (5-bit count per parser)
    bs.writeBits(static_cast<int>(patch.morphAssignments.size()), 5);

    for (const auto& ma : patch.morphAssignments)
    {
        bs.writeBits(ma.section, 1);
        bs.writeBits(ma.module, 7);
        bs.writeBits(ma.param, 7);
        bs.writeBits(ma.morph, 2);
        bs.writeBits(ma.range & 0xFF, 8);  // signed 8-bit
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// KnobMap (type 98): 23 * [assigned:1 assigned*(section:2 module:7 param:7)]
std::vector<uint8_t> PatchSerializer::serializeKnobMap(const Patch& patch)
{
    BitStreamWriter bs;
    bs.writeBits(98, 8);  // section type

    for (int i = 0; i < 23; ++i)
    {
        const auto& ka = patch.knobAssignments[static_cast<size_t>(i)];
        if (ka.assigned)
        {
            bs.writeBits(1, 1);
            bs.writeBits(ka.section, 2);  // 2 bits per parser
            bs.writeBits(ka.module, 7);
            bs.writeBits(ka.param, 7);
        }
        else
        {
            bs.writeBits(0, 1);
        }
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// ControlMap (type 96): ncontrols:7 [control:7 section:2 module:7 param:7]*ncontrols
std::vector<uint8_t> PatchSerializer::serializeControlMap(const Patch& patch)
{
    BitStreamWriter bs;
    bs.writeBits(96, 8);  // section type
    bs.writeBits(static_cast<int>(patch.ctrlAssignments.size()), 7);  // 7 bits per parser

    for (const auto& ca : patch.ctrlAssignments)
    {
        bs.writeBits(ca.control, 7);   // 7 bits per parser
        bs.writeBits(ca.section, 2);   // 2 bits per parser
        bs.writeBits(ca.module, 7);
        bs.writeBits(ca.param, 7);
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// CustomDump (type 91): section:1 nmodules:7 [index:7 nparams:8 value:8*nparams]*nmodules
std::vector<uint8_t> PatchSerializer::serializeCustomDump(const Patch& patch, int section)
{
    BitStreamWriter bs;
    bs.writeBits(91, 8);     // section type
    bs.writeBits(section, 1);

    const auto& container = patch.getContainer(section);
    const auto& preExisting = (section == 1) ? patch.polyCustomDump : patch.commonCustomDump;

    // Build map of pre-existing custom entries by module index
    std::map<int, const Patch::CustomDumpEntry*> existingMap;
    for (const auto& entry : preExisting)
        existingMap[entry.index] = &entry;

    // Collect modules that have custom params
    struct CustomEntry { int index; std::vector<int> values; };
    std::vector<CustomEntry> entries;

    for (const auto& m : container.getModules())
    {
        auto* desc = m->getDescriptor();
        if (desc == nullptr) continue;

        std::vector<int> customDefaults;
        for (const auto& pd : desc->parameters)
            if (pd.paramClass == "custom")
                customDefaults.push_back(pd.defaultValue);

        if (customDefaults.empty()) continue;

        int idx = m->getContainerIndex();
        auto it = existingMap.find(idx);
        if (it != existingMap.end() && it->second->values.size() == customDefaults.size())
            entries.push_back({ idx, it->second->values });
        else
            entries.push_back({ idx, customDefaults });
    }

    bs.writeBits(static_cast<int>(entries.size()), 7);

    for (const auto& entry : entries)
    {
        bs.writeBits(entry.index, 7);
        bs.writeBits(static_cast<int>(entry.values.size()), 8);
        for (int val : entry.values)
            bs.writeBits(val, 8);
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// NameDump (type 90): section:1 nmodules:7 [index:8 String$name]*nmodules
std::vector<uint8_t> PatchSerializer::serializeNameDump(const Patch& patch, int section)
{
    BitStreamWriter bs;
    bs.writeBits(90, 8);     // section type
    bs.writeBits(section, 1);

    const auto& container = patch.getContainer(section);
    const auto& modules = container.getModules();

    bs.writeBits(static_cast<int>(modules.size()), 7);

    for (const auto& m : modules)
    {
        bs.writeBits(m->getContainerIndex(), 8);  // index (8 bits per parser)
        bs.writeString16(m->getTitle().toStdString());
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

// NoteDump (type 105): note1(21bits) nmore:5 note2(21bits) nmore*note(21bits)
// Java sends at least 2 notes (64,0,0 defaults) with nmore=0 for empty patches.
std::vector<uint8_t> PatchSerializer::serializeNoteDump(const Patch& patch)
{
    BitStreamWriter bs;
    bs.writeBits(105, 8);  // section type

    auto writeNote = [&](int note, int attack, int release)
    {
        bs.writeBits(note, 7);
        bs.writeBits(attack, 7);
        bs.writeBits(release, 7);
    };

    if (patch.notes.size() >= 2)
    {
        // First note
        writeNote(patch.notes[0].note, patch.notes[0].attack, patch.notes[0].release);

        int nmore = static_cast<int>(patch.notes.size()) - 2;
        bs.writeBits(nmore, 5);

        // Second note
        writeNote(patch.notes[1].note, patch.notes[1].attack, patch.notes[1].release);

        // Remaining notes
        for (size_t i = 2; i < patch.notes.size(); ++i)
            writeNote(patch.notes[i].note, patch.notes[i].attack, patch.notes[i].release);
    }
    else
    {
        // Default: two silent notes at middle C
        writeNote(64, 0, 0);
        bs.writeBits(0, 5);   // nmore = 0
        writeNote(64, 0, 0);
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}
