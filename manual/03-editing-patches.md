# 3. Editing Patches

## Adding modules

- **Quick Add**: press `Enter` or double-click empty canvas, type a few
  letters and pick from ranked results. It searches names, categories and a
  hand-written tag table (try `reverb`, `random`, `snare`…).
- **Module browser**: browse the full palette by category and drag modules
  onto the canvas, into whichever sub-window you drop them on.
- **Add Module**: right-click empty canvas for the full menu by category.

Quick Add and the Add Module menu do not place the module where you asked for
the menu: they hand it to the pointer as an outline, and the click that follows
puts it down, exactly as the original editor does. Move to the spot you want,
click, and it lands there; `Escape` or a right-click throws it away without
adding anything. Because you choose the spot, you also choose the area: the
same outline can be carried from Poly to Common, or into another slot's window.
Dragging from the module browser is unchanged, since a drag already ends where
you release it.

Adding from the keyboard stays as quick as it was: `Enter`, a few letters,
`Enter` to pick the module, `Enter` again to drop it where the pointer already
is. You only need the mouse if you want to put the module somewhere else, and
the outline waits for as long as you take.

Whatever sits where you drop a module moves down its column to make room, and
anything that then gets in *its* way moves down too, so nothing is ever left
hidden underneath.

The Poly and Common areas accept different module sets, matching the hardware.
Modules use DSP resources on the synth, and every one of these three routes
prints the module's cost next to its name ("Audio In (2.2%)") so you can choose
with the budget in view. The header's Load meters track the patch total; see
[Voices and DSP load](02-interface.md#voices-and-dsp-load).

## Selection and arrangement

- Click selects; `Shift`-click and rubber-band extend the selection; `Ctrl+A`
  selects the whole section; `Escape` clears.
- Drag to move (the grid keeps everything tidy); arrow keys nudge one cell.
- `Ctrl+X/C/V` cut/copy/paste, `Ctrl+D` duplicates **with cables**. Paste works
  like Add Module: the copied modules hang off the pointer as outlines and the
  next click drops them, so one clipboard serves both areas, every slot window,
  and any spot you can see. `Escape` cancels.
- `Delete` removes the selection, cables included. Everything is undoable;
  each slot has its own undo history (`Ctrl+Z` / `Ctrl+Shift+Z`).

## Renaming modules

Give a module your own name from its right-click menu, or from the **Name**
field at the top of the Inspector. Renaming is a normal, undoable edit
(`Ctrl+Z` takes it back). The name reaches the synth as you type it, so storing
the patch to a bank right afterwards keeps the names you gave.

## Comments

A comment is a text note that lives on the canvas: what a module does, which knob
to reach for, what you were trying.

Place one the way you place a module. The module bar's last tab, **ANME**, holds
what this editor adds to a patch that the G1 knows nothing about; click or drag
its **Comment** chip onto the canvas. Right-clicking an empty part of the canvas
and choosing **Add Comment** does the same thing where you clicked.

- **Write**: double-click the note. The text is centred and bold, and it grows
  with the note, so a big note reads as a heading across the patch.
- **Move**: drag it.
- **Resize**: pull either bottom corner, sideways for more columns and downwards
  for more rows. The right-click menu has the exact sizes if you prefer them.
- **Zoom to it**: `Z`, with the note selected.
- **Copy, cut, paste, duplicate**: `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `Ctrl+D`, the
  same keys the modules use, with **Copy** and **Duplicate** in the note's
  right-click menu too. A pasted note follows the pointer as an outline until
  you click where it goes, and it can land in either voice area.
- **Delete**: `Delete`, or the right-click menu.

All of those are undoable.

A comment is painted as a module panel, in the same colour the modules around it
wear under whichever theme you are using, and it holds its rectangle of the grid
like a module: dropping a module on top of one pushes it down the column, and
vice versa. It is an editor thing only: the G1 has no such module and nothing about a
comment is ever sent to it. The text is saved inside the `.pch`, so it travels
with the patch when you share it.

## Cables

- **Create**: drag from any connector to a compatible one. Valid targets light
  up while you drag; outputs connect to inputs.
- **Chained cables**: you can also drag from one *input* to another *input*,
  daisy-chaining a net exactly like the original editor, e.g. Keyboard Note →
  OscA1 Pitch, then OscA1 Pitch → OscA2 Pitch. The hardware rule is enforced:
  a net can only be driven by **one** output, and illegal targets won't light
  up.
- **Re-route**: hold `Ctrl` (`Cmd` or `Alt` work too) and drag a connector that
  already has a cable. The cable comes off that end and follows the pointer from
  the end that stays put, so you can drop it on another connector, which is how
  you move a patch's wiring onto a replacement module one cable at a time.
  Nothing happens to the patch until you let go, so you can carry a cable end
  around to see where it is allowed to go and let go anywhere that is not a
  legal target: the cable simply comes back. If the connector has several cables
  the one drawn on top comes off first; repeat to take the ones under it. The
  move is a single undo step.
- **Delete**: right-click a connector to remove its cables.
- Cable visibility filters, styles and the `S` shake help untangle big patches.

## Parameters

- Knobs, sliders, buttons and selectors edit live and sync to the synth.
- **Hover** any control to read its value in the parameter's own units; drag it
  and the readout follows live.
- **Nudge arrows.** Hovering a knob or a slider pops two small buttons under it,
  the way the original editor does: the left one takes the value down one step,
  the right one up one, and holding either repeats. This is how you land on an
  exact frequency or MIDI note instead of hunting for it with the mouse. The
  value reads out while you step it, and the whole press undoes in one go,
  however many steps it took. The four morph dials in the header bar have the
  same arrows, drawn inside the dial because their caption sits right below it.
- **`+` and `-` do the same from the keyboard.** With the pointer resting on a
  knob or a slider, `+` steps it up and `-` steps it down, which is often easier
  than reaching for the arrows on a touchpad. Hold either and it repeats, and
  the whole run undoes in one step.
- **The Inspector lists them all.** Selecting a module fills a **Parameters**
  section at the top of the Inspector with every knob, slider and switch it has,
  each with the figure it currently reads in its own units. Drag a value up or
  down to change it, or **double-click it and type**: the editor accepts the
  reading as it is written, so `440Hz`, `C#3` or `-12(Oct)` all land where you
  expect, and a plain number picks the nearest step the parameter can actually
  hold. `Enter` keeps what you typed, `Esc` throws it away. Whatever the module
  wears as a **button** is a button here too, carrying the same lettering its
  face does: click it and a two-state switch flips, lighting up while it is on,
  and a selector like the DrumSynth's LP/BP/HP walks round its options. The
  list follows the
  module rather than the other way round, so a knob turned on the canvas reads
  true here as it moves, and every edit undoes in one step.
- Right-click a parameter to assign it to a **morph group**, a **hardware
  knob**, or a **MIDI controller**, and to **lock** it against randomization.
- **Click a frequency display** on an oscillator, a slave LFO or a filter and it
  changes the units it reads in, the way the original editor does. An absolute
  frequency alternates between hertz and the note it lands on; a slave
  oscillator's box goes round the partial ratio, the interval in semitones, and
  the frequency its master actually puts it at. Hovering shows the units the box
  is *not* displaying, so a suboscillator can be set to `-12(Oct)` and checked in
  hertz without changing anything. The choice belongs to the module and is saved
  with the patch.

## Reading a patch: the overlay keys

Five function keys label the whole patch at once. They toggle, so you can leave
a readout open while you work rather than holding a key down.

| Key | What it labels |
|-----|----------------|
| `F5` | Every parameter's value. A morphed parameter shows the span its morph sweeps it across, e.g. "46Hz-2.30kHz" |
| `F7` | Morph group membership |
| `F8` | Hardware knob assignments |
| `F9` | MIDI CC assignments |
| `F3` | Each module's DSP cost |

The same five are on **View > Overlays**, ticked so you can see which one is
open, with **None** to close it.

## Module presets

Select any module and the Inspector grows a **Presets** section under its
assignments: click a name to recall it, the **x** to delete it, right-click to
rename, and **+ Save current settings** to capture the module as it stands. The
section folds away from the chevron in its title, and the same list is on the
module's own right-click menu.

Recalling a preset is a single undo step, not one per parameter. A preset is
simply a named parameter snapshot of a module type, so any module can have them:
sequencers, filters, the DrumSynth, anything. They live in a **Presets** folder
in your patch library as one `.pchp` pack per module type; see
[Files & Formats](06-files-and-formats.md#pchp-module-presets).

## Morphs

The four morph groups from the header bar work like the hardware's: assign
parameters to a group (right-click → morph), set each parameter's morph range,
and sweep the group knob to move them all. Assigned controls of every kind
(knobs, 4-1 selectors, toggles, increment buttons and sliders) show their group
colour on the canvas, and the Inspector lists all of a module's assignments.
`F7` labels group membership across the patch and `F5` shows each morphed
parameter's swept range.

## Randomize, initialize, locks

- `Ctrl+R` randomizes parameters (uniform); `Ctrl+Shift+R` uses a gaussian
  spread around current values.
- Locked parameters and excluded modules are never touched.
- Initialize resets a patch to a clean state.

For evolutionary sound design with breeding and interpolation, see the
[Patch Mutator](05-tools-and-floaters.md#patch-mutator-ctrl8).

## Snapshots and variations

The 8 buttons in the header bar hold **patch variations**: full parameter
snapshots you can audition and switch between. They persist in a `.var` sidecar
file next to the patch (the `.pch` itself stays 100% standard). Live edits
write through to the active variation.
