# Animatek NME 0.12.0: the patch tells you what it costs

A release about seeing what a patch is made of. Every module's DSP cost is on screen where
you choose modules, every parameter's value is a hover away, and presets stop being a
DrumSynth curiosity and become a library you can share. Plus four reported bugs fixed, two
of them in the sequencers.

## What's new

### 📊 What everything costs, everywhere you choose it

The Nord Modular's DSP budget is the constraint that shapes every patch, and until now the
editor gave you one number for the whole thing. Each module's share now appears wherever a
module is picked or inspected: in the right-click **Add Module** menu ("Audio In (2.2%)"),
in the module browser, on every Quick Add row, and in the Inspector for the selected module.

The figures are rounded to the two significant figures the original Clavia editor prints,
so a patch optimised against the hardware editor reads the same numbers here. Every value
quoted in the bug report reproduces to the digit.

**Double-click a module** for its own cost, as the original editor does, or press **F10** to
label every module at once, which is the view you want when a patch is over budget and you
are looking for what to cut.

Fixed alongside it: the Load meter kept showing the previous patch's cost after any edit
that did not come from the canvas. Adding modules over the MCP bridge read 0.0%.

### 🔍 Values on hover, and the function-key readouts

Rest the cursor on a knob, slider, button or display box and its value appears, in the
parameter's own units, so a cutoff reads "440 Hz" rather than "64". Drag a control and the
value follows live with no delay, which is the moment you actually want the number.

**F5** reads out the whole patch at once. It existed before but only drew parameters
assigned to a morph group, and showed the raw morph range instead of a value, so what should
have been a patch readout was in practice a morph inspector. A morphed parameter now reads
out the span the morph sweeps it across, matching the original's "46Hz-2.30kHz".

**F7**, **F8** and **F9** complete the set the original editor documents: morph groups, knob
assignments and MIDI CC assignments, each labelled over the parameter it belongs to. All of
them toggle, so you can leave a readout open while you work rather than holding a key down.

### 🎛️ Module presets became a library

Presets used to be a DrumSynth feature hidden behind a right-click on a preset display box
57 pixels wide, and even there you could delete a preset but not recall one.

Selecting any module now puts a **Presets** section in the Inspector, under its assignments:
click a name to recall, the x to delete, right-click to rename, and **Save current settings**
to capture the module as it stands. The section folds away from a chevron in its title.
Recalling a preset is a single undo step rather than one per parameter. The same list is on
the module's own right-click menu.

Underneath, a preset is now any module's named parameter snapshot rather than a DrumSynth
structure, so the sequencers or anything else can have presets without new code. They live
in a **Presets** folder in your patch library, next to Patches, Snippets and Banks, as one
`.pchp` pack per module type. The format is plain text and meant to be edited by hand, since
transcribing the original editor's own presets is done by hand. Values are keyed by parameter
rather than by position, so a preset that names two parameters sets those two and leaves the
rest of the module alone. Presets saved by earlier versions are migrated on first run.

### 🍎 macOS runs on High Sierra again

The macOS package claimed Catalina as its minimum, which left older Intel Macs out for no
technical reason. Release builds now target macOS 10.13, and packaging verifies both the
bundle's minimum version and the Intel binary's own so the metadata cannot drift back.

## Fixes

- **The sequencers' arrow buttons step both ways again (issue #34).** The four sequencer
  modules draw their arrows as a left/right pair, but the click was split top against bottom,
  so the direction depended on which half of an arrow you hit and both arrows appeared to do
  the same thing. Only the sequencers were affected; the editor's other 50 arrow pairs are
  stacked and were always correct.
- **Clr clears the steps, not the sequencer (issue #34).** It reset every parameter to its
  minimum, taking the step count down to 1 along with the loop and transport settings. It now
  empties only the per-step values, the same set Rnd randomises.
- **The Filter Bank's jacks and bypass are back inside the module (issue #35).** Its artwork
  was drawn taller than the module is, so the bottom row of controls fell onto the canvas
  behind it. The row was rearranged rather than the module made taller, which would have
  overlapped whatever sat below it in existing patches. A sweep of all 110 modules found this
  was the only one affected.
- **`Ctrl+I` works in the main window (issue #38).** It only ever worked in the pop-out slot
  windows. It now collapses the left inspector column, `Ctrl+Shift+I` collapses the right
  patch browser, and each panel remembers the width it was dragged to.
- **Replacing a patch in a slot no longer crashes the editor.** Creating an empty patch or
  loading a file into a slot holding a patch with knob or MIDI CC assignments killed the app.
- **Module DSP costs rebuilt from Clavia's own figures.** The inherited table had 47 of 109
  values outside the rounding interval of what the original editor prints, nearly all too
  high, which is why a full patch read just over 100%.

## Known limitations

- Stuck MIDI-IN notes are cleared with the synth's front-panel panic.
- macOS builds are unsigned; Gatekeeper needs the usual right-click → Open on first launch.
- F12 (current MIDI controller values) from the original editor is not implemented: nothing
  tracks incoming controller values yet.
