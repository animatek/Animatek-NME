# 8. Troubleshooting

## The editor doesn't find the synth

- Check that **both** MIDI directions are cabled: synth OUT → interface IN and
  interface OUT → synth IN. The handshake needs a reply.
- Make sure no other application (a DAW, the original editor) is holding the
  MIDI ports.
- Verify the port selection in the editor's options if you have several
  interfaces.
- The Nord Modular protocol has a 3-second reply timeout; if the connection
  drops mid-session, the status bar will show it and the editor keeps retrying
  the handshake.

## Linux: no MIDI devices listed

Modern Linux kernels expose MIDI through the new UMP layer, which stock JUCE
builds handle incorrectly (no devices found, or UMP packets sent to legacy
interfaces). Animatek NME ships with a patched MIDI backend that handles both;
use the official AppImage or binaries. If you build from source, the required
JUCE patches are in the bundled `JUCE/` submodule (see the README's *Linux MIDI
Note*).

## A patch loads with missing modules or wrong cables

Make sure you're on the current version; 0.8.x fixed several patch-decoding
bugs (legacy 2.10 cables, chained cables fetched from the synth, custom module
data arriving out of order). If a specific file still misbehaves, open a GitHub
issue and attach the `.pch`.

## A slot shows a LOCAL badge

That slot's patch exists only in the editor; it was opened with the **Local**
option, or loaded/built while disconnected, so the synth doesn't have it. Upload
it (open it into slot A–D, or store it to a bank) and the badge clears.

## My AI assistant can't reach the editor

The MCP bridge is **off by default**. Enable it in Editor Options (`Ctrl+,`) →
MCP Bridge and check the status line says it is listening; the editor must be
running for the tools to work. See [The MCP Bridge](09-mcp-bridge.md).

## Stuck notes

If notes hang (usually from external MIDI going directly to the synth), use the
synth's front-panel panic. The editor's virtual keyboard always pairs note-offs
with its note-ons.

## My variations disappeared

Variations live in the `.var` sidecar next to the `.pch`. If you moved or
renamed the patch file, move/rename its `.var` along with it.

## A keyboard shortcut does nothing

If one shortcut is dead while its neighbours work, something outside the editor
is taking that key combination first. A program that registers a shortcut
system-wide gets it before any application does, and the editor never hears the
key at all.

The giveaway is that the failure is specific: `Ctrl+Shift+I` and `Ctrl+Shift+R`
do nothing while `Ctrl+Shift+Z` and `Ctrl+Shift+1`-`4` are fine, and the plain
`Ctrl+I` and `Ctrl+R` are fine too. A keyboard fault or a fault in the editor
would not pick and choose like that.

On Windows the usual culprit is **AMD Software: Adrenalin Edition**, which
claims several `Ctrl+Shift` combinations by default. Turn the ones you want back
off in its own settings and the editor gets them again. Windows itself also
takes `Ctrl+Shift` for switching keyboard layout when more than one input
language is installed, under **Settings > Time & language > Typing > Advanced
keyboard settings > Input language hot keys**. Screen recorders, overlays and
graphics-driver panels are worth checking for the same reason.

Everything on a shortcut is also on a menu, so nothing is out of reach while you
track it down.

## Watching what actually happens

Open the **SysEx Monitor** (`Ctrl+9`) to see the raw MIDI conversation. When
reporting a bug, a monitor capture plus the patch file makes it fixable fast.

## Reporting bugs

The app is beta software. Bugs, with reproduction steps and files, go to:
https://github.com/animatek/Animatek-NME/issues
