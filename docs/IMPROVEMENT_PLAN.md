# Improvement & Optimization Plan

Code health review of 2026-08-16 (48.5k lines of C++). Overall verdict: the
foundations are sound. The model layer is clean, undo references modules by
container index rather than pointer, all incoming MIDI is bounced to the
message thread (effectively single-threaded, no data races), and the MCP
bridge does the same. The build compiles clean. What remains are three
structural risk classes, a handful of measurable performance wins, and the
open GitHub issues folded into phases below.

Status legend: [ ] pending, [~] in progress, [x] done.

---

## Phase 0: Small-issue sweep (low risk, releasable on its own)

Quick fixes from the open issue list. Each is small and independent; together
they make a decent point release.

- [x] #65 Error messages remain visible. Root cause: "Failed to add module"
      was posted via setConnectionStatus (the permanent status line), not
      showMessage. Now transient (6 s) and any status message dismisses on
      click. (2026-08-16)
- [x] #57 `$Contents` placeholder in module help. The popup now filters all
      `$`-prefixed scraper artifacts (also hit EQ Mid, LFOC, Oscillator
      slave FM, Sine Bank). (2026-08-16)
- [x] #58 Help popup description hardcoded near-white; now
      AppTheme textPrimary. (2026-08-16)
- [x] #56 Menu shortcut alignment: shortcuts moved from "\t"-embedded label
      text to PopupMenu::Item::shortcutKeyDescription (right-aligned by the
      LookAndFeel; the macOS native menu printed the tab literally).
      (2026-08-16)
- [x] #53 Sequencer Clr now resets step params to defaultValue (CtrlSeq
      faders land on 64), not minValue. (2026-08-16)
- [x] #55 + agreed remap: DSP cost overlay F3 (F10 kept as alias), focus
      mode F4 (F11 kept as alias), wireframe on macOS Cmd+Shift+W. Manual
      chapters + in-app shortcuts dialog updated. (2026-08-16)

## Phase 1: Tests + CI (the biggest "fewer errors" lever)

Nothing else in this plan is safe to do at scale until this exists. The most
critical layers are pure and testable without GUI or hardware.

- [x] doctest (vendored, libs/doctest) + CTest wiring; `tests/` target
      nme_tests builds the pure layers straight from source/. (2026-08-16)
- [x] Round-trip tests: serialize -> parse -> serialize byte-identical over
      a synthetic patch (modules in both areas, cable, params, morph/knob/
      ctrl assignments) and over an init patch. (2026-08-16; the .pch disk
      corpus round trip is a natural extension when corpus files land in
      tests/fixtures/)
- [x] SysExCodec: envelope, checksum, header bits, malformed frames.
      (2026-08-16)
- [x] Upload packetizer: extracted to source/midi/UploadPacketizer.{h,cpp}
      (pure functions, byte-identical wire format) and pinned by tests:
      166-byte cut, sections spanning packets, boundary-exact sections,
      pack7Bit round trip through BitStream, frame first/last bits and
      checksum, the issue #40 close-transfer packet. (2026-08-16)
- [x] Placement: makeRoomForModule/restore pinned; issue #54 FIXED with a
      new canMakeRoomForModule() pre-check (refuses placements the column
      cannot absorb, walking the whole push chain) wired into AddModule,
      AddComment, ResizeComment and InsertSnippet actions; a refused paste
      rolls back whole. Needs Javier's GUI confirmation. (2026-08-16)
- [x] CI: .github/workflows/ci.yml runs build + ctest on every push/PR,
      plus a second job under ASan/UBSan, both with ccache. (2026-08-16)
- [ ] Optional: clang-tidy with a narrow set (bugprone-*, performance-*).

## Phase 2: Performance quick wins  [DONE 2026-08-16]

Measured, not guessed. Numbers below are from a 100-module patch in a Debug
build (tests/, throwaway benchmark, not committed).

- [x] The light/meter slot table was the real bottleneck, and it was worse
      than the plan thought: `paintLights` called `computeModuleLightIndex`
      **twice per module**, and each call rebuilt the whole table (a vector
      allocation, a sort, and a theme lookup per module). Painting was
      quadratic in the module count: **18.2 ms per repaint** for 100 modules,
      on top of the actual drawing, several times a second while the meters
      moved. Now built once and cached, validated against a cheap structural
      fingerprint, and looked up once per module per paint: **14 us**, about
      1300x less. Extracted to `source/model/LightMeterLayout.{h,cpp}` so the
      table and the fingerprint are unit tested (tests/test_light_meter_layout
      .cpp), including a brute-force check of the property the cache rests on:
      an unchanged fingerprint really does mean an unchanged table.
- [x] Removed the double async hop for light/meter data
      (MainComponent.cpp). Incoming SysEx is already bounced to the message
      thread before the protocol decodes it, so the extra callAsync only
      bought a copy of two 128-int arrays and a frame of latency, on the
      callback the synth sends most often. Stale "may fire from the MIDI
      thread" comments in ConnectionManager.h corrected while there.
- [x] `paintCables`' connector-to-module lookup is a sorted vector reused
      across paints instead of a `std::map` rebuilt per paint: a couple of
      thousand heap allocations per repaint gone, including on the small
      per-module repaints the LEDs trigger. Deliberately NOT cached across
      paints (raw connector pointers, issue #61's family) and the code says so.
- [x] `ValueSpinner` repainted the whole canvas on **every mouse move** over
      a control, for two little buttons that had not moved. It now repaints
      only when something visibly changed, and only the buttons' own
      rectangles, via a host-supplied `repaintArea` (the canvas applies zoom,
      the header bar does not). This is what its own doc comment always
      claimed it did. The comment-hover grips in mouseMove got the same
      treatment.
- [x] Precompiled headers: measured first and skipped. The largest file in
      the project (PatchCanvasComponent.cpp, 9.3k lines) rebuilds in 10 s;
      PCH would shave a few seconds off that in exchange for a stale-PCH
      failure mode on an NTFS working tree. Not worth it.

Deliberately NOT in this phase: caching each module as an image and
compositing. That is a big, risky refactor; only consider it if profiling
after the wins above still shows paint as the bottleneck.

**Still wants a pair of eyes on screen.** Narrowing a repaint is the kind of
change a test cannot catch: if a rectangle is too small, stale pixels are left
behind, and only looking at it finds that. The three to check are the nudge
arrows under a hovered knob (they should appear, light up under the pointer
and disappear cleanly, leaving nothing behind), the corner grips of a text
note on hover, and the LED/meter animation with the synth connected. The code
was read against each one (the spinner paints strictly inside its two button
rectangles, the grips strictly inside the note's bounds), so the expectation
is that they are clean.

## Phase 3: Kill the raw-Module* bug class  [DONE 2026-08-16]

- [x] `ModuleRef { section, containerIndex }` lives in model/Patch.h with
      `Patch::getModule(ref)` as the only way to read one, plus tests
      (tests/test_module_ref.cpp) covering the delete, the delete-then-undo
      that a pointer never survived, and the documented index-reuse case.
- [x] Canvas migrated: selection, multi-move, the module the Inspector
      follows, hover badge, nudge arrows, key-step run, cost badge.
- [x] Inspector migrated, including its assignments list's own second
      pointer.
- [x] `forgetDeletedModules()` deleted, and with it the sweep at the top of
      every paint. No hand-written liveness check against #61 is left in the
      project.

Two places still hold pointers, both deliberately and both documented where
they live:

- The canvas's drag state, because it holds a `Parameter*` and a
  `Connector*` as well as its module, and those live inside the module where
  an index cannot name them. One check before paint reads it
  (`dropDragIfModuleGone`), free while no drag is running.
- The Inspector's parameter rows, which hold `Parameter*` on purpose so a
  knob turned on the canvas reads true there without being told. Valid only
  between one rebuild and the next; the gap is guarded by asking whether the
  module reference still resolves.

Finishing those two would mean naming a parameter by index rather than by
pointer. Worth doing if the drag or the rows ever grow, not worth it on its
own today.

## Phase 4: File splits + canvas-area features

Refactors that lower the cost of touching the code, paired with the open
feature issues that live in the same files (do the split when the feature
work drags you in there anyway).

- [x] PatchCanvasComponent.cpp split into six files of the same class
      (2026-08-16): Painting 4.1k, Mouse 3.1k, Component 0.9k,
      Clipboard 0.6k, Presets 0.4k, Comments 0.4k. Proven to be a pure
      move: sort every non-blank line of the old file and of the six new
      ones, diff, and the only differences are each file's header comment
      and its includes. Each file then kept only the includes it uses.
      Next natural seam if it needs cutting again: the icon vocabulary
      and the custom displays, which dominate Painting.
- [x] The 27 duplicated forwarding setters collapsed onto one template
      (2026-08-16), 107 lines gone. Not a CanvasCallbacks struct as the
      plan guessed: that would have changed the API every caller uses, for
      no more benefit. The real prize was the copy-paste hazard, since each
      copy passed the callback to one canvas and moved it into the other.
- [ ] #67 Cable re-routing: Ctrl/Cmd+drag an existing cable end to move it,
      as the original editor does. Touches canvas mouse code; do alongside
      the mouse/drag split.
- [ ] #54 residue, if anything is left after the Phase 1 placement fixes.
- [ ] Extract from MainComponent (4,912 lines), gradually and only as each
      area is touched: Morph A/B controller, snapshots + interpolation,
      MDI window management, extras-library binding.
- [ ] #51 ABCD retile button: one click re-tiles open slot windows into the
      canonical A|B / C|D grid, with a smooth animation. Lives in
      SlotMdiArea; do together with the MDI extraction.

## Phase 5: ConnectionManager state machines (last, with the test net)

ConnectionManager (2,274 lines) holds ~15 state booleans plus "generation"
counters, which are the symptom of implicit state machines (fetch, upload,
prefetch, patch-list all interleaved). This is the most delicate code in the
app: mistakes here hang the synth.

- [ ] Extract an explicit PatchFetchStateMachine (13 sections, retries,
      stale timeouts).
- [ ] Extract a PatchUploadStateMachine (packetizer, ACK sequencing,
      transfer close on abort).
- [ ] Only start once Phase 1 tests cover the packetizer and codec.

## Feature track (independent of the phases, schedule by demand)

- [ ] #60 Preset browser: arrows to cycle presets (load next/previous with
      one click). Small.
- [ ] #50 Drag a patch from the synth browser onto a slot to load it there
      (the right-click Load to Slot A..D already exists; add DnD). Medium.
- [ ] #52 Replace pictograms with flat theme-coloured vector art. Low
      priority per label; art-heavy.
- [ ] #62 VST version. The plugin target already exists behind
      NME_BUILD_PLUGIN=OFF. Long-term: needs the MCP-less build path,
      multi-instance behaviour, and state save/restore thought through.
      Do not bundle into any of the phases above.

## Explicitly not doing

- Style rewrites for their own sake.
- Module-image caching before profiling justifies it.
- Any ConnectionManager restructuring before tests exist.

## Suggested order

Phase 0 (ship it), then 1, 2, 3, 4, 5. The feature track interleaves
wherever Javier wants a user-visible release between infrastructure phases.
