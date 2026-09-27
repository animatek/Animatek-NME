#include <doctest.h>
#include "mcp/McpRules.h"

// An MCP client names knobs in text. Getting a name wrong must fail loudly,
// never land on a neighbouring knob.

TEST_CASE("knobs are found by their panel names, loosely spelled")
{
    CHECK(McpRules::knobFromName("Knob 1") == 0);
    CHECK(McpRules::knobFromName("knob7") == 6);
    CHECK(McpRules::knobFromName("KNOB 18") == 17);
    CHECK(McpRules::knobFromName("Pedal") == 19);
    CHECK(McpRules::knobFromName("After touch") == 20);
    CHECK(McpRules::knobFromName("aftertouch") == 20);
    CHECK(McpRules::knobFromName("On/Off switch") == 22);
    CHECK(McpRules::knobFromName("on-off") == 22);

    // Every name the editor itself prints resolves back to its own index.
    for (int k = 0; k < KnobAssignmentMessage::numKnobs; ++k)
        if (KnobAssignmentMessage::isValidKnob(k))
            CHECK(McpRules::knobFromName(KnobAssignmentMessage::getKnobName(k)) == k);
}

TEST_CASE("ambiguous or impossible knob names are refused")
{
    CHECK_FALSE(McpRules::knobFromName("7").has_value());       // Knob 7, or index 7?
    CHECK_FALSE(McpRules::knobFromName("Knob 0").has_value());
    CHECK_FALSE(McpRules::knobFromName("Knob 19").has_value());
    CHECK_FALSE(McpRules::knobFromName("Knob").has_value());
    CHECK_FALSE(McpRules::knobFromName("Knob 7a").has_value());
    CHECK_FALSE(McpRules::knobFromName("(unused)").has_value());
    CHECK_FALSE(McpRules::knobFromName("").has_value());
}

TEST_CASE("assignment ranges match what the editor's own menus offer")
{
    CHECK(McpRules::isValidMorphGroup(0));
    CHECK(McpRules::isValidMorphGroup(3));
    CHECK_FALSE(McpRules::isValidMorphGroup(4));
    CHECK_FALSE(McpRules::isValidMorphGroup(-1));

    CHECK(McpRules::isValidMorphRange(-127));
    CHECK(McpRules::isValidMorphRange(127));
    CHECK_FALSE(McpRules::isValidMorphRange(128));

    CHECK(McpRules::isValidMidiCc(0));
    CHECK(McpRules::isValidMidiCc(119));
    CHECK_FALSE(McpRules::isValidMidiCc(120));
}

// The ports of a Linux machine with G1-Emu, a real G1 on a UM-ONE and a sound
// card, as ALSA names them. Several share "MIDI" and "MIDI 1", which is where a
// loose match could land on the wrong synth.
namespace
{
    const juce::StringArray portIds { "128-0", "128-1", "28-0", "32-0", "36-0" };
    const juce::StringArray portNames { "PC Port", "MIDI", "G1 MIDI 1", "UM-ONE MIDI 1", "Bitwig Connect MIDI" };
}

TEST_CASE("a port is found by its id, its name in any case, or a unique part of it")
{
    CHECK(McpRules::matchPort(portIds, portNames, "32-0") == juce::Array<int>{ 3 });
    CHECK(McpRules::matchPort(portIds, portNames, "pc port") == juce::Array<int>{ 0 });
    CHECK(McpRules::matchPort(portIds, portNames, "UM-ONE") == juce::Array<int>{ 3 });
    CHECK(McpRules::matchPort(portIds, portNames, "bitwig") == juce::Array<int>{ 4 });
}

TEST_CASE("an exact name wins over the ports that merely contain it")
{
    // "MIDI" is in four names; the port called exactly that is the one meant.
    CHECK(McpRules::matchPort(portIds, portNames, "midi") == juce::Array<int>{ 1 });
}

TEST_CASE("a part of a name several ports share is ambiguous, not a guess")
{
    CHECK(McpRules::matchPort(portIds, portNames, "MIDI 1") == juce::Array<int>{ 2, 3 });
    CHECK(McpRules::matchPort(portIds, portNames, "port").size() == 1); // only "PC Port"
}

TEST_CASE("an unknown or empty port name matches nothing")
{
    CHECK(McpRules::matchPort(portIds, portNames, "Nord").isEmpty());
    CHECK(McpRules::matchPort(portIds, portNames, "").isEmpty());
    CHECK(McpRules::matchPort(portIds, portNames, "  ").isEmpty());
}
