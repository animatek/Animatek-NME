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

- [ ] Add Catch2 (or doctest) + CTest wiring in CMakeLists.txt.
- [ ] Round-trip tests: PatchParser <-> PatchSerializer over the real .pch
      corpus (including Nocticore's patches). Any byte-level mismatch is a
      bug found for free.
- [ ] SysExCodec: 7-bit encode/decode, checksum cases.
- [ ] ConnectionManager upload packetizer: the 166-byte packet rule
      (issue #39 regression here bricks MIDI until the transfer is closed;
      see docs/RESEARCH.md and the upload-packet memory).
- [ ] Placement logic: makeRoomForModule / isAreaFree / findNearestFreeY.
      Fix issue #54 (overlap corner cases: bottom-of-canvas burial, paste
      over existing modules) WITH these tests, not before them.
- [ ] GitHub workflow: build + run tests on push (the existing
      build-binaries.yml is manual-only), plus one ASan job running the
      test suite.
- [ ] Optional: clang-tidy with a narrow set (bugprone-*, performance-*).

## Phase 2: Performance quick wins (a few days, low risk)

Measure first: enable JUCE_ENABLE_REPAINT_DEBUGGING in a debug build and/or
add a paint-time readout to the status bar. Then, in order of expected value:

- [ ] Cache `computeModuleLightRanges()` (PatchCanvasComponent.cpp:118).
      It allocates and sorts a vector on every light/meter frame from the
      synth, twice (poly + common canvas), several times per second while
      audio runs. It only depends on patch structure; invalidate on
      setPatch / module add / module remove.
- [ ] Remove the double async hop for light/meter data
      (MainComponent.cpp:663-676). The callback already arrives on the
      message thread (MidiDeviceManager bounces before dispatch), yet it
      copies two 128-int arrays and does another callAsync. Call straight
      through.
- [ ] Cache the connector-to-module `owners` map in paintCables
      (PatchCanvasComponent.cpp:5385). It is rebuilt on every repaint,
      including the small per-module LED repaints. Same structural
      invalidation as the light ranges. Optionally also cache cable Paths
      with bounding boxes for per-cable clip culling.
- [ ] Narrow the avoidable full-canvas repaint() calls on hot paths, e.g.
      the comment-hover change in mouseMove (line ~1368). Badges, spinner
      and selection changes can repaint just their rectangles.
- [ ] Dev quality of life: target_precompile_headers for the JUCE headers.

Deliberately NOT in this phase: caching each module as an image and
compositing. That is a big, risky refactor; only consider it if profiling
after the wins above still shows paint as the bottleneck.

## Phase 3: Kill the raw-Module* bug class (issue #61's family)

The UI holds raw `Module*` in selection, hover, spinner, drag state,
inspector and callbacks, defended by `forgetDeletedModules()` called inside
paint() (PatchCanvasComponent.cpp:886). It works, but every new feature that
stores a Module* can reintroduce a use-after-free (silent on Linux, fatal on
macOS).

- [ ] Introduce `ModuleRef { int section; int containerIndex; }` resolved to
      Module* at point of use. The undo system already works exactly this
      way, so this extends a proven pattern.
- [ ] Migrate zone by zone: selection first, then hover/spinner/cost badge,
      then callbacks and inspector.
- [ ] End state: delete forgetDeletedModules() and the paint-time scan.

## Phase 4: File splits + canvas-area features

Refactors that lower the cost of touching the code, paired with the open
feature issues that live in the same files (do the split when the feature
work drags you in there anyway).

- [ ] Split PatchCanvasComponent.cpp (9,276 lines) into several .cpp files
      of the same class: painting, mouse/drag, clipboard/ghost, comments,
      DrumSynth presets. Near-zero risk.
- [ ] Replace the ~40 duplicated forwarding setters in PatchCanvasComponent
      (header lines ~757-976) with a CanvasCallbacks struct injected once.
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
