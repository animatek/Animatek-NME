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

## The four slots on screen

The slots are **sub-windows inside the main window**, tiled the way the original
Clavia editor and Nomad arranged patches. This is how you work on two or more
patches side by side, and nothing ever gets lost behind another application.

Open slots **tile themselves**: one fills the work area, two split it down the
middle, three go in thirds, four go 2x2, and the layout re-flows as you open and
close them. Nothing to arrange unless you want to: drag or resize a sub-window
and the windows stay where you put them from then on, with **View > Slots > Tile
Slots** to re-flow.

- `Ctrl+Shift+1`–`Ctrl+Shift+4` (`Cmd+Alt+1`–`Cmd+Alt+4` on macOS) shows or hides a
  slot's sub-window, and so does
  right-clicking its row in the slot bar.
- `Ctrl+1`–`Ctrl+4` switches to a slot, opening it if it was closed.
- `F4` (or `F11`), or a sub-window's maximise button, blows the focused slot up to the
  whole work area and back again for a closer look.
- `Ctrl+Shift+` an arrow moves the focused slot to the neighbouring tile, so the
  patch you are working on goes where you want it. **View > Slots > Rotate
  Slots** shifts them all round at once.

The sub-windows slide to their new places rather than jumping. Turn that off
with **Animate Slot Tiling** in Editor Options (`Ctrl+,`) if you prefer it
instant.

Each slot keeps its own canvas, selection and undo history, and edits land on
the right slot even when it doesn't have hardware focus. Every sub-window
follows the synth live: turning a physical knob on the front panel, or a light or
meter moving, animates the right slot. `Ctrl+R` / `Ctrl+Shift+R` randomize
(uniform / gaussian) and `Ctrl+S` / `Ctrl+Shift+S` save / save-as act on the
**focused** slot and honour its own module selection.

The Inspector, header bar, browsers and status bar are shared and follow
whichever sub-window has focus, and a background canvas keeps its selection, so
the Inspector picks up where that slot left off instead of going blank. The
focused sub-window is edged in the theme's text colour.

Which slots you had open, which one had focus and how they were arranged all come
back when you reopen the editor. Connecting to the Nord then lines the work area
up with the slots the synth actually has enabled, once; after that the windows
are yours alone, and pressing slot buttons on the front panel moves focus without
ever closing one.

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

If you always work in one slot, the question gets in the way. Turn **Ask which
slot when opening a patch** off in Editor Options (`Ctrl+,`) and every open goes
straight into the slot on screen and uploads, with no dialog at all.

## The synth patch browser

The right-side browser (`Ctrl+B`) lists the synth's 9 internal banks. You can:

- search and hide empty positions,
- **load** a patch into a slot: double-click puts it in the slot you are on, and
  right-click offers **Load to Slot A..D** so that with several sub-windows open
  you can pull a patch into a particular one without leaving the slot you are
  working in,
- **drag** a patch onto a slot to load it there. Drop it on that slot's
  sub-window, or on its row in the slot bar down the left side, which also works
  for a slot whose window is closed: it opens on the way. The target lights up
  while you are over it, and bank nodes and empty positions cannot be picked up
  in the first place. Disk files drag onto a slot the same way, from the **Disk**
  tab or from the `Ctrl+B` window, and that includes the **BANK**-tagged ones,
  which are ordinary patches saved out of a bank rather than bank files. Unlike
  File → Open they do not ask which slot, because the drop already said. Snippets
  keep going onto the canvas, where they merge into the patch at the point they
  land,
- **store** the current patch to a bank position,
- **copy, move and delete** patches inside synth memory.

Legacy Nord Modular 2.10 files are tagged **PCH2** in the disk browser and load
transparently.

## Storing a patch in the synth

Next to the patch name in the header there is a store button showing the bank
location the patch on screen came from, as `bank:position` (`1:01` for the first
patch of bank 1, `--` when the editor does not know of one). One click writes the
patch straight back there, no dialog: the status bar confirms it.

The location follows each slot on its own, so with four sub-windows open the
button always shows the location of the slot you are looking at. A patch you
just created or opened from a file has no location yet, so the button shows `--`
and a click opens the **Store Patch to Bank** dialog instead. That dialog, which
is also what the **Store** button in the left column and **Device > Store to
Bank** open, now starts on the patch's own location rather than at bank 1
position 1, so putting a patch back where it came from is a single OK. Storing
somewhere else makes that new place the patch's location from then on.

The editor knows a location for certain when you loaded the patch from the
browser or stored it yourself. For everything else it has to work it out, because
the G1 does not say where a patch came from: loading one from the front panel
only tells the editor *which slot* changed, never from which bank position. Same
for the patches already sitting in the slots when the editor connects.

In those cases the editor looks the patch name up in the bank list:

- **one position** carries that name: that is the location, and the button shows
  it.
- **several positions** carry it: the button shows `?`, because storing on a
  guess would overwrite the wrong patch. A click opens the Store Patch to Bank
  dialog on the first of them and the status bar lists all the matches, so you
  can pick the one you loaded. From then on that slot's location is settled and
  one click stores again.
- **no position** carries it: the button shows `--` and a click opens the dialog.

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

## Borrowing the synth's display

**Show the editor on the synth display** in Editor Options (`Ctrl+,`) is off by
default. With it on, opening About, Editor Options, MIDI Settings, Synth
Settings, Patch Settings or the shortcuts list borrows the synth's own display
for as long as that window is up: it reads `ANME 0.16v` instead of the patch
name, and the patch name comes back when the window closes.

Nothing happens to the patch. Only the message that sets the name on the synth
is sent, so the editor's patch is untouched, nothing is marked modified and
nothing lands on the undo stack. On the synth it changes the edit buffer only,
never the bank, exactly like renaming a patch without storing it.

One thing to know before you turn it on: if the editor is killed with one of
those windows open, the caption stays on the synth's display until you reload
the patch, and pressing Store on the front panel in that state would write the
caption into the bank. That is why it is opt-in.
