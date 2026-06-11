# Changelog

## 0.6.0 (in development)

- Added **CI binary builds** (`.github/workflows/build-binaries.yml`): a manually
  triggered GitHub Actions workflow builds Release binaries for Linux, Windows, and
  macOS (universal arm64+x86_64, ad-hoc signed) and uploads them as short-lived
  artifacts, optionally wrapped in a password-protected zip (`ARTIFACT_PASSWORD`
  secret). Each package ships a platform README covering Gatekeeper/SmartScreen
  first-launch steps. Binaries are distributed through Patreon — no public GitHub
  Releases.

- **Connector I/O audit** (results in `MODULE_CHECKLIST.md`): all 382 themed connectors
  map 1:1 to the module descriptors and input-circle/output-square shapes are derived from
  the descriptors, so direction is correct across all 110 modules. Fixed 43 jacks whose
  theme color disagreed with their signal (e.g. the ADSR gate jack was audio-red): jacks
  are now colored from the descriptor's signal type, always matching the cables plugged
  into them.
- Added `RELEASE_CHECKLIST.md`: repeatable release checklist (version bumps, build
  targets, no-synth smoke tests, hardware tests, packaging, post-release).

- **Keyboard shortcuts audit**: added `Ctrl+A` (select all in section), `Ctrl+X` (cut),
  `Escape` (clear selection), arrow keys (nudge selected modules one grid cell, undoable),
  `Ctrl+Shift+S` (Save As), and `Ctrl+1..4` (switch slot A-D). The `S` shake-cables key the
  View menu advertised was never implemented — now it is. Full reference in
  [SHORTCUTS.md](SHORTCUTS.md) and in-app under **Help → Keyboard Shortcuts**.
- Fixed switching slots from the editor (slot bar or `Ctrl+1..4`) not loading the slot's
  patch: the immediate patch request raced the slot-command ACKs (the SlotsSelected ACK was
  mistaken for the patch-request ACK and the fetch derailed). The editor now lets the
  synth's SlotActivated echo trigger the load — the same path as front-panel slot changes —
  with a delayed fallback request if no echo arrives.

- Added **module search tags**: all 110 modules carry hand-written synonyms (lp, hp, vca,
  s&h, glide, bitcrush, wah, acid, arpeggio, sidechain...) searched by Quick Add and the
  module browser filter. Quick Add results are now ranked by relevance (name prefix >
  name > full name > category/tags) instead of flat category order.
- **Double-click on empty canvas** now opens the Quick Add popup (same as Enter);
  double-clicking a module does not.
- **Quick Add is now fully mouse-driven**: hover highlights, click adds the module,
  clicking outside dismisses, and the mouse wheel scrolls the full module list (with a
  scrollbar indicator). Arrow keys scroll the view too.
- **Quick Add favorites**: click the star on a row to pin a module as favorite (gold
  star). Favorites are listed first — also ahead of equal-relevance search results —
  and persist across sessions.

- Added the **Patch Notes Floater** (View menu): free-text notes for the active slot's patch
  in a resizable window (monospaced — original patches often carry ASCII tables). Notes are
  saved into the `.pch` `[Notes]` section and round-trip through load/save. Note this is a
  Nomad/nmedit extension: the original Clavia editor ignores the section and never persists
  its own notes, so ours survive where the original's did not.
- The main window now remembers its size, position, and maximized state across sessions,
  and is clamped back on-screen if the monitor layout changed. Floaters are restored to the
  display they were last on instead of being forced onto the primary monitor, and resizable
  floaters also remember their size.

- Added the **Knob Floater** (View menu): live overview of the 18 assignable hardware knobs
  plus Pedal, On/Off switch, and After touch, with assignment LEDs and module/parameter
  labels. Knobs are interactive — dragging one edits the assigned parameter (with undo and
  synth sync, morph assignments included), and right-click moves an assignment to a free
  knob slot. Follows hardware knob turns, canvas edits, undo/redo, slot switches, and
  patch loads in real time.
- Added the **Keyboard Floater** (View menu): virtual keyboard that plays the synth through
  the editor protocol (the PC port ignores regular MIDI notes), with octave navigation, a
  DRONE latch mode, and a REPEAT pulse mode with Rate (100-500 ms) and Gate (20-400 ms)
  sliders — one short note per tick while a key is held or a drone note is latched. Note
  on/off encoding (Note command sc=0x56: `{onOff, note}`, 0=on 1=off) was captured from the
  original Clavia editor and is verified against hardware. Stuck notes coming from the
  synth's own MIDI IN cannot be released through the PC port (NoteEvent is incoming-only
  and channel messages are ignored) — use the front-panel panic for those.
- Fixed synth error 5 ("no slot focused") on patch-modification commands sent after the
  synth bumped its patch id: every plain ACK now resyncs the editor's pid, mirroring the
  original editor's ActivePidListener.
- Both floaters remember position and open state across sessions.
- Fixed wrong wire indices for the special knob targets: Pedal, After touch, and On/Off
  switch are indices 19, 20, and 22 in the protocol and patch format (18 and 21 are unused),
  but the assignment menus sent 18-20 — so "Pedal" assignments from the editor never
  responded to the real expression pedal. All knob menus and the inspector now use the
  correct indices.

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
