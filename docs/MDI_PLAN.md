# MDI: the four slots inside the main window

Phased plan for replacing the per-slot OS pop-out windows with internal sub-windows in the
main window's central area. Written 2026-08-01 against 0.12.0. Phases 0, 1 and 2 are done;
phases 3-5 are not started.

## Why

Each slot is edited today in a floating OS window (`SlotWindow`, a `juce::DocumentWindow`).
They get lost behind other windows, cannot be arranged, and look nothing like the original
Clavia editor or Nomad, where patches live as internal frames inside a work area.

The deeper reason to do this, and to do it sooner rather than later, is that the current
design keeps **two parallel paths for the same thing**:

- the main canvas wiring lives inline in the constructor (`MainComponent.cpp:173-808`,
  ~250 lines) and resolves every action through the mutable `activeSlot`;
- `wireSlotWindowContent` (`MainComponent.cpp:2735-2958`, ~225 lines) is a deliberate
  near-duplicate that captures a fixed slot;
- 19 distinct sites special-case "main window" against "slot window".

Every feature that touches the canvas has to be written twice, and one of them has already
been missed once (the Load meter went stale on paths that did not come from the canvas).

With several canvases visible at once, resolving through `activeSlot` **stops being correct
by construction**: there is no longer a 1:1 relation between the canvas that fired a
callback and the slot the user is looking at. So this change forces the two paths into one.
It removes duplication rather than adding a layer.

## Decisions

1. Each sub-window carries **only its canvas** plus its title bar. The Inspector, the
   browsers (Synth/Disk and modules), the header bar and the status bar stay **shared** in
   the main window and follow the focused slot.
2. The OS pop-out windows are **removed entirely** (`SlotWindow` deleted). No dual mode.
3. Only opened slots appear. With one open it fills the area
   (`useFullscreenWhenOneDocument(true)`), which is how the editor behaves today.

## Approach

`juce::MultiDocumentPanel` is already in the vendored JUCE
(`JUCE/modules/juce_gui_basics/layout/juce_MultiDocumentPanel.h`) and `juce_gui_basics` is
already linked (`CMakeLists.txt:262`). It provides the internal windows, focus handling, and
the `activeDocumentChanged()` virtual used to drive `activeSlot`. It does **not** provide
tiling: `MultiDocumentPanel::resized()` only positions children in tabs mode or single
document mode (`juce_MultiDocumentPanel.cpp:569-580`), so tiling is ours to write.

**Unify on one per-slot wiring function.** All four `SlotView`s are created eagerly at
startup and each is wired with `wireSlotView(slot)`, lifted from `wireSlotWindowContent`
plus what only the inline block had (snippet save/drop, `fileCommandCallback`, the status
bar message on a failed add). The inline block is deleted. What legitimately keeps depending
on `activeSlot` is only the shared surfaces: inspector, header bar, browsers, floaters,
status bar, variations and snapshots.

**Focus has no race.** `PatchCanvas::mouseDown` calls `grabKeyboardFocus()` as its first
statement (`PatchCanvasComponent.cpp:4525-4527`) and the chain up to `activeDocumentChanged`
is synchronous. More importantly, correctness does not depend on that: with per-slot wiring,
`sendParameter(slot, ...)` and `slotUndoManagers[slot]` are right wherever focus happens to
be. That is what makes the race irrelevant rather than narrowly won.

## Phases

Each phase compiles and is testable on its own.

### Phase 0 — Independent fixes (DONE, commit e1c4b2f)

Both are real bugs today and both get hit constantly once four canvases share a window.

1. `selectSlot` discarded the parameter queue for **every** slot: it passed no slot, the
   parameter defaults to -1, and that takes the blanket branch. Nothing about it was needed;
   the queue is keyed by slot, `drainParamQueue` holds per slot, and every send carries its
   slot in the SysEx envelope. Same for the synth-initiated slot change.
2. The overlay readouts (F5, F7-F10) set an editor-wide mode but repainted only the canvas
   the key reached, leaving any other canvas stale.

Still open from the original phase 0: `PatchCanvasComponent::getPrimarySelection()`, needed
only by phase 5, deliberately not added until it has a caller.

### Phase 1 — Structural swap, one open document (DONE)

Behaviour-neutral: one slot visible, fullscreen, exactly as today. The pop-out windows are
left working, so nothing regresses mid-branch.

- New `source/ui/SlotView.{h,cpp}`: a `Component` owning one `PatchCanvasComponent`, with
  `setPatchTitle()` riding `Component::setName` (which `MultiDocumentPanel` mirrors into the
  window title).
- New `source/ui/SlotMdiArea.{h,cpp}`: derives from `MultiDocumentPanel`; owns all four
  `SlotView`s for the whole session, `openSlot/closeSlot/focusSlot`, `forEachCanvas`,
  `onSlotFocused`, plus a `showOnlySlot()` for this phase's single-document behaviour (phase
  2 replaced it with plain `openSlot` + `focusSlot`). **It
  closes its documents in its own destructor**: `~MultiDocumentPanel` closes them in its body
  (`juce_MultiDocumentPanel.cpp:107-110`), by which time the derived members are gone. Note
  `JUCE_MODAL_LOOPS_PERMITTED` is 0 here, so the synchronous `closeAllDocuments` overload does
  not exist; `closeAllDocumentsAsync(false, nullptr)` runs synchronously and is what is used.
- `MainLayout`: `PatchCanvasComponent canvasComponent` becomes `SlotMdiArea patchArea`, in the
  same slot of the 5-item layout. **No `getCanvas()` shim** — a shim resolving to "the focused
  canvas" would reintroduce the `activeSlot` coupling this removes. `setTheme` fans out over
  `forEachCanvas`.
- `MainComponent`: added `canvasFor(slot)`, `activeCanvas()`, `repaintAllCanvases()`,
  `wireSlotView(slot)`, `handleSlotFileCommand(slot, cmd)`. The inline block is gone and every
  `mainLayout->getCanvas()` site is now `canvasFor(slot)` (model-driven) or `activeCanvas()`
  (focus-driven).
- `randomizeParameters`, `savePatch`, `savePatchAs` and `savePatchToFile` are deleted;
  `initializeModule` and `importSnippetFromFile` took a slot argument. The File/Edit menus
  pass `activeSlot` explicitly, which is what they mean by "the current patch".

Two things worth carrying forward:

- **Ordering matters in `replacePatchInSlot`.** `setPatch` on the slot's canvas has to happen
  immediately after `slotPatches[slot] = std::move(patch)`, not at the end: the old `Patch`
  is already destroyed by then and `switchToSlot()` in between brings the sub-window on
  screen and resizes it, which reads the patch. This is risk 2 below, met for real.
- **`handleOverlayKey` must stay a single call.** Phase 0 made it repaint every live canvas
  itself, so looping it over `forEachCanvas` toggles the editor-wide mode four times.

Line count is roughly flat (`MainComponent.cpp` +21): `wireSlotView` is new while the inline
block and the three activeSlot-only save helpers went. The duplication is only actually
*removed* in phase 2, when `wireSlotWindowContent` and the `SlotWindow` machinery go with it.

### Phase 2 — Multiple open, focus, and removing the pop-outs (DONE)

- `activeDocumentChanged()` → `onSlotFocused(slot)` → `switchToSlot(slot)`, with the
  reentrancy guard held across the whole of `switchToSlot` rather than just the focus call,
  so the round trip (`focusSlot` → `setActiveDocument` → back) is dropped outright.
- `selectSlot` to the synth **debounced (250 ms)** in `notifySynthOfSlot` and skipped when
  the synth is already on that slot, so walking focus across four windows does not spray
  slot messages.
- Synth to editor: `switchToSlot(slot, false, /*bringOnScreen=*/false)` focuses a window
  only if it is already open, never opens one, and never steals focus while a mouse button
  is down (`Desktop::getNumDraggingMouseSources()`).
- Lights and meters go only to the hardware-focused slot's canvas, and `clearLightMeterData`
  zeroes the one being left so its LEDs do not freeze lit.
- Right-clicking a slot row now shows/hides that slot's sub-window (`onSlotViewToggled`,
  renamed from `onSlotWindowRequested`). It refuses to close the last open one.
- **Deleted**: `SlotWindow.*`, `SlotWindowContent.*`, `toggleSlotWindow`,
  `wireSlotWindowContent`, `updateSlotWindowDspLoad`, `updateSlotWindowFocusIndicators`,
  `mirrorLiveUpdateToSlotWindow`, the `slotWindows[]` array and the CMake entries.

Two things this phase turned up:

- **A new sub-window is sized to its content**, and `setContentNonOwned(view, true)` on a
  `SlotView` that has never been laid out measures 0x0 — the window lands on screen as a
  sliver of title bar. `giveUsableBounds()` gives any degenerate or off-area window a
  cascaded 3/4-size rectangle, from `openSlot` and from `resized`. Real tiling is phase 3;
  this only rescues unusable windows and leaves arranged ones alone.
- **The empty-canvas hint was centred on `g.getClipBounds()`**, so a partial repaint drew one
  copy of "Press Enter to add modules" per invalidated region and they piled up. It now
  centres on the viewport's visible area. Pre-existing, but the sub-windows repaint in
  pieces far more often than one full-width canvas did, which is what made it show.

The `slotWindowA..D{X,Y,W,H,Open}` settings keys are no longer written or read. Which slots
are open is therefore not persisted until phase 4.

### Phase 3 — Tiling and the View menu (~1 day)

`applyTiling()` over the container windows in slot order A-D: 2x2 (with sensible 3-window
and 2-window fallbacks), side by side, stacked, cascade. Dragging or resizing a window drops
the mode to Free. View menu gets per-slot open/close entries with tick marks plus
`Ctrl+Shift+1..4`.

Collision to handle: `handleFloaterShortcut` (`MainComponent.cpp:2428-2452`) matches
`Ctrl` without Shift and would swallow `Ctrl+Alt+T` into the theme cycle.

### Phase 4 — Persistence (~0.5-1 day)

Retire the `slotWindowA..D{X,Y,W,H,Open}` keys: the generic floater mechanism clamps against
**screen** coordinates (`showFloaterWindow`), which means nothing for a child window. New
keys: `mdiOpenSlots` (bitmask), `mdiFocusedSlot`, `mdiTileMode`, and each window's bounds
**normalised** to the area, which sidesteps the whole class of "the area is a different size
now" bugs when panels collapse or the monitor changes.

### Phase 5 — Polish (~1 day)

Inspector adopts the newly focused canvas's selection instead of blanking; delete the dead
`recycleWindows` option (`EditorOptionsDialog.h:17`, never consulted anywhere) and put
`mdiAutoTile` in its row; window titles carry the patch name and LOCAL badge; theme applied
to all four; docs updated (`manual/07-shortcuts.md`, the in-app shortcuts dialog, CHANGELOG,
this file, STATUS).

## Effort

**~7-9 developer-days**, the bulk in phase 1 where the duplication goes. Phases 0 and 1
leave the code better even if the MDI stopped there.

## Risks

1. **Edits lost on focus change** if phase 0's queue fix is not in. Highest-impact silent
   bug: editor and synth desync. (Done.)
2. **Dangling `Module*` in a background canvas**: every patch replacement must call
   `setPatch` on that slot's canvas unconditionally. Audit `setPatchDataCallback`,
   `replacePatchInSlot`, `newPatch`, `loadPatchFromFile`. Same shape as the slot-replace
   crash fixed in 0.12.0. (Done in phase 1; `replacePatchInSlot` needed the call moved to
   right after the `std::move`, see above.)
3. **`switchToSlot` running inside a mouse-down**: must not open modals or destroy the
   clicked component.
4. **Keyboard focus when going from 1 to 2 documents**: JUCE reparents the first view into a
   new window and focus is lost; regrab it after tiling.
5. **Zoom becomes per slot**: the View menu readout must read the focused canvas.
6. **Cable drag across windows** must end harmlessly.

## Verification

Without hardware:

- Open A and B, edit both, and confirm through the SysEx monitor (`Ctrl+9`) that each canvas
  addresses **its own** slot.
- Click between windows: inspector, header, snapshots and variations follow focus.
- Close B: its patch survives and the slot bar still shows it.
- Tile 2x2 with 2, 3 and 4 windows; collapse `Ctrl+I` and `Ctrl+Shift+I` and confirm the
  windows rescale without drifting off the area.
- Restart: open slots, focus and layout come back.
- **New capability worth testing**: drag a module from the browser into any sub-window. It
  never worked in the OS pop-outs because `MainLayout` is the only `DragAndDropContainer`;
  in the MDI every sub-window is inside it.

With the G1 connected:

- Edit an unfocused slot and confirm it reaches the right slot.
- Press a front-panel slot button during a cable drag: it must not steal focus or cut the
  drag.
- Move focus between windows repeatedly and confirm the debounce never leaves the synth on a
  different slot from the one being edited.
