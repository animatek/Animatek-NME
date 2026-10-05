#pragma once

#include <juce_core/juce_core.h>

// Where the editor is connected, for the status bar: a real G1 over MIDI or an
// emulator. Until G1-Emu has its direct link (G1-Emu#8) the only tell is the name
// of the port: the emulator's ports are called G1Emu, G1Emu 2, ... ("G1-Emu" too).
namespace SynthLink
{
    inline bool isEmulatorPort (const juce::String& portName)
    {
        const auto n = portName.toLowerCase().removeCharacters (" -_");
        return n.contains ("g1emu");
    }

    // "Real G1 (UM-ONE)", "G1-Emu (G1Emu 2)", or "Not connected" without a port.
    inline juce::String describe (bool connected, const juce::String& portName)
    {
        if (! connected)
            return "Not connected";
        const auto kind = isEmulatorPort (portName) ? "G1-Emu" : "Real G1";
        return portName.isEmpty() ? juce::String (kind) : juce::String (kind) + " (" + portName + ")";
    }
}
