# Animatek NME 0.20.0: your editor, your look

A cycle of looks and of small things that were wrong. The editor can now be drawn as hardware,
its colours are yours to edit and share, the slot panel is a row like the original editor's, and
a handful of bugs people reported are gone.

## 🎨 Look

- **Hardware look** (#89): an optional skeuomorphic render style (knobs, jacks, panels, displays,
  sliders, LEDs, meters, cables), contributed by Grant (teezdalien). It is a render style, not a
  theme: it works over every theme, is off by default, and `Ctrl+H` toggles it.
- **Flat knobs** and a new **Animatek Rack** theme, the look of the Animatek modules for VCV Rack.
- **Theme editor** (#90): View > Theme > Edit Theme Colours, with a swatch per colour and the canvas
  following live. Themes are files now: save one by name to `~/.AnimatekNME/themes`, reload it,
  share it.

## 🎛️ The slot panel and the connection

- The slot panel is **a synth row, as in the original editor**: the A B C D buttons, the synth's name
  and its DSP load as a bar. `MIDI`, `Library` and `Store` left it: MIDI Settings opens with
  **`Ctrl+M`**, the library folder is in Editor Options and Store to Bank in the Device menu.
- The status bar says **where you are connected**: Real G1 (port), G1-Emu (port) or Not connected.
- The synth's own display follows the knob you grab, as the original editor does.

## 🐞 Fixed

- Module buttons needed two clicks (#82).
- The module bar listed modules in a different order from the original editor (#81).
- Oscillator note names jumped an octave at F# (#80).
- Patches wrapped in a MacBinary header open (#87).
- Patches lost a module and a cable, or put everything in the common area, when read from a `.pch`.
- Sine Bank: the ratio arrows reset an oscillator's level.
- The synth's error codes 3, 5 and 6 carry the original editor's texts.

The complete list is in `CHANGELOG.md`.

## Known limitations

- Hardware look and the theme editor have not been through a full hardware session.
- The load bar of the slot panel adds up the module cycles of the four slots; it is an editor-side
  estimate, not a reading from the synth.
- Still one synth at a time: several synths at once is the next big piece (#84).
