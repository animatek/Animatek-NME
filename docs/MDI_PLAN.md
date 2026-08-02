# MDI: the four slots inside the main window

Phased plan for replacing the per-slot OS pop-out windows with internal sub-windows in the
main window's central area. Written 2026-08-01 against 0.12.0. **All phases are done**, plus
tile reordering and the re-tile animation (added 2026-08-02 after a second look at Hyprland
itself). The optional "Arrange" command for Free mode is still unbuilt.

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

`PatchCanvasComponent::getPrimarySelection()` was deferred here until it had a caller; phase
5 gave it one.

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

### Phase 3 — Tiling and the View menu (DONE)

Javier asked for **dynamic tiling**, the way niri and Hyprland do it, rather than the list of
manual layout modes originally planned: the layout is a function of how many sub-windows are
open, not something the user arranges. One fills the area (JUCE's
`useFullscreenWhenOneDocument`, no window frame at all), two split it down the middle, three
go in thirds, four go 2x2 in slot order. `applyLayout()` re-flows on every open, close and
resize. Column boundaries are computed from the area (`start + extent * i / n`) rather than
accumulated from a width, so they always add up with no rounding gap.

Plus a **focus mode** (`F11`), the tiling-WM monocle: the focused sub-window is laid over the
whole area while the others stay tiled behind it, so coming back out is just a re-tile with
nothing to restore. Moving focus in focus mode moves which window is blown up.

Dragging or resizing a window drops the mode to **Free** and the windows stay put;
`View > Slots > Tile Slots` goes back to Auto. Detecting that needs a
`ComponentListener` on the container windows, and `juce::MultiDocumentPanel` inherits
`ComponentListener` **privately**, so `SlotMdiArea` cannot register itself — hence the
`WindowWatcher` forwarder. An `applyingLayout` flag keeps our own `setBounds` calls from
being mistaken for the user dragging.

The View menu gets a **Slots** submenu: per-slot open/close with tick marks and the patch
name, Tile Slots, and Focus Mode. `onLayoutChanged` → `menuItemsChanged()`, or the native
macOS menu bar keeps stale ticks.

Keyboard gotcha, worse than the collision originally noted: **X11 reports a shifted digit by
its symbol**. When Ctrl swallows the character JUCE falls back to the *shifted* keysym
(`juce_XWindowSystem_linux.cpp`, `keyCode = unicodeChar` then the `< 0x20` fallback at
level 1), so `Ctrl+Shift+1` arrives as `'!'`, never as `'1'`. Letters survive this because
`KeyPress::operator==` case-folds `'S'` onto `'s'`; digits have no such luck. That is the same
mechanism behind the existing "Ctrl+8 arrives as DEL" workaround in the canvas.
`slotDigitFromShiftedKey()` maps the US, Spanish and UK symbols; the View menu covers any
layout it does not.

Still open: `Ctrl+Alt+T` was never bound, so the `handleFloaterShortcut` collision the plan
warned about did not arise. Note it still would — that function matches `Ctrl` without
checking `Alt`.

### Phase 4 — Persistence (DONE)

The `slotWindowA..D{X,Y,W,H,Open}` keys are retired (they went in phase 2): the generic
floater mechanism clamps against **screen** coordinates, which means nothing for a child
window. New keys: `mdiOpenSlots` (bitmask), `mdiFocusedSlot`, `mdiFreeLayout`, `mdiFocusMode`,
`mdiTileOrder`, and in Free mode each window's bounds **normalised** to the area, which
sidesteps the whole class of "the area is a different size now" bugs when panels collapse or
the monitor changes. In Auto the bitmask alone reproduces the layout, so no geometry is
written at all.

`SlotMdiArea` grew `getNormalisedSlotBounds`/`setNormalisedSlotBounds`, a public
`setTileMode` (restoring Free must not have to fake a drag), and `rescaleFreeWindows`, which
scales a Free arrangement with the work area instead of stranding windows in a corner when
the main window is resized.

**The ordering trap, hit for real:** `restoringMdiLayout` starts **true**, not false. Saving
is wired to `onLayoutChanged`, and the constructor used to open a slot before
`restoreMdiLayout()` ran — that fired the callback and wrote the default one-slot layout over
the stored one before anything had read it, so a seeded mask of 6 came back as 1. The
constructor no longer opens anything; `restoreMdiLayout()` is the only thing that does, and
its default mask (`1 << activeSlot`) covers a first run, an absent key and a corrupt `0`.
Verified all three by seeding the settings file and reading the `[MDI]` line back.

**The synth's enable mask is consulted once per connection, never continuously.** An earlier
cut had every `SlotsSelected` (0x07) and `SlotActivated` (0x09) re-derive the open windows from
the mask, which looked right on paper and was badly wrong in practice: that mask is *pinned +
selected*, and with nothing pinned it is a **single slot** that changes on every slot press. The
result was that the work area collapsed to one window and stayed there: every sub-window the
user opened was closed again the moment the synth echoed the slot change. Traced live:

```
[SLOT] Enabled slots: A
[MDI] Synced windows to enabled slots: mask=1 focused=A open=1
[SLOT] Enabled slots: B
[MDI] Synced windows to enabled slots: mask=2 focused=B open=1
```

So `reconcileSlotWindowsWithSynth()` now runs **once per connection**, latched by
`slotWindowsReconciled` and re-armed on disconnect. `scheduleSlotWindowReconcile()` defers it
400 ms past the first mask because `SlotsSelected` and `SlotActivated` arrive in either order
during the handshake, and a mask paired with a stale focused slot would leave a spurious window
open for the rest of the session. Reconciling drops focus mode if it ended up opening a second
window, so a restored F11 state cannot hide what just appeared.

Free-mode drags do not fire `onLayoutChanged` (only the first one does, when it leaves Auto),
so the destructor saves as well rather than persisting on every mouse move.

### Phase 5 — Polish (DONE, bar the animation)

- The inspector adopts the newly focused canvas's selection instead of blanking, through the
  new `PatchCanvasComponent::getPrimarySelection()`. Each canvas keeps its selection while it
  is in the background, so coming back to a slot looks like you left it.
- Sub-window titles carry the LOCAL badge (the patch name landed in phase 1). With four
  tiled, the slot bar's badge alone does not tell you which window is out of sync.
- The dead `recycleWindows` option is gone: it was saved, loaded and drawn as a toggle, and
  never consulted anywhere. No `mdiAutoTile` replaces it — dynamic tiling has nothing to
  configure, and View > Slots > Tile Slots is the only control it needs.
- The focus outline landed early, in commit 8f8f49d. It is the theme's primary colour at
  **75% alpha** over 2px; dark primary tones such as Nord Classic's are inverted to a light
  contrast so the outline remains visible over the canvas. The first cut was the accent at
  full strength and competed with the patch, and
  it is suppressed whenever a single window covers the work area (one open slot, or focus
  mode), where it would frame the whole area without telling you anything.
- Focus changes bring a tiled sub-window to the front without asking the window itself to take
  keyboard focus. The MDI mouse watcher runs after the canvas's own `mouseDown`, so using
  `toFront(true)` there stole back the focus that the canvas had just acquired and disabled its
  zoom and editing shortcuts whenever two or more slots were open.
- Sub-windows carry a **maximise button** left of the close button, as Linux window
  decorations do. `MultiDocumentPanelWindow::maximiseButtonPressed()` is overridden rather
  than inherited: the base flips the whole panel into tabbed mode, which is not a layout this
  editor offers and would strand the tiling somewhere unreachable. It routes out through
  `onSlotMaximiseRequested` to the same code F11 uses, so both paths keep the same guard and
  status message.

**A real bug this phase turned up:** Free mode latched on by itself and then stuck, because
`windowMovedOrResized` treated *any* move of a sub-window as the user dragging it.
`applyingLayout` covers our own `setBounds`, but JUCE moves these windows too — it positions
each new one as it is created and re-wraps them all when the document count crosses one. One
stray programmatic move switched tiling off, phase 4 then persisted that, and restoring it
re-armed the same state on the next launch, so it never recovered. It is now gated on
`Desktop::getNumDraggingMouseSources()`: a drag means a button is down.

Restoring also ran a full `switchToSlot` per slot it opened, since each new document takes
focus as it appears. `restoringMdiLayout` now gates `onSlotFocused` too, and restore does one
switch at the end for the slot that should actually end up focused.

**Animate the re-tile — DONE.** Dropped once, then done after looking at Hyprland proper
rather than one user's config. Sub-windows slide to their new tiles over 120 ms, behind
**Animate Slot Tiling** in Editor Options.

Multi-window re-tiling stays cheap through `useProxyComponent` on
`Desktop::getAnimator().animateComponent()`: JUCE animates a snapshot image and only moves
the real windows at the end. F11 is the deliberate exception: only one window moves, so it
animates the real component and keeps modules, cables and text freshly rendered instead of
stretching the snapshot. It is the same asymmetry that makes
Hyprland's own animation code useless to us — it animates GPU textures inside its render
loop, so there is nothing to port even though BSD-3-Clause would allow it.

Animation is opt-in per call (`applyLayout(animate, useProxy)`): opening, closing and
reordering use a proxy; focus mode does not. A plain window resize never animates, or dragging
the main window's edge would fire a new animation on every resize event.

What Hyprland has that JUCE does not is arbitrary cubic-bezier easing — their default curve
overshoots slightly (`0.05, 0.9, 0.1, 1.05`), which is what makes their movement feel alive.
`animateComponent` only offers start/end speeds, so this uses a 1 → 0 ease-out: it responds
immediately and decelerates into the target. The original 0 → 1 values were an ease-in that
lingered at the start and stopped at full speed, which made F11 feel slow and abrupt. If the overshoot
is ever wanted it is a ~30-line timer-driven curve, needing nobody's code.

### Tile reordering (DONE)

The tiling is fixed by how many slots are open, but which slot lands in which tile is the
user's to change — Hyprland's `swapwindow` and `rollnext` within a layout.
`Ctrl+Shift+` an arrow moves the focused slot to the neighbouring tile; Rotate Slots is on the
View menu. Arrow keysyms are stable under Shift, unlike the digits (see phase 3), and the
canvas only nudges modules with unmodified arrows.

**Directions are geometric, and that took a correction.** The first cut swapped along the
linear tile index, which reads as left/right in the two- and three-column layouts but not in
the 2x2, where "left" from the bottom-left tile landed a slot in the top-right. It also
wrapped, so with two slots open "move left" visibly moved a slot right. Now Left/Right flip
the column and Up/Down the row, an edge move is a no-op, and up/down simply do nothing in the
column layouts. Rotate lost its arrow binding so the arrows mean one thing.

**Focus is tracked here, not read back from `MultiDocumentPanel`.** After a reorder the
accent outline and the inspector followed the wrong slot, and the cause was deeper than the
animation: `MultiDocumentPanel::setActiveDocument()` **does not assign the active document**.
It calls `toFront()` and lets `updateActiveDocumentFromUIState()` re-derive it from
`TopLevelWindow::isActiveWindow()`, which is dependable for real desktop windows and not for
child windows inside a work area. Instrumenting it showed `focusSlot(0)` leaving the focused
slot at 3 — it had been doing nothing at all, so "keep focus on the moved slot" could not work
by construction.

`SlotMdiArea` now owns `focusedSlot`. `focusSlot()` sets it, brings the window forward, and
fires `onSlotFocused`; a `MouseListener` registered on each `SlotView` with
`wantsEventsForAllNestedChildComponents` catches clicks anywhere in a canvas; opening and
closing a slot move it. `activeDocumentChanged()` is overridden to do nothing, since following
it is what made focus jump. Related: `SlotSubWindow::visibilityChanged()` also overrides
`TopLevelWindow`'s, which answers a show with `toFront(true)` — child windows are not spared
because `Component::getPeer()` walks up to the main window's peer, so each window finishing
its proxy animation was yanking itself to the front.

Verified by driving the sequence in code and reading back the tile order and focused slot:
from `0123` with A focused, Right → `1023`, Down → `1320`, Left → `1302`, Up → `0312`, focus
on A throughout.

**Newly opened slots go on the right.** `openSlot` moves the slot to the end of `tileOrder`,
so a window appears after whatever is already open instead of jumping ahead of it because its
letter sorts first. Verified: opening A, B, C, D one at a time gives A, AB, ABC, ABCD.

`tileOrder` is a permutation of slot indices that `applyLayout` iterates instead of 0..3. It
persists as `mdiTileOrder`, validated on load as a genuine permutation: a duplicate or a
short string would silently drop or double a slot, so both fall back to `0123`. Verified with
seeds `3120` (honoured), `3121` and `012` (both rejected). Opening
or closing a slot currently makes the other windows jump to their new tiles. Sliding them
instead makes the re-flow legible, and JUCE gives it for free: run the `setBounds` calls
`applyLayout()` already computes through `Desktop::getAnimator().animateComponent()`.
Two conditions. Keep it short — niri uses 800 ms, which is fine for a desktop but absurd
inside an editor; ~150 ms. And measure it: animating four canvases while the synth streams
lights and meters is not obviously free, so it needs an Editor Options switch to turn off.
The gist's expanding-circle and crossfade shaders are compositor GLSL and have no cheap
equivalent here — the idea transfers, the implementation does not.

### Optional — "Arrange" for Free mode

From EdenQwQ's `smart-tile.py` ([gist](https://gist.github.com/EdenQwQ/a0c700315f6c704d03badfab6d6e45ce),
[repo](https://github.com/EdenQwQ/smart-tiling)): sort windows by area descending, anchor the
largest at the origin, then for each next window try the corners of the already-placed ones as
candidate positions, reject overlaps, score by free space (preferring unbounded), and centre
the result.

**It does not belong in Auto mode.** Its purpose is packing windows whose sizes the user
already chose, which is Hyprland's floating-window problem; our sub-windows have no size of
their own, so an exact partition of the area beats packing and leaves no gaps by
construction. Where it does fit is Free mode, after the user has dragged and resized: a
"tidy these up without changing my sizes" command next to Tile Slots. Worth having, not worth
blocking on.

**Licensing — read before writing any of it.** Neither the gist nor the repository states a
licence, and the repository was archived in January 2025. No licence means all rights
reserved, not "free because it is on GitHub": that code cannot be copied into this project
even with attribution, and it would not be GPL-3.0 compatible if it were. What is fine is
reimplementing the *approach* from the description above — algorithms are not copyrightable,
only their expression is — so write it from scratch in C++ and credit the gist as the
inspiration, as this document does. Do not translate the Python line by line; that is a
derivative work. If its actual code is ever wanted, it needs the author's permission.

Nothing from these gists has been used so far. The dynamic tiling in phase 3 came from
Javier's own spec (one full, two split, three in thirds, four 2x2), which is an exact
partition of the area and shares no code or approach with the packing heuristic above.

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
- Restart: open slots, focus and layout come back. Then connect: the windows line up with the
  synth's enabled slots once, and pressing slot buttons on the panel afterwards moves focus
  without closing anything.
- **New capability worth testing**: drag a module from the browser into any sub-window. It
  never worked in the OS pop-outs because `MainLayout` is the only `DragAndDropContainer`;
  in the MDI every sub-window is inside it.

With the G1 connected:

- Edit an unfocused slot and confirm it reaches the right slot.
- Press a front-panel slot button during a cable drag: it must not steal focus or cut the
  drag.
- Move focus between windows repeatedly and confirm the debounce never leaves the synth on a
  different slot from the one being edited.
