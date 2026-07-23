# Animatek NME Roadmap

This roadmap is intentionally limited to real remaining implementation work. Completed features and
release history belong in [STATUS.md](STATUS.md) and [CHANGELOG.md](../CHANGELOG.md).

## High Priority

- [ ] **Module Icon Bar** ([#17](https://github.com/animatek/Animatek-NME/issues/17)) —
  reinstate the original editor's drag-and-drop bar of module icons, hideable via a View
  toggle for users who prefer the text browser or Quick Add. The complete nmedit icon set
  (109 modules, 16x16 and 32x32, keyed by `modules.xml` `index`) already exists locally and
  needs no redrawing, and `PatchCanvas` already accepts the exact drag payload
  `ModuleBrowserPanel` emits. Design notes, asset paths and open questions:
  [MODULE_ICON_BAR.md](MODULE_ICON_BAR.md).

- [ ] **Slot selection dialog on patch load**
  ([#21](https://github.com/animatek/Animatek-NME/issues/21)) — the original editor asks
  which slot an opened `.pch` goes to, listing A/B/C/D with each slot's current patch name
  (`Unknown` for slots it has not fetched yet) plus a separate **Local** option that loads
  into the editor without touching the synth. ANME has neither: a file load always targets
  the active slot and always uploads when connected (`MainComponent.cpp:1618`), so there is
  no way to open a patch without overwriting synth state, and no way to load into a slot
  you have not visited. Reference screenshots (gitignored):
  `Implementaciones/Dialogo de carga de slots selection.png`.

- [ ] **Slot windows: live fan-out and global commands**
  ([#22](https://github.com/animatek/Animatek-NME/issues/22)) — editing *from* a slot window
  is hardware-verified, but front-panel knob moves, lights and meters never reach it
  (milestone 5 of the multi-window plan, `MainComponent.cpp:2325`), and `Ctrl+R` /
  Mutator / snapshots silently do nothing there (`MainComponent.cpp:2406`).

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

- [x] **Patch Notes Floater** — implemented in 0.6.0 (View menu): resizable monospaced
  notes window bound to the active slot's patch. Notes persist in the `.pch` `[Notes]`
  section (a Nomad/nmedit extension; the original Clavia editor ignores it), so no sidecar
  file is needed.

- [x] **Window Management** — implemented in 0.6.0: main window size/position/maximized
  state persists (clamped on-screen if the monitor layout changes); floaters restore to
  the display they were last on, and resizable floaters remember their size.

## Search And Navigation

- [x] **Module Search Tags** — implemented in 0.6.0: hand-written tag table for all 110
  modules (`source/model/ModuleTags.cpp`, kept next to the descriptors without touching
  the third-party modules.xml), searched by Quick Add and the module browser filter.
  Quick Add ranks results by relevance (name prefix > name > full name > category/tags),
  and double-clicking empty canvas opens Quick Add like Enter does.

- [x] **Keyboard Shortcuts Audit** — done in 0.6.0: compared against the original
  nmedit/Nomad editor and added the missing set (Ctrl+A/X, Escape, arrow-key nudge,
  Ctrl+Shift+S, Ctrl+1..4 slot switch, S shake cables). Documented in
  [SHORTCUTS.md](../manual/07-shortcuts.md) and in-app (Help → Keyboard Shortcuts). The audit also
  surfaced and fixed editor-initiated slot switches not loading the slot's patch.

## Verification

- [x] **Input/Output Connector Verification** — done in 0.6.0 (results in
  `MODULE_CHECKLIST.md`): automated cross-check of theme vs descriptors. Structure and
  direction (circles/squares) correct across all 110 modules; 43 jack colors disagreed
  with their signal type and are fixed by coloring jacks from the descriptor signal,
  matching the cables.

- [x] **Release Checklist** — done in 0.6.0: see [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)
  (version bumps, build targets, no-synth smoke tests, hardware tests, packaging,
  post-release issue sweep).

## Reported Bugs

Tracked as GitHub issues; the detail lives there.

- [x] **Resource usage reads a flat 100%** ([#18](https://github.com/animatek/Animatek-NME/issues/18))
  — already fixed after 0.9.0 and shipped in 0.10.0 (one-decimal Load meters); reported
  against an older build. Closed.

- [ ] **Theme submenu checkmark sticks on the initial theme**
  ([#19](https://github.com/animatek/Animatek-NME/issues/19)) — a fix shipped in 0.10.0, but
  it is left open until verified in a real session. Note the original report came from macOS,
  where the native menu bar handles ticks differently from the in-window menu on Linux, so
  confirming it on Linux alone does not close it.

## Parked / Future

- [ ] **Community Patch Library — standby / decision pending**
  - No editor integration or release target is currently planned.
  - A private bootstrap repository exists at
    `animatek/Animatek-NME-Community-Patches`, with CC0 documentation but no patches.
  - Reassess the value, curation workload, submission flow, and maintenance cost before
    making the repository public or implementing downloads in the preset browser.

- [ ] **Plugin Productization**
  - VST3/CLAP targets currently exist but are experimental.
  - Architecture notes live in [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md).
  - Treat this as a separate product track after the desktop editor is stable.
