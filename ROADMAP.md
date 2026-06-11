# Animatek NME Roadmap

This roadmap is intentionally limited to real remaining implementation work. Completed features and
release history belong in [STATUS.md](STATUS.md) and [CHANGELOG.md](CHANGELOG.md).

## High Priority

- [x] **Bank Upload from Synth** — implemented in 0.6.0 as "Save Bank to Disk" plus
  "Backup All Banks to Library" (Device menu). Position metadata is preserved in the
  `NN - Name.pch` filename. Verified against real hardware.

- [x] **Bank Download to Synth** — implemented in 0.6.0 as "Send Bank to Synth"
  (Device menu), folder source, overwrite warning, stops cleanly on failure.
  Verified against real hardware.

- [x] **Controller Snapshot** — implemented in 0.6.0 as "Send Controller Snapshot"
  (Device menu). Research against the original protocol resolved the scope question:
  the `SendControllerSnapshot` command (sc=0x55) asks the *synth* to emit the current
  values of the patch's MIDI CC assignments as CC messages on its DIN MIDI OUT
  (sequencer recording aid, same as the front-panel CTRL SNAP SHOT menu); it does not
  modify synth state. Verified against real hardware.

## Editor Workflow

- [x] **Keyboard Floater** — implemented in 0.6.0 (View menu): virtual keyboard with octave
  navigation, DRONE latch mode, and REPEAT pulse mode (Rate 100-500 ms, Gate 20-400 ms).
  Notes go through the editor protocol (Note command sc=0x56, `{onOff, note}` with 0=on
  1=off — captured from the original Clavia editor and hardware-verified; the PC port
  ignores plain MIDI). Stuck MIDI IN notes are out of the PC port's reach (NoteEvent is
  incoming-only, CC 120/123 ignored): that is what the front-panel panic is for.

- [x] **Knob Floater** — implemented in 0.6.0 (View menu): 18 knobs + pedal/switch/aftertouch
  with assignment LEDs and module/parameter labels. Knobs are interactive (edit + sync +
  undo, morphs included); right-click reassigns to a free knob. Also fixed the special knob
  wire indices (Pedal=19, After touch=20, On/Off=22; 18/21 unused).

- [ ] **Patch Notes Floater**
  - Patch notes/comments window.
  - Decide whether notes are editor-only metadata or saved into a compatible sidecar file.

- [ ] **Window Management**
  - Remember main window size and position.
  - Remember floating window positions.
  - Restore sensible defaults if monitor layout changes.

## Search And Navigation

- [ ] **Module Search Tags**
  - Add tags such as bass, pad, utility, modulation, clock, random, sequencing, mixer, audio, logic.
  - Improve QuickAdd and module browser search ranking.
  - Keep tags close to module descriptors or generate them from a simple data file.

- [ ] **Keyboard Shortcuts Audit**
  - Compare current shortcuts with original editor expectations.
  - Add missing shortcuts only where they do not conflict with modern platform conventions.
  - Document final shortcut set in-app or in a concise docs file.

## Verification

- [ ] **Input/Output Connector Verification**
  - Audit connector direction and visual distinction across all modules.
  - Confirm input circles/output squares are correct against module descriptors and original editor behavior.
  - Track any module-specific corrections in `MODULE_CHECKLIST.md`.

- [ ] **Release Checklist**
  - Add a small repeatable checklist for tagged releases.
  - Include build targets, smoke tests, MIDI connection check, patch load/save check, and issue cleanup.

## Parked / Future

- [ ] **Community Patch Library**
  - Browse bundled or downloaded community patches from inside the preset browser.
  - Prefer bundled/offline packs first; live archive integration can come later.

- [ ] **Plugin Productization**
  - VST3/CLAP targets currently exist but are experimental.
  - Architecture notes live in [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md).
  - Treat this as a separate product track after the desktop editor is stable.
