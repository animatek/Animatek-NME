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
| `Ctrl+E` | Editor options |
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
| `F5` | Toggle morph values overlay |
| `F7` | Toggle morph groups overlay |
| `Z` | Zoom to selection (or reset when nothing selected) |
| `Shift+Z` | Reset zoom to 100% |
| `Ctrl++` / `Ctrl+-` | Zoom in / out |
| `Ctrl+T` | Cycle color theme |
| `S` | Shake cables |
| Middle-drag | Pan the canvas |

## Slots

| Shortcut | Action |
|----------|--------|
| `Ctrl+1`..`Ctrl+4` | Switch to slot A..D |

## Floaters

| Shortcut | Action |
|----------|--------|
| `Ctrl+5` | Knob Floater |
| `Ctrl+6` | Keyboard Floater |
| `Ctrl+7` | Patch Notes |
| `Ctrl+8` | Patch Mutator |
| `Ctrl+9` | SysEx Monitor |

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
