# Manual test plan: the unreleased work since 0.15.1

What still needs a pair of eyes, and what exactly to look at. Everything here
was written after the automated checks passed: the unit tests and the
sanitizers cover the pure layers, and the compiler covers the file split, but
none of them can see the screen.

Four blocks, roughly 35 minutes in total. Blocks A to C need no synth; block D
needs the Nord.

## Setup

```bash
cd /mnt/SPEED/CODE/Nomad2026
cmake --build build -j$(nproc)
./build/AnimatekNME_artefacts/Debug/AnimatekNME
```

Launch it **from a terminal** so that if it crashes the output is captured.

Two things worth having to hand:

- A patch with nearly every module type in it, for the drawing checks:
  `nmedit/jMod/src/patches/all.pch`
- The previous release, to compare speed against:
  `#Ejecutables/0151/GitHub-31366273857/AnimatekNME-0.15.1-linux`

---

## Block A: does everything still draw? (~5 min)

The canvas was split from one 9,330-line file into six. It is provably the same
code, but every line that draws a module moved, so this block is a sweep for
anything that vanished.

1. Launch with no patch. The empty canvas reads "Press Enter to add modules".
2. Open `all.pch`. Every module draws its face: knobs, sliders, buttons, LEDs,
   text displays, the little icons on the buttons.
   **Look for**: a blank module face, missing knobs, a wrong colour, a missing
   icon.
3. Cables are drawn in their signal colours (red audio, blue control, yellow
   logic, grey master/slave).
4. Add a **DrumSynth**. Its preset box draws, and right-clicking it opens the
   preset menu.
5. Add a note: module bar, ANME group, Comment. It draws with its text centred,
   and double-clicking opens the editor.
6. Add a sequencer (NoteSeqA or EventSeq). Its step display draws and its
   Rnd/Clr buttons are there.

## Block B: selection, delete, undo (~15 min)

**The block that matters most.** How the editor holds on to a module was
rewritten: it used to keep the module itself and now keeps its address in the
patch. Everything that remembers a module went through this.

1. Click a module. It highlights, and the Inspector fills with its name and
   parameters.
2. Shift-click a second one. Both are highlighted.
3. `Ctrl+A` selects everything in that area. `Escape` clears it.
4. Drag a box over empty canvas across several modules. They all select, notes
   included.
5. Select one module and press `Delete`. It goes, and the Inspector falls back
   to the patch-wide "Assignments" view.
6. **`Ctrl+Z`. The module comes back _and it is selected again_.** This is new:
   the selection used to be dropped. If it comes back unselected, say so.
7. Undo and redo five times back and forth over that delete. Nothing should
   crash, and no module should appear twice.
8. Select three modules, delete them, undo. All three come back.
9. Drag one module around. It snaps to the grid and **cannot leave the canvas**
   in any direction, including off the bottom.
10. Select several and drag. They move together keeping their shape.
11. Nudge the selection with the arrow keys. At an edge it stops rather than
    disappearing.
12. `Ctrl+C` then `Ctrl+V`. Outlines hang off the pointer; a click puts them
    down; the block that lands is selected.
13. `Ctrl+D` duplicates the selection with its cables.
14. **With the Inspector showing a module, delete that module.** No crash, and
    the panel falls back cleanly.
15. Double-click a module. Its DSP cost appears in a badge. A click elsewhere
    puts the badge away.
16. Double-click for a badge, then delete that same module. The badge goes with
    it, no crash.
17. Open two slot windows (`Ctrl+Shift+1` and `Ctrl+Shift+2`). Select in one and
    edit in the other; then delete in one while the other has a selection.

## Block C: redrawing (~5 min)

Several redraws were narrowed to just the area that changed, which is what made
big patches fast. The risk that carries is **leftover pixels**: if a rectangle
is a shade too small, something stays on screen that should have gone. That is
the only thing to look for here.

1. Rest the pointer on a knob. Two small arrows appear just under it.
2. Move along the module's controls. The arrows follow from one to the next,
   leaving nothing behind.
3. Move the pointer off the module. The arrows disappear cleanly.
4. Rest on one of the arrows. It lights up. Move to the other: the highlight
   moves, and the first one goes dark.
5. Press and hold an arrow. The value steps repeatedly and the readout follows.
6. Rest the pointer on a text note. Two small triangles appear at its bottom
   corners; moving off takes them away cleanly.
7. Rest exactly on a corner triangle. The cursor becomes a resize cursor.
8. Zoom in a few steps (`Ctrl++`) and repeat 1 to 3. The arrows still land under
   the right control.

## Block D: with the Nord connected (~10 min)

1. Connect and load a patch from a bank.
2. Play. LEDs and meters animate; nothing freezes lit or smears.
3. Switch slots from the editor's slot bar. The slot you left goes dark rather
   than staying frozen.
4. Close every slot sub-window but one, then press **B, C and D on the synth's
   front panel**. Each one opens its window and takes focus.
   (This is the one that caught a real bug: the editor used to follow the panel
   only into a window that was already open, so the buttons looked dead.)
5. Start dragging a cable and, mid-drag, press another slot on the panel. It
   must **not** cut the drag.
6. **Speed.** Open your heaviest patch (or `all.pch`). Scroll it, drag a module
   about, zoom in and out, with the meters running. Then do exactly the same in
   0.15.1 (path above) with the same patch. The new one should feel smoother,
   and the gap should be widest on a patch with many modules and the meters
   moving.
7. Turn a knob on the synth: the editor follows.
8. Turn a knob in the editor: the synth follows.
9. `F3` (module DSP cost), `F5` (values), `F7`, `F8`, `F9`. Each overlay reads
   out and turns off when pressed again. `F10` still works as the old alias for
   `F3`.
10. `F4` blows the focused slot up to the full area; `F4` again puts it back.
    `F11` still does the same.

---

## Reporting

For anything that fails, the useful three things are: **which step**, **what
happened instead**, and a **screenshot** if it is visual. If it crashes, the
terminal output is the valuable part.

Blocks A and C are about pixels, so a screenshot usually says it faster than a
sentence. Block B is about behaviour, so the sequence of clicks matters more
than the picture.
