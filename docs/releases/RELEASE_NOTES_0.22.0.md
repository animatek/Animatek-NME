# Animatek NME 0.22.0: shared themes, and old patches that sound

Themes become something you can share: a public repository the editor reads by itself, your own
themes as single files, and themes that bring their own render style. Old 2.10 patches are now
converted the way Clavia's own editor converts them, and most factory patches that loaded silent
play again. And from this version on, NME tells you when a new one is out.

## 🎨 Community themes

- **View > Theme > Community Themes** (#91) lists the themes shared in
  [animatek/nme-themes](https://github.com/animatek/nme-themes): name, author, a **New** badge on
  the ones you have not seen and **Update** on the ones changed since you installed them. One
  click installs a theme and switches to it.
- NME looks at the list once a day at most when it starts. When there is something new, a **red
  dot** appears on View and on Theme, as VCV Rack does for its library. Editor Options can turn
  the daily look off; **Check Now** looks at once.
- **NME's own themes live there too**, so a built-in theme can be touched up and reach everyone
  without waiting for a release.
- Anyone can add a theme by pull request; the repository's README says how.

## 🖌️ Your own themes

- **Import Theme File...** and **Export Current Theme...** in View > Theme (#90): a theme is a
  single file you can pass around.
- **A theme can set the render style** (wireframe, hardware look, knob style, flat knobs). Choosing
  it applies that style, and choosing a theme that sets none gives you back your own. **Animatek
  Rack** now turns flat knobs on. The theme editor has a box to save the current style with the
  theme.
- The theme editor gets a swatch for the **module panel colour**, and an **Auto** button on the
  colours that can be left to the theme.

## 🎛️ Old 2.10 patches

- **2.10 patches are converted with the rules of Clavia's editor 3.03** (G1-Emu #65): selectors
  that meant "off", switches that were the inverse of 3.0's mutes, sequencer steps that came out
  empty, and the morphs, knob and MIDI CC assignments that were dropped. In the emulated G1, of 40
  factory 2.10 patches 16 were silent; now all sound but five that are silent on purpose.

## 🔔 New version notice

- **NME tells you when a new version is out** (#97): a red dot on **About** and an item that
  opens Patreon, where the builds are. It checks once a day at most, never downloads anything,
  and Editor Options can turn it off.

## 🪟 Windows installer

- Besides the ZIP, Windows gets **`AnimatekNME-0.22.0-windows-setup.exe`**: it installs for your
  user only (no administrator, no UAC prompt), adds a Start menu entry, replaces an earlier
  version in place, and uninstalls from Settings → Apps. Your settings and patches are never
  touched.

## 🐞 Fixed

- The theme editor's colour picker showed its values black on black.
- Every MIDI port looked taken after a few emulators came and went.
- A new G1-Emu kept the name "Modular" until it was clicked.
- OK in Editor Options could put back what the theme and version checks had just written.

The complete list is in `CHANGELOG.md`.

## Known limitations

- The macOS menu bar fix of 0.21 (#95) has not been confirmed on a Mac yet.
- On macOS the native menu bar shows the "new" mark as a text bullet, not a red dot.
- The plugin does not yet restore its state when a DAW project is reopened (#94).
