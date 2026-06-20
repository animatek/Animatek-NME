# Animatek NME 0.7.0

Animatek NME 0.7.0 adds sound breeding, persistent patch variations, protocol diagnostics,
and a major visual customization update. Parameter delivery has also been hardened so rapid
Mutator use and patch transitions do not leave the Nord Modular unresponsive.

## Highlights

- **Patch Mutator** inspired by the Nord Modular G2 workflow: Mutate, Randomize, Interpolate,
  sequential/independent Cross, Quick Locks, temporary storage, and timed auditioning.
- **Eight persistent variations** per patch, including morph values, copy/init actions, and
  interpolation. Variation data is stored in a standard-adjacent `.var` sidecar; `.pch` files
  remain compatible with the original editors.
- **SysEx Monitor** with live TX/RX capture, timestamps, filters, copy, and clear controls.
- **Configurable synth send speed** with Safe, Balanced, Fast, and Maximum presets. Balanced
  remains the recommended default.
- **Thirteen editor themes**, with Nord as the default, plus persistent wireframe module mode.
- Distinct Patch, Snippet, and Bank labels in the preset browser.
- Floaters behave as normal windows and can be toggled with `Ctrl+5` through `Ctrl+9`.

## Stability

- Bulk parameter changes now use one coalescing queue instead of overlapping send chains.
- Pending changes are discarded before slot switches, patch requests, bank loads, failed
  uploads, and disconnects.
- Active interpolation stops before changing slot or replacing a patch, preventing parameter
  spill into another hardware slot.
- Parameter edits made during a full patch upload wait until it completes; parameter traffic
  is suppressed during patch fetch streams.

## Shortcuts added

- `Ctrl+T`: cycle editor theme.
- `Ctrl+W`: toggle wireframe modules.
- `Ctrl+8`: Patch Mutator.
- `Ctrl+9`: SysEx Monitor.

See [SHORTCUTS.md](SHORTCUTS.md) for the complete keyboard reference.

## Known limitations

- The desktop editor is the supported target. VST3 and CLAP builds remain experimental.
- Keep a patch's `.var` file beside its `.pch` file to preserve variations and mutation
  exclusions when moving it between computers or folders.
- Fast and Maximum send speeds depend on the MIDI interface and patch size. Use Balanced if
  the editor feels uneven or the synth stops responding reliably.
- Stuck external MIDI notes must be cleared with the Nord Modular's front-panel panic function.
