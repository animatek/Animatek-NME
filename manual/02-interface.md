# 2. The Interface

## The main window

From top to bottom:

- **Menu bar** — File, Edit, View, Device and Help menus. Device holds the
  synth-facing tools (bank transfers, controller snapshot); View holds the
  floating windows.
- **Header bar** — the current patch name (click to rename), the four **morph
  group knobs**, and the **snapshot/variation buttons** (8 per slot).
- **Slot bar** — the four hardware slots A–D with hardware-style LEDs:
  a *blinking* LED is the selected slot, *steady* LEDs are enabled slots.
  Plain click selects a slot; `Ctrl+click` enables/disables a slot without
  selecting it — exactly like `Shift+slot button` on the front panel.
- **Patch canvas** — the heart of the editor, split into the **Poly area**
  (one instance per voice) and the **Common area** (one instance per patch,
  e.g. keyboard, sequencers, effects, outputs). A draggable divider separates
  them. Middle-drag pans, `Ctrl++`/`Ctrl+-` zooms, `Z` zooms to selection.
- **Preset browser** (right side, `Ctrl+B`) — two worlds in one panel: the
  synth's internal memory (9 banks) and your disk preset library, with search
  and patch/snippet/bank filters.
- **Status bar** — connection state, synth information and activity.

## Modules on the canvas

Each module is drawn pixel-faithful to the original editor: knobs, buttons,
selectors, displays, connectors and lights. Live data from the synth animates
the **VU meters and LEDs** in real time while connected.

- Click and drag a knob to change it (the synth follows instantly).
- Right-click a knob for parameter options (morph assignment, MIDI CC,
  hardware knob, locks).
- Drag a module by its title/body to move it on the grid; multi-select with a
  rubber band or `Shift`-click.
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

The View menu and header tools let you hide cable colors selectively, change
cable style (curved/straight, thick/thin) and opacity, and `S` "shakes" the
cables so overlapping runs redistribute visually.

## Themes

`Ctrl+T` cycles 13 color themes (Nord red is the default); `Ctrl+W` toggles a
wireframe module style that works with every theme. Both persist across
sessions, as do the window size/position and floater layouts.
