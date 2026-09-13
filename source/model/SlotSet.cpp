#include "SlotSet.h"

namespace
{
constexpr const char* kFormatTag = "animatek-nme-slot-set";
const char* const kLetters[] = { "A", "B", "C", "D" };

juce::String modeName(SlotSet::Mode mode)
{
    switch (mode)
    {
        case SlotSet::Mode::Patch:   return "patch";
        case SlotSet::Mode::Disable: return "disable";
        case SlotSet::Mode::Keep:    break;
    }
    return "keep";
}

bool modeFromName(const juce::String& name, SlotSet::Mode& mode)
{
    if (name == "patch")   { mode = SlotSet::Mode::Patch;   return true; }
    if (name == "disable") { mode = SlotSet::Mode::Disable; return true; }
    if (name == "keep")    { mode = SlotSet::Mode::Keep;    return true; }
    return false;
}

int slotFromLetter(const juce::String& letter)
{
    for (int i = 0; i < 4; ++i)
        if (letter == kLetters[i])
            return i;
    return -1;
}

// createLegalFileName leaves leading dots (a hidden file) and trailing dots
// (which Windows silently drops) alone.
juce::String legalName(const juce::String& original)
{
    auto name = juce::File::createLegalFileName(original.trim()).trim();
    while (name.startsWithChar('.'))
        name = name.substring(1).trimStart();
    while (name.endsWithChar('.'))
        name = name.dropLastCharacters(1).trimEnd();
    return name;
}
}

bool SlotSet::changesAnything() const
{
    for (const auto& s : slots)
        if (s.mode != Mode::Keep)
            return true;
    return false;
}

int SlotSet::numPatches() const
{
    int n = 0;
    for (const auto& s : slots)
        if (s.mode == Mode::Patch)
            ++n;
    return n;
}

juce::String slotSetToJson(const SlotSet& set)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", kFormatTag);
    root->setProperty("version", SlotSet::kFormatVersion);
    root->setProperty("name", set.name);
    if (set.notes.isNotEmpty())
        root->setProperty("notes", set.notes);
    if (set.focusSlot >= 0 && set.focusSlot < 4)
        root->setProperty("focus", kLetters[set.focusSlot]);

    auto* slots = new juce::DynamicObject();
    for (int i = 0; i < 4; ++i)
    {
        const auto& s = set.slots[static_cast<size_t>(i)];
        auto* entry = new juce::DynamicObject();
        entry->setProperty("mode", modeName(s.mode));
        if (s.mode == SlotSet::Mode::Patch)
        {
            entry->setProperty("file", s.file);
            entry->setProperty("patchName", s.patchName);
            entry->setProperty("enabled", s.enabled);
            const int bank = slotSetBankNumber(s.bankSection, s.bankPosition);
            if (bank > 0)
                entry->setProperty("bank", bank);
        }
        slots->setProperty(kLetters[i], juce::var(entry));
    }
    root->setProperty("slots", juce::var(slots));
    return juce::JSON::toString(juce::var(root));
}

bool slotSetFromJson(const juce::String& text, SlotSet& set, juce::String& error)
{
    juce::var root;
    if (juce::JSON::parse(text, root).failed() || !root.isObject())
    {
        error = "Not a slot set: the manifest is not valid JSON";
        return false;
    }
    if (root["format"].toString() != kFormatTag)
    {
        error = "Not a slot set: unknown format";
        return false;
    }

    const auto version = root["version"];
    if (!(version.isInt() || version.isInt64() || version.isDouble()) || static_cast<int>(version) < 1)
    {
        error = "Not a slot set: no format version";
        return false;
    }
    if (static_cast<int>(version) > SlotSet::kFormatVersion)
    {
        error = "This slot set was saved by a newer editor (format "
              + juce::String(static_cast<int>(version)) + ")";
        return false;
    }

    SlotSet result;
    result.name = root["name"].toString().trim();
    result.notes = root["notes"].toString();

    const auto focus = root["focus"];
    if (!focus.isVoid())
    {
        result.focusSlot = slotFromLetter(focus.toString());
        if (result.focusSlot < 0)
        {
            error = "Unknown focus slot \"" + focus.toString() + "\"";
            return false;
        }
    }

    const auto slots = root["slots"];
    auto* slotsObject = slots.getDynamicObject();
    if (slotsObject == nullptr)
    {
        error = "Not a slot set: no slots";
        return false;
    }
    for (const auto& property : slotsObject->getProperties())
    {
        if (slotFromLetter(property.name.toString()) < 0)
        {
            error = "Unknown slot \"" + property.name.toString() + "\"";
            return false;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto entry = slots[kLetters[i]];
        if (entry.isVoid())
            continue;   // a slot the manifest leaves out is left alone

        const juce::String letter = kLetters[i];
        if (!entry.isObject())
        {
            error = "Slot " + letter + ": not an object";
            return false;
        }

        auto& s = result.slots[static_cast<size_t>(i)];
        if (!modeFromName(entry["mode"].toString(), s.mode))
        {
            error = "Slot " + letter + ": unknown mode \"" + entry["mode"].toString() + "\"";
            return false;
        }
        if (s.mode != SlotSet::Mode::Patch)
            continue;

        s.file = entry["file"].toString();
        if (!isSafeSlotSetFileName(s.file))
        {
            error = "Slot " + letter + ": the patch must be a .pch file inside the set's folder";
            return false;
        }
        s.patchName = entry["patchName"].toString();
        s.enabled = entry.hasProperty("enabled") ? static_cast<bool>(entry["enabled"]) : true;

        // Informational only, so a bad number is dropped rather than refused.
        const auto bank = entry["bank"];
        if (!bank.isVoid())
            slotSetBankFromNumber(static_cast<int>(bank), s.bankSection, s.bankPosition);
    }

    set = std::move(result);
    return true;
}

bool isSafeSlotSetFileName(const juce::String& fileName)
{
    if (fileName.length() <= 4 || fileName != fileName.trim())
        return false;
    if (!fileName.endsWithIgnoreCase(".pch"))
        return false;
    return !fileName.containsAnyOf("/\\:") && !fileName.startsWithChar('.');
}

juce::String slotSetPatchFileName(int slot, const juce::String& patchName)
{
    const juce::String letter = kLetters[juce::jlimit(0, 3, slot)];
    auto name = legalName(patchName);
    if (name.isEmpty())
        name = "Untitled";
    if (name != letter && !name.startsWith(letter + " "))
        name = letter + " " + name;
    return name + ".pch";
}

juce::String slotSetFolderName(const juce::String& setName)
{
    return legalName(setName);
}

int slotSetBankNumber(int section, int position)
{
    if (section < 0 || section > 8 || position < 0 || position > 98)
        return -1;
    return (section + 1) * 100 + position + 1;
}

bool slotSetBankFromNumber(int number, int& section, int& position)
{
    const int s = number / 100 - 1;
    const int p = number % 100 - 1;
    if (number < 101 || s > 8 || p < 0 || p > 98)
        return false;
    section = s;
    position = p;
    return true;
}

std::array<bool, 4> slotSetEnableMask(const SlotSet& set, const std::array<bool, 4>& current)
{
    auto mask = current;
    for (size_t i = 0; i < 4; ++i)
    {
        if (set.slots[i].mode == SlotSet::Mode::Patch)
            mask[i] = set.slots[i].enabled;
        else if (set.slots[i].mode == SlotSet::Mode::Disable)
            mask[i] = false;
    }
    return mask;
}

int slotSetFocusAfterLoad(const SlotSet& set, const std::array<bool, 4>& mask, int currentFocus)
{
    auto on = [&mask](int slot) { return slot >= 0 && slot < 4 && mask[static_cast<size_t>(slot)]; };
    if (on(set.focusSlot))
        return set.focusSlot;
    if (on(currentFocus))
        return currentFocus;
    for (int i = 0; i < 4; ++i)
        if (on(i))
            return i;
    // Nothing is left on. The synth will not switch its focused slot off, so
    // the honest answer is to stay where we are.
    return currentFocus;
}
