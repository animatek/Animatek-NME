# Keyboard Shortcuts

Also available in-app: **Help → Keyboard Shortcuts...**
(Keep this file in sync with `showKeyboardShortcutsDialog()` in `source/MainComponent.cpp`.)

On macOS, `Ctrl` is `Cmd`.

## File

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New patch |
| `Ctrl+O` | Open patch |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save as |
| `Ctrl+B` | Preset browser |
| `Ctrl+P` | Patch settings |
| `Ctrl+G` | Synth settings |
| `Ctrl+,` | Editor options |
| `Ctrl+Q` | Quit |

## Edit

| Shortcut | Action |
|----------|--------|
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z`, `Ctrl+Y` | Redo |
| `Ctrl+A` | Select all modules in the section |
| `Ctrl+X` / `Ctrl+C` / `Ctrl+V` | Cut / copy / paste modules |
| `Ctrl+D` | Duplicate selection with cables |
| `Delete`, `Backspace` | Delete selection |
| `Escape` | Clear selection |
| Arrow keys | Nudge selected modules one grid cell |
| `Ctrl+R` | Randomize parameters |
| `Ctrl+Shift+R` | Randomize parameters (gaussian) |

## Canvas

| Shortcut | Action |
|----------|--------|
| `Enter`, double-click | Quick Add module at mouse position |
| `F1` | Module help for the hovered/selected module |
| `F5` | Toggle parameter values overlay for the whole patch (morphed ones show their range) |
| hover | Rest the cursor on a control to read its value |
| `F7` | Toggle morph groups overlay |
| `F8` | Toggle knob assignments overlay |
| `F9` | Toggle MIDI CC assignments overlay |
| `F10` | Toggle module DSP cost overlay |
| double-click | Double-click a module to read its DSP cost |
| `Z` | Zoom to selection (or reset when nothing selected) |
| `Shift+Z` | Reset zoom to 100% |
| `Ctrl++` / `Ctrl+-` | Zoom in / out |
| `Ctrl+T` | Cycle color theme |
| `Ctrl+W` | Toggle wireframe modules |
| `Ctrl+I` | Toggle the inspector panel (left side) |
| `Ctrl+Shift+I` | Toggle the patch browser (right side) |
| `S` | Shake cables |
| Middle-drag | Pan the canvas |

## Slots

| Shortcut | Action |
|----------|--------|
| `Ctrl+1`..`Ctrl+4` | Switch to slot A..D (opens its sub-window if closed) |
| `Ctrl+Shift+1`..`Ctrl+Shift+4` | Show/hide slot A..D's sub-window |
| `F11` | Focus mode: blow the focused slot up to the full area, and back |
| Maximise button | The same, on that sub-window's own title bar |
| `Ctrl+Shift+` arrows | Move the focused slot to the neighbouring tile, swapping with whatever is there. Up and down only exist in the four-slot 2x2; nothing happens at an edge |
| Right-click a slot row | Show/hide that slot's sub-window |
| Right-click a patch in the Synth browser | **Load to Slot A..D**: fetch it into a named slot |
| `Ctrl+click` a slot row | Enable/disable the slot without selecting it |

Open slots tile themselves, the way a tiling window manager does: one fills the
work area, two split it down the middle, three go in thirds, four go 2x2. The
layout re-flows whenever you open or close one. Dragging or resizing a
sub-window leaves the windows where you put them from then on; **View > Slots >
Tile Slots** puts them back.

## Floaters

| Shortcut | Action |
|----------|--------|
| `Ctrl+5` | Knob Floater |
| `Ctrl+6` | Keyboard Floater |
| `Ctrl+7` | Patch Notes |
| `Ctrl+8` | Patch Mutator |
| `Ctrl+9` | SysEx Monitor |

## Sub-window (the one with focus)

All of these act on that window's own slot and selection.

| Shortcut | Action |
|----------|--------|
| `Ctrl+R` / `Ctrl+Shift+R` | Randomize parameters (uniform / gaussian) |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save as |

## Patch Mutator (window focused)

| Shortcut | Action |
|----------|--------|
| `1`-`8` | Focus Mother / Children / Father |
| `O` / `T` | Copy focused sound to Mother / Father |
| `E` / `U` | Mutate from focused / from Mother |
| `N` | Randomize |
| `I` / `X` | Interpolate / Cross (Mother + Father) |
| `S` | Save focused sound to Temporary Storage |
| Shift+drag | Interpolate two sounds |
| Ctrl+drag | Cross two sounds |
