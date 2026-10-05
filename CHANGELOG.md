# Changelog

## Unreleased

### Added

- **Theme editor** (2026-10-05, #90, step 2; the colour options asked for by Grant). View > Theme > Edit Theme Colours opens a window with a swatch per colour, grouped: LEDs and meters, cables and matching jacks, morph knobs, sequencers, knobs, modules and the app. The canvas follows live while a colour is dragged; Save writes a user theme file, closing without saving restores the chosen theme. New colours: NoteSeqA/CtrlSeq slider grip, EventSeq lit step, NoteSeqB notes and playhead note; left empty they keep the old derived look.
- **Theme files and a user theme folder** (2026-10-05, #90, step 1). A theme can be saved by name to `~/.AnimatekNME/themes`, reloaded and opened from View > Theme. Grant's drawing code now carries his credit in its headers (licence confirmed by him).
- **Hardware look: an optional skeuomorphic way to draw the canvas** (2026-10-05, #89;
  code by Grant (teezdalien), a Patreon supporter, integrated by Claude). Knobs, jacks,
  module faceplates, displays, sliders, LEDs, meters and cables are drawn as physical
  hardware instead of flat shapes. It is a render style, not a theme: it works over every
  theme, like Wireframe, is off by default and is ignored while Wireframe is on. `Ctrl+H`
  (macOS: `Cmd+Shift+H`), View menu, or Editor Options > Appearance, where a knob style
  can be picked: Domed cap, Black knurled, Aluminium knurled (renamed from Grant's
  original names to avoid a trademark). All the drawing is in the new header
  `source/ui/SkeuoDraw.h`, in canvas coordinates, so hit testing and cable anchors are
  untouched. Builds and the unit tests pass; Javier looked at it with the Release build and
  it runs smoothly (the Debug build is visibly slower). Not measured on a very large patch,
  and **Grant has not yet confirmed the licence of his contribution.**

- **Flat knobs, and an "Animatek Rack" theme** (2026-10-05, #89; Claude, at Javier's
  request). A second knob look for the normal (non-hardware) mode, taken from the Animatek
  modules for VCV Rack: a disc with a thin rim, a slightly different face, a pointer, and
  the value painted over the rim (bipolar ranges fill from the centre). Editor Options >
  Appearance > "Flat knobs"; off by default, ignored with Hardware look or Wireframe on.
  The colours follow the theme: light disc and dark pointer on light themes, dark disc on
  dark ones, and the arc takes a new per-theme colour (`ColorScheme::knobArc`: the palette's
  blue, or indigo on the light themes). A morph group still colours the rim. New theme
  **Animatek Rack**, last in the list so saved theme indices do not move: bluish graphite
  panels, pale blue-grey text and the style guide's accents. Builds, tests pass, and Javier
  looked at it on the Release build; some themes still need their colours polished.

### Changed

- **Sine Bank: the ratio arrows sit beside the tune knob** (2026-10-05, #89; Grant,
  integrated by Claude). The two-halves bar under each ratio window hid the "Tune" label.
  Drawing and hit test now take the arrow rectangles from one helper
  (`source/ui/PartialArrows.h`) so they cannot drift apart, and the first column is moved
  3 px in from the module's left edge.

- **A patch fetch skips the common area's parameters and names when it has no
  modules** (2026-10-04; Claude, from the original editor, see
  `../G1originaleditor/notes/12`). The original asks for the common area's
  `4C 00` and `4E 00` only when its module list is not empty. NME asked for
  both always, two round trips for nothing on most patches. Once the common
  module list comes back empty those two requests are dropped from the queue
  and counted as received, so the patch does not read as incomplete. The poly
  area is never skipped. Tried on G1-Emu with a patch with an empty common
  area (the synth answered 11 sections instead of 13, patch received complete,
  34 poly modules as in the `.pch`) and one with common modules (13 sections as
  before). Not tried on a G1. Unit test for `moduleDumpCount` added.

### Fixed

- **Buttons sometimes needed two clicks** (2026-10-04, #82 by Nocticore; Claude).
  `mouseDrag` had no case for a button, so the pointer drifting a pixel during a
  click fell into the generic path, which puts the parameter back to the value it
  had when the press began and sends it: the new value went out, then the old one
  a few ms behind it, exactly as in Nocticore's SysEx dumps (`04 03` then `04 00`;
  mute `01` then `00`). A button now ignores drags. Found by reading the code and
  matched against the dumps; **not reproduced or tried on a synth or with a
  trackpad.**

- **Patches wrapped in a MacBinary header open** (2026-10-04, #87; Claude). Old
  Mac patches carry 128 binary bytes (name, type `PCH `, creator `NORD`) before
  the text, and its leading 0 made the reader see an empty file. The header is
  now taken off (`PchFileIO::patchTextFromBytes`). Tried on all six of them in the
  community archive; unit test added.

- **Synth error codes 3, 5 and 6 were mislabelled** (2026-10-04; Claude, from the original
  editor). NME called 5 "no slot focused" and 3/6 "non-fatal warnings". The original editor's
  own table says 3 = "Error in synth", 5 = "Stream execute error. Synth may be corrupted.
  Please turn it off and on." and 6 = "Unfinished bubble error". The numbering was checked on
  G1-Emu (a bad checksum comes back as 4, as in that table); 3, 5 and 6 were not provoked.
  One function, `synthErrorName`, now serves both the console and the status bar, which had
  their own copy. The MCP `synth_error` event carries the new description text. Unit test added.

- **The module bar listed modules in modules.xml order, not the original's**
  (2026-10-04, #81 by Nocticore; Claude). Each category now follows the order of
  the original editor's icon bar (read off its screenshots in
  `Implementaciones/Icon Bar Icons`): Keyboard, Keyboard Patch, MIDI Global, ...
  in In/Out, Master Osc first among the oscillators, and so on. Also applies to
  the module tree. A module missing from the list goes last. Mixer, Audio and Logic were first read off icons and then corrected against the original exe's own menu builder (`G1originaleditor/notes/02-orden-real-barra-modulos.md`).

- **Oscillator note names jumped an octave at F#** (2026-10-04, #80 by
  Nocticore; Claude). `fmtNote` rounded the octave instead of flooring it, so
  every note from F# up read an octave high. Same cause as NoteSeqB's #76.
  Unit test added.

- **Sine Bank: the ratio arrows reset an oscillator's level** (2026-10-04,
  reported by Grant on Patreon; Claude). The arrows reset the fine detune as a
  hard-coded `p3`, which on `OscSineBank` is osc 1's level, not any fine. The
  fine is now the parameter after the coarse. Builds; not tried by ear.

- **Patches lost a module and a cable, or put everything in the common area**
  (2026-10-02, Javier's request; Claude; #85, #86). The per-area sections of a
  `.pch` open with the area number, and the reader took it from the first line
  and the rows from the second on. Files that glue the first row to the area
  (`1 1 18 1 10`) lost that module or cable without a warning (1,344 patches
  of the community archive), and files with `\r\r\n` line endings read the
  area from an empty line as 0, so every module went to the common area (9).
  `PchFileIO::splitAreaRows` now skips empty lines and keeps whatever follows
  the area as the first row, for `[ModuleDump]`, `[CableDump]`,
  `[ParameterDump]`, `[CustomDump]` and `[NameDump]`. Verification: two new
  test cases (`test_pch_area_rows.cpp`) that fail without the fix; full suite
  passes, also under ASan/UBSan; `nme_pch_dump` over the 29,639 readable
  archive patches against g1-taller's reader: 27,763 identical (26,478
  before), none left with #85 or #86, the rest only editor decisions (empty
  titles filled in, titles cut to 16, clamped values) or odd files.

### Added

- **The synth's display follows the knob you grab** (2026-10-04; Claude, from a capture of the
  original editor against G1-Emu). Like the original, NME now sends a `ParamFocus` (cc 0x13, sc
  0x2F, no checksum) the first time a different parameter is moved in a slot, from the canvas, the
  inspector, the knob floater or the morph knobs, and then the value. The frame is byte for byte the
  captured one (unit test). Undo, morph sweeps and randomize still send values only, so they never
  move the synth's focus. The emulator does not answer a focus, so **what the G1's display does
  with it has not been seen yet**: check on hardware.

- **Incoming checksums are checked, and a `CableRecolor` message exists** (2026-10-04;
  Claude, from reading the original editor with Ghidra, see `../G1originaleditor`).
  `SysEx::decode` now reports whether a frame carries a checksum (header bit 4, the
  bit the original reads) and whether it matches; `NmProtocol` counts and logs a
  mismatch but still dispatches the frame, because nothing is known to fail yet and
  dropping on a misread flag would cut the link. `RecolorCableMessage` (sc 0x54) is the
  original's way of recolouring a cable on the synth; nothing calls it, so cable colour
  still changes only in the editor. Unit tests added (3 cases). **Neither has been tried
  on a synth.** Docs: the envelope is `F0 33 [cc|slot] 06`, not `F0 33 06 [cc|slot]`
  as `CLAUDE.md`, `AGENTS.md` and `RESEARCH.md` said.

- **`nme_pch_dump`: a reading battery for the patch loader** (2026-10-01,
  Javier's request; Claude). A console target next to the tests
  (`tests/pch_dump.cpp`): it reads every `.pch` path on stdin with `PchFileIO`
  and prints, one JSON line per patch, what NME understood (modules with
  their parameter, custom and title values; cables as pairs of ends). The
  `g1-taller` repo compares that with its own reader over the 29,639 readable
  patches of the community archive: 26,478 match, 1,784 differ only by editor
  decisions (empty titles filled with the module name, titles cut to 16,
  out-of-range values clamped), and the rest are real reading bugs: #85 (area
  number on the first row: 1,344 patches lose a module and its cables), files
  with `\r\r\n` line endings (9 patches: everything lands in the common area)
  and MacBinary-wrapped files NME cannot open (10). Verification: builds; ran
  over the whole archive in ~45 s.

## 0.19.0 — 2026-09-27

### Added

- **MIDI settings: Reconnect, and a dialog that follows the connection**
  (2026-09-27, Javier's request; Claude). While connected the dialog offers
  **Reconnect** beside **Disconnect**: the handshake again on the ports chosen
  in it, so switching between a real G1 and G1-Emu is one click
  (`ConnectionManager::connect` already drops the old connection first). Enter
  presses Connect/Reconnect, never Disconnect. Also fixed: the dialog kept the
  state it opened with, so Connect stayed Connect after the synth answered;
  `MainComponent` now passes it every connection change while it is open. The
  dialog is a little wider for the status text. Manual: chapter 1, English and
  Spanish. Verification: builds; Javier tried Reconnect and the switch between
  synths in the dialog.

- **MCP: choose the MIDI ports and reload a slot from the synth** (2026-09-27,
  Javier's request; Claude). Four bridge tools: `list_midi_ports`,
  `connect_midi(input, output)`, `disconnect_midi` and `fetch_patch(slot?)`.
  `connect_midi` does what the MIDI settings dialog does, naming a port by id,
  exact name or a part only one port has (`McpRules::matchPort`); anything
  ambiguous or unknown is refused with the list, because on a machine with
  G1-Emu and a real G1 the wrong port is the wrong synth. `fetch_patch` asks the
  synth for a slot again (`MainComponent::refetchSlotFromSynth`), refused while
  another transfer runs. Also fixed: `get_synth_status` kept reporting the
  previous synth's name for a second or two after switching; it is now null
  from a disconnect until the new synth sends its settings.
  Verification: builds; 4 new test cases (id, name, exact over partial,
  ambiguous and empty names); full suite passes, also under ASan/UBSan. Live:
  switched three times between G1-Emu (`PC Port`, "Modular") and the real G1
  (`UM-ONE`, "animatek"), the name read null right after each switch and then
  the right one; ButohDrone fetched back from both matched the file section by
  section except the header's 3-bit `unknown4`, which the OS itself rewrites.

- **Slot sets: save the four slots as one sound and load them back together**
  (2026-09-13, Javier's request; Claude). Layered patches, one per slot, only
  make their sound as a group, and the G1 cannot store that: a bank position
  holds one patch and performances only arrived with the G2. **File > Save Slot
  Set...** asks for a name, optional notes and, per slot, *Include patch*,
  *Switch slot off* or *Leave as is*, so a set can be two or three patches. It
  writes `Sets/<name>/` in the preset library: a copy of each patch with its
  `.var`, named after the slot letter, and a `<name>.nmset` JSON manifest
  (`source/model/SlotSet`). **File > Open Slot Set...** or a double click on the
  new **SET** entries of the Disk tab (with their own filter) says what goes
  where and what it replaces, then loads the patches one at a time, each
  waiting for the previous upload's ACK, switches slots on/off as saved with a
  single mask message (`ConnectionManager::setSlotPins`) and moves focus to a
  slot the set leaves on, since the synth never disables its focused slot.
  Disconnected, it loads LOCAL and touches no enable state; a failed upload
  leaves that slot LOCAL and is reported at the end. Manifests naming files
  outside their folder, unknown modes or a newer version are refused. The bank
  location is recorded (`414`) but not used yet: loading always uses the set's
  own copies. Manual: chapter 4 *Slot sets* and chapter 6 `.nmset`, English and
  Spanish. Three starter sets (Mareas en Re, Horizontes, Fractura) were created
  in the library from `patches/*-ABCD/`, bank numbers 406–417.
  Verification: builds; 8 new test cases (manifest round trip, refusals, file
  and folder names, bank numbers, enable mask and focus); full suite 105 cases,
  2,859 assertions, also under ASan/UBSan. Javier tried it the same day and
  reports that it works.

- **Twelve original G1 patches and a private ABCD patch guide** (2026-09-13,
  Javier's request; Codex). Bank 406–409 holds Mareas en Re with the user's
  live tweaks preserved and slot-letter names. This includes A at 115 BPM
  and B at 52 BPM with its clock stopped. Bank 410–413 adds Horizontes:
  four continuous drones with independent smooth random sources, quantized
  melody/countermelody, and no clocks, sequencers or triggered envelopes.
  Bank 414–417 adds Fractura: kick/sub, FM metal/tom, digital hats and acid
  voice, with 4:3, 8:3 and 3:2 internal clock ratios and 5/7/11-step cycles.
  New parts start at 132 BPM with free phase between slots. Pitched voices
  share D harmonic minor. All twelve have eight knobs and two morph groups.
  The final working slots hold Horizontes. Later live A-drone and D-techno
  performances are also preserved as separate snapshots; the latter was
  reconstructed from the reported parameter/morph events before restoring
  the drones. Files and limits: `patches/README.md` and
  `patches/generative-collections/verification.json`.
  Verification: all eight new patches parsed and loaded on G1 OS 3.03,
  four enabled slots with one voice each, live generator/sequencer activity,
  bank cache listing all twelve names at 406–417. Intermittent B upload
  timeouts were recovered from disk; no synth errors after recovery through
  event 464. Parameter ranges, morph endpoints, scale masks, eight knob
  assignments per new patch and connected audio paths validated. Two short
  stereo-mix captures through Komplete Audio 6 contain signal with no clipped
  samples (techno peak −13.51 dBFS; drones −11.72 dBFS); these are not isolated
  stems or a listening assessment. No power-cycle or memory-recall test.
  The patches and their guide live outside the repository (`patches/` is not
  tracked) and are not part of the release.

- **Key Quantizer scales on any root, and the one it is on now** (2026-09-13,
  Javier's request). The right-click menu already had sixteen scales, all on C and
  undocumented, so A minor meant choosing C's natural minor and moving every switch
  by hand. The **Scale** submenu now opens with what the switches spell at the
  moment, then Chromatic, then one list per root from C to B, and the current scale
  is ticked. Every scale with those notes is ticked, since the switches do not know
  the root: the white keys are C Major, A Natural Minor and D Dorian at once. New
  scales: Phrygian Dominant (Andalusian), Double Harmonic, Hungarian Minor,
  Neapolitan Minor and Major, Hirajoshi, In-sen and Diminished (H-W). Scales are
  written once as intervals in `source/model/KeyQuantScales.*` and moved onto the
  switches for the root chosen.
  Verification: `tests/test_keyquant_scales.cpp` checks that the sixteen scales on
  C write exactly the switches the old menu did, the move to other roots (E
  Andalusian is E F G# A B C D), recognition of every scale a set of switches
  spells, and the switch ids against the module description; the suite passes, 97
  test cases / 2,766 assertions, plain and under ASan/UBSan. **Not verified:** the menu on screen. Choosing a
  scale is still one undo step per switch, as it was.

- **Replace a module with another of its family, from its right-click menu**
  (2026-09-13, Javier's request). Turning a FilterE into a FilterD used to mean
  deleting it, adding the new one and wiring it all again. "Replace with" lists the
  module's own family, each entry with its DSP share and how many cables it would
  drop, and swaps it in place as one undo step. It keeps the position, the name if
  the module was renamed, the cables on connectors the new type also has (by name,
  or where only one choice exists: FilterD's lp into FilterE's single out), the
  values of parameters with the same name and range, and the knob, morph and MIDI
  CC assignments on those parameters. Anything else is dropped and said in the
  status bar, never rewired to a plausible guess: an OscA's sync does not become an
  OscB's FM input. A taller module pushes the ones below it down the column, or is
  refused if the column has no room. The MCP bridge gets the same as
  `replace_module`. Logic in `source/model/ModuleReplacement.*`, the undoable swap
  in `ReplaceModuleAction`.
  Verification: `tests/test_module_replacement.cpp` covers the connector and
  parameter matching against the real module descriptions, the family list, and a
  swap with a cable, a knob, a morph and a CC through undo and redo; the suite
  passes, 92 test cases / 2,398 assertions, plain and under ASan/UBSan. Javier
  tried it in the editor the same day and it works; edge cases to come from use.

- **The MCP bridge can read the synth back, and assign knobs, morphs and MIDI CCs**
  (M1, and the groundwork for testing against hardware, 2026-09-13). Until now a
  client could send edits but never learn what the synth made of them. Thirteen new
  tools. `get_synth_status`: connection and synth OS version, the slot the synth
  has focused versus the editor's active tab, transfers in flight, and per slot the
  patch, LOCAL, enabled, voice count and bank location. `get_events`: a 512-entry
  log of synth errors, connection changes, values the synth reported itself (a
  front-panel knob, a morph dial, a MIDI CC), slot focus and enable changes, patches
  received or incomplete and voice counts, read with a cursor that says when the log
  wrapped. `read_lights`: per-module LEDs and meters for the focused slot. `list_bank`: the patch name in
  each position of a synth bank, so a store can pick a free one.
  `list_assignments`, `assign_knob`/`unassign_knob`, `assign_morph`/`unassign_morph`,
  `assign_midi_cc`/`unassign_midi_cc`, and `set_morph_value` and `play_note` to hear
  or meter the result. Assignments go through the same undo actions as the canvas. A
  knob or CC already in use is refused unless the caller asks to replace it, and one
  undo then gives it back; morph groups are capped at the G1's 25 assignments; knobs
  are named as on the panel ("Knob 7", "Pedal"), and a bare "7" is refused as
  ambiguous. `set_parameter` now shares its parameter lookup with the new tools,
  with the same behaviour.
  Verification: `tests/test_mcp_event_log.cpp` (cursor, limit, wrap-around) and
  `tests/test_mcp_rules.cpp` (knob names, ranges); the full suite passes, 86 test
  cases / 2,284 assertions, plain and under ASan/UBSan, and `server.py` registers
  all 29 tools. Against a G1 (OS 3.03) the same day, in a scratch slot: a patch
  built over the bridge, notes played, the ADSR's gate LED and the LFO's LED read
  back through `read_lights`, knobs assigned, moved, refused when in use and
  removed, a morph group put on a knob, two morph assignments with ranges, a CC,
  the morph dial turned and everything unassigned again. 44 calls, and no synth
  error or disconnect in the event log. **Not verified:** the sound itself, which
  nobody listened to on this side, and the front-panel LEDs. The G1 sent no
  voice-count change while notes played, so `voice_count` is not a way to tell a
  note sounded. LOCAL slots still receive edits while connected (S4).

### Changed

- **Disk tab type filters are text chips now, not icons** (2026-09-18,
  Javier's request; Claude). The six abstract icons (grid, sheet, brackets,
  stacked squares, bars, a struck-out "2") said nothing about what they kept.
  Each filter is now a neutral chip with the word its rows carry (ALL, PATCH,
  SNIP, BANK, SET); only the active one is highlighted, so the row tags stay
  the one place with colours. Chips never cut their label: on a narrow panel
  the padding tightens instead. *Hide PCH2* was drawn like one more filter though it
  narrows whichever one is active; it is now a **Hide 2.10** checkbox next to
  the file count. The count line no longer trails the library path, which
  never fit; the path is its tooltip now.

### Fixed

- **The source builds again with the compilers CI uses** (2026-09-27; Claude).
  `KeyQuantScales.cpp` used `size_t` without including `<cstddef>`, which this
  machine's compiler forgave and CI's did not: every push since the Key
  Quantizer scales failed to build there. Verification: CI green again.

- **Closing G1-Emu (or unplugging the MIDI interface) disconnects the editor**
  (2026-09-27, found testing against G1-Emu; Claude). The ports vanished and the editor went on saying "Connected",
  sending into nothing, until the user reconnected by hand; nothing reported
  it, because a synth that is gone cannot time out a request nobody makes.
  `MidiDeviceManager` now listens to JUCE's device-list changes
  (`MidiDeviceListConnection`) and, when a port it has open leaves the system,
  the connection drops with "MIDI port gone: <port>" (`ConnectionManager::
  disconnect` takes the reason). Verification: a new test opens a virtual port
  as the synth, connects, destroys it and expects the disconnect (fails without
  the change; skipped where virtual ports cannot be made); the suite passes.
  Live: Javier closed G1-Emu with NME connected and it showed "MIDI port gone".

- **A patch loaded from the synth's front panel arrives whole** (2026-09-27,
  found testing against G1-Emu; Claude). Loading
  from the panel ended in "Incomplete Patch Load: 0 of 13 sections", and the
  editor looked disconnected until the deferred fetch caught up. The OS
  announces such a load with two NewPatchInSlot in a row (pids `0b` then `0c`
  in the capture); the editor's RequestPatch was answered with the first, the
  second landed while the 13 GetPatch went out with the first, and the synth
  answers a GetPatch with a pid the slot no longer has with a bare ACK and no
  section. Now a newer pid on the slot being fetched restarts the fetch
  (dropping the GetPatch still queued, `NmProtocol::discardQueued`), a newer
  pid while the request is unanswered makes the answer ask again, only the
  RequestPatch answer (ACK `0x36`) starts a fetch, and a stalled fetch whose
  slot has moved to another pid asks for the patch again instead of the same
  sections. Verification: a new test replays the captured exchange in both
  orders (fails without the change); the suite passes. Live: Javier loaded
  patches from G1-Emu's panel with NME connected and each one arrived.

- **A failed upload, a silent synth and a switch of synths no longer leave the
  editor pretending** (2026-09-27, found testing against G1-Emu; Claude). Three
  things the tests turned up, all about the editor believing a slot matched the
  synth when it did not:
  - An upload that timed out or was rejected only put a message in the status
    bar: the slot stayed as if uploaded, with its synchronizer sending edits to a
    synth slot holding another patch. Now the slot turns LOCAL, its synchronizer
    goes, the status bar says so, and the MCP log gets an `upload_failed` event
    (`ConnectionManager::setUploadFailedCallback`).
  - After such a timeout, or an unanswered patch request, the editor stayed
    "Connected" to a synth that answered nothing. It now says hello and, if
    nothing at all comes back within the reply timeout, marks the synth "not
    responding" (disconnected, port kept open, per-connection state forgotten
    through the new `forgetSynthState`); it reconnects by itself as soon as the
    synth speaks again.
  - Losing the connection now turns every loaded slot LOCAL and drops its
    synchronizer, and connecting no longer gives a LOCAL slot one. Switching
    from G1-Emu to the real G1 left slot A showing the emulator's patch, not
    LOCAL and synced, for the ~3.5 s before the new synth's patch arrived, so an
    edit then went to the real synth's own patch.
  - Module names are held to the G1's 16 characters in `Module::setTitle`, the
    one place every path goes through; renames already were, but a `.pch` with
    longer names loaded them whole and the synth gave them back cut, so two
    patches in bank 9 no longer matched their own files.
  Verification: builds; 2 new test cases (title limit, long name from a file);
  full suite 121 cases, 2,935 assertions, also under ASan/UBSan. Live against
  G1-Emu: with the emulator made to hang (G1-Emu's `G1_SINGLEOP_CACHE=1`) the
  upload timeout gave `upload_failed`, slot A LOCAL and "Synth not responding"
  3 s later; switching G1-Emu -> real G1 -> G1-Emu showed slot A LOCAL until
  each synth's patch arrived (no writes to the real G1); the two bank 9 patches
  now round-trip with no difference.

- **The Synth Settings dialog shows the master tune centred on 0** (2026-09-27). The
  setting is a signed byte, 0 = in tune, -127 to 127 cents, as nmedit's editor reads
  it; NME took 64 as the centre, so Javier's G1, at 0 on its own display, showed as
  -64 cents, and saving the dialog would have detuned it. `SynthSettings` gets
  `masterTuneCents()`/`setMasterTuneCents()`, the dialog's slider now runs -127..127.
  Verification: `tests/test_synth_settings_clock.cpp` reads 0 cents from a real reply
  and round-trips -127, -10, 0, 25 and 127 through the encoder; suite green plain and
  under ASan/UBSan, the app builds. Javier opened the dialog against his G1 with this
  build: Master Tune 0, +0 cents, as on the synth's own display.

- **The Synth Settings dialog shows the MIDI clock source the right way round**
  (2026-09-27). The bit is 1 for Internal and 0 for External, and NME read it the
  other way: Javier's G1, set to Internal at 100 BPM on its own display, showed as
  External, and choosing Internal in the dialog stopped the synth's master clock, so
  every patch clocked by MIDIGlobal (sequencers, arpeggios) fell silent. Found in
  G1-Emu, whose OS starts the internal clock only on 1 (G1-Emu `NOTES.md`, "The master
  clock"). `SynthSettings` now says what the bit means (`clockIsInternal()`,
  `setClockInternal()`), the dialog uses them, and settings not yet read from the
  synth default to Internal.
  Verification: `tests/test_synth_settings_clock.cpp` decodes a real settings reply
  whose bit is 0 as External and round-trips both values through the encoder; the
  suite passes plain and under ASan/UBSan and the app builds. Javier opened the
  dialog against his G1 with this build: Internal, 100 BPM, as on the synth's display.

- **The keyboard floater no longer leaves notes stuck on the synth** (2026-09-26,
  G1-Emu issue #4, "the keyboard floater stacks notes"). The G1's OS takes the
  editor's note messages as one key, not a keyboard: a press stores its note and
  sets the key down, a release sets it up without looking at the note, and only the
  key's changes become note-on and note-off. Two editor notes that overlapped (the
  floater's repeat and drone modes, or two `play_note` calls from the MCP) left one
  sounding for ever. Checked on a real G1 with a gate-only test patch, whose gate
  LED stayed lit after both notes were released; the G1-Emu emulator does the same
  (G1-Emu `NOTES.md`, "The editor's keyboard is one key"). `ConnectionManager` now
  sends every editor note through `EditorKey` (`source/midi/EditorKey.h`): a press
  while another note is down releases that note first, and a release of a note that
  is no longer down sends nothing. The editor's keyboard stays monophonic, as the
  synth makes it; chords still need the regular MIDI input.
  Verification: `tests/test_editor_key.cpp` covers a lone note, a second press,
  releases in either order, a repeated press and a reset; the suite passes plain and
  under ASan/UBSan. On the G1-Emu test bench, the sequence it produces for two
  overlapping notes plays the second note (330 Hz for note 64) and ends silent,
  where the raw overlap left a note at full level. Not yet tried from the editor
  against the real synth.
- **`Ctrl+R` leaves modules excluded from mutation alone** (2026-09-13, reported by
  Javier). The manual said locked parameters and excluded modules are never
  touched, but `randomizeSlotParameters` only checked locks, so "Exclude from
  Mutation" protected a module from the Patch Mutator and the MCP bridge and not from
  `Ctrl+R` or `Ctrl+Shift+R`. Both now skip excluded modules, and the status bar says
  how many were left alone, or that nothing was randomized because everything left
  is excluded. To make the exclusion usable for this: the menu item is now **Exclude
  from Random & Mutation**, it is also on the right-click menu of a selection (one
  state for all of it: exclude them all, or include them all again when all already
  are), and the red frame marking an excluded module is drawn always, not only in
  Mutator mode. The manual also said `Ctrl+Shift+R` spreads values around the current
  ones; the code centres them on the middle of each parameter's range, and the manual
  now says so. Verification: built, full test suite passes (97 test cases); the
  randomize path and the menus have no unit tests. **Not verified:** on screen.

- **A store to a bank no longer gets lost when it follows an upload** (2026-09-13).
  Storing a patch uploads it first and sends the store as soon as the synth ACKs
  the upload, and that store went out raw, without waiting for an answer. Of four
  stores in a row made over the MCP bridge, one never reached the bank, with no
  error on either side: only the synth's bank list, fetched again, showed the gap.
  The store now goes through the ACK queue, as `storeLoadedSlotToBank` already did.
  Verification: the missing patch was stored again, and all four positions read
  back from a patch list fetched fresh after reconnecting. The failure depends on
  timing and was seen once, so this is the likely cause, not a reproduced one.

- **The bank list follows a store** (2026-09-13). The editor fetches the synth's
  patch names once, on connect, and a store never updated them, so the patch
  browser and `list_bank` went on showing the stored position as empty, or under
  its old name, until the next connection. The stored name is now written into the
  list and the browser is refreshed. Built and tested, not yet seen on screen.

- **Undoing a morph assignment no longer leaves it drawn on the knob** (2026-09-13).
  A morph is held twice: in the patch's assignment list, which is what is saved and
  sent, and in each parameter's own group and range, which is what the canvas and
  the inspector draw. The canvas copied the list into the parameters only when a
  patch was set, and the undo actions touched only the list, so Ctrl+Z took the
  morph out of the patch but left it drawn until the patch was reloaded. The actions
  now update both. Found by reading the code while adding the MCP morph tools, which
  set nothing on the parameter themselves. `tests/test_morph_assign_action.cpp` covers
  assign, range change, undo and redo; not yet checked on screen.

- **A failed backup no longer destroys the good one** (S2, 2026-09-12). Backing up
  all banks deleted every `.pch` in `Bank1`-`Bank9` before fetching a single patch,
  so a synth that stopped answering halfway through, a cancelled run or a
  disconnect left you with neither the new backup nor the old one. The download
  now goes to a staging folder beside the mirror, and the bank folders are
  replaced only once every patch has been fetched, parsed and written. Anything
  short of that (a failure, a timeout, a cancellation, a disconnect, or items
  never reached) discards the staged copy and leaves the previous backup exactly
  as it was, reporting that it was kept. If the move itself fails, the complete
  staged copy is kept rather than thrown away and its path is logged.
  Mirror semantics are unchanged but now applied at publication, so patches
  deleted on the synth still drop out of a backup that completed. An all-banks
  backup also refuses to start without a loaded patch list: every position would
  read as empty, which is how an empty mirror could replace a good one. The
  dialog's warning has been rewritten to say what now happens.
  Verification: `tests/test_bank_backup_safety.cpp` covers the two refusals,
  asserting the existing files and their contents survive untouched and no
  staging is left behind. Cancellation, mid-download disconnect, a partial fetch
  and a publication failure are **not** covered by tests: they need a patch list
  on the connection, which needs the injectable seam T1 calls for. Those paths
  rest on the staging design and still need hardware testing.

- **A patch loaded from disk goes to the slot you chose, with its own contents**
  (S3, 2026-09-11). The upload was deliberately addressed to the destination slot,
  but 200 ms later it read `currentPatch()`, which follows the focused tab. Switch
  tabs inside that window and the patch you had just switched to was uploaded into
  the slot the file was meant for, silently overwriting it. The delayed upload now
  addresses that slot's own patch, and a per-slot generation counter drops it
  entirely if the slot was loaded again, replaced by New Patch or filled from the
  synth in the meantime. The Patch Settings dialog had the same mismatch twice
  over: it applied its result to whichever patch had focus when the dialog closed
  and uploaded it to whichever slot the synth was looking at. It is now bound to
  the slot it was opened on, and does nothing if that patch is gone.

- **Adding a module sends its custom values** (S7, 2026-09-11). The synchronizer
  always passed an empty CustomDump, so OscA's frequency-display unit, the note
  sequencers' steps and every other custom control arrived at the synth as
  whatever default it chose for itself. The values are now collected in
  descriptor order, zeros included: the dump is positional, so a skipped zero
  shifts every later value. The encoder and its byte-for-byte agreement with
  jnmprotocol's reference OscA packet were already right and already tested; only
  the caller was empty. This was reported fixed in 0.18.0 and was not: the change
  was never committed and the release shipped without it.
  Verification: `tests/test_synchronizer_new_module.cpp` drives the real
  synchronizer against a real patch and the real descriptors, recording the frames
  off the protocol instead of a MIDI port, and checks OscA (one custom value) and
  NoteSeqB (two). Confirmed to fail without the fix. This has nothing to do with
  the G1 freezing when an oscillator was added: that was the corrupt patch left
  by an incremental `DeleteModule`, fixed and hardware-tested in 0.18.0. What
  was missing here is simply that the synth chose its own defaults for a new
  module's custom controls. Not yet exercised on hardware.

- **Disconnecting cancels the old MIDI session, not just its ports** (S1,
  2026-09-11). The protocol's sender captured a destroyed `MidiDeviceManager`,
  so a queued request drained after its timeout through freed memory. Detaching
  or replacing the sender now clears the queue and reply wait; input callbacks
  already posted to the message thread are invalidated before the ports close.
- Connection-owned ACK/parameter queues, partial downloads, bank locations and
  one-shot completion callbacks are cleared on disconnect. Delayed operations
  use a fresh cancellation flag per session, preventing settings/list requests
  and upload completions from crossing a reconnect. The settings-readback timer
  also had an unguarded `this` capture, reproduced under ASan before the fix.
  An interrupted upload still sends its terminating packet before detaching;
  an outstanding bank-upload callback receives failure once, never stale success.
- **MIDI lifetime regressions now run in the normal test target.** Immediate and
  queued sends share the existing transport callback, allowing frame recording
  and simulated replies without opening MIDI ports. Ten new test cases cover
  teardown, timeout, reconnect, queue cancellation and upload completion.
  Verification: full Linux Debug app build succeeded (existing warnings remain);
  all 75 cases / 2,153 assertions pass in Debug and ASan/UBSan, including five
  consecutive sanitizer runs with DISPLAY and WAYLAND_DISPLAY unset.
  Real port-open failure/reconnect and G1/platform smoke tests remain pending;
  no hardware was operated and no new release was published.

### Documentation

- Added [the post-0.18.0 development plan](docs/POST_0180_PLAN.md): seven safety
  findings with regression criteria, integration/performance work, session recovery,
  patch and bank version history, library tags/favourites, comparison and MCP.
  The proposed release sequence is linked from the roadmap; the August plan is
  retained as historical context. This is planning only, not shipped functionality.

## 0.18.0 — 2026-09-10

### Added

- **Note sequencer steps can read as note names** (#76). A new **Read sequencer steps as
  note names** in Editor Options changes what a step says wherever a value is shown: hovering
  it, dragging it, the F5 overlay and the Inspector, which also accepts what it shows, so a
  NoteSeqB step can be set by typing `C4`. NoteSeqB reads absolute pitches on the same
  60 = C4 its piano roll is drawn around; NoteSeqA's steps are a bipolar control value whose
  zero is 64 and which only become a pitch once they reach an oscillator, so they read as the
  interval they transpose by: `0`, `+7 (G)`, `-1 (B)`. Off by default, and the editor's
  setting rather than the patch's: the note sequencers have no room left in the G1's patch
  format for a preference of their own.

- **Dragging a note in NoteSeqB's piano roll says which note it landed on** (#76). Every
  other parameter edit puts its value on screen while the mouse is down; the one gesture that
  sets notes did not. The badge appears over the step's own arrow button, so it reads in
  whichever of the two units the setting above asks for.

- **The Inspector marks the preset it recalled, and the arrows walk them**
  ([#60](https://github.com/animatek/Animatek-NME/issues/60)). Recalling a preset changed
  the module and left no trace of which one it had been, so a list of eight was eight
  identical rows and no way back to the one you liked. The recalled row is now marked, and
  with the list focused the up and down keys walk the presets **recalling each as they pass
  it** — the gesture wanted here is auditioning, not selecting and then confirming. They
  walk the visible rows, so a collapsed Factory folder is skipped by the arrows exactly as
  it is skipped by the eye, and the chosen row scrolls into view if it falls outside it.
  Nothing wraps.

- **Arrows to step through the disk preset browser**
  ([#60](https://github.com/animatek/Animatek-NME/issues/60)). Every preset asked for a
  double click, so auditioning a folder meant going back to the mouse for each file. Two
  arrows beside the counter load the previous and next entry outright, and in the list the
  arrow keys plus `Enter` do the same without leaving the keyboard. They walk the
  **visible** entries, so the search box and the type filters bound them exactly as they
  bound the list. Nothing wraps: the arrow greys out at either end, which on a long library
  is what is meant.

### Fixed

- **An orphaned LED no longer stays lit for good.** The sweep for dangling assignments
  dropped them from the model and stopped there: the synth still held the assignment and
  still had its LED on, and the editor no longer knew it existed, so no delete and no
  upload could ever reach it. Caught on "DRUM VICIUS": the patch arrived with an assignment
  dangling off Knob 16, the sweep threw it away, and deleting the modules sent deassigns
  for knobs 1-15 and 18 — not 16, because it was no longer in the model. The sweep now
  reports which knobs and which MIDI CCs it dropped, and installing a patch that came from
  the synth deassigns each of them.

- **Replacing a slot's patch no longer reads freed memory.** All four paths that destroy a
  patch — the one arriving from the synth, the MCP bridge's `replacePatchInSlot`, New
  Patch, and opening a `.pch` — detached the Inspector with `clearModule()`, which **keeps**
  its own patch pointer and re-arms the assignments list with it: the very patch the next
  line destroys. `clearSnapshots()` then walks that list and reads freed memory. It survived
  only because the freed container's module slots read back as null. They now detach with
  `setPatch(nullptr)`, the one call that drops both pointers; all four re-point at the
  incoming patch further down, as they already did. The crash this caused has been covered
  since #61, but the use-after-free underneath it had not been touched.

- **Deleting several modules at once puts their LEDs out, not just deleting them one by
  one.** The knob lights do not follow the patch: they move only for the incremental
  assign and deassign messages, and both are resolved against the patch the synth holds at
  that moment. A `deassign` lands if the synth still has the module the knob pointed at and
  if it is addressed to the current patch. Deleting a selection runs the actions back to
  back in a single pass, and one incremental `DeleteModule` or `DeleteCable` let out
  mid-pass was enough to move the synth's patch on: every deassign still to go was
  addressed to a patch that no longer existed, and the synth dropped it. Which is why one
  at a time worked and all at once did not. **Every** delete is now suppressed and synced
  by one full upload, even one that frees nothing: with nothing let out during the pass the
  synth's patch and its id hold still, and every deassign is valid. Knob assignment changes
  do not move the id themselves — the synth ACKs them with it unchanged.

- **An upload states only what the patch holds.** Blanket-deassigning the rest from there
  achieved nothing: after an upload that removed modules, the assignment the message names
  is already gone, the synth has nothing to act on, and the LED stays lit anyway. Putting a
  light out is the delete's job, before the module goes; lighting one is the upload's, once
  the synth has the module.

- **Deleting a module no longer leaves its assignments on the synth.** The editor's model
  did drop the deleted module's morphs, knobs and MIDI CCs, but there is no way in the
  protocol to unassign a morph, so an incremental `DeleteModule` left the synth's morph and
  knob maps naming the module that had just gone. Re-reading the patch brought those
  leftovers back into the editor and into the saved `.pch`: the patch was corrupt, and the
  rubbish latched onto whatever module landed on that index next. That is where the G1
  froze when an oscillator was added. A module that carried assignments is now synced by a
  full upload, which rewrites both maps; it is debounced and coalesced, so deleting a whole
  selection sends one. A module with no assignments keeps the cheap incremental path.

- **A patch coming in is swept of orphaned assignments.** Every morph, knob and MIDI CC
  naming a module the patch does not have is discarded as it is read, from the synth or
  from a `.pch`. An already corrupted patch therefore heals when it is opened, instead of
  carrying the leftovers into the next upload. Section 2, the morph section, is left alone:
  its module is not in a voice area's list and always exists.

- **NewModule's pid fits in its six bits.** `PatchPacket := 0:1 command:1 pid:6`:
  NewModule is the only edit message on cc=0x1f, and so the only one whose pid is six bits
  wide — move, delete, rename and the cables all ride cc=0x17, where it gets seven. It was
  masked with `0x7F` both when the message was built and when the pid was rewritten in the
  send queue, so a pid of 64 or more raised the `command` bit, which is what marks a bulk
  upload stream (`UploadPacketizer::frame`), and the synth would have read the packet as
  something else entirely.

- **An unanswered PatchPacket closes its transfer.** When an acked-queue message on
  cc=0x1c-0x1f runs out its three seconds with no reply, the empty terminating packet goes
  out. The synth stays in bulk-receive state until it sees a packet flagged `last`, and
  there it stops answering anything at all (issue #40): the close gets it back without a
  power cycle.

- **Adding an OscA carries its custom values in the SysEx.** The synchronizer always sent
  an empty CustomDump, leaving out OscA's frequency-display unit and every other module's
  custom settings. NewModule now keeps the protocol's eight-bit fields and respects the
  sixteen-character limit without the extra NUL that shifted the sections along. Verified
  byte for byte against jnmprotocol's reference OscA packet.

- **No patch and an empty patch no longer look the same** (#75). Starting the editor with
  the synth off left you looking at the working canvas, grid and all, under "Press Enter to
  add modules", which is an instruction that cannot be followed: there is nothing to add the
  module to. Without a patch the canvas now goes flat, drops its grid and its grain, and says
  "No patch open" over the two keys that make one.

- **NoteSeqB's note arrows line up with the steps they belong to** (#76). The piano roll kept
  a 16px piano-key strip down its left side, so its sixteen steps were squeezed into the
  remaining width while the arrow buttons below stayed on the module's own 12px grid: every
  arrow pointed at roughly the step after the one above it, and the key strip itself read as
  an extra empty first step. The strip is gone and the steps fill the display, one per arrow.
  The black-key lanes still mark the pitches.

- **Loop starts on in NoteSeqA and NoteSeqB**, as it does in the original editor. Both sat at
  off on a freshly added module.

- **The Vocoder's buttons can be read again** (#77). Mon, INV, Rnd and the five shift buttons
  are cyclic buttons 20px and under, which put them down the path meant for the single-letter
  mute and bypass toggles: it squares the button off to 13x13, so INV lost its I and Rnd came
  out as "...". That shape is now only used where the face is an icon or one letter, and any
  label too wide for its button shrinks to fit instead of being handed to the ellipsis. Mon
  also keeps its name in both states rather than reading "Off" when off.

- **The Vocoder's Rnd leaves the output gain alone** (#77). It shuffled every parameter on the
  module that was not a switch, the output level with them. It is declared in the theme as a
  call on the routing display, so it now moves what that display shows: the 16 analysis bands.
  FilterBank's Rnd, which is declared on the module, still takes the whole module.

- **`+` and `-` step the arrow-pair controls** (#76, #77). The keys looked for a knob or a
  slider under the pointer, so the controls that are already a pair of arrows, the Vocoder's
  bands and NoteSeqB's notes, had nothing to step; NoteSeqA, which has sliders, worked. The
  arrows are now targets like anything else, without a second floating pair appearing over
  them.

- **The numeric keypad's minus steps and zooms out.** Its plus always worked. JUCE's X11 layer
  folds the keypad onto the main keyboard before a component sees the key, and it folds
  KP_Subtract onto the soft hyphen rather than the ASCII one, so the code that arrived matched
  nothing. This was true of every knob and slider, not just the new arrow targets.

- **NoteSeqB's Gclk output is spelled the way NoteSeqA spells it** (#76). It read "Gclck".

- **A synth switched on after the editor connects on its own again** (#73). Starting the editor
  with the synth off sent the editor's hello into a dead cable, and the only route from
  disconnected to connected without opening MIDI Settings was an `IAm` from the synth, which a
  G1 does not reliably volunteer: it answers the hello it is sent. The editor therefore sat at
  "No response from synth (timeout)" with the synth's own chatter arriving all the while.
  Anything the synth sends of its own accord, a light frame, the slot mask, the voice counts, is
  now taken as proof that it is there and the hello is sent again, at most once every three
  seconds and only while disconnected.

### Changed

- **The About box names the people paying for this editor.** The Supporters section listed only
  a Patreon link; it now lists the backers on the 5 EUR tier and above by the name they use on
  Patreon, alphabetically, with no ranking by amount and nothing else from the membership export.
  Kept in `data/credits.txt`, so it is one file to update per release.

- **The mixers' input knobs are named "In level" rather than "In sense"** (#78). Clavia's own
  documentation calls them attenuation controls and never uses "sense", which came from the
  community's reverse-engineered parameter dump along with NoteSeqA's "active" above;
  EqMid and EqShelving already called the same kind of control an in level. The name is only
  ever seen in the Inspector, the hover badge and the right-click menus, since the knobs carry
  no label on the panel. The MCP bridge's `set_parameter` still accepts the old name, so a
  script that used it keeps working.

## 0.17.0 — 2026-08-19

### Added

- **Cables can be re-routed, not just cut** (#67). Hold `Ctrl` (`Cmd` or `Alt` also work)
  and drag a connector that already has a cable: the cable comes off that end and follows the
  pointer from the end that stays put, ready to drop on another connector. This is the gesture
  the original editor had, and it is what moving a patch's wiring onto a replacement module one
  cable at a time needs. Nothing happens to the patch until you let go: the cable is only hidden
  from the canvas while you carry it, so a re-route that lands nowhere legal costs nothing, sends
  nothing to the synth and leaves nothing on the undo stack. Where the connector has several
  cables the one drawn on top comes off first, and repeating the gesture takes the ones under
  it. The move itself is one undo step.

- **Drag a patch onto a slot to load it there** (#50). Works from both browsers: a patch in
  the synth's banks from the **Synth** tab, and a `.pch` from the **Disk** tab or the `Ctrl+B`
  window. Two places accept the drop: the slot's own sub-window, which is the obvious gesture,
  and its row in the slot bar down the left side, which is the one that still works when that
  slot's window is closed (it opens on the way). The target lights up while you are over it, and
  bank nodes and empty bank positions cannot be picked up at all. Each drop ends in
  the load that already existed, so nothing new goes to the synth: the bank ones in the same call
  as the right-click **Load to Slot A..D**, the disk ones in the same call as File → Open, minus
  the slot question because the drop already answered it. Snippets keep dragging onto the canvas
  as before.

  The `Ctrl+B` preset window could not start a drag at all until now, snippets included: it was
  missing the drag container that the same panel gets from the main window.

### Fixed

- **The patch stops reloading itself every few seconds** (#73). The canvas jumped back to its
  top-left corner on its own, and the status bar flashed "loading patch 1/13", with nobody
  touching anything. The G1 does not only answer an `IAm`, it announces itself: the SysEx dumps
  on the issue have one arriving roughly every three seconds for as long as the synth is on.
  The editor read each announcement as a connection that had just come up and re-ran its whole
  opening sequence, patch list and synth settings and a patch fetch included, so the patch was
  pulled down again on top of the one you were working in. An announcement from a synth the
  editor is already talking to now changes nothing. It is still listened to while disconnected,
  so a synth switched on after the editor still brings the connection up by itself.

- **The macOS menus show their keyboard shortcuts again** (#74). The Mac menu bar is the
  system's own, and it only prints a shortcut that comes from a JUCE command manager: the
  hint field that the Windows and Linux menus right-align is thrown away there. Moving the
  hints into that field in 0.16.0 (so the Mac would stop printing a literal tab, #56) left
  every Mac menu with no shortcuts at all. They are now written into the item's own text on
  macOS, in the symbols a Mac reads: **New Patch ⌘N**, **Save As... ⇧⌘S**, **Slot A ⌥⌘1**.
  Nothing changes on Windows or Linux.

- **Cable opacity moves to Editor Options, where every platform can reach it** (#74). It was
  a slider inside the View menu, and the Mac's menu bar is the system's own: it cannot host a
  custom control, so JUCE dropped it and left a blank line. The setting was untouchable on
  macOS. It now sits under **File → Editor Options**, next to the cable style it belongs
  with, as a percentage slider: the canvas follows it while you drag so you can see what you
  are choosing, and Cancel puts the old value back. It also survives a restart now, which it
  never did from the menu: the value went to the canvas and nowhere else, so it came back at
  80% on the next launch.

- **A patch opened from a bank backup no longer keeps the position number in its name.** Bank
  backups are saved as `NN - Name.pch` so the folder sorts by position, and a classic `.pch`
  carries no name inside it, so the reader falls back to the file name and the prefix travelled
  into the patch and on to the synth: loading `35 - BELLS++` stored it as `35 - BELLS++` instead
  of `BELLS++`. Restoring a whole bank already stripped it; now the reader does, so every way of
  opening one agrees, including the browsers, File → Open and a drag onto a slot.

- **An ABCD button that puts the slot windows back in order** (#51). Right of MUT in the header
  bar, its four letters drawn in the quadrants they land in so the button pictures the
  arrangement it produces. It re-tiles the open sub-windows into A|B over C|D, or A|B|C in thirds, or A|B side by
  side, depending on how many are open, and it puts the slots back into that order whatever
  order you opened or shuffled them into. It also drops focus mode and a layout you had dragged
  about, so one click is the way back to a patch you can read after loading four patches in
  whatever order they came. Greyed out when there is nothing left to put in order.
  **View > Slots > Reset Slot Order (ABCD)** is the same thing.

- **The editor can show itself on the synth's own display.** Off by default; turn on
  **Show the editor on the synth display** in Editor Options. The G1's display normally shows the
  active slot's patch name, and with this on any editor dialog borrows it: it reads `ANME 0.17v`
  while the window is up, and the patch name comes back when it closes. Nothing happens to the
  patch. Only the
  message that sets the name on the synth is sent, so the editor's own patch object is untouched,
  nothing is marked modified, nothing lands on the undo stack, and the bank-location matching that
  works by comparing patch names goes on seeing the real one. On the synth it changes the edit
  buffer only, never the bank. It is opt-in because an editor killed with a dialog open leaves the
  caption on the display until the patch is reloaded. The caption is also never sent while an edit
  or a transfer is already on the wire, and it is put back before the port closes on quit.

- **Pinch to zoom on a trackpad** (#72). The canvas zooms around the pointer on a two-finger
  pinch, the same zoom `Ctrl`/`Cmd`+wheel already did, so the gesture Mac users reach for first
  now works. The pinch multiplies the zoom rather than adding to it, which is what makes
  pinching out and back in land on the level you started from.

### Changed

- **Uni/Bip switches say which polarity they are in** (#69). Constant, LevMult, LevAdd and the
  Control Sequencer all have a polarity switch, and the inherited panel data labelled *both* of
  its states "Uni": the panel read the same whichever way the switch was set, and the only clue
  was the bevel. The button now reads **Bip** when it is out and **Uni** when it is in. The rule
  is keyed off the parameter the button drives rather than a list of modules, so a polarity
  switch that turns up later is labelled without anyone remembering to do it, and a test pins
  all four.

### Fixed

- **The empty-canvas hint is readable on every theme** (#70). "Press Enter to add modules" took
  its ink from the module text colour, which is black on the themes with light module faces
  (Nomad, Nord Classic) while the canvas behind it is nearly black. It now takes its ink from
  the canvas background it is actually drawn on: white on the dark canvases, black on Nord
  Classic's mid grey, which is the one canvas in the registry that falls on the light side.

- **The status bar follows the theme** (#71). The slot/connection readout and the centre message
  were an accent green and an accent orange that washed out on several palettes. Both now use
  the same ink as the Inspector's assignments, which is white on the dark themes and black on
  the light ones; the LED beside the readout still goes green when the synth is connected. The
  bar also re-reads the palette when the theme changes, which it never did: its colours were
  whatever the theme at startup had been.

## 0.16.0 — 2026-08-16

### Added

- **A unit test suite and continuous integration.** `tests/` covers the layers where a quiet
  regression costs the most: the patch codec (a patch is serialized, parsed back and serialized
  again, and the bytes must match), the SysEx envelope, the upload packetizer (the 166-byte
  packet rule from #39 and the transfer-closing packet from #40), and module placement
  including the #54 refusals. They run in seconds with `ctest`, need no synth and no display,
  and GitHub Actions now builds the app and runs them on every push, once normally and once
  under AddressSanitizer/UBSan. The packet cutting and framing rules moved into their own
  little module (`UploadPacketizer`) to be testable at all: byte-for-byte the same wire format,
  now with a test holding it there.

- **Every parameter of the selected module, listed in the Inspector, and editable there.** A
  module's face is knobs, and a knob is a few pixels holding 128 steps: fine for sweeping,
  useless for saying what a value actually is. Selecting a module now fills a **Parameters**
  section at the top of the Inspector with every control it has, each as its name and the figure
  it reads in its own units. Drag a value to walk it, or **double-click it and type one in**.
  Typing accepts the reading as it is written, so `440Hz`, `C#3` and `-12(Oct)` all land where
  you mean them to: the editor formats every step the parameter can hold and looks for the one
  you asked for, and a bare number picks the nearest step to it, so `9.5` finds the 9.45s next
  door rather than being refused. Whatever the module wears as a **button** is a button here
  too, carrying the same lettering its face does: a two-state switch flips and lights up while
  it is on, and a selector like the DrumSynth's LP/BP/HP walks round its options. Every edit
  goes out to the synth as it happens and undoes in one step, and the list follows the module
  rather than the other way round, so a knob turned on the canvas reads true here as it moves.

- **`+` and `-` step the control the pointer is over** (#66). The nudge arrows that pop under a
  hovered knob are easy to reach with a mouse and awkward with a touchpad, and the original
  editor took these two keys for the same job. They step whatever has the arrows under it, so
  the note on a NoteDetect can be walked to exactly the one you want without touching the
  pointer. Held down the key repeats, and the whole run is a single undo step, the same way a
  held arrow is.

- **The Inspector's sections fold away, and stay folded.** Parameters, Morphs, Knobs and MIDI CC
  each grew the chevron the Presets section already had, so a module with thirty parameters and
  no assignments doesn't push its presets off the bottom of the column. The choice is a display
  preference like the panel toggles: it is shared by every Inspector on screen and remembered
  between runs.

- **Nudge arrows under the knob you are pointing at.** A knob packs its whole range into a few
  pixels, so landing on an exact frequency or MIDI note by dragging one is guesswork. Hovering a
  knob or a slider now pops the same two little buttons the original editor shows: the left one
  steps the value down by one, the right one up by one, and holding either repeats. The value
  reads out while you step it, without the pause a plain hover waits through, and the whole
  press is a single undo step however many times it repeated. The four morph dials in the header
  bar have them too, sitting inside the dial rather than under it because the caption is directly
  below, so a macro can be set to an exact figure instead of swept to roughly the right place.

- **A patch loaded from the synth gets its comments, notes, variations and Mutator exclusions
  back.** None of that fits on the wire, so a patch read off the front panel used to arrive
  stripped of everything the editor knows about it, and only the `.pch` on disk had it. The
  editor now keeps its own copy of every patch's extras, one small file per patch in `extras/`
  beside the settings, and puts them back the moment it recognises the patch. Recognition goes
  by the id patches saved here carry in a new `[NME]` section of the `.pch`, and, for a patch
  coming off the wire, by a fingerprint of its name, modules and cables. The fingerprint
  deliberately leaves the parameter values out, so turning a knob never costs a patch its notes,
  and the previous fingerprints are remembered as the patch grows so the link survives editing.
  Nothing about this is on screen and there is nothing to set up: load PercDetect from the synth
  and your notes are on it. Opening a `.pch` from disk still lets the file win, and a patch you
  create is always given an entry of its own rather than being matched against other new patches,
  which all look exactly alike.

- **Comments on the patch: a text note you can drop into any empty space.** The module bar has
  a new **ANME** tab at the end, holding what this editor adds to a patch that Clavia never
  shipped, and a **Comment** chip is the first thing in it: click or drag it onto the canvas the
  way you place a module, or right-click the canvas and pick **Add Comment**. A two-row note
  lands where you put it, ready to write in: double-click it to type, drag it around, pull either
  bottom corner to make it as wide and as tall as you want, right-click for exact sizes or to
  delete it, `Delete` while it is selected does the same, and `Z` zooms to it. It copies, cuts,
  pastes and duplicates on the keys the modules use, `Ctrl+C`/`Ctrl+X`/`Ctrl+V`/`Ctrl+D`, and a
  pasted note hangs off the pointer as an outline until you click where it goes, so it can be
  copied into the other voice area or into another slot's window. Every one of those is undoable. It is painted as a module panel, in the same colour the modules around it wear
  under whichever theme you are using, and its text is centred and bold and grows with the panel,
  so a note you made big is a heading you can read across the patch. It holds its rectangle of the grid the way a module does, so dropping a module makes
  room for it and it makes room for modules. The synth never hears about it: the G1 has no such
  module, and nothing about a comment ever goes on the wire. Notes are saved inside the `.pch`,
  in a `[Comments]` section of the same kind as the `[Notes]` the patch already carries, so they
  travel with the patch when you share it and editors that don't know the section skip it.

- **A store button next to the patch name**, showing the bank location the patch on screen came
  from as `bank:position`. One click writes the patch straight back there, no dialog and no
  hunting: exactly what the front panel's STORE does, without leaving the editor. Each slot
  carries its own location, so with four sub-windows open the button always speaks for the slot
  you are looking at, and a patch with nowhere to go yet (just created, or opened from a file)
  shows `--` and falls back to the dialog. Loading from the browser or storing gives the editor
  the location outright. It has to work the rest out on its own, because the G1 never says where
  a patch came from: a front-panel load only reports which slot changed, and the patches already
  in the slots at connection time are announced by nobody. There the patch name is looked up in
  the bank list, and a single match settles it. Several positions with the same name leave the
  button showing `?`: a click then opens the dialog on the first match with the full list of
  matches in the status bar, and picking one settles that slot for good.

- **The Store Patch to Bank dialog opens on the patch's own location** instead of bank 1
  position 1 every time. Putting a patch back where it came from is now a single OK, and storing
  it somewhere else makes that new place its location from then on.

- **Save Patch As opens with the filename already typed in.** The name field used to come up
  empty, so every save meant typing the patch's name out again, and a patch read off the synth
  is normally saved under the name the synth gives it. The dialog now suggests the file the
  patch was last saved to, or, for a patch that has none yet, the patch's own name with `.pch`
  on the end.

- **Opening a patch can stop asking which slot** (#59). Editor Options gains **Ask which slot
  when opening a patch**; turn it off and every open goes straight into the slot on screen and
  uploads, which is what anyone working in a single slot wants.

- **Knobs assigned to the morph groups are listed in the Inspector** (#63). The Knob Floater has
  always shown them, but the Inspector's patch-wide view (click empty canvas, nothing selected)
  listed only the knobs assigned to module parameters, so the four that drive Macro 1 to Macro 4
  were invisible there. They now sit in the same Knobs section as the rest, named by the group
  they drive, and their **x** deassigns them like any other.

### Changed

- **The editor no longer keeps hold of modules it has deleted.** Selecting a module, hovering
  one, pointing at a knob or reading a DSP cost all left the editor holding the module itself
  in memory, and deleting it, or undoing the add that made it, left those pointing at nothing.
  That was the crash macOS users saw and Linux users never did (#61), and it was kept at bay by
  sweeping every one of them before each redraw. Modules are now identified by where they live
  in the patch rather than by where they sit in memory, so the question cannot be got wrong, and
  the sweep is gone. It also behaves better than it did: deleting a selected module and undoing
  it gets the selection back, where before the selection was simply dropped.

- **Big patches draw far faster.** Every time the canvas painted a module it worked out, from
  scratch and twice over, where every module in the patch keeps its LEDs and meters: a sorted
  table rebuilt once per module, per repaint. On a hundred-module patch that came to roughly
  18 ms of pure bookkeeping on every single redraw, and the synth asks for a redraw several
  times a second while the meters are moving. The table is now worked out once and reused
  until the patch's structure actually changes, which takes that 18 ms down to about 14
  microseconds. Cables no longer rebuild a lookup of every connector in the patch on each
  redraw either, and resting the pointer on a knob stopped redrawing the entire canvas on
  every mouse movement for the sake of the two little nudge arrows.

- **The DSP cost overlay moved to `F3` and focus mode to `F4`** (#55). F10 belongs to the menu
  bar on Windows and some Linux desktops, and macOS keeps F11 for Show Desktop, so both readouts
  now live on keys nobody else is holding. The old keys still work as quiet aliases for fingers
  that learned them.

- **On macOS, wireframe modules moved to `Cmd+Shift+W`** (#55). The naked `Cmd+W` is the
  system's own "close window" and fired both ways at once. Linux and Windows keep `Ctrl+W`.

- **Menu shortcuts sit right-aligned in their own column** (#56), instead of riding inside the
  item text, which the macOS menu bar printed literally, tab character and all.

- **A sequencer's Clr button now parks every step at its default** (#53): a CtrlSeq fader
  returns to centre (64) rather than being slammed to zero, which is a floor, not "empty".

### Fixed

- **Closing the editor with the slot chooser open no longer leaks it.** Open a patch, leave the
  "open into which slot" question on screen, quit, and the dialog and everything in it were
  left behind, which a debug build reports as a page of leaked-object warnings. The same was
  true of the store-location dialog. Both put themselves on screen and take themselves down
  when answered, so nobody owned one that was never answered; the editor closes them itself
  now. The same change also stops each of them deleting itself from inside its own button's
  click, which was a crash waiting for the right timing.

- **Select All takes the text notes too**, instead of every module and none of the notes.

- **Nudging a selection into an edge stops it as a block.** With several modules selected, the
  arrow keys clamped each one on its own, so at the border they ended up stacked on the same
  row. The selection stops when its leading edge reaches the edge, and nothing moves.

- **Undoing a delete gives the selection back.** The modules returned unselected, so recovering
  something you had just deleted meant finding and re-picking it.

- **Pressing A, B, C or D on the synth now brings that slot up on screen.** It looked like the
  buttons did nothing: the editor was adopting the slot and moving its own slot bar, but it only
  ever opened onto a sub-window that already happened to be open, so with the others closed
  there was nothing to see and no way to tell whether the press had landed. Opening a slot from
  the editor first made the panel buttons appear to start working, which is what made it look
  intermittent rather than broken. A slot button on the front panel is the user asking for that
  slot from the other end of the cable, and the editor follows it now the way it follows its own
  slot bar. It still will not do so in the middle of a gesture, where a slot press would cut a
  cable drag in half.

- **A module can no longer be buried under another at the bottom of the canvas** (#54).
  Dropping, pasting or growing something into a column with no room left used to clamp the
  push at row 128 and leave two things on the same rows; every placement now checks the whole
  chain of pushes first and refuses outright when the column cannot absorb it, the way the
  original editor does. A refused paste rolls the whole block back rather than leaving half
  of it placed.

- **Nothing can be dragged off the canvas any more.** Dragging a module (or a selection)
  kept going past the edges: below row 128 or beyond the last column it still existed but
  could not be seen or grabbed again. Drags, arrow-key nudges and the free-spot search now
  stop at all four edges, and the bottom bound accounts for the module's own height, so a
  tall module cannot be left hanging half out of the area either.

- **Error messages no longer outstay their welcome** (#65). "Failed to add module" was posted
  to the permanent status line and sat there for the rest of the session; it is a transient
  message now, and any status message can also be dismissed by clicking it.

- **The Help popup's description follows the theme** (#58) instead of staying near-white, which
  made it invisible on light themes.

- **Module help no longer shows `$Contents` and friends** (#57). The scraper that lifted the
  original manual's text dragged a few of its navigation pages in as if they were controls, on
  Constant, EQ Mid, LFOC, Oscillator slave FM and Sine Bank; they are filtered out now.

- **The Knob Floater follows the morph dials** (#64). A knob assigned to a morph group has a cell
  in the floater like any other, and it redrew when a module parameter moved but not when a morph
  did: dragging a morph dial in the header bar, stepping it with its arrows, recalling a
  variation or sweeping the Morph A/B fader all left that cell reading whatever it last showed.
  Every one of those now redraws it. A morph moved on the synth itself already did.

- **Renaming a module now reaches the synth.** The new name went into the editor's patch and
  waited there for a full patch upload, which meant that storing the patch to a bank right after
  the rename saved the module's old name: Store to Bank keeps whatever the synth already holds.
  Renames are now sent as they happen with the protocol's SetModuleTitle message, the same way
  the patch name has always been sent, so the synth is in step and the stored patch carries the
  names you gave. Undo and redo of a rename update the synth too.

## 0.15.1 — 2026-08-10

### Fixed

- **Deleting a selected module no longer crashes the editor on macOS** (#61). The Inspector holds
  a plain pointer to whatever module is selected, and deleting one redrew the Inspector *after*
  the patch had already destroyed it, so the panel read freed memory. On Linux and Windows that
  memory still happened to be readable and nothing showed; macOS segfaulted on the spot. The
  selection is now dropped before anything is deleted, and the Inspector and the canvas both
  check with the patch that a module is still there before touching it, which also covers undoing
  an Add Module or a paste while the new modules are selected.

## 0.15.0 — 2026-08-09

### Added

- **The module bar is back** (#17), the way the original editor presents its palette: a row of
  category tabs (In/Out, Osc, LFO, Env, Filter, Mixer, Audio, Ctrl, Logic, Seq) with the modules
  of the chosen category underneath, each in a thin outlined chip carrying its name. Drag a chip
  onto the patch area, or click it and drop it where you like, the same gesture Add Module has
  used since 0.14.0. The bar sits under the header, remembers which tab it was left on, and can
  be turned off from **View > Module Icon Bar** for anyone who prefers the text browser or Quick
  Add. Pictograms are what the original shows and what this will show too, but only once the
  artwork is ours and follows the theme (#52).

- **The frequency displays rotate their units when clicked** (#30), as the original does. An
  oscillator's or filter's box alternates between the frequency and the note it lands on; a slave
  oscillator's box goes round ratio, semitones and the frequency its master puts it at. Hovering
  reads out the units the box is *not* showing, so `-12` and the octave below are both one glance
  away. The choice is per module and is stored in the patch, in the same place the original keeps
  it, and never reaches the synth.

### Fixed

- **A patch with around a hundred modules can be uploaded again** (#39). The upload sent each of
  the sixteen sections as one packet, which the synth refuses once a section grows past about a
  kilobyte: it answered with a checksum error and the transfer died partway through, so patches
  such as `SY-1 RndBlips1` could be downloaded but never sent back. The patch now goes out as one
  continuous stream cut into packets of 166 bytes, which is what the original protocol library
  does and what the synth was built to receive.

- **An upload that fails no longer leaves the synth deaf** (#40). The synth waits in bulk-receive
  state until a packet arrives marked as the last one, and an upload that simply stopped — a
  rejected section, a timeout, unplugging mid-transfer — never sent one. From then on it answered
  nothing at all: no ACKs, no reply to the handshake, not even its idle stream, so it looked dead
  and appeared to need a power cycle. Every path out of an upload now closes the transfer.

- **Loading a patch from the synth's own bank refreshes the editor** (#41). A synth busy writing a
  large patch into a slot never answered the request that follows its own notification, and the
  editor gave up silently and sat on the previous patch. It now asks again, and a notification
  that arrives while another transfer owns the wire is held rather than dropped.

- **Buttons answer as readily as knobs** (#37). Every press of `KBT`, a mute, or a waveform
  selector put the same message on the wire twice and rebuilt the morph list and recounted the DSP
  cost, which walk the whole patch — work a knob does once per drag and a button was doing on
  every click. A parameter edit adds and removes nothing, so none of that can have changed.

- **Zooming the Note Sequencer's piano roll no longer retunes its first step.** The zoom is a
  display-only parameter, but it was being sent to the synth as an ordinary one, and its index is
  the same as `note 1`'s.

## 0.14.0 — 2026-08-08

*Never published: 0.14.0 was prepared but no binary went out, so everything below reaches
people for the first time in 0.15.0.*

### Changed

- **Paste and Add Module hand you the modules on the pointer** (#42, #36), the way the original
  editor does: instead of placing anything, they show the modules as outlines that follow the
  cursor, and the click that follows puts them down. `Escape` or a right-click calls it off.
  Because the click chooses the spot, it chooses the area too, so the same gesture pastes from
  Poly into Common, or into another slot's window, and the clipboard is now shared by the whole
  editor rather than kept per area. That closes the rest of what was reported: copies landing far
  away, no way to cross between the two areas, and no way to paste into Common at all. Dragging
  from the module browser is unchanged, since a drag already ends where you release it.

  Adding a module from the keyboard stays as quick as it was: `Enter`, a few letters, `Enter` to
  pick it, `Enter` again to drop it where the pointer already is. The mouse is only needed to put
  the module somewhere else. The outlines also appear the moment the command is given, rather than
  waiting for the mouse to move, which had left the `+` cursor showing with nothing attached to it.

- **Whatever is in the way moves down instead of being buried** (#36). Dropping a module on top of
  another one used to leave the older module hidden underneath, with no sign it was still there.
  Now the modules below shift down their column to make room, and anything they run into shifts
  down too. Undo puts the whole column back.

- **Paste and Duplicate can be undone**, which they could not before: both created their modules
  outside the undo history, so `Ctrl+Z` after either one did nothing. They now go through the same
  undoable insert the snippet browser uses.

- **Knobs start out answering vertical movement**, which is how the original editor behaves and
  what most people reach for first. Only the default changes: whatever you have chosen under
  **Ctrl+, > Knob Control** is left alone.
- **Circular knob control reads the dial, rather than counting turns.** Grab a knob at the point
  marked 100 and it goes to 100, instead of turning up by 100 from wherever it was. The angle is
  measured around the knob's centre over the same -135 to +135 degree arc the pointer is drawn
  with, so the spot you touch is the value you get, and the response no longer speeds up near the
  point you grabbed and crawls far from it. A knob does not move until the drag has actually
  moved, so a plain click leaves it alone and a double-click still just restores the default.

### Added

- **The Drum Synthesizer's factory presets ship with the editor** (#33): all 29 of Clavia's own,
  from Kick 1 to Perc 6. They sit in a **Factory** group, folded away by default in the Inspector
  and behind a **Factory** submenu on the module's right-click menu, so the presets you saved
  yourself stay where you can reach them. Factory presets cannot be renamed or deleted, and never
  touch your own preset files.
- **The overlay readouts are on the View menu** as well as on their function keys (#44), under
  **View > Overlays**, ticked to show which one is open. They had been on `F5` and `F7`-`F10`
  only, which is easy to miss: the request that prompted this was for a whole-patch module cost
  readout, which `F10` had been giving since 0.13.0.

### Fixed

- **Cut, Copy, Paste and Duplicate are on the Edit menu** (#42), where anyone would look for them.
  They have always been on the keyboard and on a module's right-click menu, and nowhere else.

- **Saving a selection as a snippet no longer overwrites the clipboard.** It read the selection
  through the clipboard, so anything copied earlier was quietly replaced.

- **A long-lived patch can no longer run out of module slots when inserting a snippet.** Snippet
  insertion numbered new modules one past the highest in use, and the number is stored in seven
  bits, so enough add-and-delete cycles pushed it past 127 and the insert failed. It now reuses
  the lowest free number, which is what adding a single module already did.

- **On macOS, showing and hiding a slot moves to `Cmd+Alt+1`-`Cmd+Alt+4`** (#49). macOS keeps
  `Cmd+Shift+3` and `Cmd+Shift+4` for its own screen capture and never passes them on, so two of
  the four slots could not be toggled from the keyboard at all. Nothing changes on Windows or
  Linux, where `Ctrl+Shift+1`-`Ctrl+Shift+4` works and is already familiar.

- **The slave LFOs no longer carry arrow buttons they never had** (#48). The arrows ran through
  LFOSlvA's Mono button and over the bottom edge of LFOSlvC and LFOSlvE. The original editor puts
  them only on the slave oscillators and the sine bank; on the LFOs and the two random generators
  the ratio is set with the knob beside the display, which is how it has always worked here too,
  so nothing loses a control.

- **The Drum Synthesizer's filter type was labelled the wrong way round.** HP and LP were
  swapped, so picking LP sent the synth the value it reads as HP: the label said one thing and
  the module played the other. Clavia's own factory presets settle it (their filter values are
  LP, BP, HP in that order), and every patch built or loaded now reads correctly.
- **A new Drum Synthesizer comes up the way the original's does**, with its own default settings
  rather than a middle value in every knob. Its preset box names the preset the module's settings
  match, as the original editor does, and reads `none` when they match nothing: before, it named
  the first preset in the list whatever the module was set to. Opening a patch no longer carries
  preset names over from the patch that was in the slot before it.

- **The editor no longer corrupts its own memory as it closes.** Both browser panels let their
  tree view outlive the item it was displaying, and JUCE writes to that item as the view goes
  away, so every session ended by writing into memory that had just been freed. It aborted on
  exit, which was invisible unless you had started the editor from a terminal, but a stray write
  like that can land anywhere, so it was worth chasing down.

- **The morph knobs follow the knob-control setting** (#47). They had always read vertical
  movement and nothing else, whatever the editor was set to, because the header bar carried its
  own copy of the knob-dragging code. There is one copy now, shared with the module knobs.
- **Turning a knob is no longer stopped by the edge of the desktop** (#46). The pointer is hidden
  while you turn, as in the original editor, and comes back on the knob when you let go, so a
  sweep covers the whole range wherever the knob sits on screen. This applies to module knobs and
  sliders as well as to the morph knobs, which suffered worst: they sit at the top of the window,
  a few pixels from the top of the screen.
- **Accented and typographic characters no longer come out as garbage** (#43). The slot chooser
  showed `Local (editor only â€ don't upload)` on macOS, and the same fault ran much deeper: every
  quotation mark in the module help text was mangled too, because JUCE decodes a plain C++ string
  literal as Latin-1 rather than UTF-8. All of the editor's own text is now plain ASCII, and the
  build fails if a non-ASCII character creeps back into a literal.

## 0.13.0 — 2026-08-02

- **The four slots now live inside the main window**, as sub-windows in the work area, the way
  the original Clavia editor and Nomad arranged patches. The per-slot pop-out OS windows are
  gone: they got lost behind other windows, could not be arranged, and meant every feature
  touching the canvas had to be written twice, once for the main window and once for a pop-out.
  There is one canvas now, wired once per slot.

  Open slots **tile themselves**, the way a tiling window manager does: one fills the work
  area, two split it down the middle, three go in thirds, four go 2x2, and the layout re-flows
  as you open and close them. **Ctrl+Shift+1..4** shows or hides a slot (so does right-clicking
  its row in the slot bar), **Ctrl+1..4** still switches to a slot and opens it if it was
  closed, and **F11** blows the focused slot up to the whole area and back again for a closer
  look, and each sub-window has a maximise button next to its close button that does the same.
  Dragging or resizing a sub-window leaves the windows where you put them from then on;
  **View > Slots > Tile Slots** re-flows them.

  The inspector, header bar, browsers and status bar stay shared and follow whichever slot has
  focus. Telling the synth which slot you moved to is now coalesced, so walking focus across
  four windows no longer sprays slot messages down the MIDI cable, and a front-panel slot press
  will not steal focus in the middle of a cable drag. Dragging a module out of the browser
  works into any sub-window, which never worked in the pop-outs.

  Which slot sits in which tile is yours to change: **Ctrl+Shift+** an arrow moves the focused
  slot to the neighbouring tile, so you can put the patch you are working on where you want it
  without closing and reopening anything. **View > Slots > Rotate Slots** shifts them all round
  at once. The
  sub-windows slide to their new places rather than jumping; turn that off with **Animate Slot
  Tiling** in Editor Options if you prefer it instant. F11 and the other transitions now start
  promptly and decelerate into place over 120 ms instead of hesitating and stopping abruptly.

  The focused sub-window is edged in the theme's primary colour (automatically changed to a
  light contrast when that primary is dark), so with several tiled
  you can always see which one you are editing; it stays out of the way when a single window
  fills the area, where there is nothing to tell apart. Its title carries the LOCAL badge when
  that slot's patch is not in sync with the synth. The inspector picks up whichever module that slot had
  selected instead of going blank, since each canvas keeps its selection while it sits in the
  background. Clicking a tiled sub-window also leaves keyboard focus on the clicked canvas, so
  its zoom and editing shortcuts keep working with two or more slots open.

  Which slots you had open, which one had focus, and how they were arranged all come back
  when you reopen the editor. Connecting to the Nord then lines the work area up with the slots
  the synth actually has enabled, once: after that the windows are yours alone, and pressing
  slot buttons on the panel never closes one.

  F11 now animates the real focused window instead of stretching a bitmap snapshot of it, so
  modules, cables and text remain correctly rendered throughout the transition. Multi-window
  re-tiling keeps the cheaper snapshot path because several canvases can move at once.

- **The application's own text is larger across the board.** Panels, browsers, the inspector,
  the header bar, the status bar and every dialog were each given their sizes by hand and had
  drifted small next to the rest of the desktop. They now share one scale, which lifts them
  together and keeps their relative weights intact. Module bodies on the canvas are deliberately
  untouched: their text sits inside a fixed grid that comes from the hardware.

- **Fixed: the macro captions were painted in their own macro colour**, which made green M2
  unreadable on the light Nord Classic chrome and left the four reading as separate things.
  M1 to M4 above the morph dials, and the Macro 1..4 headers in the Inspector, now use the
  theme's text colour: light on the dark themes, dark on Nord Classic. The dials and the colour
  stripes still carry the colour, which is what identifies a macro in the first place.

- **Load a synth patch into a slot you name.** Double-clicking a patch in the Synth browser
  still loads it into the slot you are on; right-clicking now offers **Load to Slot A..D**, so
  with several sub-windows open you can pull a patch into a particular one without leaving the
  slot you are working in first.

- **Removed the "Recycle Windows" editor option**, which has never done anything: it was
  saved, loaded and drawn as a checkbox, and nothing ever read it.

- **Fixed: "Press Enter to add modules" could pile up on itself** on an empty canvas. The hint
  was centred on whatever rectangle was being repainted rather than on the canvas, so a partial
  repaint left a copy behind at its own position.

## 0.12.0 — 2026-08-01

- **macOS builds run on High Sierra again**: the package claimed Catalina as its minimum,
  which left older Intel Macs out for no technical reason, since the editor uses nothing
  newer and the vendored JUCE goes back further still. Release builds now target macOS 10.13,
  the oldest the current toolchain can deploy to, and packaging verifies both the bundle's
  minimum version and the Intel binary's own, so the metadata cannot drift back unnoticed.

- **Parameter values read out on hover and on F5** (issue #32). Resting the cursor on a knob,
  slider, button or display box shows its value in a hint box, in the parameter's own units,
  so a cutoff reads "440 Hz" rather than "64". **F5** does the same for the whole patch at
  once: it existed already but only drew parameters assigned to a morph group, and showed the
  raw morph range ("+34") instead of a value, so the readout the original editor gives for a
  whole patch was in practice a morph inspector. A morphed parameter reads out the span the
  morph sweeps it across, matching the original's "46Hz-2.30kHz". **F7** is unchanged, since
  it is about morph assignments. Both remain toggles rather than hold-to-show. Dragging a
  control reads its value out straight away, with no hover delay, so you can see where a knob
  lands while you turn it. **Double-clicking a module** reads out what it costs the DSP, which
  is the last thing issue #31 was missing, and **F10** does the same for every module at once,
  which the original editor has no equivalent of and which is what you want when hunting for
  something to cut in a patch that is over budget. **F8** and **F9** complete the original
  editor's readouts, showing knob assignments and MIDI CC assignments over the parameters they
  are attached to.

- **The Filter Bank's input, output and bypass are back inside the module** (issue #35): its
  artwork was drawn 120px tall against a module that is 7 grid rows, or 105px, so the bottom
  row of controls fell outside the body. The row is rearranged rather than the module made
  taller, which would have overlapped whatever sat below it in existing patches: Min/Max/Rnd
  move to the left end, and the jacks and bypass take the right, with room between them. A
  sweep of all 110 modules found this was the only one whose controls escaped its body, plus
  a 1px groupbox hairline poking out of the Spectral Oscillator, also corrected.

- **Two sequencer fixes** (issue #34). **The arrow buttons step both ways again**: the four
  sequencer modules draw their arrows as a left/right pair, but the click was always split
  top/bottom, so which way the value moved depended on whether the upper or lower half of an
  arrow was hit, and both arrows appeared to do the same thing. Landscape arrow pairs are now
  split on x. Only the sequencers were affected; the other 50 arrow pairs in the editor are
  stacked and were always correct. **Clr now clears the steps rather than the sequencer**: it
  reset every parameter to its minimum, which took the step count down to 1 and the loop and
  transport settings with it. It now empties only the per-step values, the same set Rnd
  randomises, so Note Sequencer A and B, the Event Sequencer and the Control Sequencer all
  keep their step count, loop and transport settings when a pattern is cleared.

- **Per-module DSP cost is visible again** (issue #31): every module's share of the DSP
  budget now shows where you choose a module and where you inspect one. It appears next to
  each entry in the right-click Add Module menu ("Audio In (2.2%)"), in the module browser,
  on every Quick Add row, and on the Inspector's header for the selected module. The figure
  is rounded to two significant figures exactly as the original Clavia editor prints it, so
  a patch optimised against the hardware editor reads the same numbers here: all seven values
  quoted in the issue reproduce to the digit. Fixed alongside it, the Load meter was left
  showing the previous patch's cost after any edit that did not come from the canvas, since
  the shared repaint path redrew the meter without recomputing it; the MCP bridge and the
  Mutator both hit that.

- **Module presets are now a library, shown in the Inspector**: selecting a module puts a
  **Presets** section under its assignments, listing every preset for that module type with an
  x on each to delete it and a **Save current settings** button that captures the module as it
  stands. Recalling one is a single undo step rather than one per parameter. The same list is
  also on the module's own right-click menu as a **Preset** submenu, with a check mark on the
  one in use; it used to be reachable only by right-clicking the small preset display box, and
  even there it could delete but not recall.

  Underneath, presets are no longer specific to the DrumSynth: a preset is any module's named
  parameter snapshot, so enabling them for the sequencers or anything else costs no code. They
  live in a new **Presets** folder in the patch library, next to Patches, Snippets and Banks,
  as one `.pchp` pack per module type. The format is plain text, commented and hand-editable,
  since transcribing the original editor's own presets is done by hand. Values are keyed by
  parameter rather than by position, so a preset written with two lines in it sets those two
  parameters and leaves the rest of the module alone. Presets saved by earlier versions are
  migrated into the new library on first run.

- **About box** (About > About Animatek NME): version and build date, a "Copy version" button
  that puts the version, OS and JUCE build on the clipboard for bug reports, and the credits.
  The credits live in `data/credits.txt` and cover the original nmedit/Nomad developers, the
  people filing hardware-tested bug reports, and the Patreon backers.

- **Module DSP costs rebuilt from Clavia's own figures**: the per-module `cycles` table
  came from nmedit, where 47 of the 109 values fall outside the rounding interval of the
  figures the original Clavia editor prints in its insert-module menu, nearly all of them
  too high. That is why a full patch read just over 100%: Nocticore's MorgBass02 summed to
  exactly 100.50%, matching his report. Since Clavia shows only two significant figures,
  each published figure defines an interval; searching for a total DSP budget consistent
  with all 109 intervals at once yields a single answer under 12000, namely 3148 units, so
  one unit is 0.0318%. That pins the exact integer cost of 46 modules, and the remaining 63
  (mostly heavy ones, where two significant figures are coarse) take the interval midpoint.
  MorgBass02 now reads 100.03% and SY-1 RndBlips1 97.73%. Source screenshots are in
  `Implementaciones/Consumo módulos/`; issue #18.

- **The main window can hide its side panels** (issue #38): `Ctrl+I` was only wired in the
  pop-out slot windows, so it did nothing in the main window. It now collapses the left
  inspector column there too, and `Ctrl+Shift+I` collapses the right patch browser, handing
  the width to the canvas. Each panel remembers the width it was dragged to. The slot bar
  goes with the left column; slots stay reachable through `Ctrl+1`..`Ctrl+4`. Both are in
  the View menu with check marks.

- **Replacing a patch in a slot no longer crashes the editor**: creating an empty patch or
  loading a file into a slot that held a patch with knob or MIDI CC assignments killed the
  app outright (`SIGSEGV` in `Module::getContainerIndex` with `this == nullptr`). The detach
  block in `MainComponent::replacePatchInSlot` exists precisely to drop the editor surfaces
  that cache pointers into the outgoing `Patch`, but it called
  `InspectorPanel::clearModule()`, which re-arms the assignments list with `currentPatch`
  instead of releasing it. The old patch was then freed by `slotPatches[slot] = std::move(patch)`
  and, two lines later, `clearSnapshots()` → `resetMorphAB()` → `refreshMorphUi()` walked that
  freed object: `buildHwFromPatch()` read the stale `knobAssignments`, asked the
  half-destroyed container for a module index, and dereferenced one of the already-nulled
  `unique_ptr` slots. Both `ModuleContainer::getModuleByIndex` overloads now skip null
  entries rather than dereferencing them, which is what stops the crash, and
  `InspectorPanel::setPatch(nullptr)` genuinely detaches the assignments list (it used to
  fall through its `p != nullptr` guard and leave the pointer dangling). Reproduced through
  the MCP bridge's `create_patch` against a slot holding an assignment-carrying patch, and
  verified fixed against that same sequence. Note this makes the crash unreachable without
  removing the underlying use-after-free: `replacePatchInSlot` still detaches via
  `clearModule()`, which re-arms the assignments list with the outgoing patch, so
  `buildHwFromPatch()` still reads freed memory — it just no longer dereferences a null
  module. See the roadmap entry for the remaining half.

- **Manual brought up to date with 0.10/0.11**: the user manual still described
  the editor as of 0.9.0. It now covers the Inspector and its hardware knob map,
  the Voices control and the PVA/E load meters, slot pop-out windows (live synth
  updates, per-window Randomize/Save, `Ctrl+I`), the slot chooser and **Local**
  option when opening a patch plus the **LOCAL** badge, background prefetch and
  instant slot switching, undoable module rename, the Nord Classic theme and
  theme-aware chrome, and the disk browser's PCH2 filter. A new chapter 9
  documents the MCP bridge. Shortcuts were corrected (`Ctrl+,` for Editor
  Options) and extended (slot right-click/`Ctrl+click`, slot-window
  Randomize/Save), in the manual and in the in-app Help → Keyboard Shortcuts
  dialog alike. `mcp-bridge/README.md` gained the `rename_module`, `save_patch`
  and `store_to_bank` tools added in 0.11.0.

## 0.11.0 — 2026-07-24

- **Slot windows now track live synth updates and support Randomize/Save (issue #22)**:
  a popped-out slot window used to be edit-only — turning a physical knob (or moving a
  meter/light) on the front panel animated the main window's canvas but never the slot
  window's, even when it showed the very slot being touched, and Randomize/Save silently did
  nothing there. Now the live fan-out from the synth (parameter values, morph knobs, lights
  and meters) reaches the focused slot's own window too, and Ctrl+R / Ctrl+Shift+R (Randomize
  simple/Gaussian) and Ctrl+S / Ctrl+Shift+S (Save / Save As) act on that window's slot,
  honouring its own module selection. The top settings bar (macros, CPU/voice meters) stays
  main-window-only, matching the original Nomad editor.

- **Rapid Voices changes no longer corrupt the synth slot (issue #28)**: each voice
  increment re-uploads the whole patch, and pressing the Voices arrows quickly fired
  overlapping uploads — a second `uploadPatch()` clobbered the first's in-flight section/ACK
  state, the SysEx sections interleaved, and the synth's slot ended up corrupt (patch read
  back as name "Error" with 0 modules) after a `sc=0x7e code=6` warning. The voice-change
  upload is now debounced and coalesced: rapid presses collapse into a single upload for the
  final voice count, and a new upload never starts while one is still in flight.

- **Renaming a module is now undoable (issue #23)**: renaming a module — from the canvas
  context menu, the Inspector name field, or a slot window — previously changed the title
  outside the undo system (the callback only logged), so Ctrl+Z couldn't take it back. There
  is now a `RenameModuleAction`; all three rename paths go through it, in the main window and
  slot windows alike. A new MCP `rename_module` tool exposes the same undoable operation to
  bridge clients (existing modules could be created with a name but never renamed). The name
  lives in the patch/editor and reaches the synth on the next full patch upload.

- **MCP clients can now save a patch to disk (issue #23)**: a patch built through the bridge
  lived only in memory — there was no way to persist it without going to the UI by hand. A
  new MCP `save_patch` tool writes a slot's patch (and its `.var` sidecar) to a `.pch`;
  absolute paths are used as-is, relative paths resolve inside the configured patches folder,
  a missing extension defaults to `.pch`, and parent folders are created as needed.

- **MCP clients can store a patch to a synth bank (issue #23)**: a new `store_to_bank` tool
  (bank 1-9, position 1-99) uploads a slot's patch to the synth and writes it to the bank
  once the upload is ACKed, so an assistant-built patch can be persisted to the hardware, not
  just to disk. Requires a connected synth with its patch list loaded.

- **Slot chooser when opening a patch, with a Local option (issue #21)**: opening a `.pch`
  used to silently target the active slot and, when connected, always upload — so there was
  no way to just look at a patch without overwriting the synth's current slot, and no way to
  load into a slot you hadn't visited. Opening a patch (File > Open and both preset browsers)
  now shows a chooser listing A/B/C/D with the patch currently in each slot, defaulting to
  the active one, plus a **Local** option that loads into the editor only without uploading.
  Slots whose editor patch is not known to match the synth — loaded Local, or
  loaded/built while disconnected — carry a **LOCAL** badge in the slot bar; the badge clears
  once the patch is uploaded to, or fetched from, the synth.

- **Front-panel Voices arrows now reach the synth (issue #25)**: clicking the Voices
  up/down arrows in the header bar updated the on-screen number but sent nothing to the
  synth, so the voice count never actually changed — only Ctrl+P Patch Settings worked. The
  header bar's `voiceChangeCallback` was defined but never wired up. It now re-uploads the
  patch (the G1 stores the voice count in the patch header, so a re-upload is how the change
  propagates), matching the Patch Settings path, for both the main window and slot windows.

- **Windows build fixed (issue #24)**: the MCP-bridge "Copy" button's revert timer in the
  Editor Options dialog used a lambda init-capture initialised by a function-style cast
  (`SafePointer<...> (this)`), which MSVC refused to parse — Clang and GCC accepted it, so
  only the Windows build broke, with a cascade of C2440/C2119/C2512/C2660 errors. The
  `SafePointer` is now hoisted to a local before the `callAfterDelay`, matching every other
  call site in the codebase.

- **Patch Mutator no longer wrecks pitch and cutoff**: mutating or randomizing a patch
  slammed oscillator frequency, slave detune, filter cutoff and some LFO rates down to
  almost nothing — a filter at 88 could land on 1, silencing the patch, from a setting whose
  range should have moved it by about 23. Those parameters share an internal index with the
  hidden "display units" setting, whose range is 0–2, and the mutator was reading its limits
  by mistake. Present since the Mutator shipped in 0.7.0; Mutate and Randomize are affected,
  Interpolate and Cross are not.

- **MCP bridge: usable from a client that isn't sitting on the source tree**: the browsing
  tools returned far more than a client could hold — a library listing came to 185 KB, and a
  large patch 109 KB — and the connector names needed for cabling were only obtainable by
  dumping every connector of all 110 module types. There is now
  `describe_module_type` for a single type, `list_module_types` is compact by default with a
  category filter, and `list_modules` omits the `morph:` duplicates, reports each module's
  connectors, and accepts a `container_index` filter so a caller can read the patch
  structure first and ask for detail only where it matters. A new `mutate_patch` exposes the
  editor's own Mutator as one undoable, throttled step instead of a burst of individual
  parameter writes.

## 0.10.0 — 2026-07-23

- **Editing one slot is no longer blocked by another slot's transfer**: a patch upload
  held back parameter edits for *every* slot until it finished, and a patch fetch discarded
  them outright — so with a slot window open, turning a knob in slot A while slot B was
  uploading or downloading did nothing, or was silently swallowed. Only the slot actually
  transferring is held now. A related fix stops an upload or fetch acknowledgement meant for
  a background slot from overwriting the focused slot's patch identifier, which could send
  subsequent edits against the wrong patch. Verified on hardware, including a patch running
  at 100% DSP load with two slot windows open.

- **Calmer, consistent controls across every theme**: ordinary buttons, pressed labels,
  menu highlights, combo arrows, toggles and editable values now use neutral palette roles
  instead of unrelated green, orange or red accents. Patch Settings, Synth Settings,
  Editor Options, MIDI Settings, Store to Bank and bank-transfer dialogs now share the same
  button treatment; their section dividers are derived from the real control bounds so they
  no longer cross labels or fields. Editor Options also follows the standard `Ctrl+,`
  shortcut, and Inspector text is refreshed correctly when switching between light and dark
  themes.

- **Hardware knob-assignment map in the Inspector**: a compact four-panel, 18-LED diagram
  mirrors the physical Nord Modular knob layout beside the Assignments heading. Assigned
  knobs glow a bright lamp green and free knobs retain the hardware's dark-green unlit-lens
  colour, so both states read correctly on light and dark themes alike; the map updates
  immediately after edits and undo/redo. The Knob Floater's assignment lamps match.
  Morph-knob pointers now choose black or white against each knob's actual fill colour, while
  the Disk browser's oversized text filters have become compact vector-icon toggles that fit
  the narrow embedded panel.

- **New "Nord Classic" theme**: a light, warm-grey theme echoing the look of the original
  Clavia Nord Modular editor — flat grey module bodies (light `#bfbfbf`) over a darker
  lavender-grey canvas, black labels, grey knobs, indigo LCD-style value readouts and the
  classic signal-cable colours, all sampled from the original editor. (The previous "Classic"
  theme, which used Nomad's own colours, is now named **"Nomad"**; the little-used "Frost"
  theme has been removed.)

- **Subtle canvas grain texture on every theme**: a very light, seamless procedural grain
  is now drawn over the patch canvas, giving it a soft paper feel instead of a flat fill —
  most noticeable on Nord Classic but applied to all themes.

- **Chrome text is now theme-aware**: labels across the header, status bar, slot list,
  inspector (knob/CC/morph rows) and the module browser used hard-coded light greys that
  were unreadable on a light theme; they now follow the palette, so text stays legible in
  both light and dark themes. The empty-canvas "Press Enter to add modules" hint was fixed
  the same way. Settings-dialog titles/section headers (MIDI, Synth, Editor Options, Patch)
  drop the yellow accent for plain adaptive text — the bold weight already carries them — and
  the MIDI dialog's Connect/Disconnect button and status text now use themed colours instead
  of a hard-coded dark red/green. The header's morph (macro) knobs are filled with their
  group colour and outlined in a theme-aware ink so they read on any background, and the
  inspector gained a right-edge divider so adjacent modules don't blend into it.

- **Add-module search pops up at the mouse, not on another monitor**: on a multi-monitor
  setup the Quick-Add popup (Enter / double-click on the canvas) was clamped to the primary
  display, so it jumped to the wrong screen; it now opens on whichever monitor the mouse is on.

- **The menu bar follows the active theme**: File/Edit/… kept the colours of whichever theme
  was loaded first; it now repaints with the current theme when you switch.

- **Disk browser: hide legacy 2.10 patches**: a new **PCH2** toggle in the Disk browser's
  filter row hides legacy `.pch2` (2.10) patches, so your list can show only current patches.

- **DSP load shown with one decimal**: the Load meters (PVA / Σ) now read to one decimal
  place — e.g. `47.6%` — matching the original Nord Modular editor. The figure is a
  client-side estimate (the synth doesn't report its own load), so the previous whole-number
  rounding could nudge a 99.5%-cycle patch up to a misleading `100%`.

- **Theme menu checkmark follows the active theme**: switching themes (View menu, Ctrl+T or
  Editor Options) now moves the checkmark in the Theme submenu to the theme actually in use;
  on macOS the native menu previously kept the tick on the initially-selected theme.

- **MCP bridge — drive the editor from an AI assistant**: the standalone editor can now
  expose a local control channel that lets an MCP client (Claude Code, Claude Desktop,
  OpenCode…) work the patch as if it were you at the canvas — adding and moving modules,
  connecting and cutting cables, setting parameters, creating patches and opening them
  from your preset library. Everything goes through the normal undo system, so an
  assistant's edits are reviewable and undoable exactly like your own.

  It is **off by default** — no port is opened unless you ask for it. Enable it under
  *Editor Options → MCP Bridge*, where the panel also shows whether the bridge is
  listening and gives you the exact command to register it with your client.
  It listens on `127.0.0.1` only, is off the wire entirely when the toggle is off, and
  can be compiled out completely with `-DNME_MCP_BRIDGE=OFF`. The bridge is
  standalone-app-only — it is deliberately not built into the plugin, where several
  instances would contend for the same port.

- **Sequencer step numbers line up with their steps**: the 1–16 labels above the
  sequencer rows were drawn from a fixed left edge into an oversized box, so they
  drifted out of alignment with the LEDs below them. Labels can now carry their own
  size and centring, and the sequencer numbers are centred over their own step.

- **Reversed vertical selectors show the right labels**: on blocks whose selector is
  drawn bottom-to-top (LFOA/LFOB among others), the theme numbers its entries the
  opposite way round from the parameter value, so the displayed label could belong to
  a different setting than the one selected. Labels are now keyed by document order.

- **Patches with heavy editing no longer run out of module slots**: module indices are
  stored in seven bits, but new modules always took "highest existing index + 1", so a
  long session of adding and deleting modules could walk that counter past 127 and start
  producing patches the synth would reject. New modules now reuse the lowest free index,
  so add/delete cycles are no longer bounded by how many modules you ever created.

## 0.9.0 — 2026-07-21

- **Collapsible inspector in slot pop-out windows**: a slot's own window can now hide
  its Inspector panel (morph/knob/MIDI-CC assignments) to give the canvas the full
  width of the window — click the thin arrow strip at the canvas's left edge, or press
  `Ctrl+I` while the window is focused, to toggle it back and forth.

- **Post-release code review fixes** (found during a review pass right after 0.8.2's
  work): a stale internal flag could let a real patch fetch get mistaken for an
  abandoned background prefetch and corrupted; opening a `.pch` file while viewing a
  slot that didn't have hardware focus could silently upload it to the wrong physical
  slot; a queued batch of parameter changes for one slot (e.g. a Mutator audition in a
  background window) could be wiped out by an unrelated fetch on a different slot;
  morph keyboard (velocity/note) assignment could target the wrong slot; and importing
  a legacy 2.10 patch with a morph keyboard assignment set could silently corrupt that
  assignment (an over-broad string match also caught it, not just the intended output
  routing fix).

## 0.8.2 — 2026-07-20

- **Pop-out windows for editing 2+ slots at once**: right-click a slot row in the slot
  bar to open that slot's patch in its own window — cables, modules, parameters, morph/
  knob/MIDI-CC assignment, rename and undo/redo all work independently there, alongside
  the main window's A–D tabs (which keep working exactly as before). Edits made in a
  background slot's window land correctly even without hardware focus — confirmed on
  real hardware that the synth accepts a parameter/cable edit addressed to a
  non-focused slot. When the synth's own front-panel focus changes to a slot that has
  a window open, that window is brought forward and its title marked "- Focused",
  mirroring the original Nomad editor's highlighted title bar for the focused patch.

- **Background-prefetch every enabled slot on connect**: as soon as the editor learns
  which of the 4 slots are actually populated on the synth, it downloads their patches
  in the background (one at a time, same as the original Nomad editor), not just the
  focused one. Switching to any of them for the first time after connecting is now
  instant instead of triggering a 13-section fetch. A genuine slot activation (pressing
  a slot button on the rack) always takes priority over this background work.

- **Fixed the preset browser only showing the first few banks on startup**: connecting
  starts an 891-patch name fetch, but the synth's slot-activation notification (which
  triggers loading the current patch) arrives a moment later and aborted the name fetch
  partway through — commonly leaving only banks 1–6 populated until the user manually
  hit refresh. The name fetch now resumes automatically once the interrupting patch
  load finishes, instead of silently staying partial.

- **Console log now marks each patch load** with a `===== LOAD PATCH: slot=X source=... =====`
  line (synth fetch, bank load, or disk file), making it easy to isolate one load's log
  lines when copying console output between patches for a bug report.

- **Fixed large patches losing cables and modules on Linux** (root cause behind #15's
  hidden cables on complex patches): JUCE 8's ALSA MIDI input feeds each ~256-byte
  sequencer event straight to its MIDI-2.0 (UMP) conversion layer, which only handles
  complete messages — a SysEx patch section bigger than one chunk was silently
  truncated after the first chunk. Real patches routinely exceed this: a 65-cable
  CableDump section is delivered in several ALSA chunks, and only the first ~57 cables
  ever reached the parser, with no error reported to the user. Patched the vendored
  JUCE submodule (`packaging/patches/juce-alsa-sysex-reassembly.patch`, auto-applied by
  CMake) to reassemble chunked SysEx into a single message before decoding.

- **Instant slot switching**: changing between slots A–D no longer re-downloads the
  patch from the synth when the editor already holds a model that matches the
  synth-side content (delivered by a complete fetch or a finished upload). The model
  is still re-fetched when the patch in the slot genuinely changes on the synth
  (program change, bank load) and on reconnect.

- **Reliable patch fetches from a busy synth** (#15): a rack running at 99–100% DSP
  load answers patch-download requests slowly; the editor used to time out and
  silently keep a partial patch — cables and parameters went missing, editing desynced
  the rack, and saving produced broken .pch files with invalid cable nets. The editor
  now re-requests only the missing sections (up to two retries), drops duplicated data
  from slow replies, and shows a clear warning if the load is still incomplete. The
  bank-backup watchdog was extended to cover the retry window.

- **Morph highlighting on every control type** (#16): 4-1 selector switches, toggles,
  increment buttons and sliders assigned to a morph group now show the group color in
  the patch area — colored fill on the selected segment plus a thicker border — the
  same way knobs already did.

- **Legacy 2.10 patches now play out of outputs 1/2** (#14 follow-up): the old 2.10
  format stores output destinations 1-based, so importing them verbatim routed every
  legacy patch to outputs 3/4 and they appeared silent. Destinations are remapped on
  import; all 857 known factory patches load with the correct 1/2 routing.

- **Windows**: the MSVC runtime is now statically linked, so the app no longer requires
  the Visual C++ Redistributable 2015–2022 (x64/x86) to be installed on a fresh Windows
  machine.

## 0.8.1 — 2026-07-04

- **Chained input→input cables**, matching the original editor: drag from a connected
  input to another input to daisy-chain a net (e.g. OscA1 Pitch → OscA2 Pitch, or
  2 Outputs L → R). The one-output-per-net hardware rule is enforced during drag and on
  drop. Fixed the binary patch decoder, which misread chained cables fetched from the
  synth (the type bit belongs to the source end; the destination is always an input),
  and the serializer, which stripped the chain on upload.

- **Rendering performance pass**: light/meter SysEx messages that re-send unchanged
  values no longer wake the UI; when values do change, only the affected modules'
  rectangles are repainted instead of the whole canvas; cable painting uses a
  connector→module map built once per paint and skips cables outside the repaint
  region. Large patches render dramatically faster and idle CPU drops to near zero.

## 0.8.0 — 2026-07-04

- Saving a patch now writes the **current** values of custom module controls (sequencer
  events, clock-divider displays) instead of the values captured when the patch was
  loaded — edits made to sequencer steps since loading are no longer reverted in the
  saved .pch file.

- Custom control values are applied after the whole patch is parsed (file loads and
  synth fetches), so a CustomDump section arriving before its ModuleDump is no longer
  silently dropped.

- Legacy 2.10 patches: decoded the `Ih` cable field correctly — bit 6 distinguishes an
  output source from a daisy-chained input source. Chained cables (about 5% of all
  cables in real 2.10 patch libraries) previously connected to the wrong connector.

- Track the synth patch id per slot. A patch fetch, upload or `NewPatchInSlot`
  notification for a background slot no longer overwrites the focused slot's patch id,
  and queued structural edits are stamped with the pid of the slot they were built for —
  preventing cross-slot contamination when several slots are in use.

- **Fixed slot handling to match the hardware's two-level slot model.** The synth
  distinguishes *enabled* slots (fixed LED, several can be on at once) from the single
  *selected* slot (blinking LED). A plain click now reproduces the panel's slot-button
  rule: the newly selected slot is always enabled, the slot being left turns off unless
  it was pinned, and pinned slots stay on. Ctrl+click enables/disables a slot without
  selecting it (`SlotsSelected` with the full 4-slot mask), like Shift+button on the
  panel and Ctrl+click in the original 3.3 editor.
  Previously the editor rewrote the enable mask with a single bit on every slot change,
  silently disabling every other enabled slot. The editor now tracks the real enable
  mask (from `SlotsSelected` notifications and the extended synth settings on connect),
  never sends a mask before the synth has reported its actual state, and the initial
  patch fetch targets the really selected slot instead of always slot A.

- The slot bar now shows a hardware-style LED per slot: fixed = enabled, blinking =
  selected, off = disabled.

- Patches fetched for a background slot (for example a `NewPatchInSlot` notification from
  a non-focused slot) no longer hijack the editor's current slot — parameter edits keep
  going to the slot you are working on.

- Updated the application icon to the new Animatek artwork from
  `Implementaciones/icon.png`, wired through the JUCE app/plugin icon metadata, bundled
  binary resources, and runtime window/taskbar icon.

- Fixed patch data loading issues reported in GitHub issues #13 and #14: synth/file
  `CustomDump` values are now applied to custom module controls (sequencer events,
  clock-divider displays, etc.), and legacy Nord Modular `2.10` `.pch` files with
  `[Module N]` sections now load their modules, positions, names, parameters, and input
  cables instead of opening as empty patches. Legacy module rows are normalized per column
  so the old compact coordinates do not stack modules on top of each other in the modern
  canvas.

- Fixed the real-time meter channel ordering used by VU indicators (#12). Meter values
  are stored in wire order (even slot = channel B, odd = channel A) exactly like NOMAD's
  LightProcessor, and the per-light channel choice happens at render time: a stereo
  module's left meter reads channel A and its right meter reads channel B, while modules
  with a single light — sequencer step led-arrays and gain-reduction meters — read
  channel B. This fixes both the swapped VU channels and erratic sequencer step LEDs.

- The disk preset browser now marks legacy Nord Modular `2.10` patches with a distinct
  purple `PCH2` tag so they can be distinguished from modern `.pch` files at a glance.

- Legacy `2.10` detection is now shared between the file loader and the preset browser
  (both look for the `Version=Nord Modular patch 2.10` header line), so a modern patch
  whose notes mention `[Module` text is no longer misrouted to the legacy loader. The
  browser also sniffs files lazily as rows become visible instead of reading every file
  during the library scan, keeping large libraries snappy.

- Fixed overlapping dB scale digits on the Audio In module: the renderer no longer draws
  its synthetic meter scale when the classic theme already ships printed scale labels
  between the meters. Modules without printed scales (PolyAreaIn, Expander, Compressor)
  keep the synthetic scale.

## 0.7.0 — 2026-06-20

- **Hardened Mutator/variation parameter delivery across patch transitions.** Pending
  parameter changes are now invalidated before slot switches, patch requests, bank loads,
  full uploads, and disconnects, with a context generation as a defensive backstop.
  Interpolations stop before the active slot or patch changes, preventing their next timer
  tick from applying old module indices to a different patch. Parameter edits made during a
  full upload wait and coalesce until it completes; edits are suppressed during patch fetches
  so they cannot interleave with the request stream.

- Added a **Wireframe modules** mode — toggle in Editor Options → Behaviour, in View →
  Wireframe Modules, or with **Ctrl+W** (persisted, works with any color theme). When on, the
  canvas draws module frames, group boxes, knob bodies, text displays and icon boxes as
  outlines only — the grid shows through — while cables, LEDs, meters and labels keep their
  colors so the patch stays readable. Each module gets a crisp outline; morph-group color
  moves to the knob ring so assignments stay visible. On Classic-style themes (where the dark
  module text would vanish) the outline/labels use each module's own color, so module types
  stay distinct. (Buttons, sliders and curve "screens" keep their fills for now since they
  encode state/values.)

- Reworked the **color theme set** (13 total, cycled with `Ctrl+T` or in View → Theme), with
  **Nord** as the new default: Classic, Dark, Deep Dark, Tokyo Night, Gruvbox Dark, Dracula,
  **Nord**, plus six **Bitwig community themes** — **Ghosty**, **Frost** (light/frost),
  **Magnetic Revival**, **MothWig** (teal night), **Macchiato** (Catppuccin Macchiato), and
  **Cubitwig**. The Bitwig palettes are distilled from each theme's Bitwig Theme Editor file
  (Berikai/awesome-bitwig-themes): the window background seeds a tinted 5-step ramp and the
  Bitwig "Panel" accent slots map onto our cable/accent colors. The theme menu uses a wider id
  range, so more themes can be added freely. Opening Editor Options no longer resets the
  canvas theme.

- The preset browser **type pills** (Patch / Snippet / Bank) now use three different color
  families — Patch = cool blue, Snippet = green, Bank = warm orange — so they no longer blur
  together (Patch/Snippet were near-identical text colors, and Patch/Bank were both warm).
  Added a cool `accentInfo` color to every theme palette to drive the Patch pill; all three
  pills follow the active color theme.

- Added the **SysEx Monitor** floater (View menu, `Ctrl+9`): a live hex log of MIDI SysEx
  traffic to and from the synth (TX/RX with relative timestamps and byte counts). Built for
  diagnosing protocol issues in release builds with no console and on macOS/Windows. Toggles
  for Enable, TX/RX filtering, and Auto-scroll, plus Clear and Copy. Capture and polling run
  only while the window is open and enabled — when closed it costs nothing (the capture hook
  in the MIDI path is a single atomic check when disabled). Captures every inbound SysEx,
  even non-Nord frames, so unexpected traffic is visible too.

- Added a **Send speed** selector in Editor Options (synth parameter throughput): Safe
  (~160/s), Balanced (~400/s, default), Fast (~800/s), and Maximum (~1600/s). Higher is more
  responsive for the Mutator but riskier on big patches; the setting applies immediately
  without restarting and persists across sessions.

- **Fixed the synth dropping the connection when mutating large patches.** Applying a
  Mutator/Randomize snapshot or an interpolation used to spawn a fresh throttled send chain
  per apply with nothing cancelling the previous one, so auditioning several children or
  mutating repeatedly on a big patch stacked parallel chains (~1000 SysEx/s) and overran the
  G1 until it stopped responding. Parameter delivery now goes through a single coalescing
  queue keyed by parameter, so a newer value for a parameter overwrites the pending one
  instead of piling up — no overlapping chains, smoother interpolations, and rapid auditions
  discard intermediate values.

- **Improved the Patch Mutator's musical results** using the papers Clavia's design drew on
  (Karl Sims 1991; Palle Dahlstedt 2004):
  - Mutation now uses a **Gaussian distribution** (small changes far more likely than large
    ones) instead of uniform random offsets, and the default knobs match Sims' recommended
    values (probability 0.20 / range 0.40).
  - **Oscillator pitch mutations snap to musical intervals** (5th, octave, etc.) rather than
    arbitrary values, preserving the harmonic ratios that make FM/modular patches usable.
  - **Cross** has a new **independent per-gene** mode (Seq/Ind toggle) alongside the original
    sequential crossover.
  - The **1/2/4 Output modules are never mutated** (they set overall volume and routing, not
    timbre), even with Solo active. Parameter-less modules were already left untouched.

- Updated the **About → Animatek NME Website** menu link to the current
  `https://animatek.net/animatek-nme-eng/` page.

- The 8 header snapshots are now full **Patch Variations** (G2-style): they capture all
  parameter values plus the four morph knobs, persist in a `<patch>.var` sidecar file next
  to the `.pch` (which stays 100% compatible with the original editors), and reload when the
  patch is opened. Right-click a variation for **Copy to 1-8** and **Init (default values)**
  alongside the existing interpolation-time menu. Live parameter edits write through into
  the active variation, and switching slots now restores the variation button states
  (previously they appeared empty after a slot change).

- Added the **Patch Mutator** (View menu, `Ctrl+8`, or the `MUT` header button) — an
  interactive-evolution sound breeder modeled on the Nord Modular G2's Patch Mutator:
  - **Mother + 6 Children + Father** boxes with chromosome-graph previews, plus a 24-slot
    **Temporary Storage** and a **Variations row** mirroring the header variations
    (drag any sound onto a variation to store it).
  - Operations: **Mutate** (probability/range knobs, linked G2-style), **Randomize**,
    **Interpolate**, and **Cross** (crossover probability knob). Generation knobs are drawn
    like the canvas module knobs with their percentage readout above.
  - Clicking a sound **auditions it on the synth** (throttled parameter deltas); the
    **Audition selector** (Instant/0.5s/1s/2s/5s/10s) morphs smoothly to the clicked sound
    using the snapshot interpolation engine.
  - **Quick Locks** with Solo per category (OscFreq, OscFine, Envelope, SeqValue, SeqEvent,
    Delays, Effects), derived from the module descriptors.
  - Right-click any module on the canvas for **Exclude from Mutation** (red frame while the
    mutator is open; saved in the `.var` sidecar). `Parameter` locks are respected too.
  - G2 keyboard map inside the window: `1-8` focus, `O/T` to Mother/Father, `E/U` mutate,
    `N` randomize, `I` interpolate, `X` cross, `S` to temp storage, Shift+drag interpolates
    two sounds, Ctrl+drag crosses them.

- Added **`Ctrl+5..9` shortcuts to toggle the floaters** (Knob, Keyboard, Patch Notes,
  Patch Mutator, SysEx Monitor). The shortcuts — including `Ctrl+1..4` slot switching — now
  also work while a floater window has keyboard focus, so pressing the same shortcut again
  closes the floater. Fixed `Ctrl+8` never matching on Linux: X11 delivers it as the legacy
  DEL control character (0x7F) instead of the digit.

- **Lighter floater handling.** Floater windows are now plain windows: they come to front
  when opened but no longer stay glued in front or re-raise on every click. The previous
  always-on-top / per-click re-raise made context menus and right-clicks feel sluggish
  (`toFront` and always-on-top are costly on Linux compositors). Floaters can now fall behind
  the editor like normal windows.

### Known limitations

- The desktop editor remains the supported product; VST3/CLAP targets are experimental.
- Patch variations and mutation exclusions live in a `.var` sidecar. Keep it next to the
  corresponding `.pch` when moving a patch if those extras should travel with it.
- Balanced is the recommended synth send speed. Fast and Maximum depend on the MIDI interface
  and patch size and may be less reliable on some setups.
- MIDI notes stuck upstream of the PC port cannot be cleared by the editor; use the synth's
  front-panel panic function.

## 0.6.0 — 2026-06-11

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
  [manual/07-shortcuts.md](manual/07-shortcuts.md) and in-app under **Help → Keyboard Shortcuts**.
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
