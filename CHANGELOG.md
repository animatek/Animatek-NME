# Changelog

## 0.6.0 (in development)

- Added **Save Bank to Disk** (Device menu): saves every non-empty patch of a synth bank as
  `NN - Name.pch` files into a chosen folder, with progress, per-patch failure reporting,
  cancellation, and a selectable temp slot used as the transfer buffer.
- Added **Send Bank to Synth** (Device menu): uploads a folder of `.pch` files (sorted by name)
  into a destination bank at positions 1-N, with an explicit overwrite warning, progress,
  cancellation, and a clean stop on the first upload failure.
- Added **Backup All Banks to Library** (Device menu): mirrors all 9 synth banks into
  `Banks/Bank1`-`Banks/Bank9` inside the preset library (next to `Patches/` and `Snippets/`).
  Existing `.pch` files in those folders are replaced so each folder exactly reflects the synth.
- Added a **Banks** filter to the disk preset browser. Bank backups are listed as
  `BankN/NN - Name`, searchable, and double-click loads them like any patch.
- Fixed bank sends failing on upload: each patch upload now focuses the temp slot first
  (`SlotsSelected`/`SlotActivated` + settle delay, same as the single-file disk upload path),
  and the next upload waits for the previous `StorePatch` ACK to drain so it is not
  discarded when the upload clears the ACK queue.
- Added **Send Controller Snapshot** (Device menu): asks the synth to transmit the current
  values of all MIDI-CC-assigned parameters of the active patch as Control Change messages
  on its **DIN MIDI OUT** (not the PC port) — record it at the start of a sequencer track so
  playback initializes the patch state. Equivalent to the synth's front-panel CTRL SNAP SHOT
  menu. Read-only; verified against hardware.
- Fixed the editor going deaf ("lost connection") after changing banks/patches on the synth
  front panel: an unanswered patch request left the fetch state stuck forever, ignoring every
  later patch change. A 3 s watchdog now resets it.
- Fixed the synth freezing when loading a preset from the Synth browser while the patch list
  was still streaming: patch loads/fetches/uploads now cancel the in-flight list fetch first.
- Fixed garbled synth settings names (`????...`) and stalled patch loads: the heuristic
  SynthSettings decode no longer runs on packets that belong to a streaming patch fetch, so
  it can no longer swallow patch sections.
- Fixed patch list corruption: duplicate refreshes no longer interleave two response streams,
  a cancelled fetch gets a 400 ms cooldown before restarting, and 16-character patch names
  (which carry no null terminator) no longer merge with the following entries and shift every
  later bank position.
- Fixed Send Bank to Synth storing filename-derived names like "86 - DoubleSawPa": the
  "NN - " backup position prefix is stripped from patch names before upload.
- Added a patch-load progress bar to the status bar (sections received while fetching from
  the synth), with automatic hiding on completion or stall.
- Made verbose MIDI logging opt-in via the `NME_MIDI_LOG=1` environment variable. Hex-dumping
  every received SysEx and per-entry patch list logging noticeably slowed transfers in debug
  builds.
- Renamed the project from **Nomad2026** to **Animatek NME — Nord Modular Editor G1**.
  - New app name, window title, CMake targets, and binary name (`AnimatekNME`).
  - New plugin identifiers (manufacturer `Antk`, code `Nme1`, CLAP id `com.animatek.nme`). DAWs will
    see the experimental plugin as a new plugin.
  - Settings and drum presets stored under the old `Nomad2026` name are migrated automatically.
  - Added a Clavia non-affiliation notice (Nord Modular is a trademark of Clavia DMI AB).

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
