# Changelog

## Unreleased

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

  Which slot sits in which tile is yours to change: **Ctrl+Shift+Left/Right** moves the
  focused slot one tile over and **Ctrl+Shift+Up/Down** rotates them all round, so you can put
  the patch you are working on where you want it without closing and reopening anything. The
  sub-windows slide to their new places rather than jumping; turn that off with **Animate Slot
  Tiling** in Editor Options if you prefer it instant.

  The focused sub-window is outlined in the theme's accent colour, so with four tiled you can
  always see which one you are editing, and its title carries the LOCAL badge when that slot's
  patch is not in sync with the synth. The inspector picks up whichever module that slot had
  selected instead of going blank, since each canvas keeps its selection while it sits in the
  background.

  Which slots you had open, which one had focus, and how they were arranged all come back
  when you reopen the editor.

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
