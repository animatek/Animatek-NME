# Animatek NME Roadmap

This roadmap is intentionally limited to real remaining implementation work. Completed features and
release history belong in [STATUS.md](STATUS.md) and [CHANGELOG.md](../CHANGELOG.md).

## High Priority

- [x] **Module Icon Bar** ([#17](https://github.com/animatek/Animatek-NME/issues/17)) —
  shipped as the module bar under the header: category tabs with the modules of the chosen
  category, dragged or clicked onto a patch area, hideable via View. Issue closed
  2026-08-09. The chips carry names rather than pictograms on purpose; the artwork is
  [#52](https://github.com/animatek/Animatek-NME/issues/52), deliberately parked.

- [x] **Slot selection dialog on patch load**
  ([#21](https://github.com/animatek/Animatek-NME/issues/21)) — shipped
  (`source/ui/SlotSelectDialog.h`). Issue closed 2026-07-24.

- [x] **MCP bridge: save, store and rename from a client**
  ([#23](https://github.com/animatek/Animatek-NME/issues/23)) — shipped: `save_patch`,
  `store_to_bank` and `rename_module` all exist. Issue closed 2026-07-24.

- [x] **`replacePatchInSlot` read the patch it had just freed** — fixed. All four paths
  that destroy a patch (the synth fetch, `replacePatchInSlot`, `newPatch`, and the disk
  load) detached the Inspector with `clearModule()`, which keeps its own `currentPatch`
  and re-arms `assignmentsList->setPatchWide()` with it — the very patch the next line
  destroys. `clearSnapshots()` then walked that list and read freed memory; it survived
  only because the freed container's module slots read back as null. They now detach with
  `setPatch(nullptr)`, the only one that drops both pointers, and all four re-point at the
  incoming patch further down as they already did. The `clearModule()` calls that remain
  (`switchToSlot`, `prepareSlotModuleDeletion`, the selection change) are not
  detach-before-destroy: their patch outlives the call.

- [ ] **MCP bridge: no way to assign knobs or morphs from a client** — the editor has full
  hardware knob and morph assignment (`KnobAssignmentMessage`, `AssignmentsListComponent`,
  the Inspector's patch-wide assignments view), but none of it is reachable over the bridge:
  `McpRequestHandler` answers 16 methods and not one touches assignments, so an
  assistant that has just built a patch cannot finish the job by putting its most
  performance-relevant parameters under the front-panel knobs — the user has to do it by
  hand in the Inspector. Wanted: `assign_knob` / `assign_morph` / `list_assignments`
  (plus their removals), taking the same `section` + `container_index` + parameter
  name/id trio `set_parameter` already accepts, so a client can say "cutoff on knob 7".
  Both halves need writing: the method in `source/mcp/McpRequestHandler.cpp` and the
  wrapper in `mcp-bridge/server.py`, following the `set_parameter` pattern. Assignments
  are already part of the patch model and upload path (`buildHwFromPatch` reads
  `patch->knobAssignments`), so this is plumbing rather than new protocol work.

- [x] **MDI: the four slots inside the main window.** Done, unreleased. The per-slot OS
  pop-out windows are replaced by internal sub-windows in the central area, as the original
  Clavia editor and Nomad do. Each sub-window carries only its canvas; the Inspector,
  browsers, header and status bar stay shared and follow the focused slot.

  Tiling turned out better than the 2x2-plus-modes originally planned: it is dynamic, the way
  niri and Hyprland do it, so the layout is a function of how many slots are open (one full,
  two split, three in thirds, four 2x2) and re-flows as you open and close them. Plus a focus
  mode on F11 and on each sub-window's maximise button.

  It did what it was for: canvas wiring existed twice, inline in the constructor resolving
  through `activeSlot` and again per fixed slot in `wireSlotWindowContent`, with 19
  main-vs-slot-window special cases between them. There is one `wireSlotView(slot)` now, and
  `SlotWindow`/`SlotWindowContent` are gone.

  All phases and what each one turned up are in [`docs/MDI_PLAN.md`](MDI_PLAN.md). Two things
  are deliberately left out: the re-tile animation (not wanted) and an optional "Arrange"
  command for the manual Free layout.

  This superseded the old "slot windows: live fan-out and global commands"
  ([#22](https://github.com/animatek/Animatek-NME/issues/22)) item, whose live fan-out and
  per-window `Ctrl+R`/`Ctrl+S` shipped in 0.11.0.

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

- [x] **Theme submenu checkmark sticks on the initial theme**
  ([#19](https://github.com/animatek/Animatek-NME/issues/19)) — fix shipped in 0.10.0 and
  the issue was closed 2026-07-23.

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
