# Nomad2026

A modern reimplementation of the **Nomad** editor for the **Clavia Nord Modular G1** synthesizer,
built with JUCE/C++ as a cross-platform native application.

## About

The Nord Modular G1 is a legendary modular synthesizer from the late 1990s that is programmed
via a computer editor. The original Nomad editor was written in Java and is no longer compatible
with modern Java runtimes. Nomad2026 aims to bring this editor back to life as a native
application that runs on macOS, Windows, and Linux without requiring Java.

## Development Status

### Completed
- [x] Original editor running and verified (JDK 8)
- [x] Technical research and protocol documentation
- [x] JUCE project setup with CMake
- [x] MIDI SysEx protocol implementation (C++)
- [x] Synth connection manager (auto-connect, IAm handshake)
- [x] Patch retrieval from synth (RequestPatch + 13-section GetPatch flow)
- [x] Patch data parser (modules, cables, parameters, morphs, knob/ctrl maps, names, notes)
- [x] Module descriptions loader (110+ modules from modules.xml)
- [x] UI framework (menu bar, module browser, patch canvas, inspector, status bar)
- [x] MIDI settings dialog with port persistence
- [x] Pixel-perfect module rendering (classic-theme.xml: connectors, knobs, sliders, labels, text displays, lights)
- [x] **Real-time VU meters and LEDs**: synth sends NMInfo sc=0x39 (lights) and sc=0x3A (meters); meters animate green/yellow/red, clip LEDs activate at ledOnValue threshold
- [x] **Module rendering pass (in progress)**: output connectors=squares, input connectors=circles, multiline labels, flat module background, Fira Sans bold titles, top/bottom separator lines
- [x] Radio-selector buttons (multi-option with highlighted selection)
- [x] Increment/arrow buttons
- [x] Custom display renderers (ADSR/AD/AHD envelopes, LFO waveforms, filter response, overdrive/clip curves, EQ, compressor/expander, phaser)
- [x] Cables with signal-type colors, dark outline, rendered on top of modules
- [x] Bidirectional parameter control (click knobs/buttons/sliders to modify values)
- [x] Status bar parameter indicator (show parameter name + value on hover)
- [x] Real-time parameter changes sent to synth with MIDI feedback
- [x] Patch header bar (patch name, voices spinner, load meters, morph knobs with colors, cable visibility toggles)
- [x] Cable visibility toggles (click colored circles to hide/show cables by signal type)
- [x] Morph knobs with per-morph colors (red, green, blue, yellow — matching original editor)
- [x] Correct patch name and module name parsing (null-terminated PDL2 strings)
- [x] Patch file I/O (.pch load/save) with PchFileIO
- [x] Real-time patch synchronization (PatchSynchronizer: cables, modules, parameters sync to synth)
- [x] Protocol messages: NewCable, DeleteCable, MoveModule, StorePatch, GetPatchList, LoadPatch
- [x] **Synth Patch Browser** (right panel):
  - Interactive hierarchical tree (9 banks, 99 patches each = 891 total)
  - Real-time search filter by patch name
  - "Hide Empty" toggle to show only patches (not empty slots)
  - Refresh button to reload from synth
  - Double-click any patch to load it into current slot
  - Auto-loads patch list on connection
  - Context menu (right-click): Copy, Move, Delete patch operations
- [x] **Patch Name Editing**: Double-click patch name in header bar to rename (max 15 chars, syncs to synth immediately)
- [x] **Quick Save Button**: Diskette icon next to patch name — saves current patch back to its original bank location
- [x] **Universal Location Selector**: Consistent Slot/Bank/Position dialog for all save/copy/move operations
  - Position dropdown shows patch names from selected bank
  - Updates dynamically when bank selection changes
  - Shows "01: PatchName" or "01: --" for empty slots
- [x] **Help Menu**: Links to Nord Modular forum, Facebook group, and patch archive
- [x] **About Menu**: Links to Patreon, GitHub source code, and project website
- [x] **Store to Bank** (Device menu): Uploads the editor patch and stores it directly to a selected synth bank location
  - Optional "Store to Bank" dialog after upload
- [x] **New Patch** (File menu): Creates empty Init Patch in editor
- [x] **Split Poly/Common Canvas**: Two independent scrollable panels (Poly top, Common bottom), each 128 rows tall
- [x] **Slot Tabs (1-4)**: Tab bar at top of main layout for the 4 synth slots
- [x] **Multi-module selection**:
  - Rubber-band drag on empty canvas area to select multiple modules
  - Shift+click to add/remove modules from selection
  - Visual highlight: yellow border + semi-transparent overlay on selected modules
- [x] **Multi-module move**: Drag selected group maintaining relative positions
- [x] **Copy/Paste modules** (Ctrl+C / Ctrl+V): Copies modules with their parameters and internal cables
- [x] **Duplicate** (context menu): Duplicate selection with or without internal cables
- [x] **QuickAdd popup**: Press Space or double-click empty canvas → searchable module list, Enter/click to add
- [x] **Parameter context menu** (right-click knob/slider/button): Assign/remove Morph Group (1-4)
- [x] **MorphAssignmentMessage**: New protocol message — assigns a parameter to a morph group
- [x] **MorphRangeChangeMessage**: New protocol message — changes morph range/direction for a parameter
- [x] **Inspector morph integration**: onMorphGroupChanged / onMorphRangeChanged callbacks sync changes live to synth
- [x] **Code robustness review** (14 bugs fixed):
  - *Critical*: `QuickAddPopup` safe destructor — `PatchCanvas::~PatchCanvas` clears callbacks before deleting popup, prevents dangling-pointer crash on window close
  - *Critical*: `dragState` reset on `setPatch()` — raw pointers into old patch cleared before loading new patch
  - *Critical*: `const_cast<Parameter*>` eliminated — `findParameter()` non-const overload returns `Parameter*` directly
  - *High*: `uploadCompleteCallback` and all dialog callbacks use `SafePointer<MainComponent>` — safe if window closed mid-upload
  - *High*: `synthErrorCallback` captured by value in async lambda — safe if `ConnectionManager` destroyed before async fires
  - *High*: `MorphListComponent` saves `paramIndex` before `rebuild()` invalidates rows vector
  - *High*: `ParameterEncoder` logs unknown module types instead of silently returning empty vector
  - *Medium*: `onPatchDelete/Copy/Move` browser callbacks use `SafePointer` — safe against MainComponent destruction while dialog open
  - *Medium*: `dragState.module` null guard added to `MorphRange` drag handler
  - *Medium*: Dead code loop removed from `duplicateSelection()`
  - *Low*: Timeout constants documented with rationale (8s patch, 2s stale, 10s patch list)
  - *Low*: `duplicateSelection` stores section in `oldToNew` map — eliminates O(N²) re-search after module creation
- [x] **Morph system bug fixes** (6 bugs fixed):
  - *Critical*: Inspector dangling pointer crash — `clearModule()` before replacing patch prevents SIGSEGV when `refreshMorphList()` accesses destroyed module (all 3 patch-replace paths: synth load, file load, new patch)
  - *Critical*: Morph assignments lost after full patch upload — suppress auto-refetch (`NewPatchInSlot`) after upload; `currentPatch` is already authoritative
  - *High*: `getParameter(int)` matched wrong parameter on modules with overlapping indices (e.g. FilterF: custom "freq display units" and regular "frequency" both index=0) — now filters to `paramClass == "parameter"` only
  - *Medium*: "Zero Morph" now fully removes morph assignment (group + range) instead of only zeroing the range
  - *Medium*: Default morph range changed from 64 to 0 on new assignments — matches synth expectation
  - *Low*: Bidirectional morph range sync between canvas (Ctrl+drag) and inspector panel
- [x] **Hardware Knob Assignment** (parameter context menu → Knob):
  - Assign any parameter to Knob 1-18, Pedal, After touch, or On/Off switch
  - Shows current assignment with tick mark and "(used)" indicator
  - Reassign between knobs or disable assignment
  - KnobAssignmentMessage protocol: SysEx sc=0x25 (new) / sc=0x26 (reassign/deassign)
- [x] **MIDI Controller Assignment** (parameter context menu → MIDI Ctrl):
  - Assign any parameter to MIDI CC 0-119
  - Shows current assignment with tick mark
  - Reassign between CCs or disable assignment
  - MidiCtrlAssignmentMessage protocol: SysEx sc=0x22 (new) / sc=0x23 (reassign/deassign)
- [x] **Inspector Panel: Assignments View** (3-section unified display):
  - **Morphs** (purple): morph group headers, X remove button, drag-to-adjust range bar
  - **Knobs** (blue): badge label ("Knob 3", "Pedal"), X remove button with SysEx deassign
  - **MIDI CC** (gold): badge label ("CC 74"), X remove button with SysEx deassign
  - Dual mode: single-module view (when module selected) or patch-wide view (when no module selected)
  - Auto-refresh on every assignment change
- [x] **Resizable 3-panel layout**: Module browser, canvas, and inspector with draggable dividers

- [x] **Cable creation/deletion**: Drag between connectors to create cables, right-click to delete
- [x] **Module drag & drop**: Add modules from browser to canvas
- [x] **Shake Cables**: "S" button in header bar randomizes cable curvature to reduce overlap with controls; also available via canvas context menu with "Reset Cables" option
- [x] **Hidden cable connector indicator**: When cables are hidden via color filter, connected connectors show a "capped" visual (filled center) instead of the open hole, indicating a hidden connection exists
- [x] **Undo/Redo system** (Ctrl+Z / Ctrl+Y):
  - Add/delete modules, move modules, add/delete cables
  - Parameter changes (with coalescing for rapid knob turns)
  - Morph assignments/ranges, hardware knob assignments, MIDI CC assignments
  - Rename patch
  - Structural undos (add/delete module) trigger a debounced full patch upload to synth
  - Multi-module grouped undo (e.g. delete selection) handled correctly — single upload after all actions settle

- [x] **Visual indicator for loaded patch in browser**: Currently loaded bank patch highlighted with ▶ icon and amber color
- [x] **Preset browser click feedback**: Single-click selects/highlights a preset in blue before double-click loading
- [x] **Module Help System** (F1):
  - Press F1 with a module selected or hovered to open a floating help popup
  - Displays module description and per-parameter documentation from the original Nord Modular Editor v3.03 help file (157 modules)
  - Fuzzy name matching (fullname, short name, normalized hyphens/spaces)
  - Draggable popup, close with Escape/F1/X button
- [x] **Multi-slot support (A/B/C/D)**:
  - 4 independent slots with separate patches, undo managers, synchronizers
  - Slot bar in left column (below inspector) with synth icons and patch names
  - Switching slots selects and activates the matching hardware slot (A/B/C/D) on the synth, then requests patch data if empty
  - New Patch uploads empty patch to synth to reset the slot
- [x] **Beta warning popup**: Styled floating popup at startup (matches F1 help style), "Don't show again" option, re-show from Help menu
- [x] **Bug reporting**: "Report a bug" button in header bar linking to GitHub Issues
- [x] **MIDI Connect and Store to Bank buttons**: Toolbar buttons in left column above slot bar
- [x] **Canvas hint watermark**: "Press Enter to add modules" shown on empty canvas sections
- [x] **Poly/Common default split**: 90/10 default divider position (poly dominant)
- [x] **Synth name indicator**: Connected synth name displayed in header bar
- [x] **File command shortcuts**: Ctrl+N/O/S/P wired from canvas, menu items show shortcut hints
- [x] **Patch Settings Dialog** (Ctrl+P): Voices, velocity/keyboard range, pedal mode, bend range, portamento, octave shift, voice retrigger; working and synced with the synth
- [x] **Synth Settings Dialog** (Ctrl+G): Synth name, MIDI channels, clock, master tune, knob mode, pedal polarity, keyboard mode, local/LED/program-change options; working and synced with the synth
  - Fixed Synth Settings SysEx upload: uses the active patch PID and sends the correct PatchPacket payload, so synth name changes persist instead of reverting after refresh
- [x] **Morph knob context menu**: Right-click morph knobs for Knob, MIDI Controller, and Keyboard (Velocity/Note) assignments
- [x] **MorphKeyboardAssignmentMessage**: SysEx sc=0x67 — assign morph knobs to keyboard velocity or note
- [x] **macOS menu bar**: File/Edit/Device/Help/About now appear in the system menu bar; Device menu items enable correctly on synth connect
- [x] **macOS SysEx communication**: working correctly — previous issues were caused by a faulty USB hub, not a software bug
- [x] **Patch file compatibility fix**: .pch files from new patches now load correctly in original editors (Nomad, NM 3.03) — module indices start at 1, CustomDump generated from descriptors, correct header defaults

- [x] **Canvas Zoom** (Ctrl+scroll, Ctrl++/-, Z, Shift+Z):
  - Ctrl + mouse wheel: zoom in/out centered on cursor (75%-300%)
  - Ctrl++ / Ctrl+-: keyboard zoom in/out
  - Z: zoom-to-selection (fits selected modules in view) or reset to 100%
  - Shift+Z: always reset to 100%
  - Middle-click drag: pan canvas (hand tool)
  - View menu with all zoom controls and current zoom indicator

- [x] **Randomize Parameters** (Ctrl+R / Ctrl+Shift+R):
  - Simple (uniform random) and Gaussian (center-biased, musical) algorithms
  - Auto-excludes: morph/custom params, mute, level, volume, active, bypass
  - **Parameter Lock**: right-click any knob/slider → "Lock Parameter" to protect from randomization
  - Visual indicator: yellow padlock icon on locked knobs, yellow dot on locked sliders
  - Full undo/redo support (single transaction for all randomized changes)
  - Status bar shows count of randomized parameters
- [x] **Initialize Module** (context menu):
  - Right-click module → "Initialize Module" resets all parameters to factory defaults
  - Works on multi-selection: right-click selection → "Initialize" resets all selected modules
  - Respects parameter locks (locked params are not reset)
  - Full undo/redo support, throttled SysEx sync
- [x] **Parameter Snapshots** (8 memory slots):
  - 8 snapshot buttons in header bar (after bug report button)
  - Click on empty slot = save current parameter state
  - Click on filled slot = recall (instant or interpolated)
  - Shift+click = overwrite/save to slot
  - Right-click any snapshot button = set interpolation time (Instant, 1s, 2s, 5s, 10s, 20s, 30s, 60s)
  - **Sticky interpolation time**: once set, all subsequent clicks use that time — no need to right-click each time
  - Current time displayed in gold next to snapshot buttons
  - Timed interpolation: smooth 30ms-tick morph between current state and target snapshot
  - Progress bar during interpolation
  - Snapshots automatically cleared on patch change (new patch, load file, synth patch switch)
  - Respects parameter locks
  - Visual: empty=gray, filled=blue, active=gold
  - Full undo/redo for recall operations
- [x] **Oscillator & LFO module visual pass** (complete — all 30 modules):
  - All waveform selector buttons now render icons: sine, tri, saw, **inverted saw** (wf_saw_inv), square, noise
  - Static waveform badges on all slave oscillators and LFOs (11×9px, dark rounded box, white icon)
  - Random generator visual badges: S&H staircase (RndStepGen/ClkRndGen), smooth curve (RandomGen), pulse train (RndPulsGen)
  - DrumSynth drum icon badge; PercOsc Click/Decay repositioned, Punch moved right
  - SpectralOsc label centered, Partials button repositioned
  - **Display format system**: oscillator Hz (440·2^((v-69)/12)), LFO Hz/seconds, phase degrees, BPM (piecewise), partial ratios (1:1, 2:1…), drum Hz/partials
  - **LFO waveform display**: phase-offset animation (p7/p3), exponential rate scaling 0.25–8 cycles, clean discontinuity handling for saw/square
  - **PatternGen step**: increment ▲▼ arrow button replacing knob, OFF/1–127 display, rapid-click safe (no double-click reset)
- [x] **Envelope module visual pass** (complete — all envelope modules):
  - ADSR/AD/AHD envelope custom displays rendering with Bezier curves and INV flag
  - Multi-Env (m52): 3 curve-shape icons (bipolar, uni-exp, uni-lin) in landscape selector
  - ADSR time format, attack/release lookup tables (128-entry ms/s display)
- [x] **Filter module visual pass** (complete — all filter/EQ/vocoder modules):
  - **TextDisplay formatters**: filterHz1 `504·2^((v-64)/12)`, filterHz2 `330·2^((v-60)/12)`, eqHz `471·2^((v-60)/12)`, eqGain `(v-64)·0.28125 dB`, eqBw `v/75 Oct`, vowel (A/E/I/O/U/Y/AA/AE/OE for VocalFilter)
  - **FilterE/F displays**: angle-based Bezier LP/BP/HP/BR curves with slope (12/24 dB) and gain-control support
  - **EqMid display**: EXP/LOG Bezier bell curve, live bandwidth and gain tracking
  - **EqShelving display**: Lo-shelf and Hi-shelf modes (mirrored), live gain tracking
  - **Vocoder**: routing-line display (band→output mapping), 16 increment buttons with value readout, Rnd randomize action button, shift buttons **-2/-1/0/+1/+2/INV** (shift all band routes or invert), Emp button shows hi-freq emphasis curve
  - **FilterBank**: Min/Max/Rnd action buttons below faders; frequency labels centered under sliders
  - **FilterA/B**: 6 dB LP/HP filter curve icons drawn in code (ds-2-8 / ds-2-7)
  - **FilterC/D**: multimode routing bracket — gray lines connecting audio `in` to HP/BP/LP outputs; transparent overlay (no background box)
  - **FilterE button order**: `reverse="true"` attribute on vertical radio buttons (BR top → LP bottom, matching hardware values)
  - **Bug fix**: morph-parameter pollution in display lookups — switched to component-ID-based `findParameter()` instead of name matching
- [x] **Sequencer module visual/functional pass** (complete — NoteSeqA, EventSeq, NoteSeqB, CtrlSeq):
  - 16-step LED arrays rendered as real arrays and driven from synth `MeterMessage` active-step values, matching original NOMAD behavior
  - Removed local/autonomous sequencer animation: sequencers only advance when clocked by the synth patch
  - NoteSeqB piano-roll editor renders note lanes, keyboard strip, step grid, active step, disabled steps, zoom state, and scrollbar
  - NoteSeqB editor supports direct note editing, step selection, draggable scrollbar, zoom in/out buttons, randomize/clear actions, and loop/rec/stop icons
  - Fixed 1-based `currentstep` display mapping so step 1 is drawn in column 1, not column 2
  - Sequencer buttons and step toggles verified across NoteSeqA/EventSeq/NoteSeqB/CtrlSeq
- [x] **Module add menu organization**:
  - Canvas context menu uses explicit original-style categories: IN/OUT, OSC, LFO, ENV, FILTER, MIXER, AUDIO, CTRL, LOGIC, SEQUENCER
  - Each category has internal separators matching the original menu grouping
  - Internal Morph descriptor is hidden from module browser, QuickAdd, and Add Module menus while remaining available for morph protocol/state handling
- [x] **Module checklist complete for instantiable modules**:
  - Sequencer, Logic, Audio, Filter, Envelope, Oscillator/LFO, In/Out, Mixer, and Control passes recorded in `MODULE_CHECKLIST.md`
  - Non-instantiable Morph category removed from the user-facing checklist/menu
- [x] **Dark Theme System**: `ColorScheme` struct (50 fields), runtime-switchable via View → Theme menu
  - Two themes: **Dark** (default, carbon palette) and **Classic** (original colors, dev reference)
  - `ThemeId` enum for identity tracking; no color-comparison fragility
  - Module backgrounds override XML colors in Dark mode (`#2D3033` uniform panels)
  - All 86 color literals in `PatchCanvasComponent.cpp` wired to scheme fields
  - Selection border: inset 1.5px rounded rect, 1px stroke, theme color (blue/yellow)
  - Always starts Dark; no persistence
- [x] **Module refinement pass — v0.5.2**:
  - **Frequency displays** — MasterOsc, OscA/B, FormantOsc, FilterA/B now show Hz/kHz instead of raw integers or note names (matching SpectralOsc/FilterC)
  - **LfoB square waveform** — vertical edges now drawn between high/low segments
  - **Phaser** — center-frequency knob now drives both the textDisplay (frequency in Hz/kHz) and the horizontal position of the peak cluster on the response display
  - **Sample&Hold** — trig connector recoloured yellow (logic) to match its actual signal type
  - **Multi-option buttons** — radio-style click: jumps directly to the segment under the cursor instead of cycling through all values
  - **Sequencer Random** — new `Rnd` button on EventSeq and NoteSeqA (left of `Clr`); randomises only the per-step controls (toggles in EventSeq, sliders in NoteSeqA), leaving transport/loop/length/UI params untouched
  - **KeyQuantizer scale presets** — right-click → Scales (root C): Chromatic, Major (Ionian), Natural/Harmonic/Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Pentatonic Major/Minor, Blues Major/Minor, Whole Tone, Diminished W-H (mirrors the VCV Fundamental Quantizer set)

### Recently Completed
- [x] **Patch Settings Dialog** (Ctrl+P) — expanded dialog, working and synced with the synth
- [x] **Synth Settings Dialog** (Ctrl+G) — synth name, MIDI channels, clock, master tune, knob mode, pedal polarity, etc.; sends/receives SysEx and persists synth name changes
- [x] **Slot switching sync** — changing slots from the editor selects and activates the correct hardware slot across A/B/C/D
- [x] **Preset browser click feedback** — single-click selects/highlights a preset before double-click loading
- [x] **Help System** — Module Help (F1), Help Contents, and About dialog are implemented
- [x] **Synth DSP load indicator** — DSP bar in header + Voice/DSP in status bar

## TODO

### Next Steps
- [x] **Morph overlay display** — F7 shows morph group color-coded badges (M1/M2/M3/M4), F5 shows morph range delta (+N/-N) per parameter

### Synth Communication
- [ ] **Upload Active Slot** (Ctrl+U) - Upload current synth patch to editor
- [ ] **Send Controller Snapshot** - Send current controller state to synth
- [ ] **Bank Upload from Synth** - Upload entire bank (99 patches) from synth to disk
- [ ] **Bank Download to Synth** - Download entire bank from disk to synth

### Editor Preferences
- [x] **Editor Options Dialog** (Ctrl+E) - Configure editor behavior (implemented)

### Floating Windows
- [ ] **Keyboard Floater** (Ctrl+F) - Virtual MIDI keyboard
- [ ] **Knob Floater** (Ctrl+K) - Hardware knob mapper
- [ ] **Notes Floater** - Patch notes/comments window
- [ ] **Browser** (Ctrl+B) - Standalone preset/snippet browser

### Patch Tools
- [x] **Snippet System** - Save/import `.pch` module groups with synth-safe incremental sync
- [ ] **Preset Browser Window** (Ctrl+B) - Bitwig-style standalone browser for patches and snippets

### Quality of Life
- [ ] **Module Search Tags** - Add tags (bass, pad, utility, modulation...) to module descriptors for improved QuickAdd/Browser search
- [ ] **Verify Input/Output Connectors** - Ensure visual distinction between inputs and outputs
- [ ] **Keyboard Shortcuts** - Complete keyboard shortcut system matching original editor
- [ ] **Window Management** - Remember window positions and sizes across sessions

## Roadmap

This section outlines all planned features to achieve feature parity with the original Nomad editor. Features are organized by category.

### File Operations
- [x] **New Patch** (Ctrl+N) - Create new empty patch (resets current slot)
- [x] **Open Patch** (Ctrl+O) - Load .pch file from disk
- [x] **Save Patch** (Ctrl+S) - Save current patch to disk
- [x] **Save Patch As** - Save patch with new filename
- [x] **Quit Application** (Ctrl+Q) - Exit Nomad2026

### Patch Management
- [x] **Patch Settings Dialog** (Ctrl+P) - Edit voices, velocity/keyboard range, pedal mode, bend range, portamento, octave shift, voice retrigger
- [x] **Save Patch in Synth** - Store uploaded patch to a bank location
- [x] **New Patch** - Create a new empty patch in the editor
- [x] **Multi-Slot Support** - All 4 slots (A, B, C, D) with independent patches, undo, sync, hardware selection, and active-slot switching

### Synth Communication
- [x] **Synth Settings Dialog** (Ctrl+G) - Configure synth parameters:
  - Synth name editing
  - MIDI channel assignment per slot (1-16)
  - MIDI clock (Internal/External, BPM, Global sync)
  - Master tune (cents, Hz display)
  - Keyboard mode (Active slot / Selected slots)
  - MIDI velocity scale (min/max 0-127)
  - Knob mode (Immediate / Hook)
  - Pedal polarity (Normal / Inverted)
  - Program change send/receive
  - Local off, LEDs active
  - Sends/receives settings over SysEx and verifies refreshed synth state
- [ ] **Upload Active Slot** (Ctrl+U) - Upload current synth patch to editor
- [ ] **Send Controller Snapshot** - Send current controller state to synth
- [ ] **Bank Upload from Synth** - Upload entire bank (99 patches) from synth to disk
  - Bank selection dropdown
  - Destination folder browser
  - Progress bar
- [ ] **Bank Download to Synth** - Download entire bank from disk to synth
  - Source selection (bank file or folder)
  - Bank number selection
  - Progress bar with overwrite warning

### Editor Preferences
- [x] **Editor Options Dialog** (Ctrl+E) - Configure editor behavior (cable style, knob control, auto upload, recycle windows — persisted in user settings)

### Floating Windows
- [ ] **Keyboard Floater** (Ctrl+F) - Virtual MIDI keyboard
  - Octave navigation (<<, <, >, >>)
  - Drone mode (sustain notes)
  - Repeat mode
  - Visual key press feedback
- [ ] **Knob Floater** (Ctrl+K) - Hardware knob mapper
  - 18 assignable knobs with LED indicators
  - Morph group selection arrows
  - Displays current module/parameter assignments
  - Sustain pedal icon
  - Keyboard hold icon
  - Joystick/modulation wheel icon
- [ ] **Notes Floater** - Patch notes/comments window
- [ ] **Browser** (Ctrl+B) - See **Preset Browser Window** under Patch Tools for full spec

### Module Canvas Editing
- [x] **Drag & Drop Modules** - Add modules from browser to canvas by dragging
- [x] **Move Modules** - Reposition modules on canvas (single and multi-move)
- [x] **Delete Modules** - Remove modules from patch (Delete key or context menu)
- [x] **Cable Creation** - Drag between connectors to create cables
- [x] **Cable Deletion** - Right-click cables to delete
- [x] **Module Copy/Paste** - Ctrl+C / Ctrl+V with parameter values and internal cables
- [x] **Duplicate** - Duplicate selected modules with or without cables (context menu)
- [x] **Selection Tool** - Rubber-band + Shift+click multi-selection
- [x] **QuickAdd** - Space / double-click to add modules by name search
- [x] **Parameter Context Menu** - Right-click params to assign/remove morph group

### Help System
- [x] **Module Help** (F1) - Context-sensitive module help from original v3.03 help file
- [x] **Help Contents** - Integrated help documentation
- [x] **About Dialog** - Version info, credits, license

### Patch Tools
- [x] **Randomize Parameters** (Ctrl+R) - Randomize parameter values on entire patch
  - Two algorithms: Simple (uniform) and Gaussian (musical, biased toward center values)
  - Auto-excludes level, mute, morph, and UI parameters
  - **Parameter Lock**: Right-click any parameter to lock/unlock it from randomization (lock icon indicator)
  - Locked parameters are preserved across randomize operations
  - Full undo/redo support
- [x] **Snippet System** - Save and reuse module groups
  - Select modules → right-click → "Save as Snippet..." → saves standard `.pch` file
  - File → Import Snippet... → inserts modules at grid (3,3) with overlap avoidance
  - Imported snippets sync to the synth incrementally (modules first, then cables) instead of forcing a full patch upload
  - Full undo/redo support via `InsertSnippetAction` (proper model-level undo)
  - Snippets are valid `.pch` files — openable in NM 3.03, Nomad, and any NM tool
- [ ] **Preset Browser Window** (Ctrl+B) - Bitwig-style standalone browser for patches and snippets
  - Separate resizable window (not embedded in canvas), toggle open/close
  - **User Preset Library**: configurable root folder for .pch files, set on first launch
  - Indexed metadata (name, category, date) cached in JSON/SQLite for fast search
  - Sections: Recent, All Presets, Snippets
  - Real-time search/filter by name and tags
  - Double-click or drag & drop to load preset into current slot / insert snippet into canvas
  - File → Import Snippet also available as alternative to browser
  - **Synth Dump to Disk**: download all 9 banks × 99 slots from synth to user's preset folder
  - **Community Patches** (future): browse electro-music.com patch archive from within browser
    - Bundled offline packs as initial approach; live scraping as stretch goal

### Quality of Life
- [x] **Module Rendering Polish** - Systematic review of all instantiable modules against original editor (see [MODULE_CHECKLIST.md](MODULE_CHECKLIST.md))
  - Per-module verification: layout/size, graphics/custom displays, LEDs/meters, buttons/icons, labels, parameter interaction
  - Categories: In/Out, Oscillator, Filter, Amplifier, Envelope, LFO, FX, Delay, Mixer, Switch, Logic, Sequencer, MIDI, Random
- [ ] **Module Search Tags** - Add tags (bass, pad, utility, modulation...) to module descriptors for improved QuickAdd/Browser search
- [x] **Dark/Light Theme** - Runtime-switchable via View → Theme; Dark (default) + Classic; `ColorScheme` struct with 50 fields
- [ ] **Verify Input/Output Connectors** - Ensure visual distinction between inputs and outputs
- [x] **Undo/Redo System** - Full undo/redo for all patch operations
- [ ] **Keyboard Shortcuts** - Complete keyboard shortcut system matching original editor
- [ ] **Window Management** - Remember window positions and sizes across sessions

## Building

```bash
# Clone with submodules (JUCE is a git submodule)
git clone --recurse-submodules https://github.com/animatek/Nomad2026.git
# Or if already cloned:
git submodule update --init --recursive

# Configure (Debug mode recommended for development)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run (Linux/Windows)
./build/Nomad2026_artefacts/Debug/Nomad2026

# Run (macOS)
build/Nomad2026_artefacts/Debug/Nomad2026.app/Contents/MacOS/Nomad2026
```

**macOS Universal Binary** (Intel + Apple Silicon, for distribution):
```bash
cmake -B build-universal -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal -j$(sysctl -n hw.logicalcpu)
# Result: build-universal/Nomad2026_artefacts/Release/Nomad2026.app
```

**Windows** (requires [VS 2022 Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) with the "Desktop development with C++" workload):
```bash
cmake -B build-win-release -G "Visual Studio 17 2022" -A x64
cmake --build build-win-release --config Release
# Result: build-win-release/Nomad2026_artefacts/Release/Nomad2026.exe
```

**Note:** Currently using Debug builds during active development. Some features (like URL opening in Help/About menus) require Debug mode to work correctly on all platforms.

Nord Modular data files (modules, theme) are embedded in the binary via JUCE BinaryData — no external data files required.

### Experimental: VST3/CLAP Plugin Build

The editor can also be built as a VST3 or CLAP plugin (for use inside DAWs like Bitwig, Reaper, etc.). This is experimental and not yet production-ready.

```bash
# Build all plugin formats
cmake --build build --target Nomad2026Plugin_VST3 Nomad2026Plugin_CLAP -j$(nproc)

# Install plugins
cp -r build/Nomad2026Plugin_artefacts/Debug/VST3/Nomad2026.vst3 ~/.vst3/
cp build/Nomad2026Plugin_artefacts/Debug/CLAP/Nomad2026.clap ~/.clap/
```

**Note:** Plugin builds require the clap-juce-extensions submodule: `git submodule update --init --recursive`

Requires JUCE (included as a git submodule in `JUCE/`) and a C++17 compiler.

### Linux MIDI: JUCE Patches

The local JUCE copy includes two patches to `JUCE/modules/juce_audio_devices/native/juce_Midi_linux.cpp` that fix MIDI on Linux with modern kernels (6.5+) that expose UMP (Universal MIDI Packet) APIs:

1. **Synchronous endpoint cache** (Client constructor): `cachedEndpoints = findEndpoints(handle.get());`
   Without this, `getAvailableDevices()` returns 0 devices on startup because the ALSA sequencer thread hasn't delivered port-change events yet.

2. **Bytestream for legacy ports** (OutputImplNative::send): `port->isUmpEndpoint()` check added.
   Without this, JUCE sends UMP packets to legacy MIDI devices (e.g. Roland UM-ONE), which causes assertion failures and no data is transmitted.

These patches are required on any Linux distribution with kernel UMP support (CachyOS, Fedora 40+, Ubuntu 24.10+, etc.). If you update the JUCE submodule, reapply these patches.

## Technical Documentation

See [RESEARCH.md](RESEARCH.md) for:
- Nord Modular MIDI SysEx protocol (v3.03)
- Patch file format (.pch) specification
- Module system (110+ modules with parameters, connectors, signals)
- PDL2 binary format description language
- Architecture overview of the original editor

## Credits

This project is a reimplementation based on the work of the original nmedit/Nomad developers:

| Person | Contribution |
|--------|-------------|
| **Marcus Andersson** | Reverse-engineered the Nord Modular MIDI protocol; C++ and Java protocol libraries |
| **Christian Schneider** | Nomad Java editor (v0.2, v0.3) |
| **Ian Hoogeboom** | Nomad v0.4 update, macOS compatibility |
| **Jan Punter** | Nord Modular patch file format documentation |
| **Jelle Herold** | Original project founder |
| **Stefan Keel** | Module SVG icon designs |
| **Tobias Weinald** | Splash screen artwork |

## Original Project

- **Website**: https://nmedit.sourceforge.net/
- **Source (v0.3)**: https://github.com/wesen/nmedit
- **Source (v0.4)**: https://github.com/Airell/nmedit

## License

This project is licensed under the [GNU General Public License v3](LICENSE)
(upgraded from v2 for JUCE AGPLv3 compatibility).
