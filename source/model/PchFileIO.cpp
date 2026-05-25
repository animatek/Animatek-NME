#include "PchFileIO.h"

PchFileIO::PchFileIO(const ModuleDescriptions& moduleDescs)
    : descs(moduleDescs)
{
}

juce::StringArray PchFileIO::tokenize(const juce::String& line)
{
    juce::StringArray tokens;
    tokens.addTokens(line.trim(), " \t", "");
    tokens.removeEmptyStrings();
    return tokens;
}

// =============================================================================
// Reader
// =============================================================================

std::unique_ptr<Patch> PchFileIO::readFile(const juce::File& file)
{
    auto text = file.loadFileAsString();
    if (text.isEmpty())
        return nullptr;

    auto patch = std::make_unique<Patch>();

    // Split into lines, handling both \r\n and \n
    juce::StringArray allLines;
    allLines.addLines(text);

    // Parse sections: find [SectionName] ... [/SectionName] blocks
    int i = 0;
    while (i < allLines.size())
    {
        auto line = allLines[i].trim();

        // Match opening tag [SectionName]
        if (line.startsWith("[") && !line.startsWith("[/"))
        {
            auto closeBracket = line.indexOf("]");
            if (closeBracket < 0) { ++i; continue; }

            auto sectionName = line.substring(1, closeBracket).toLowerCase();
            auto closeTag = "[/" + line.substring(1, closeBracket + 1);

            // Collect lines until closing tag
            juce::StringArray sectionLines;
            ++i;
            while (i < allLines.size())
            {
                auto sline = allLines[i].trim();
                if (sline.equalsIgnoreCase(closeTag))
                {
                    ++i;
                    break;
                }
                sectionLines.add(allLines[i]);
                ++i;
            }

            // Dispatch to section parser
            if (sectionName == "header")              parseHeader(sectionLines, *patch);
            else if (sectionName == "moduledump")     parseModuleDump(sectionLines, *patch);
            else if (sectionName == "currentnotedump") parseCurrentNoteDump(sectionLines, *patch);
            else if (sectionName == "cabledump")      parseCableDump(sectionLines, *patch);
            else if (sectionName == "parameterdump")  parseParameterDump(sectionLines, *patch);
            else if (sectionName == "morphmapdump")   parseMorphMapDump(sectionLines, *patch);
            else if (sectionName == "keyboardassignment") parseKeyboardAssignment(sectionLines, *patch);
            else if (sectionName == "knobmapdump")    parseKnobMapDump(sectionLines, *patch);
            else if (sectionName == "ctrlmapdump")    parseCtrlMapDump(sectionLines, *patch);
            else if (sectionName == "customdump")     parseCustomDump(sectionLines, *patch);
            else if (sectionName == "namedump")       parseNameDump(sectionLines, *patch);
            else if (sectionName == "notes")
            {
                // Notes section: preserve all text as-is
                patch->patchNotes = sectionLines.joinIntoString("\n");
            }
            // Skip unknown sections (e.g. [Info])
        }
        else
        {
            ++i;
        }
    }

    // Derive patch name from filename if not set from notes
    if (patch->getName() == "Init Patch")
        patch->setName(file.getFileNameWithoutExtension());

    DBG("PchFileIO: loaded \"" + patch->getName() + "\" from " + file.getFileName());
    DBG("  Poly modules: " + juce::String(patch->getPolyVoiceArea().getModules().size())
        + ", Common modules: " + juce::String(patch->getCommonArea().getModules().size()));

    return patch;
}

// --- Header ---
void PchFileIO::parseHeader(const juce::StringArray& lines, Patch& patch)
{
    // Collect all numbers from lines (skip "Version=..." line)
    juce::StringArray allTokens;
    for (auto& line : lines)
    {
        if (line.trim().startsWithIgnoreCase("Version"))
            continue;
        auto tokens = tokenize(line);
        allTokens.addArray(tokens);
    }

    if (allTokens.size() < 23)
    {
        DBG("PchFileIO: Header has only " + juce::String(allTokens.size()) + " values (expected 23)");
        return;
    }

    auto& h = patch.getHeader();
    h.keyRangeMin        = allTokens[0].getIntValue();
    h.keyRangeMax        = allTokens[1].getIntValue();
    h.velRangeMin        = allTokens[2].getIntValue();
    h.velRangeMax        = allTokens[3].getIntValue();
    h.bendRange          = allTokens[4].getIntValue();
    h.portamentoTime     = allTokens[5].getIntValue();
    h.portamento         = allTokens[6].getIntValue() != 0;
    h.voices             = allTokens[7].getIntValue();  // 1-based in .pch file
    h.separatorPosition  = allTokens[8].getIntValue();
    h.octaveShift        = allTokens[9].getIntValue();
    h.voiceRetriggerPoly = allTokens[10].getIntValue();
    h.voiceRetriggerCommon = allTokens[11].getIntValue();
    h.unknown1           = allTokens[12].getIntValue();
    h.unknown2           = allTokens[13].getIntValue();
    h.unknown3           = allTokens[14].getIntValue();
    h.unknown4           = allTokens[15].getIntValue();
    h.cableVisRed        = allTokens[16].getIntValue() != 0;
    h.cableVisBlue       = allTokens[17].getIntValue() != 0;
    h.cableVisYellow     = allTokens[18].getIntValue() != 0;
    h.cableVisGray       = allTokens[19].getIntValue() != 0;
    h.cableVisGreen      = allTokens[20].getIntValue() != 0;
    h.cableVisPurple     = allTokens[21].getIntValue() != 0;
    h.cableVisWhite      = allTokens[22].getIntValue() != 0;
}

// --- ModuleDump ---
void PchFileIO::parseModuleDump(const juce::StringArray& lines, Patch& patch)
{
    if (lines.size() < 1) return;

    int voiceAreaId = tokenize(lines[0])[0].getIntValue();
    auto& container = patch.getContainer(voiceAreaId == 1 ? 1 : 0);

    for (int i = 1; i < lines.size(); ++i)
    {
        auto tokens = tokenize(lines[i]);
        if (tokens.size() < 4) continue;

        int index = tokens[0].getIntValue();
        int type  = tokens[1].getIntValue();
        int xpos  = tokens[2].getIntValue();
        int ypos  = tokens[3].getIntValue();

        auto* desc = descs.getModuleByIndex(type);
        if (desc == nullptr)
        {
            DBG("PchFileIO: ModuleDump unknown module type " + juce::String(type));
            continue;
        }

        auto module = Module::createFromDescriptor(*desc);
        module->setContainerIndex(index);
        module->setPosition({ xpos, ypos });
        container.addModule(std::move(module));
    }
}

// --- CurrentNoteDump ---
void PchFileIO::parseCurrentNoteDump(const juce::StringArray& lines, Patch& patch)
{
    // All values on one or more lines, groups of 3
    juce::StringArray allTokens;
    for (auto& line : lines)
        allTokens.addArray(tokenize(line));

    for (int i = 0; i + 2 < allTokens.size(); i += 3)
    {
        NoteSlot ns;
        ns.note    = allTokens[i].getIntValue();
        ns.attack  = allTokens[i + 1].getIntValue();
        ns.release = allTokens[i + 2].getIntValue();
        patch.notes.push_back(ns);
    }
}

// --- CableDump ---
void PchFileIO::parseCableDump(const juce::StringArray& lines, Patch& patch)
{
    if (lines.size() < 1) return;

    int voiceAreaId = tokenize(lines[0])[0].getIntValue();
    auto& container = patch.getContainer(voiceAreaId == 1 ? 1 : 0);

    for (int i = 1; i < lines.size(); ++i)
    {
        auto tokens = tokenize(lines[i]);
        if (tokens.size() < 7) continue;

        int color    = tokens[0].getIntValue();
        int firstMod   = tokens[1].getIntValue();
        int firstConn  = tokens[2].getIntValue();
        int firstType  = tokens[3].getIntValue();  // 0=input, 1=output
        int secondMod  = tokens[4].getIntValue();
        int secondConn = tokens[5].getIntValue();
        int secondType = tokens[6].getIntValue();  // 0=input, 1=output
        (void)color;

        auto* firstModule = container.getModuleByIndex(firstMod);
        auto* secondModule = container.getModuleByIndex(secondMod);

        if (firstModule == nullptr || secondModule == nullptr)
        {
            DBG("PchFileIO: CableDump missing module first=" + juce::String(firstMod)
                + " second=" + juce::String(secondMod));
            continue;
        }

        auto* firstConnector = firstModule->getConnector(firstConn, firstType != 0);
        auto* secondConnector = secondModule->getConnector(secondConn, secondType != 0);

        if (firstConnector == nullptr || secondConnector == nullptr)
        {
            DBG("PchFileIO: CableDump missing connector first_conn=" + juce::String(firstConn)
                + " second_conn=" + juce::String(secondConn));
            continue;
        }

        Connector* outputConnector = (firstType != 0) ? firstConnector : secondConnector;
        Connector* inputConnector  = (firstType != 0) ? secondConnector : firstConnector;
        container.addConnection(outputConnector, inputConnector);
    }
}

// --- ParameterDump ---
void PchFileIO::parseParameterDump(const juce::StringArray& lines, Patch& patch)
{
    if (lines.size() < 1) return;

    int voiceAreaId = tokenize(lines[0])[0].getIntValue();
    auto& container = patch.getContainer(voiceAreaId == 1 ? 1 : 0);

    for (int i = 1; i < lines.size(); ++i)
    {
        auto tokens = tokenize(lines[i]);
        if (tokens.size() < 3) continue;

        int index = tokens[0].getIntValue();
        int type  = tokens[1].getIntValue();
        // tokens[2] is paramCount — we don't need it since we use the descriptor

        auto* module = container.getModuleByIndex(index);
        auto* desc = descs.getModuleByIndex(type);

        if (desc == nullptr) continue;

        // Map parameter values to the module's "parameter" class params
        int valueIdx = 3;
        for (auto& pd : desc->parameters)
        {
            if (pd.paramClass != "parameter") continue;
            if (valueIdx >= tokens.size()) break;

            int value = tokens[valueIdx].getIntValue();
            if (module != nullptr)
            {
                auto* param = module->getParameter(pd.index);
                if (param != nullptr)
                    param->setValue(value);
            }
            ++valueIdx;
        }
    }
}

// --- MorphMapDump ---
void PchFileIO::parseMorphMapDump(const juce::StringArray& lines, Patch& patch)
{
    // Collect all tokens
    juce::StringArray allTokens;
    for (auto& line : lines)
        allTokens.addArray(tokenize(line));

    if (allTokens.size() < 4) return;

    // First 4 values are morph values
    patch.morphValues[0] = allTokens[0].getIntValue();
    patch.morphValues[1] = allTokens[1].getIntValue();
    patch.morphValues[2] = allTokens[2].getIntValue();
    patch.morphValues[3] = allTokens[3].getIntValue();

    // Remaining values are quintets: section module param morph range
    for (int i = 4; i + 4 < allTokens.size(); i += 5)
    {
        MorphAssignment ma;
        ma.section = allTokens[i].getIntValue();
        ma.module  = allTokens[i + 1].getIntValue();
        ma.param   = allTokens[i + 2].getIntValue();
        ma.morph   = allTokens[i + 3].getIntValue();
        ma.range   = allTokens[i + 4].getIntValue();
        patch.morphAssignments.push_back(ma);
    }
}

// --- KeyboardAssignment ---
void PchFileIO::parseKeyboardAssignment(const juce::StringArray& lines, Patch& patch)
{
    juce::StringArray allTokens;
    for (auto& line : lines)
        allTokens.addArray(tokenize(line));

    for (int i = 0; i < 4 && i < allTokens.size(); ++i)
        patch.morphKeyboard[static_cast<size_t>(i)] = allTokens[i].getIntValue();
}

// --- KnobMapDump ---
void PchFileIO::parseKnobMapDump(const juce::StringArray& lines, Patch& patch)
{
    for (auto& line : lines)
    {
        auto tokens = tokenize(line);
        if (tokens.size() < 4) continue;

        int section  = tokens[0].getIntValue();
        int module   = tokens[1].getIntValue();
        int param    = tokens[2].getIntValue();
        int knobIdx  = tokens[3].getIntValue();

        if (knobIdx >= 0 && knobIdx < 23)
        {
            auto& ka = patch.knobAssignments[static_cast<size_t>(knobIdx)];
            ka.assigned = true;
            ka.section = section;
            ka.module = module;
            ka.param = param;
        }
    }
}

// --- CtrlMapDump ---
void PchFileIO::parseCtrlMapDump(const juce::StringArray& lines, Patch& patch)
{
    for (auto& line : lines)
    {
        auto tokens = tokenize(line);
        if (tokens.size() < 4) continue;

        CtrlAssignment ca;
        ca.section = tokens[0].getIntValue();
        ca.module  = tokens[1].getIntValue();
        ca.param   = tokens[2].getIntValue();
        ca.control = tokens[3].getIntValue();
        patch.ctrlAssignments.push_back(ca);
    }
}

// --- CustomDump ---
void PchFileIO::parseCustomDump(const juce::StringArray& lines, Patch& patch)
{
    if (lines.size() < 1) return;

    int voiceAreaId = tokenize(lines[0])[0].getIntValue();
    auto& dumpVec = (voiceAreaId == 1) ? patch.polyCustomDump : patch.commonCustomDump;

    for (int i = 1; i < lines.size(); ++i)
    {
        auto tokens = tokenize(lines[i]);
        if (tokens.size() < 2) continue;

        Patch::CustomDumpEntry entry;
        entry.index = tokens[0].getIntValue();
        int nparams = tokens[1].getIntValue();

        for (int j = 0; j < nparams && (2 + j) < tokens.size(); ++j)
            entry.values.push_back(tokens[2 + j].getIntValue());

        dumpVec.push_back(std::move(entry));
    }
}

// --- NameDump ---
void PchFileIO::parseNameDump(const juce::StringArray& lines, Patch& patch)
{
    if (lines.size() < 1) return;

    int voiceAreaId = tokenize(lines[0])[0].getIntValue();
    auto& container = patch.getContainer(voiceAreaId == 1 ? 1 : 0);

    for (int i = 1; i < lines.size(); ++i)
    {
        auto line = lines[i].trim();
        if (line.isEmpty()) continue;

        // First token is module index, rest is the name (may contain spaces)
        auto spaceIdx = line.indexOfChar(' ');
        if (spaceIdx < 0) continue;

        int index = line.substring(0, spaceIdx).getIntValue();
        auto name = line.substring(spaceIdx + 1);

        auto* module = container.getModuleByIndex(index);
        if (module != nullptr && name.isNotEmpty())
            module->setTitle(name);
    }
}

// =============================================================================
// Writer
// =============================================================================

bool PchFileIO::writeFile(const Patch& patch, const juce::File& file)
{
    juce::String out;

    writeHeader(out, patch);
    writeModuleDump(out, patch.getPolyVoiceArea(), 1);
    writeModuleDump(out, patch.getCommonArea(), 0);
    writeCurrentNoteDump(out, patch);
    writeCableDump(out, patch.getPolyVoiceArea(), 1);
    writeCableDump(out, patch.getCommonArea(), 0);
    writeParameterDump(out, patch.getPolyVoiceArea(), 1);
    writeParameterDump(out, patch.getCommonArea(), 0);
    writeMorphMapDump(out, patch);
    writeKeyboardAssignment(out, patch);
    writeKnobMapDump(out, patch);
    writeCtrlMapDump(out, patch);
    writeCustomDump(out, patch, patch.getPolyVoiceArea(), 1);
    writeCustomDump(out, patch, patch.getCommonArea(), 0);
    writeNameDump(out, patch.getPolyVoiceArea(), 1);
    writeNameDump(out, patch.getCommonArea(), 0);

    if (patch.patchNotes.isNotEmpty())
        writeNotes(out, patch);

    return file.replaceWithText(out);
}

// --- Header ---
void PchFileIO::writeHeader(juce::String& out, const Patch& patch)
{
    auto& h = patch.getHeader();
    out += "[Header]\n";
    out += "Version=Nord Modular patch 3.0\n";
    out += juce::String(h.keyRangeMin) + " "
        + juce::String(h.keyRangeMax) + " "
        + juce::String(h.velRangeMin) + " "
        + juce::String(h.velRangeMax) + " "
        + juce::String(h.bendRange) + " "
        + juce::String(h.portamentoTime) + " "
        + juce::String(h.portamento ? 1 : 0) + " "
        + juce::String(h.voices) + " "
        + juce::String(h.separatorPosition) + " "
        + juce::String(h.octaveShift) + " "
        + juce::String(h.voiceRetriggerPoly) + " "
        + juce::String(h.voiceRetriggerCommon) + " "
        + juce::String(h.unknown1) + " "
        + juce::String(h.unknown2) + " "
        + juce::String(h.unknown3) + " "
        + juce::String(h.unknown4) + " "
        + juce::String(h.cableVisRed ? 1 : 0) + " "
        + juce::String(h.cableVisBlue ? 1 : 0) + " "
        + juce::String(h.cableVisYellow ? 1 : 0) + " "
        + juce::String(h.cableVisGray ? 1 : 0) + " "
        + juce::String(h.cableVisGreen ? 1 : 0) + " "
        + juce::String(h.cableVisPurple ? 1 : 0) + " "
        + juce::String(h.cableVisWhite ? 1 : 0) + " \n";
    out += "[/Header]\n";
}

// --- ModuleDump ---
void PchFileIO::writeModuleDump(juce::String& out, const ModuleContainer& container, int voiceAreaId)
{
    out += "[ModuleDump]\n";
    out += juce::String(voiceAreaId) + " \n";

    for (auto& m : container.getModules())
    {
        out += juce::String(m->getContainerIndex()) + " "
            + juce::String(m->getDescriptor()->index) + " "
            + juce::String(m->getPosition().x) + " "
            + juce::String(m->getPosition().y) + " \n";
    }

    out += "[/ModuleDump]\n";
}

// --- CurrentNoteDump ---
void PchFileIO::writeCurrentNoteDump(juce::String& out, const Patch& patch)
{
    out += "[CurrentNoteDump]\n";

    if (patch.notes.empty())
    {
        // Default: two silent notes
        out += "64 0 0 64 0 0 \n";
    }
    else
    {
        for (auto& ns : patch.notes)
            out += juce::String(ns.note) + " " + juce::String(ns.attack) + " " + juce::String(ns.release) + " ";
        out += "\n";
    }

    out += "[/CurrentNoteDump]\n";
}

// --- CableDump ---
void PchFileIO::writeCableDump(juce::String& out, const ModuleContainer& container, int voiceAreaId)
{
    out += "[CableDump]\n";
    out += juce::String(voiceAreaId) + " \n";

    // Build connector -> module lookup
    std::map<const Connector*, const Module*> connectorToModule;
    for (auto& m : container.getModules())
        for (auto& c : m->getConnectors())
            connectorToModule[&c] = m.get();

    for (auto& conn : container.getConnections())
    {
        auto* srcModule = connectorToModule.count(conn.output) ? connectorToModule.at(conn.output) : nullptr;
        auto* dstModule = connectorToModule.count(conn.input) ? connectorToModule.at(conn.input) : nullptr;

        if (srcModule == nullptr || dstModule == nullptr) continue;

        // Color comes from the source connector's signal type
        int color = static_cast<int>(conn.output->getDescriptor()->signalType);

        out += juce::String(color) + " "
            + juce::String(dstModule->getContainerIndex()) + " "
            + juce::String(conn.input->getDescriptor()->index) + " "
            + juce::String(conn.input->getDescriptor()->isOutput ? 1 : 0) + " "
            + juce::String(srcModule->getContainerIndex()) + " "
            + juce::String(conn.output->getDescriptor()->index) + " "
            + juce::String(conn.output->getDescriptor()->isOutput ? 1 : 0) + " \n";
    }

    out += "[/CableDump]\n";
}

// --- ParameterDump ---
void PchFileIO::writeParameterDump(juce::String& out, const ModuleContainer& container, int voiceAreaId)
{
    out += "[ParameterDump]\n";
    out += juce::String(voiceAreaId) + " \n";

    for (auto& m : container.getModules())
    {
        auto* desc = m->getDescriptor();

        // Count "parameter" class params
        int paramCount = 0;
        for (auto& pd : desc->parameters)
            if (pd.paramClass == "parameter") ++paramCount;

        if (paramCount == 0) continue;

        out += juce::String(m->getContainerIndex()) + " "
            + juce::String(desc->index) + " "
            + juce::String(paramCount);

        for (auto& pd : desc->parameters)
        {
            if (pd.paramClass != "parameter") continue;
            auto* param = m->getParameter(pd.index);
            int value = param ? param->getValue() : pd.defaultValue;
            out += " " + juce::String(value);
        }
        out += " \n";
    }

    out += "[/ParameterDump]\n";
}

// --- MorphMapDump ---
void PchFileIO::writeMorphMapDump(juce::String& out, const Patch& patch)
{
    out += "[MorphMapDump]\n";
    out += juce::String(patch.morphValues[0]) + " "
        + juce::String(patch.morphValues[1]) + " "
        + juce::String(patch.morphValues[2]) + " "
        + juce::String(patch.morphValues[3]) + " \n";

    for (auto& ma : patch.morphAssignments)
    {
        out += juce::String(ma.section) + " "
            + juce::String(ma.module) + " "
            + juce::String(ma.param) + " "
            + juce::String(ma.morph) + " "
            + juce::String(ma.range) + " ";
    }
    if (!patch.morphAssignments.empty())
        out += "\n";

    out += "[/MorphMapDump]\n";
}

// --- KeyboardAssignment ---
void PchFileIO::writeKeyboardAssignment(juce::String& out, const Patch& patch)
{
    out += "[KeyboardAssignment]\n";
    out += juce::String(patch.morphKeyboard[0]) + " "
        + juce::String(patch.morphKeyboard[1]) + " "
        + juce::String(patch.morphKeyboard[2]) + " "
        + juce::String(patch.morphKeyboard[3]) + " \n";
    out += "[/KeyboardAssignment]\n";
}

// --- KnobMapDump ---
void PchFileIO::writeKnobMapDump(juce::String& out, const Patch& patch)
{
    out += "[KnobMapDump]\n";

    for (int i = 0; i < 23; ++i)
    {
        auto& ka = patch.knobAssignments[static_cast<size_t>(i)];
        if (!ka.assigned) continue;

        out += juce::String(ka.section) + " "
            + juce::String(ka.module) + " "
            + juce::String(ka.param) + " "
            + juce::String(i) + " \n";
    }

    out += "[/KnobMapDump]\n";
}

// --- CtrlMapDump ---
void PchFileIO::writeCtrlMapDump(juce::String& out, const Patch& patch)
{
    out += "[CtrlMapDump]\n";

    for (auto& ca : patch.ctrlAssignments)
    {
        out += juce::String(ca.section) + " "
            + juce::String(ca.module) + " "
            + juce::String(ca.param) + " "
            + juce::String(ca.control) + " \n";
    }

    out += "[/CtrlMapDump]\n";
}

// --- CustomDump ---
void PchFileIO::writeCustomDump(juce::String& out, const Patch& patch, const ModuleContainer& container, int voiceAreaId)
{
    const auto& preExisting = (voiceAreaId == 1) ? patch.polyCustomDump : patch.commonCustomDump;

    out += "[CustomDump]\n";
    out += juce::String(voiceAreaId) + " \n";

    // Build a map of pre-existing entries by module index
    std::map<int, const Patch::CustomDumpEntry*> existingMap;
    for (auto& entry : preExisting)
        existingMap[entry.index] = &entry;

    for (auto& m : container.getModules())
    {
        auto* desc = m->getDescriptor();
        if (desc == nullptr) continue;

        // Count "custom" class params
        std::vector<int> customValues;
        for (auto& pd : desc->parameters)
        {
            if (pd.paramClass != "custom") continue;
            customValues.push_back(pd.defaultValue);
        }

        if (customValues.empty()) continue;

        int idx = m->getContainerIndex();

        // Use pre-existing values if available, otherwise defaults
        auto it = existingMap.find(idx);
        if (it != existingMap.end() && it->second->values.size() == customValues.size())
        {
            out += juce::String(idx) + " " + juce::String(static_cast<int>(it->second->values.size()));
            for (auto v : it->second->values)
                out += " " + juce::String(v);
        }
        else
        {
            out += juce::String(idx) + " " + juce::String(static_cast<int>(customValues.size()));
            for (auto v : customValues)
                out += " " + juce::String(v);
        }
        out += " \n";
    }

    out += "[/CustomDump]\n";
}

// --- NameDump ---
void PchFileIO::writeNameDump(juce::String& out, const ModuleContainer& container, int voiceAreaId)
{
    out += "[NameDump]\n";
    out += juce::String(voiceAreaId) + " \n";

    for (auto& m : container.getModules())
    {
        // Only write if the title differs from the default descriptor name
        auto& title = m->getTitle();
        if (title.isNotEmpty())
        {
            out += juce::String(m->getContainerIndex()) + " " + title + "\n";
        }
    }

    out += "[/NameDump]\n";
}

// --- Notes ---
void PchFileIO::writeNotes(juce::String& out, const Patch& patch)
{
    out += "[Notes]\n";
    out += patch.patchNotes + "\n";
    out += "[/Notes]\n";
}
