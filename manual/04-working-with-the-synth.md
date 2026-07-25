# 4. Working with the Synth

## Slots

The G1 runs up to four patches at once in slots A–D. The editor models the
hardware's two-level slot system faithfully:

- **Selected slot** (blinking LED): the one you're editing and playing from the
  keyboard. Plain-click a slot in the slot bar to select it; the editor loads
  that slot's patch. `Ctrl+1`–`Ctrl+4` switch from the keyboard.
- **Enabled slots** (steady LED): slots that sound. Several can be enabled at
  once. `Ctrl+click` a slot to enable/disable it without selecting, the same
  gesture as `Shift+slot button` on the front panel.

Each slot keeps its own patch, undo history and sync state; background slots
never contaminate the one you're working in. A transfer in one slot no longer
blocks the others: you can keep editing slot A while slot B is uploading or
downloading.

As soon as the editor learns which slots are populated, it downloads their
patches in the background, one at a time, so switching to a slot for the first
time is instant instead of triggering a full fetch. Switching back to a slot the
editor already holds an up-to-date copy of doesn't re-download it either; a
genuine change on the synth (program change, bank load, reconnect) always does.

## Slot pop-out windows

**Right-click a slot row** in the slot bar to open that slot's patch in its own
window. This is how you work on two or more patches side by side; the main
window keeps its A–D slots working exactly as before.

Inside a slot window, everything is independent: canvas, modules, cables,
parameters, morph/knob/MIDI-CC assignment, module rename and its own undo/redo
history. Edits land on the right slot even when it doesn't have hardware focus.

- The window follows the synth live: turning a physical knob on the front panel,
  or a light or meter moving, animates that slot's window too.
- `Ctrl+R` / `Ctrl+Shift+R` randomize (uniform / gaussian) and `Ctrl+S` /
  `Ctrl+Shift+S` save / save-as act on **that window's** slot and honour its own
  module selection.
- `Ctrl+I`, or the thin arrow strip at the canvas's left edge, hides the
  Inspector to give the canvas the window's full width.
- When the synth's front-panel focus moves to a slot that has a window open,
  that window comes forward and its title gains **"- Focused"**, mirroring the
  original Nomad editor's highlighted title bar.

The top settings bar (macros, CPU/voice meters) stays in the main window only,
matching the original editor.

## Editor ↔ synth sync

While connected, every edit (parameters, cables, modules, morphs, knob and CC
assignments, patch name) is streamed to the synth as you make it, and changes
made on the synth's front panel come back to the editor. There is no "send"
button to remember.

Selecting a slot fetches its patch from the synth.

## Opening a patch: choosing where it goes

Opening a `.pch` (File → Open, or either preset browser) asks **where to put
it**. The chooser lists slots A–D with the patch currently in each, defaults to
the active slot, and adds a **Local** option:

- Pick **A–D** and the patch loads into that slot and uploads to the synth,
  replacing what was there.
- Pick **Local** and the patch loads into the editor only; nothing is sent to
  the synth. Use it to look through patches without disturbing what the rack is
  playing.

A slot whose editor patch is not known to match the synth (loaded Local, or
loaded/built while disconnected) carries a **LOCAL** badge in the slot bar. The
badge clears as soon as that patch is uploaded to, or fetched from, the synth.

## The synth patch browser

The right-side browser (`Ctrl+B`) lists the synth's 9 internal banks. You can:

- search and hide empty positions,
- **load** a patch into a slot,
- **store** the current patch to a bank position,
- **copy, move and delete** patches inside synth memory.

Legacy Nord Modular 2.10 files are tagged **PCH2** in the disk browser and load
transparently.

## Bank transfers (Device menu)

- **Save Bank to Disk**: dump a whole synth bank to a folder; position
  metadata is preserved in the `NN - Name.pch` filenames.
- **Send Bank to Synth**: upload a folder of patches into a bank, with an
  overwrite warning; a failed transfer stops cleanly.
- **Backup All Banks to Library**: mirror all 9 banks into your preset
  library's `Banks/Bank1`–`Bank9` folders in one action.

All transfers show progress and can be cancelled.

## Controller snapshot (Device menu)

**Send Controller Snapshot** asks the *synth* to emit the current values of the
patch's MIDI CC assignments as CC messages from its MIDI OUT, the same
function as the front panel's CTRL SNAP SHOT, handy for priming a sequencer
recording. It does not change any synth state.

## Send speed

Editor Options (`Ctrl+,`) includes a **send speed** setting that throttles bulk
parameter streams (Mutator, Randomize) so large patches don't overrun the
synth. Normal knob edits are always sent immediately.
