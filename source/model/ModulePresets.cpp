#include "ModulePresets.h"
#include "Patch.h"

// ─── .pchp pack format ───────────────────────────────────────────────────────
//
//   # Animatek NME module preset pack
//   format 1
//   module DrumSynth
//
//   [Basic Kick]
//   p1 = 60      ; MTune
//   p2 = 0       ; STune
//
// Deliberately line-based and commented rather than XML or JSON: the original
// editor's preset values have to be transcribed by hand from screenshots, and
// this is the shape that is pleasant to type and to diff. Unknown keys and
// malformed lines are skipped rather than failing the file, so a pack edited by
// hand degrades to "the presets that parsed" instead of to nothing.

static constexpr int packFormatVersion = 1;

static juce::String sanitiseForFilename(const juce::String& s)
{
    return s.retainCharacters("abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");
}

void ModulePresetLibrary::setFolder(const juce::File& presetsFolder)
{
    folder = presetsFolder;
    reload();
}

void ModulePresetLibrary::addBuiltIn(ModulePreset preset)
{
    preset.builtIn = true;
    auto& list = byType[preset.moduleType];
    // Built-ins stay ahead of user presets, which are appended on load.
    auto firstUser = std::find_if(list.begin(), list.end(),
                                  [](const ModulePreset& p) { return !p.builtIn; });
    list.insert(firstUser, std::move(preset));
}

const std::vector<ModulePreset>& ModulePresetLibrary::forType(const juce::String& moduleType) const
{
    static const std::vector<ModulePreset> none;
    auto it = byType.find(moduleType);
    return it == byType.end() ? none : it->second;
}

const ModulePreset* ModulePresetLibrary::find(const juce::String& moduleType, int index) const
{
    const auto& list = forType(moduleType);
    if (index < 0 || index >= static_cast<int>(list.size()))
        return nullptr;
    return &list[static_cast<size_t>(index)];
}

juce::String ModulePresetLibrary::suggestName(const juce::String& moduleType) const
{
    const auto& list = forType(moduleType);
    for (int n = 1; n < 1000; ++n)
    {
        auto candidate = moduleType + " " + juce::String(n);
        const bool taken = std::any_of(list.begin(), list.end(),
            [&candidate](const ModulePreset& p) { return p.name.equalsIgnoreCase(candidate); });
        if (!taken)
            return candidate;
    }
    return moduleType;
}

int ModulePresetLibrary::add(ModulePreset preset)
{
    if (!canSave() || preset.moduleType.isEmpty())
        return -1;

    preset.builtIn = false;
    auto& list = byType[preset.moduleType];
    list.push_back(std::move(preset));

    const auto type = list.back().moduleType;
    if (!writePack(type))
    {
        list.pop_back();
        return -1;
    }
    return static_cast<int>(list.size()) - 1;
}

bool ModulePresetLibrary::remove(const juce::String& moduleType, int index)
{
    auto it = byType.find(moduleType);
    if (it == byType.end() || index < 0 || index >= static_cast<int>(it->second.size()))
        return false;

    auto& list = it->second;
    if (list[static_cast<size_t>(index)].builtIn)
        return false;   // the editor's own data, not the user's

    auto removed = list[static_cast<size_t>(index)];
    list.erase(list.begin() + index);
    if (!writePack(moduleType))
    {
        list.insert(list.begin() + index, std::move(removed));
        return false;
    }
    return true;
}

bool ModulePresetLibrary::rename(const juce::String& moduleType, int index, const juce::String& newName)
{
    auto it = byType.find(moduleType);
    if (it == byType.end() || index < 0 || index >= static_cast<int>(it->second.size()))
        return false;

    auto& preset = it->second[static_cast<size_t>(index)];
    if (preset.builtIn || newName.trim().isEmpty())
        return false;

    auto oldName = preset.name;
    preset.name = newName.trim();
    if (!writePack(moduleType))
    {
        preset.name = oldName;
        return false;
    }
    return true;
}

ModulePreset ModulePresetLibrary::capture(const Module& m, juce::String name)
{
    ModulePreset preset;
    preset.name = std::move(name);

    auto* desc = m.getDescriptor();
    if (desc == nullptr)
        return preset;
    preset.moduleType = desc->name;

    for (const auto& param : m.getParameters())
    {
        auto* pd = param.getDescriptor();
        // "morph" parameters are the twins carrying a morph amount and "custom"
        // ones are controls rather than settings; neither belongs in a preset.
        if (pd == nullptr || pd->paramClass != "parameter" || pd->componentId.isEmpty())
            continue;
        preset.values[pd->componentId] = param.getValue();
    }
    return preset;
}

// ─── Files ───────────────────────────────────────────────────────────────────

juce::File ModulePresetLibrary::packFile(const juce::String& moduleType) const
{
    if (folder == juce::File())
        return {};
    return folder.getChildFile(sanitiseForFilename(moduleType) + ".pchp");
}

bool ModulePresetLibrary::writePack(const juce::String& moduleType)
{
    auto file = packFile(moduleType);
    if (file == juce::File())
        return false;

    const auto& list = forType(moduleType);

    // A type whose user presets have all been deleted leaves no pack behind.
    const bool anyUser = std::any_of(list.begin(), list.end(),
                                     [](const ModulePreset& p) { return !p.builtIn; });
    if (!anyUser)
    {
        if (file.existsAsFile())
            file.deleteFile();
        return true;
    }

    folder.createDirectory();

    juce::String text;
    text << "# Animatek NME module preset pack\n"
         << "format " << packFormatVersion << "\n"
         << "module " << moduleType << "\n";

    for (const auto& preset : list)
    {
        if (preset.builtIn)
            continue;
        text << "\n[" << preset.name << "]\n";

        // Written in parameter order rather than the map's alphabetical one,
        // which would file p10 between p1 and p2 and make a pack transcribed by
        // hand painful to check against the module in front of you.
        std::vector<std::pair<juce::String, int>> ordered(preset.values.begin(),
                                                          preset.values.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& a, const auto& b)
                  {
                      const int na = a.first.retainCharacters("0123456789").getIntValue();
                      const int nb = b.first.retainCharacters("0123456789").getIntValue();
                      if (na != nb) return na < nb;
                      return a.first < b.first;
                  });

        for (const auto& [componentId, value] : ordered)
            text << componentId << " = " << value << "\n";
    }

    return file.replaceWithText(text);
}

void ModulePresetLibrary::readPack(const juce::File& file)
{
    juce::StringArray lines;
    file.readLines(lines);

    juce::String moduleType;
    ModulePreset current;
    bool haveCurrent = false;

    auto flush = [this, &current, &haveCurrent]()
    {
        if (haveCurrent && current.name.isNotEmpty() && current.moduleType.isNotEmpty())
            byType[current.moduleType].push_back(current);
        haveCurrent = false;
    };

    for (auto rawLine : lines)
    {
        auto line = rawLine.upToFirstOccurrenceOf(";", false, false)
                           .upToFirstOccurrenceOf("#", false, false).trim();
        if (line.isEmpty())
            continue;

        if (line.startsWith("[") && line.endsWith("]"))
        {
            flush();
            current = ModulePreset{};
            current.name = line.substring(1, line.length() - 1).trim();
            current.moduleType = moduleType;
            haveCurrent = true;
            continue;
        }

        if (line.startsWithIgnoreCase("module "))
        {
            // The header names the type; the filename is only a convenience.
            moduleType = line.fromFirstOccurrenceOf(" ", false, false).trim();
            continue;
        }
        if (line.startsWithIgnoreCase("format "))
            continue;

        if (!haveCurrent)
            continue;

        auto key = line.upToFirstOccurrenceOf("=", false, false).trim();
        auto val = line.fromFirstOccurrenceOf("=", false, false).trim();
        if (key.isEmpty() || val.isEmpty() || !line.containsChar('='))
            continue;
        current.values[key] = val.getIntValue();
    }
    // flush() drops any preset that has no module type, so a pack missing its
    // header contributes nothing rather than creating a bogus type.
    flush();
}

void ModulePresetLibrary::reload()
{
    // Built-ins are registered in code, not read from disk, so they survive.
    for (auto& [type, list] : byType)
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [](const ModulePreset& p) { return !p.builtIn; }),
                   list.end());

    if (folder == juce::File() || !folder.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(folder, false, "*.pchp",
                                                           juce::File::findFiles))
        readPack(entry.getFile());
}

void ModulePresetLibrary::migrateLegacyDrumPresets(const juce::File& legacyFile)
{
    if (!canSave() || !legacyFile.existsAsFile())
        return;

    auto target = packFile("DrumSynth");
    if (target == juce::File() || target.existsAsFile())
        return;

    // Old format: one preset per line, "name|v1,v2,...,v15" positionally p1..p15.
    juce::StringArray lines;
    legacyFile.readLines(lines);

    int migrated = 0;
    for (const auto& line : lines)
    {
        if (line.trim().isEmpty())
            continue;
        auto parts = juce::StringArray::fromTokens(line, "|", "");
        if (parts.size() < 2)
            continue;

        ModulePreset preset;
        preset.name = parts[0].trim();
        preset.moduleType = "DrumSynth";
        auto values = juce::StringArray::fromTokens(parts[1], ",", "");
        for (int i = 0; i < values.size(); ++i)
            preset.values["p" + juce::String(i + 1)] = values[i].getIntValue();

        if (preset.name.isNotEmpty() && !preset.values.empty())
        {
            byType["DrumSynth"].push_back(std::move(preset));
            ++migrated;
        }
    }

    if (migrated > 0)
        writePack("DrumSynth");
}
