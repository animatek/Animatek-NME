# Changelog

## 0.5.7

- Fixed Linux/JUCE MIDI device discovery.
  - Forced ALSA sequencer client mode to legacy MIDI 1.0 so hardware MIDI ports remain visible on systems with UMP support.
  - Preloads the ALSA endpoint cache so MIDI inputs/outputs appear immediately in the editor.
  - Sends bytestream MIDI to legacy/proxy ALSA ports instead of incorrectly using UMP output.
- Fixed slow startup and the X11 `BadLength` warning caused by the oversized app icon.
  - The window icon is now downscaled before being passed to X11.
- Improved JUCE build robustness on filesystems without executable bits.
  - Skips the `juceaide` runtime test only when the failure indicates a non-executable filesystem.
- Fixed Vocoder monitor switch instability (#10).
  - Shows the off state explicitly instead of displaying `Mon` for both positions.
  - Keeps binary switches and level controls out of parameter randomization so the monitor switch does not appear to toggle unexpectedly.
- Continued patch upload and sync reliability work.
  - Added upload ACK timeouts and short inter-section delays.
  - Added clearer upload section diagnostics.
  - Kept full-patch uploads in the original 16-section Java order for better firmware compatibility.
  - Treats `NewPatchInSlot` as an ACK-compatible reply for structural edit messages.
- Fixed `.pch` loading/upload to hardware slots (#1).
  - Focuses the active synth slot before uploading a patch opened from disk.
  - Reports disk patch upload progress and completion in the status bar.
- Fixed `.pch` serialization for newly created patches.
  - Always writes the expected empty morph, keyboard, knob, and MIDI control sections.
  - Matches the PDL2 7-bit `MasterOsc.kbt` field so the `ParameterDump` no longer shifts and the synth does not reject fresh patches with `ERROR`.

## 0.5.6

- Added integrated disk preset browser.
  - `Ctrl+B` opens the right panel on the `Disk` tab.
  - Searches local patches and snippets.
  - Filters by `All`, `Patches`, and `Snippets`.
  - Recursively scans `.pch` files.
  - Double-click loads patches into the current slot.
  - Double-click or drag imports snippets into the canvas.
- Added configurable user preset library root in Editor Options.
  - Creates `Patches/` and `Snippets/` subfolders automatically.
  - Keeps full patches separate from reusable snippets.
- Split the right browser into `Synth` and `Disk` tabs.
- Improved snippet workflow for reusable modular building blocks.
- Improved contextual module help from `F1`.
- Added dark-mode app icon.
- Improved plugin and UI stability.
  - Improved VST3 plugin configuration for DAW recognition.
  - Fixed VST3 close crash.
  - Moved more hardcoded colors into `ColorScheme`.
  - Improved bank operations and snippet synchronization.
  - Fixed duplicate/incorrect cable cases in copy, paste, duplicate, and snippet import.
- Improved patch loading, serialization, and SysEx reliability.
  - Better cable detection and normalization when importing `.pch` files.
  - Duplicate connection prevention in the patch model.
  - Clearer patch upload logs.
  - ACK timeouts to prevent indefinite upload hangs.
  - Short upload delays so the Nord Modular can keep up.
  - Split `ParameterDump` uploads by module for better synth compatibility.
  - MIDI queue handles `NewPatchInSlot` responses during upload.

## 0.5.5

- Improved synth patch browser reliability.
- Fixed double-click patch loading into the active A/B/C/D slot.
- Added stale load protection.
- Fixed bank copy, move, delete, and store operations using the wrong selected slot/bank/position.
- Hardened snippet import/export.
  - `.pch` snippets sync incrementally.
  - Connector direction is preserved.
  - Singleton modules such as `KeyboardPatch` and `MIDIGlobal` are filtered.
- Cleaned up Device menu by removing the confusing manual "Upload to Active Slot" command.

## 0.5.2

- Refined module display formatting.
  - MasterOsc, OscA/B, FormantOsc, FilterA/B now show useful Hz/kHz values.
  - LfoB square waveform renders vertical transitions correctly.
  - Phaser center-frequency knob now drives text and display position.
  - Sample&Hold trig connector uses logic signal color.
- Improved multi-option buttons so clicks jump directly to the clicked segment.
- Added sequencer random buttons for EventSeq and NoteSeqA.
- Added KeyQuantizer scale presets.
- Completed major visual passes for oscillator/LFO, envelope, filter/EQ/vocoder, and sequencer modules.

## Earlier Milestones

- Native JUCE/C++ app setup with CMake.
- MIDI SysEx protocol implementation and synth connection manager.
- Patch retrieval, parsing, serialization, upload, and `.pch` file I/O.
- Module browser, patch canvas, inspector, status bar, and main layout.
- Pixel-oriented module rendering using classic theme data.
- Real-time VU meters, LEDs, parameters, morphs, hardware knob assignments, and MIDI CC assignments.
- Patch Settings and Synth Settings dialogs.
- Synth patch browser with bank operations.
- Multi-slot support for A/B/C/D.
- Undo/redo system for patch editing.
- QuickAdd, multi-selection, copy/paste, duplicate, cable tools, zoom, randomize, initialize, parameter locks, and snapshots.
- Help system and About/Help links.
