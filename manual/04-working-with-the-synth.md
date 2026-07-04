# 4. Working with the Synth

## Slots

The G1 runs up to four patches at once in slots A–D. The editor models the
hardware's two-level slot system faithfully:

- **Selected slot** (blinking LED): the one you're editing and playing from the
  keyboard. Plain-click a slot in the slot bar to select it; the editor loads
  that slot's patch. `Ctrl+1`–`Ctrl+4` switch from the keyboard.
- **Enabled slots** (steady LED): slots that sound. Several can be enabled at
  once. `Ctrl+click` a slot to enable/disable it without selecting — the same
  gesture as `Shift+slot button` on the front panel.

Each slot keeps its own patch, undo history and sync state; background slots
never contaminate the one you're working in.

## Editor ↔ synth sync

While connected, every edit — parameters, cables, modules, morphs, knob and CC
assignments, patch name — is streamed to the synth as you make it, and changes
made on the synth's front panel come back to the editor. There is no "send"
button to remember.

Loading a `.pch` from disk uploads it to the selected slot. Selecting a slot
fetches its patch from the synth.

## The synth patch browser

The right-side browser (`Ctrl+B`) lists the synth's 9 internal banks. You can:

- search and hide empty positions,
- **load** a patch into a slot,
- **store** the current patch to a bank position,
- **copy, move and delete** patches inside synth memory.

Legacy Nord Modular 2.10 files are tagged **PCH2** in the disk browser and load
transparently.

## Bank transfers (Device menu)

- **Save Bank to Disk** — dump a whole synth bank to a folder; position
  metadata is preserved in the `NN - Name.pch` filenames.
- **Send Bank to Synth** — upload a folder of patches into a bank, with an
  overwrite warning; a failed transfer stops cleanly.
- **Backup All Banks to Library** — mirror all 9 banks into your preset
  library's `Banks/Bank1`–`Bank9` folders in one action.

All transfers show progress and can be cancelled.

## Controller snapshot (Device menu)

**Send Controller Snapshot** asks the *synth* to emit the current values of the
patch's MIDI CC assignments as CC messages from its MIDI OUT — the same
function as the front panel's CTRL SNAP SHOT, handy for priming a sequencer
recording. It does not change any synth state.

## Send speed

Editor Options (`Ctrl+E`) includes a **send speed** setting that throttles bulk
parameter streams (Mutator, Randomize) so large patches don't overrun the
synth. Normal knob edits are always sent immediately.
