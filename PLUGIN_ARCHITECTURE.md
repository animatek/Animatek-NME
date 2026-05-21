# Plugin Architecture Notes

These are parked notes for the experimental VST3/CLAP direction. They are not the main desktop
editor roadmap.

## Dual MIDI Design

The Nord Modular has two independent MIDI port pairs:

- **MIDI standard**: notes, CC, and performance data.
- **PC MIDI**: dedicated SysEx traffic for editor communication.

These are hardware-independent and can operate simultaneously without interference.

## Plugin MIDI Flow

```text
DAW automation lanes -> VST params -> SysEx PC port -> Nord Modular
DAW MIDI track       -> MIDI standard             -> Nord Modular
                       notes and CC
```

- The plugin opens the PC MIDI port directly via `juce::MidiInput` and `juce::MidiOutput`.
- The DAW sends notes and CC to the Nord through normal MIDI routing.
- Both paths should stay independent.

## Parameter Automation Options

### Option A: VST Parameter Slots

- Expose module parameters as native VST/CLAP parameters.
- DAW automates them through automation lanes.
- Plugin translates changes to SysEx parameter changes over the PC port.
- Challenge: patch parameters are dynamic.
- Possible solution: fixed pool of parameter slots mapped dynamically per patch.

### Option B: MIDI CC Passthrough

- Plugin receives MIDI CC from the DAW input bus.
- Maps CCs to Nord parameters and sends SysEx.
- More flexible, but harder to configure clearly per patch.

### Option C: Knobs And Morphs Only

- Expose only hardware knobs, pedal/aftertouch/switch, and morph sources as plugin parameters.
- Simpler and closer to the hardware workflow.
- Users assign knobs/morphs in the editor and automate those in the DAW.

## Known Context

- VST3/CLAP build targets exist.
- The desktop editor remains the primary supported product.
- Plugin work should be treated as a separate productization track after the editor is stable.
