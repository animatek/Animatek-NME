#pragma once

#include "../protocol/KnobAssignmentMessage.h"
#include <juce_core/juce_core.h>
#include <optional>

// The limits an MCP client's assignment request is checked against before it
// reaches an undo action. The UI enforces these by only offering valid choices
// in its menus; a bridge client can type anything, so they are spelled out
// here, apart from the handler, where they can be tested without a window.
namespace McpRules
{
    constexpr int kNumMorphGroups = 4;
    // "There are four Morph groups available in each patch and you may assign up
    // to 25 different parameters" (Clavia's module help, ModuleHelpData.cpp).
    constexpr int kMaxMorphAssignments = 25;
    // Parameter::morphRange is a signed 8-bit span; the canvas drag clamps to it.
    constexpr int kMaxMorphRange = 127;
    // The parameter context menu offers CC 0-119; 120-127 are channel mode messages.
    constexpr int kMaxMidiCc = 119;
    // Morph values travel as ordinary parameter changes addressed to this module.
    constexpr int kMorphSection = 2;
    constexpr int kMorphModule = 1;

    inline bool isValidMorphGroup(int group) { return group >= 0 && group < kNumMorphGroups; }
    inline bool isValidMorphRange(int range) { return range >= -kMaxMorphRange && range <= kMaxMorphRange; }
    inline bool isValidMidiCc(int cc) { return cc >= 0 && cc <= kMaxMidiCc; }

    // The MIDI port a client means, among ports given as parallel id/name lists:
    // its id, its exact name in any case, or a part of its name that only one port
    // has. Returns every candidate, so exactly one is a match, none is "not found"
    // and several is "ambiguous": the caller refuses both instead of guessing,
    // because the wrong port can be the real G1 instead of G1-Emu.
    inline juce::Array<int> matchPort(const juce::StringArray& ids, const juce::StringArray& names,
                                      const juce::String& wanted)
    {
        if (wanted.trim().isEmpty())
            return {};
        for (int i = 0; i < ids.size(); ++i)
            if (ids[i] == wanted)
                return { i };
        for (int i = 0; i < names.size(); ++i)
            if (names[i].equalsIgnoreCase(wanted))
                return { i };
        juce::Array<int> partial;
        for (int i = 0; i < names.size(); ++i)
            if (names[i].containsIgnoreCase(wanted))
                partial.add(i);
        return partial;
    }

    // A knob by its panel name, as list_assignments reports it: "Knob 7",
    // "Pedal", "After touch", "On/Off switch". Case, spaces, hyphens,
    // underscores and slashes are ignored, so "knob7" and "aftertouch" work.
    // A bare number is refused on purpose: "7" could mean Knob 7 or index 7,
    // which is Knob 8, and guessing wrong assigns the wrong knob silently.
    inline std::optional<int> knobFromName(const juce::String& name)
    {
        const auto compact = name.toLowerCase().removeCharacters(" -_/");
        if (compact.isEmpty() || compact.containsOnly("0123456789"))
            return std::nullopt;

        if (compact.startsWith("knob"))
        {
            const auto digits = compact.fromFirstOccurrenceOf("knob", false, false);
            if (digits.isEmpty() || !digits.containsOnly("0123456789") || digits.length() > 2)
                return std::nullopt;
            const int number = digits.getIntValue();
            if (number < 1 || number > 18)
                return std::nullopt;
            return number - 1;
        }
        if (compact == "pedal")
            return 19;
        if (compact == "aftertouch")
            return 20;
        if (compact == "onoffswitch" || compact == "onoff" || compact == "switch")
            return 22;
        return std::nullopt;
    }
}
