# 2. The Interface

## The main window

From top to bottom:

- **Menu bar**: File, Edit, View, Device and Help menus. Device holds the
  synth-facing tools (bank transfers, controller snapshot); View holds the
  floating windows.
- **Header bar**: the current patch name (click to rename), the **Voices**
  counter with up/down arrows, the **Load** meters, the four **morph group
  knobs**, and the **snapshot/variation buttons** (8 per slot).
- **Inspector** (left column): everything about the selected module that isn't
  on its face: its name (editable, and undoable), its section, every one of its
  **parameters** as a name and a value you can drag or type into, its
  **assignments** (morph groups, hardware knobs and MIDI CCs), its **DSP cost**,
  and its **presets**. Each of those sections folds away from the chevron in its
  own title, and stays folded next time you run the editor. Beside the
  Assignments heading sits a **hardware knob
  map**: a four-panel, 18-LED diagram mirroring the physical knob layout, with
  assigned knobs lit bright green and free knobs in the hardware's dark
  unlit-lens colour. With nothing selected it lists the assignments of the whole
  patch, including the knobs assigned to the four morph groups, which belong to
  the patch rather than to any module. `Ctrl+I` folds the whole column away.
- **Slot bar** (below the Inspector): the four hardware slots A–D with
  hardware-style LEDs: a *blinking* LED is the selected slot, *steady* LEDs are
  enabled slots. Plain click selects a slot; `Ctrl+click` enables/disables a
  slot without selecting it, exactly like `Shift+slot button` on the front
  panel; **right-click shows or hides that slot's sub-window** (see
  [Working with the Synth](04-working-with-the-synth.md#the-four-slots-on-screen)).
  A slot showing a **LOCAL** badge holds a patch the synth doesn't have.
- **Module bar** (under the header, full width): the module palette as the
  original editor presents it. The tabs pick a category (In/Out, Osc, LFO, Env,
  Filter, Mixer, Audio, Ctrl, Logic, Seq) and the modules of that category appear
  underneath, each in a thin outlined chip carrying its name. **Drag a chip**
  onto a patch area, or **click it** and the module follows the pointer until you
  click where you want it, which also chooses the area and the slot. The bar
  remembers the tab you left it on, and **View > Module Icon Bar** turns it off
  if you would rather have the pixels.
- **Work area**: the four slots live here as **sub-windows**, tiled inside the
  main window. Each holds a **patch canvas**, split into the **Poly area**
  (one instance per voice) and the **Common area** (one instance per patch,
  e.g. keyboard, sequencers, effects, outputs). A draggable divider separates
  them. Middle-drag pans, `Ctrl++`/`Ctrl+-` zooms, `Z` zooms to selection.
- **Preset browser** (right side, `Ctrl+B`) holds two worlds in one panel: the
  synth's internal memory (9 banks) and your disk preset library, with search
  and patch/snippet/bank filters. `Ctrl+Shift+I` folds it away.
- **Status bar**: connection state, synth information and activity.

The Inspector, header bar, browsers and status bar are shared: they follow
whichever sub-window has focus, and the focused one is edged in the theme's text
colour so you can always see which slot you are editing.

## Voices and DSP load

The header's **Voices** field sets the patch's polyphony; the up/down arrows
change it and the patch is re-uploaded so the synth follows (the G1 stores the
voice count inside the patch header, so that re-upload *is* the change). Holding
the arrows down is safe; rapid presses are coalesced into a single upload for
the final value. The same setting also lives in Patch Settings (`Ctrl+P`).

Next to it, two **Load** bars show the DSP cost of the patch to one decimal
place, as the original editor did: `PVA:` for the poly/voice area and `E:` for
the common (effects) area. The figure is the editor's own estimate from each
module's cycle cost; the synth does not report its load.

Each module's individual share is on screen wherever you pick or inspect a
module: in the right-click **Add Module** menu ("Audio In (2.2%)"), in the module
browser, on every Quick Add row, and in the Inspector. **Double-click a module**
for its own cost, as the original editor does, or press `F3` to label every
module at once, which is the view you want when a patch is over budget and you
are looking for what to cut. The figures are rounded to the two significant
figures the original Clavia editor prints, so a patch optimised against the
hardware editor reads the same numbers here.

## Modules on the canvas

Each module is drawn pixel-faithful to the original editor: knobs, buttons,
selectors, displays, connectors and lights. Live data from the synth animates
the **VU meters and LEDs** in real time while connected.

- Click and drag a knob to change it (the synth follows instantly).
- Right-click a knob for parameter options (morph assignment, MIDI CC,
  hardware knob, locks).
- Drag a module by its title/body to move it on the grid; multi-select with a
  rubber band or `Shift`-click.
- **Rest the cursor on any control** to read its value in the parameter's own
  units, so a cutoff reads "440 Hz" rather than "64". Drag it and the value
  follows live.
- `F1` shows help for the hovered module, straight from the original Nord
  Modular documentation.

## Cable colors

Cables take the color of the signal they carry:

| Color | Signal |
|-------|--------|
| Red | Audio |
| Blue | Control |
| Yellow | Logic / gate |
| Gray | Master/slave (osc sync groups) |
| Green / Purple | User-recolored cables |
| White | Unknown |

The View menu and header tools let you hide cable colors selectively, and `S`
"shakes" the cables so overlapping runs redistribute visually. Cable style
(curved/straight, thick/thin) and opacity live in **File → Editor Options**;
the canvas follows the opacity slider while you drag it, and Cancel puts the
old value back.

## Themes

`Ctrl+T` cycles the 13 color themes, or pick one from **View → Theme** (the
checkmark always follows the theme actually in use). **Nord** is the default;
**Nord Classic** is a light, warm-grey theme that echoes the original Clavia
Nord Modular editor: flat grey module bodies, black labels, indigo LCD-style
value readouts and the classic cable colours, all sampled from the original.
The theme that used Nomad's own colours is called **Nomad**.

Themes apply to the whole application, not just the canvas: menu bar, header,
status bar, slot list, Inspector and all dialogs follow the palette, so text
stays legible on light and dark themes alike. A very light procedural grain over
the canvas gives it a paper feel instead of a flat fill, most visible on Nord
Classic.

`Ctrl+W` (macOS: `Cmd+Shift+W`) toggles a wireframe module style that works with every theme. Both
settings persist across sessions, as do the window size/position and floater
layouts.
