# 3. Editing Patches

## Adding modules

- **Quick Add**: press `Enter` or double-click empty canvas, type a few
  letters and pick from ranked results. It searches names, categories and a
  hand-written tag table (try `reverb`, `random`, `snare`…).
- **Module browser**: browse the full palette by category and drag modules
  onto the canvas, into whichever sub-window you drop them on.
- **Add Module**: right-click empty canvas for the full menu by category.

The Poly and Common areas accept different module sets, matching the hardware.
Modules use DSP resources on the synth, and every one of these three routes
prints the module's cost next to its name ("Audio In (2.2%)") so you can choose
with the budget in view. The header's Load meters track the patch total; see
[Voices and DSP load](02-interface.md#voices-and-dsp-load).

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
(`Ctrl+Z` takes it back). The name lives in the patch and reaches the synth with
the next full upload.

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
- **Hover** any control to read its value in the parameter's own units; drag it
  and the readout follows live.
- Right-click a parameter to assign it to a **morph group**, a **hardware
  knob**, or a **MIDI controller**, and to **lock** it against randomization.

## Reading a patch: the overlay keys

Five function keys label the whole patch at once. They toggle, so you can leave
a readout open while you work rather than holding a key down.

| Key | What it labels |
|-----|----------------|
| `F5` | Every parameter's value. A morphed parameter shows the span its morph sweeps it across, e.g. "46Hz-2.30kHz" |
| `F7` | Morph group membership |
| `F8` | Hardware knob assignments |
| `F9` | MIDI CC assignments |
| `F10` | Each module's DSP cost |

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
