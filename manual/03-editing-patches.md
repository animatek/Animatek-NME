# 3. Editing Patches

## Adding modules

- **Quick Add**: press `Enter` or double-click empty canvas, type a few
  letters and pick from ranked results. It searches names, categories and a
  hand-written tag table (try `reverb`, `random`, `snare`…).
- **Module browser**: browse the full palette by category and drag modules
  onto the canvas.

The Poly and Common areas accept different module sets, matching the hardware.
Modules use DSP resources on the synth; the status display tracks the load.

## Selection and arrangement

- Click selects; `Shift`-click and rubber-band extend the selection; `Ctrl+A`
  selects the whole section; `Escape` clears.
- Drag to move (the grid keeps everything tidy); arrow keys nudge one cell.
- `Ctrl+X/C/V` cut/copy/paste, `Ctrl+D` duplicates **with cables**.
- `Delete` removes the selection, cables included. Everything is undoable;
  each slot has its own undo history (`Ctrl+Z` / `Ctrl+Shift+Z`).

## Renaming modules

Give a module your own name from its right-click menu, or from the **Name**
field at the top of the Inspector. Renaming is a normal, undoable edit
(`Ctrl+Z` takes it back) and works the same way in a slot pop-out window. The
name lives in the patch and reaches the synth with the next full upload.

## Cables

- **Create**: drag from any connector to a compatible one. Valid targets light
  up while you drag; outputs connect to inputs.
- **Chained cables**: you can also drag from one *input* to another *input*,
  daisy-chaining a net exactly like the original editor, e.g. Keyboard Note →
  OscA1 Pitch, then OscA1 Pitch → OscA2 Pitch. The hardware rule is enforced:
  a net can only be driven by **one** output, and illegal targets won't light
  up.
- **Delete**: right-click a connector to remove its cables.
- Cable visibility filters, styles and the `S` shake help untangle big patches.

## Parameters

- Knobs, sliders, buttons and selectors edit live and sync to the synth.
- Right-click a parameter to assign it to a **morph group**, a **hardware
  knob**, or a **MIDI controller**, and to **lock** it against randomization.
- The **DrumSynth** module has a local preset spinner (bottom-right corner):
  right-click it to save or manage your own drum presets.

## Morphs

The four morph groups from the header bar work like the hardware's: assign
parameters to a group (right-click → morph), set each parameter's morph range,
and sweep the group knob to move them all. Assigned controls of every kind
(knobs, 4-1 selectors, toggles, increment buttons and sliders) show their group
colour on the canvas, and the Inspector lists all of a module's assignments.
Overlays visualize them too: `F5` shows morph values, `F7` shows group
membership.

## Randomize, initialize, locks

- `Ctrl+R` randomizes parameters (uniform); `Ctrl+Shift+R` uses a gaussian
  spread around current values.
- Locked parameters and excluded modules are never touched.
- Initialize resets a patch to a clean state.

For evolutionary sound design with breeding and interpolation, see the
[Patch Mutator](05-tools-and-floaters.md#patch-mutator).

## Snapshots and variations

The 8 buttons in the header bar hold **patch variations**: full parameter
snapshots you can audition and switch between. They persist in a `.var` sidecar
file next to the patch (the `.pch` itself stays 100% standard). Live edits
write through to the active variation.
