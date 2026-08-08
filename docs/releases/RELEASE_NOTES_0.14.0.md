# Animatek NME 0.14.0: put it where you want it

Adding a module and pasting one stop guessing where you meant. Both hand the modules to the
pointer as outlines and let the click that follows place them, which is how the original
Clavia editor has always worked, and what four separate complaints turned out to be asking
for. The Drum Synthesizer also arrives with all 29 of Clavia's factory presets, and the knobs
now behave the way the original's do out of the box.

## What's new

### 🫥 Paste and Add Module hand you the modules

Neither one places anything any more. They show the modules as outlines that follow the
cursor, and you click where you want them. `Escape` or a right-click throws them away without
adding anything.

Because the click chooses the spot, it chooses the **area** as well, and that quietly closes a
lot at once:

- Copies land where you are working instead of somewhere off in the canvas.
- Poly to Common and back works, in either direction, for the first time.
- Pasting inside the Common area works at all.
- The clipboard belongs to the whole editor, so a copy made in one slot can be pasted into
  another slot's window.

Adding from the keyboard is still the quickest route and got quicker: **Enter**, a few
letters, **Enter** to pick the module, **Enter** again to drop it where the pointer already
is. Reach for the mouse only when the module belongs somewhere else, and the outline waits for
as long as you take. Dragging from the module browser is unchanged, since a drag already ends
where you release it.

### 🪜 Nothing gets buried

Drop a module on top of another and the modules below move down their column to make room,
cascading into whatever they run into. Before, the older module was simply covered and there
was nothing on screen to say it was still in the patch. Undo puts the whole column back.

### 🥁 The Drum Synthesizer's factory presets

All 29 of Clavia's own, from Kick 1 to Perc 6, in a **Factory** group that stays folded away
in the Inspector and sits behind a **Factory** submenu on the module's right-click menu, so
your own presets stay where you can reach them. Factory presets cannot be renamed or deleted
and never touch your own preset files.

The module also comes up the way the original's does now, with its own default settings rather
than a middle value in every knob, and its preset box names the preset its settings actually
match, reading `none` when they match nothing.

### 🎛️ Knobs behave like the original's

Knobs answer vertical movement by default, which is what the original editor does and what
most people reach for first. Whatever you have chosen under **Ctrl+, > Knob Control** is left
alone.

Circular knob control now reads the dial rather than counting turns: grab a knob at the point
marked 100 and it goes to 100, instead of turning up by 100 from wherever it was. The angle is
measured around the knob's centre over the same -135 to +135 degree arc the pointer is drawn
with, so the spot you touch is the value you get, and the response no longer speeds up near
the point you grabbed and crawls far from it.

### 👁️ Overlays on the View menu

The overlay readouts are under **View > Overlays** as well as on their function keys, ticked
to show which one is open. They had been on `F5` and `F7`-`F10` only, which is easy to miss:
the request that prompted this was for a whole-patch module cost readout, which `F10` had been
giving since 0.13.0.

## Fixed

- **Cut, Copy, Paste and Duplicate are on the Edit menu**, where anyone would look for them.
  They had always been on the keyboard and on a module's right-click menu, and nowhere else.

- **Paste and Duplicate can be undone.** Both created their modules outside the undo history,
  so `Ctrl+Z` after either one did nothing at all.

- **The Drum Synthesizer's filter type was labelled the wrong way round.** HP and LP were
  swapped, so picking LP sent the synth the value it reads as HP: the label said one thing and
  the module played the other. Clavia's own factory presets settle it, and every patch built
  or loaded now reads correctly.

- **The slave LFOs no longer carry arrow buttons they never had.** The arrows ran through
  LFOSlvA's Mono button and over the bottom edge of LFOSlvC and LFOSlvE. The original editor
  puts them only on the slave oscillators and the sine bank.

- **On macOS, showing and hiding a slot moves to `Cmd+Alt+1`-`Cmd+Alt+4`.** macOS keeps
  `Cmd+Shift+3` and `Cmd+Shift+4` for its own screen capture and never passes them on, so two
  of the four slots could not be toggled from the keyboard at all. Nothing changes on Windows
  or Linux.

- **Saving a selection as a snippet no longer overwrites the clipboard**, which it did on its
  way past.

- **A long-lived patch can no longer run out of module numbers when inserting a snippet.**
  The number is stored in seven bits, and snippet insertion counted up from the highest in
  use, so enough add-and-delete cycles pushed it past 127 and the insert failed.

## Thanks

Nocticore, again, for the reports behind most of this release.

## Notes

Still beta. Back up any patch you care about before using this with your Nord Modular.
