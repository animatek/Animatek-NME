#include "PatchParser.h"

PatchParser::PatchParser(const ModuleDescriptions& moduleDescs)
    : descs(moduleDescs)
{
}

int PatchParser::bitWidth(int maxValue)
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

static int parameterBitWidth(const ModuleDescriptor& desc, const ParameterDescriptor& pd)
{
    // PDL2 Param97 stores MasterOsc.kbt as 7 bits even though modules.xml
    // describes the UI range as Off/On.
    if (desc.index == 97 && pd.index == 2 && pd.paramClass == "parameter")
        return 7;

    return pdlBitWidth(pd.maxValue);
}

std::unique_ptr<Patch> PatchParser::parse(const std::vector<std::vector<uint8_t>>& sections)
{
    auto patch = std::make_unique<Patch>();

    DBG("PatchParser: starting parse, " + juce::String(sections.size()) + " sections");

    for (size_t i = 0; i < sections.size(); ++i)
    {
        BitStream bs(sections[i]);
        // A single PatchPacket response may contain multiple PDL2 sections
        // (e.g. Header + PatchName2 in one response).
        while (bs.remaining() >= 8)
        {
            try
            {
                parseSection(bs, *patch);
            }
            catch (const std::exception& e)
            {
                DBG("PatchParser: ERROR in packet " + juce::String(i)
                    + " at bit " + juce::String(bs.getPosition())
                    + "/" + juce::String(bs.getTotalBits())
                    + " - " + juce::String(e.what()));
                break;  // Stop parsing this packet — likely truncated data
            }
        }
    }

    DBG("PatchParser: done. Patch name: \"" + patch->getName() + "\"");
    DBG("  Poly modules: " + juce::String(patch->getPolyVoiceArea().getModules().size())
        + ", Common modules: " + juce::String(patch->getCommonArea().getModules().size()));
    DBG("  Poly cables: " + juce::String(patch->getPolyVoiceArea().getConnections().size())
        + ", Common cables: " + juce::String(patch->getCommonArea().getConnections().size()));

    return patch;
}

void PatchParser::parseSection(BitStream& bs, Patch& patch)
{
    size_t sectionStart = bs.getPosition();
    int type = static_cast<int>(bs.readBits(8));

    DBG("PatchParser: section type=" + juce::String(type)
        + " at bit " + juce::String(sectionStart));
    juce::ignoreUnused(sectionStart);

    switch (type)
    {
        case 55:  parsePatchName(bs, patch);      break;
        case 39:  parsePatchName2(bs, patch);     break;
        case 33:  parseHeader(bs, patch);          break;
        case 74:  parseModuleDump(bs, patch);      break;
        case 82:  parseCableDump(bs, patch);       break;
        case 77:  parseParameterDump(bs, patch);   break;
        case 101: parseMorphMap(bs, patch);         break;
        case 98:  parseKnobMapDump(bs, patch);     break;
        case 96:  parseControlMapDump(bs, patch);  break;
        case 91:  parseCustomDump(bs, patch);      break;
        case 90:  parseNameDump(bs, patch);        break;
        case 105: parseNoteDump(bs, patch);        break;
        default:
            DBG("PatchParser: unknown section type " + juce::String(type) + ", skipping");
            // Can't continue safely — we don't know the section length
            throw std::runtime_error("Unknown section type " + std::to_string(type));
    }

    // PDL2: Section % 8 — align to byte boundary after each section
    bs.alignToByte();
}

// PatchName := 0:8 0:8 0:8 String$name
void PatchParser::parsePatchName(BitStream& bs, Patch& patch)
{
    bs.readBits(8); // padding 0
    bs.readBits(8); // padding 0
    bs.readBits(8); // padding 0
    auto name = bs.readString16();
    patch.setName(juce::String(name));
    DBG("  PatchName: \"" + patch.getName() + "\"");
}

// PatchName2 := String$name  (no padding, used by synth PatchPacket responses)
void PatchParser::parsePatchName2(BitStream& bs, Patch& patch)
{
    auto name = bs.readString16();
    patch.setName(juce::String(name));
    DBG("  PatchName2: \"" + patch.getName() + "\"");
}

// Header := krangemin:7 krangemax:7 vrangemin:7 vrangemax:7
//           brange:5 ptime:7 portamento:1 pedalMode:1 voices:5
//           unknown2:2 sspos:12 octave:3
//           red:1 blue:1 yellow:1 gray:1 green:1 purple:1 white:1
//           cretrigger:1 pretrigger:1 unknown3:4 unknown4:3
void PatchParser::parseHeader(BitStream& bs, Patch& patch)
{
    auto& h = patch.getHeader();
    h.keyRangeMin      = static_cast<int>(bs.readBits(7));
    h.keyRangeMax      = static_cast<int>(bs.readBits(7));
    h.velRangeMin      = static_cast<int>(bs.readBits(7));
    h.velRangeMax      = static_cast<int>(bs.readBits(7));
    h.bendRange        = static_cast<int>(bs.readBits(5));
    h.portamentoTime   = static_cast<int>(bs.readBits(7));
    h.portamento       = bs.readBits(1) != 0;
    h.pedalMode        = static_cast<int>(bs.readBits(1));
    h.voices           = static_cast<int>(bs.readBits(5)) + 1;  // stored 0-based, display 1-based
    /*unknown2*/         bs.readBits(2);
    h.separatorPosition = static_cast<int>(bs.readBits(12));
    h.octaveShift      = static_cast<int>(bs.readBits(3));
    h.cableVisRed      = bs.readBits(1) != 0;
    h.cableVisBlue     = bs.readBits(1) != 0;
    h.cableVisYellow   = bs.readBits(1) != 0;
    h.cableVisGray     = bs.readBits(1) != 0;
    h.cableVisGreen    = bs.readBits(1) != 0;
    h.cableVisPurple   = bs.readBits(1) != 0;
    h.cableVisWhite         = bs.readBits(1) != 0;
    h.voiceRetriggerCommon  = static_cast<int>(bs.readBits(1));
    h.voiceRetriggerPoly    = static_cast<int>(bs.readBits(1));
    h.unknown3              = static_cast<int>(bs.readBits(4));
    h.unknown4              = static_cast<int>(bs.readBits(3));

    DBG("  Header: voices=" + juce::String(h.voices)
        + " keys=" + juce::String(h.keyRangeMin) + "-" + juce::String(h.keyRangeMax)
        + " bend=" + juce::String(h.bendRange));
}

// ModuleDump := section:1 nmodules:7 nmodules*Module
// Module := type:7 index:7 xpos:7 ypos:7
void PatchParser::parseModuleDump(BitStream& bs, Patch& patch)
{
    int section  = static_cast<int>(bs.readBits(1));
    int nmodules = static_cast<int>(bs.readBits(7));
    auto& container = patch.getContainer(section);

    DBG("  ModuleDump: section=" + juce::String(section) + " nmodules=" + juce::String(nmodules));

    for (int i = 0; i < nmodules; ++i)
    {
        if (bs.remaining() < 28)  // 4 * 7 bits per module
        {
            DBG("    ModuleDump: truncated at module " + juce::String(i) + "/" + juce::String(nmodules));
            break;
        }

        int type  = static_cast<int>(bs.readBits(7));
        int index = static_cast<int>(bs.readBits(7));
        int xpos  = static_cast<int>(bs.readBits(7));
        int ypos  = static_cast<int>(bs.readBits(7));

        auto* desc = descs.getModuleByIndex(type);
        if (desc == nullptr)
        {
            DBG("    Module type " + juce::String(type) + " not found in descriptions, skipping");
            continue;
        }

        auto module = Module::createFromDescriptor(*desc);
        module->setContainerIndex(index);
        module->setPosition({ xpos, ypos });
        container.addModule(std::move(module));

        DBG("    Module: " + desc->fullname + " idx=" + juce::String(index)
            + " pos=(" + juce::String(xpos) + "," + juce::String(ypos) + ")");
    }
}

// CableDump := section:1 ncables:15 ncables*Cable
// Cable := color:3 source:7 inputOutput:6 type:1 destination:7 input:6
void PatchParser::parseCableDump(BitStream& bs, Patch& patch)
{
    int section = static_cast<int>(bs.readBits(1));
    int ncables = static_cast<int>(bs.readBits(15));
    auto& container = patch.getContainer(section);

    DBG("  CableDump: section=" + juce::String(section) + " ncables=" + juce::String(ncables));

    for (int i = 0; i < ncables; ++i)
    {
        if (bs.remaining() < 30)  // 3+7+6+1+7+6 = 30 bits per cable
        {
            DBG("    CableDump: truncated at cable " + juce::String(i) + "/" + juce::String(ncables));
            break;
        }

        int color       = static_cast<int>(bs.readBits(3));
        int firstModuleIndex = static_cast<int>(bs.readBits(7));
        int firstConnectorIndex = static_cast<int>(bs.readBits(6));
        int firstConnectorType = static_cast<int>(bs.readBits(1));  // 0=input, 1=output
        int secondModuleIndex = static_cast<int>(bs.readBits(7));
        int secondConnectorIndex = static_cast<int>(bs.readBits(6));
        (void)color; // stored implicitly through connector signal types

        // The binary CableDump carries a type bit only for the first connector.
        // Clavia patches exist in both orders: strict 3.03 stores input first,
        // while some factory 3.0x patches store output first. Normalize either
        // encoding to the model convention: output connector first, input second.
        bool firstIsOutput = (firstConnectorType != 0);
        bool secondIsOutput = !firstIsOutput;

        auto* firstModule = container.getModuleByIndex(firstModuleIndex);
        auto* secondModule = container.getModuleByIndex(secondModuleIndex);

        if (firstModule == nullptr || secondModule == nullptr)
        {
            DBG("    Cable: missing module first=" + juce::String(firstModuleIndex)
                + " second=" + juce::String(secondModuleIndex));
            continue;
        }

        auto* firstConn = firstModule->getConnector(firstConnectorIndex, firstIsOutput);
        auto* secondConn = secondModule->getConnector(secondConnectorIndex, secondIsOutput);

        if (firstConn == nullptr || secondConn == nullptr)
        {
            DBG("    Cable: missing connector first_conn=" + juce::String(firstConnectorIndex)
                + " first_type=" + juce::String(firstConnectorType)
                + " second_conn=" + juce::String(secondConnectorIndex));
            continue;
        }

        auto* outputConn = firstIsOutput ? firstConn : secondConn;
        auto* inputConn = firstIsOutput ? secondConn : firstConn;
        container.addConnection(outputConn, inputConn);
    }
}

// ParameterDump := section:1 nmodules:7 nmodules*[index:7 type:7 params...]
// Params are data-driven: look up ModuleDescriptor, get parameter bit widths from maxValue
void PatchParser::parseParameterDump(BitStream& bs, Patch& patch)
{
    int section  = static_cast<int>(bs.readBits(1));
    int nmodules = static_cast<int>(bs.readBits(7));
    auto& container = patch.getContainer(section);

    DBG("  ParameterDump: section=" + juce::String(section) + " nmodules=" + juce::String(nmodules));

    for (int i = 0; i < nmodules; ++i)
    {
        int index = static_cast<int>(bs.readBits(7));
        int type  = static_cast<int>(bs.readBits(7));

        auto* desc = descs.getModuleByIndex(type);
        if (desc == nullptr)
        {
            DBG("    ParamDump: unknown module type " + juce::String(type) + ", cannot continue section");
            throw std::runtime_error("ParameterDump: unknown module type " + std::to_string(type));
        }

        // Get parameters with paramClass == "parameter", sorted by index
        // (they should already be in index order from XML parsing)
        auto* module = container.getModuleByIndex(index);

        for (auto& pd : desc->parameters)
        {
            if (pd.paramClass != "parameter")
                continue;

            int bits = parameterBitWidth(*desc, pd);
            int value = static_cast<int>(bs.readBits(bits));

            if (module != nullptr)
            {
                auto* param = module->getParameter(pd.index);
                if (param != nullptr)
                    param->setValue(value);
            }
        }
    }
}

// MorphMap := morph1:7 morph2:7 morph3:7 morph4:7
//             keyboard1:2 keyboard2:2 keyboard3:2 keyboard4:2
//             nknobs:5 nknobs*Morph
// Morph := section:1 module:7 parameter:7 morph:2 range:8
void PatchParser::parseMorphMap(BitStream& bs, Patch& patch)
{
    patch.morphValues[0] = static_cast<int>(bs.readBits(7));
    patch.morphValues[1] = static_cast<int>(bs.readBits(7));
    patch.morphValues[2] = static_cast<int>(bs.readBits(7));
    patch.morphValues[3] = static_cast<int>(bs.readBits(7));

    patch.morphKeyboard[0] = static_cast<int>(bs.readBits(2));
    patch.morphKeyboard[1] = static_cast<int>(bs.readBits(2));
    patch.morphKeyboard[2] = static_cast<int>(bs.readBits(2));
    patch.morphKeyboard[3] = static_cast<int>(bs.readBits(2));

    int nknobs = static_cast<int>(bs.readBits(5));

    DBG("  MorphMap: nknobs=" + juce::String(nknobs));

    for (int i = 0; i < nknobs; ++i)
    {
        MorphAssignment ma;
        ma.section = static_cast<int>(bs.readBits(1));
        ma.module  = static_cast<int>(bs.readBits(7));
        ma.param   = static_cast<int>(bs.readBits(7));
        ma.morph   = static_cast<int>(bs.readBits(2));
        ma.range   = static_cast<int>(bs.readBits(8));
        // range is signed 8-bit
        if (ma.range > 127)
            ma.range -= 256;
        patch.morphAssignments.push_back(ma);
    }
}

// KnobMapDump := 23 * Knob
// Knob := assigned:1 assigned*KnobAssignment
// KnobAssignment := section:2 module:7 parameter:7
void PatchParser::parseKnobMapDump(BitStream& bs, Patch& patch)
{
    DBG("  KnobMapDump");

    for (int i = 0; i < 23; ++i)
    {
        int assigned = static_cast<int>(bs.readBits(1));
        patch.knobAssignments[static_cast<size_t>(i)].assigned = (assigned != 0);

        if (assigned)
        {
            patch.knobAssignments[static_cast<size_t>(i)].section = static_cast<int>(bs.readBits(2));
            patch.knobAssignments[static_cast<size_t>(i)].module  = static_cast<int>(bs.readBits(7));
            patch.knobAssignments[static_cast<size_t>(i)].param   = static_cast<int>(bs.readBits(7));
        }
    }
}

// ControlMapDump := ncontrols:7 ncontrols*Control
// Control := control:7 section:2 module:7 parameter:7
void PatchParser::parseControlMapDump(BitStream& bs, Patch& patch)
{
    int ncontrols = static_cast<int>(bs.readBits(7));

    DBG("  ControlMapDump: ncontrols=" + juce::String(ncontrols));

    for (int i = 0; i < ncontrols; ++i)
    {
        CtrlAssignment ca;
        ca.control = static_cast<int>(bs.readBits(7));
        ca.section = static_cast<int>(bs.readBits(2));
        ca.module  = static_cast<int>(bs.readBits(7));
        ca.param   = static_cast<int>(bs.readBits(7));
        patch.ctrlAssignments.push_back(ca);
    }
}

// CustomDump := section:1 nmodules:7 nmodules*CustomModule
// CustomModule := index:7 nparams:8 nparams*CustomValue
// CustomValue := value:8
void PatchParser::parseCustomDump(BitStream& bs, Patch& patch)
{
    int section  = static_cast<int>(bs.readBits(1));
    int nmodules = static_cast<int>(bs.readBits(7));

    DBG("  CustomDump: section=" + juce::String(section) + " nmodules=" + juce::String(nmodules));

    auto& dumpVec = (section == 1) ? patch.polyCustomDump : patch.commonCustomDump;

    for (int i = 0; i < nmodules; ++i)
    {
        Patch::CustomDumpEntry entry;
        entry.index  = static_cast<int>(bs.readBits(7));
        int nparams  = static_cast<int>(bs.readBits(8));

        for (int j = 0; j < nparams; ++j)
            entry.values.push_back(static_cast<int>(bs.readBits(8)));

        dumpVec.push_back(std::move(entry));
    }
}

// NameDump := section:1 nmodules:7 nmodules*ModuleName
// ModuleName := index:8 String$name
// String := 16*chars:8/0
void PatchParser::parseNameDump(BitStream& bs, Patch& patch)
{
    int section  = static_cast<int>(bs.readBits(1));
    int nmodules = static_cast<int>(bs.readBits(7));
    auto& container = patch.getContainer(section);

    DBG("  NameDump: section=" + juce::String(section) + " nmodules=" + juce::String(nmodules));

    for (int i = 0; i < nmodules; ++i)
    {
        // Each module name needs at least 8 (index) + 8 (null terminator) = 16 bits
        if (bs.remaining() < 16)
            break;

        int index = static_cast<int>(bs.readBits(8));
        auto name = bs.readString16();

        auto* module = container.getModuleByIndex(index);
        if (module != nullptr && !name.empty())
            module->setTitle(juce::String(name));
    }
}

// NoteDump := Note$note1 nmorenotes:5 Note$note2 nmorenotes*Note$notes
// Note := value:7 attack:7 release:7
void PatchParser::parseNoteDump(BitStream& bs, Patch& patch)
{
    // First note is always present
    NoteSlot note1;
    note1.note    = static_cast<int>(bs.readBits(7));
    note1.attack  = static_cast<int>(bs.readBits(7));
    note1.release = static_cast<int>(bs.readBits(7));
    patch.notes.push_back(note1);

    int nmorenotes = static_cast<int>(bs.readBits(5));

    // Second note
    NoteSlot note2;
    note2.note    = static_cast<int>(bs.readBits(7));
    note2.attack  = static_cast<int>(bs.readBits(7));
    note2.release = static_cast<int>(bs.readBits(7));
    patch.notes.push_back(note2);

    // Additional notes
    for (int i = 0; i < nmorenotes; ++i)
    {
        NoteSlot note;
        note.note    = static_cast<int>(bs.readBits(7));
        note.attack  = static_cast<int>(bs.readBits(7));
        note.release = static_cast<int>(bs.readBits(7));
        patch.notes.push_back(note);
    }

    DBG("  NoteDump: " + juce::String(patch.notes.size()) + " notes");
}
