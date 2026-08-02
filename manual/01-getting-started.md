# 1. Getting Started

## What you need

- A **Clavia Nord Modular G1** (keyboard or rack) or Micro Modular.
- A MIDI interface connected to the synth's MIDI IN **and** MIDI OUT. The
  editor talks SysEx in both directions, so a one-way connection is not enough.
- The Animatek NME binaries for your platform, distributed through
  [Patreon](https://www.patreon.com/c/animatek).

## Installing

**Linux.** Two options:

- *AppImage*: make it executable and run it with
  `chmod +x AnimatekNME-x.y.z-x86_64.AppImage && ./AnimatekNME-x.y.z-x86_64.AppImage`.
  The AppImage bundles everything, including the patched MIDI backend (see
  [Troubleshooting](08-troubleshooting.md) if your distro's stock builds show no
  MIDI devices).
- *Plain binary*: unzip and run `AnimatekNME`. No installation required; settings
  are stored in your user profile.

**Windows.** Unzip and run `AnimatekNME.exe`. No installer is required.

**macOS.** Unzip, move `AnimatekNME.app` to Applications and run it. The build is
universal (Apple Silicon + Intel). On first launch you may need to allow the app
in System Settings → Privacy & Security.

## Connecting the synth

1. Connect the synth's MIDI IN/OUT to your interface and power it on.
2. Launch Animatek NME. The editor scans MIDI ports and performs the Nord
   Modular handshake automatically; when it finds the synth, the status bar
   shows the connection and the editor fetches the active slot's patch.
3. If you have several MIDI interfaces, pick the right ports in the editor's
   options.

Once connected, everything is live: turning a knob in the editor changes the
sound immediately, and turning a knob on the synth's front panel updates the
editor.

## Your first patch

- Press `Ctrl+1`–`Ctrl+4` to pick a slot (A–D).
- Press `Enter` or double-click the canvas to open **Quick Add** and type a
  module name. Try `keyboard`, then `oscA`, then `2 outputs`.
- Drag cables between the colored connectors: Keyboard *Note* → OscA *Pitch*,
  OscA *Out* → 2 Outputs *L*.
- Play a note (your MIDI keyboard, or the virtual keyboard on `Ctrl+6`).
- Save with `Ctrl+S`. Patches are standard `.pch` files, compatible with the
  original editors.

## Where things live on disk

The editor keeps a **preset library** folder (configurable in the preset
browser) with `Patches/`, `Snippets/`, `Presets/` and `Banks/` subfolders. Bank
backups, snippets, module presets and your saved patches all land there, and the
patches show up in the built-in browser (`Ctrl+B`).
