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

struct LegacyCableEntry
{
    int sourceModule = 0;
    int sourceConn = 0;
    bool sourceIsOutput = true;
    int targetModule = 0;
    int targetInput = 0;
};

juce::String PchFileIO::getLegacyValue(const juce::StringArray& lines, const juce::String& key)
{
    const auto prefix = key + "=";
    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();
        if (trimmed.startsWithIgnoreCase(prefix))
            return trimmed.substring(prefix.length()).trim();
    }

    return {};
}

void PchFileIO::normalizeLegacyModulePositions(ModuleContainer& container)
{
    struct ModulePos
    {
        Module* module = nullptr;
        int x = 0;
        int y = 0;
    };

    std::vector<ModulePos> modules;
    for (const auto& module : container.getModules())
    {
        const auto pos = module->getPosition();
        modules.push_back({ module.get(), pos.x, pos.y });
    }

    std::sort(modules.begin(), modules.end(), [](const ModulePos& a, const ModulePos& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.module->getContainerIndex() < b.module->getContainerIndex();
    });

    std::map<int, int> nextFreeRowByColumn;
    for (const auto& entry : modules)
    {
        if (entry.module == nullptr || entry.module->getDescriptor() == nullptr)
            continue;

        auto& nextFreeRow = nextFreeRowByColumn[entry.x];
        const int normalizedY = juce::jmax(entry.y, nextFreeRow);
        entry.module->setPosition({ entry.x, normalizedY });

        nextFreeRow = normalizedY + entry.module->getDescriptor()->height + 1;
    }
}

void PchFileIO::connectLegacyCable(ModuleContainer& container, int sourceModule, int sourceConn,
                                   bool sourceIsOutput, int targetModule, int targetInput)
{
    auto* source = container.getModuleByIndex(sourceModule);
    auto* target = container.getModuleByIndex(targetModule);
    if (source == nullptr || target == nullptr)
        return;

    // The source side may be another input (daisy-chained cable); pass it
    // through as the "output" side just like parseCableDump does for the
    // 3.0 format's input-input rows.
    auto* src = source->getConnector(sourceConn, sourceIsOutput);
    auto* input = target->getConnector(targetInput, false);

    if (src == nullptr || input == nullptr)
    {
        DBG("PchFileIO: legacy cable missing connector src=" + juce::String(sourceModule)
            + ":" + juce::String(sourceConn) + (sourceIsOutput ? " (out)" : " (in)")
            + " dst=" + juce::String(targetModule)
            + ":" + juce::String(targetInput));
        return;
    }

    container.addConnection(src, input);
}

// =============================================================================
// Legacy 2.10 detection
// =============================================================================

// The Version= marker sits within the first few lines of a 2.10 file.
static constexpr int kLegacySniffLines = 12;

static bool isLegacyVersionLine(const juce::String& rawLine)
{
    auto line = rawLine.trim();
    return line.startsWithIgnoreCase("Version=")
        && line.containsIgnoreCase("Nord Modular patch 2.10");
}

juce::String PchFileIO::patchNameFromFileName(const juce::File& file)
{
    const auto stem = file.getFileNameWithoutExtension();

    // "NN - " and something after it. Two digits and that exact separator is
    // narrow enough that a patch genuinely called "35 - Bells" is the only way
    // to trip it, and the prefix is what we wrote in the first place.
    if (stem.length() > 5
        && juce::CharacterFunctions::isDigit(stem[0])
        && juce::CharacterFunctions::isDigit(stem[1])
        && stem.substring(2, 5) == " - ")
        return stem.substring(5);

    return stem;
}

bool PchFileIO::isLegacyPatch210(const juce::File& file)
{
    auto stream = file.createInputStream();
    if (stream == nullptr)
        return false;

    for (int i = 0; i < kLegacySniffLines && !stream->isExhausted(); ++i)
        if (isLegacyVersionLine(stream->readNextLine()))
            return true;

    return false;
}

bool PchFileIO::isLegacyPatch210(const juce::StringArray& lines)
{
    for (int i = 0; i < kLegacySniffLines && i < lines.size(); ++i)
        if (isLegacyVersionLine(lines[i]))
            return true;

    return false;
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

    if (isLegacyPatch210(allLines))
        return readLegacyFile(allLines, file);

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
            else if (sectionName == "comments")  parseComments(sectionLines, *patch);
            else if (sectionName == "nme")
            {
                for (const auto& line : sectionLines)
                    if (line.trim().startsWith("Id="))
                        patch->extrasId = line.trim().fromFirstOccurrenceOf("=", false, false).trim();
            }
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

    // Apply collected custom dumps now that every ModuleDump has been parsed
    // (section order in the file is not guaranteed).
    for (const auto& entry : patch->polyCustomDump)
        patch->applyCustomDumpEntry(1, entry);
    for (const auto& entry : patch->commonCustomDump)
        patch->applyCustomDumpEntry(0, entry);

    // Derive patch name from filename if not set from notes
    if (patch->getName() == "Init Patch")
        patch->setName(patchNameFromFileName(file));

    DBG("PchFileIO: loaded \"" + patch->getName() + "\" from " + file.getFileName());
    DBG("  Poly modules: " + juce::String(patch->getPolyVoiceArea().getModules().size())
        + ", Common modules: " + juce::String(patch->getCommonArea().getModules().size()));

    return patch;
}

std::unique_ptr<Patch> PchFileIO::readLegacyFile(const juce::StringArray& allLines, const juce::File& file)
{
    auto patch = std::make_unique<Patch>();
    auto& header = patch->getHeader();
    std::vector<LegacyCableEntry> pendingCables;

    int i = 0;
    while (i < allLines.size())
    {
        const auto line = allLines[i].trim();
        if (!line.startsWith("[") || !line.endsWith("]"))
        {
            ++i;
            continue;
        }

        const auto sectionName = line.substring(1, line.length() - 1);
        juce::StringArray sectionLines;
        ++i;
        while (i < allLines.size())
        {
            const auto sectionLine = allLines[i].trim();
            if (sectionLine.startsWith("[") && sectionLine.endsWith("]"))
                break;

            sectionLines.add(allLines[i]);
            ++i;
        }

        if (sectionName.equalsIgnoreCase("Header"))
        {
            const auto name = getLegacyValue(sectionLines, "Name");
            if (name.isNotEmpty()) patch->setName(name);

            auto assignInt = [&sectionLines](const char* key, int& target) {
                const auto value = PchFileIO::getLegacyValue(sectionLines, key);
                if (value.isNotEmpty()) target = value.getIntValue();
            };

            assignInt("KbRangeMin", header.keyRangeMin);
            assignInt("KbRangeMax", header.keyRangeMax);
            assignInt("VelRangeMin", header.velRangeMin);
            assignInt("VelRangeMax", header.velRangeMax);
            assignInt("BendRange", header.bendRange);
            assignInt("PMTime", header.portamentoTime);
            assignInt("Voices", header.voices);
            assignInt("OctShift", header.octaveShift);
            assignInt("Retrig", header.voiceRetriggerPoly);
            header.voiceRetriggerCommon = header.voiceRetriggerPoly;

            const auto pmMode = getLegacyValue(sectionLines, "PMMode");
            if (pmMode.isNotEmpty()) header.portamento = pmMode.getIntValue() != 0;
        }
        else if (sectionName.startsWithIgnoreCase("Module "))
        {
            const int moduleIndex = sectionName.fromFirstOccurrenceOf(" ", false, false).getIntValue();
            const int type = getLegacyValue(sectionLines, "Type").getIntValue();

            if (auto* desc = descs.getModuleByIndex(type))
            {
                auto module = Module::createFromDescriptor(*desc);
                module->setContainerIndex(moduleIndex);
                module->setPosition({ getLegacyValue(sectionLines, "Col").getIntValue(),
                                      getLegacyValue(sectionLines, "Row").getIntValue() });

                const auto name = getLegacyValue(sectionLines, "Name");
                if (name.isNotEmpty()) module->setTitle(name);

                for (const auto& param : module->getParameters())
                {
                    if (param.getDescriptor()->paramClass != "parameter")
                        continue;

                    const auto value = getLegacyValue(sectionLines, "P" + juce::String(param.getDescriptor()->index));
                    if (value.isNotEmpty())
                        if (auto* mutableParam = module->getParameter(param.getDescriptor()->index))
                        {
                            int paramValue = value.getIntValue();
                            // 2.10 stores output destinations 1-based (1 = "1/2");
                            // 3.0 is 0-based, so a verbatim copy lands every
                            // factory patch on outputs 3/4 (issue #14 follow-up).
                            // Only the "audio,out,assign" role means an output
                            // destination — "morph,assign" (morph keyboard
                            // assignment) also contains "assign" and must NOT
                            // be remapped, or legacy patches using it get
                            // silently decremented (found in code review).
                            if (param.getDescriptor()->role.contains("out,assign"))
                                paramValue = juce::jmax(0, paramValue - 1);
                            mutableParam->setValue(paramValue);
                        }
                }

                for (const auto& cableLine : sectionLines)
                {
                    const auto trimmed = cableLine.trim();
                    if (!trimmed.startsWithIgnoreCase("Im"))
                        continue;

                    const auto equals = trimmed.indexOfChar('=');
                    if (equals < 0)
                        continue;

                    const int targetInput = trimmed.substring(2, equals).getIntValue();
                    const int sourceModule = trimmed.substring(equals + 1).getIntValue();
                    const auto sourceOutputValue = getLegacyValue(sectionLines, "Ih" + juce::String(targetInput));
                    if (sourceModule <= 0 || sourceOutputValue.isEmpty())
                        continue;

                    // Ih = [output flag:bit6][connector index:bits0-5].
                    // Bit 6 set: the source connector is an output; clear: it
                    // is another input (daisy-chained cable, like the 3.0
                    // CableDump's input-input rows). Observed values in real
                    // 2.10 patches are only 0, 1 and 64-66.
                    const int ih = sourceOutputValue.getIntValue();
                    pendingCables.push_back({ sourceModule,
                                              ih & 0x3f,
                                              (ih & 0x40) != 0,
                                              moduleIndex,
                                              targetInput });
                }

                patch->getPolyVoiceArea().addModule(std::move(module));
            }
        }
    }

    normalizeLegacyModulePositions(patch->getPolyVoiceArea());

    for (const auto& cable : pendingCables)
        connectLegacyCable(patch->getPolyVoiceArea(), cable.sourceModule, cable.sourceConn,
                           cable.sourceIsOutput, cable.targetModule, cable.targetInput);

    if (patch->getName() == "Init Patch")
        patch->setName(patchNameFromFileName(file));

    DBG("PchFileIO: loaded legacy \"" + patch->getName() + "\" from " + file.getFileName());
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

        // Only collect here — applied after all sections are parsed, so a
        // CustomDump appearing before its ModuleDump is not silently lost.
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
    writeCustomDump(out, patch.getPolyVoiceArea(), 1);
    writeCustomDump(out, patch.getCommonArea(), 0);
    writeNameDump(out, patch.getPolyVoiceArea(), 1);
    writeNameDump(out, patch.getCommonArea(), 0);

    // Which entry in the editor's extras library this patch is, so opening the
    // file again says outright which comments and variations belong to it, and
    // so a patch passed to another user takes its identity with it.
    if (patch.extrasId.isNotEmpty())
    {
        out += "[NME]\n";
        out += "Id=" + patch.extrasId + "\n";
        out += "[/NME]\n";
    }

    if (!patch.getComments().empty())
        writeComments(out, patch);

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
void PchFileIO::writeCustomDump(juce::String& out, const ModuleContainer& container, int voiceAreaId)
{
    out += "[CustomDump]\n";
    out += juce::String(voiceAreaId) + " \n";

    for (auto& m : container.getModules())
    {
        // Write the current values of the module's custom-class parameters
        // (sequencer events, clock-divider displays, ...). Parameters are
        // created 1:1 from the descriptor, so untouched modules still write
        // their defaults — but edits made since loading are preserved,
        // instead of reverting to the dump captured at load time.
        std::vector<int> customValues;
        for (auto& p : m->getParameters())
            if (p.getDescriptor()->paramClass == "custom")
                customValues.push_back(p.getValue());

        if (customValues.empty()) continue;

        out += juce::String(m->getContainerIndex()) + " "
             + juce::String(static_cast<int>(customValues.size()));
        for (auto v : customValues)
            out += " " + juce::String(v);
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

// --- Comments ---
// One line per note: section x y size text, with newlines escaped so a note
// spanning several lines still occupies exactly one line of the file.
//   [Comments]
//   1 0 12 2 kick: raise Decay\nif it sounds short
//   1 4 0 3x2 two columns wide, three rows tall
//   [/Comments]
//
// The size field is the row count on its own while the note is one column wide,
// and "rows x columns" once it is wider. That keeps the field a plain number for
// every note written before notes could be widened, and a reader that only knows
// the old form still gets the height right out of "3x2" (getIntValue stops at
// the x) instead of choking on the line.
void PchFileIO::writeComments(juce::String& out, const Patch& patch)
{
    out += "[Comments]\n";
    for (const auto& c : patch.getComments())
    {
        auto text = c.text.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "");
        juce::String size(c.gridHeight());
        if (c.gridWidth() > 1)
            size += "x" + juce::String(c.gridWidth());

        out += juce::String(c.section) + " " + juce::String(c.x) + " "
             + juce::String(c.y) + " " + size + " " + text + "\n";
    }
    out += "[/Comments]\n";
}

void PchFileIO::parseComments(const juce::StringArray& lines, Patch& patch)
{
    for (const auto& line : lines)
    {
        auto trimmed = line.trim();
        if (trimmed.isEmpty())
            continue;

        // Four numbers, then the rest of the line is the text (which may itself
        // contain spaces, so this cannot go through a plain tokenizer).
        juce::StringArray fields;
        auto rest = trimmed;
        for (int i = 0; i < 4; ++i)
        {
            auto space = rest.indexOfChar(' ');
            if (space < 0) { fields.clear(); break; }
            fields.add(rest.substring(0, space));
            rest = rest.substring(space + 1);
        }
        if (fields.size() != 4)
            continue;

        PatchComment c;
        c.section = juce::jlimit(0, 1, fields[0].getIntValue());
        c.x       = juce::jmax(0, fields[1].getIntValue());
        c.y       = juce::jmax(0, fields[2].getIntValue());
        c.height  = juce::jlimit(1, 128, fields[3].getIntValue());
        // "rows x columns", or just the rows for the one-column notes every file
        // written before notes could be widened holds.
        c.width   = fields[3].contains("x")
                        ? juce::jlimit(1, 40, fields[3].fromFirstOccurrenceOf("x", false, false)
                                                       .getIntValue())
                        : 1;
        // Unescape in one left-to-right pass. Two replace() calls would turn the
        // escaped backslash in "C:\\new" into a line break.
        juce::String text;
        for (int i = 0; i < rest.length(); ++i)
        {
            if (rest[i] == '\\' && i + 1 < rest.length())
            {
                const auto next = rest[i + 1];
                if (next == 'n')       { text += '\n'; ++i; continue; }
                if (next == '\\')      { text += '\\'; ++i; continue; }
            }
            text += rest[i];
        }
        c.text = text;
        patch.addComment(c);
    }
}

// --- Notes ---
void PchFileIO::writeNotes(juce::String& out, const Patch& patch)
{
    out += "[Notes]\n";
    out += patch.patchNotes + "\n";
    out += "[/Notes]\n";
}
