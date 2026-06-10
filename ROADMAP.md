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

- [ ] **Keyboard Floater**
  - Virtual MIDI keyboard.
  - Octave navigation.
  - Drone/sustain mode.
  - Repeat mode if it proves useful.
  - Visual key feedback.

- [ ] **Knob Floater**
  - Hardware knob overview for 18 knobs, pedal, aftertouch, and on/off switch.
  - Show current module/parameter assignments.
  - Allow reassignment/removal from a dedicated view.
  - Include morph-related controls only if the workflow stays clear.

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
