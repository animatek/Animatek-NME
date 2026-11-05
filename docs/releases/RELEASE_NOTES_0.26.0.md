# Animatek NME 0.26.0: many synths at once

This version is about working with many synths, real or emulated, from one NME: a menu on each synth's
name to free its port or swap another synth onto it, ports that are no longer held for nothing, and
emulator names that stick. Use it with **G1-Emu alpha.30** or later
(https://github.com/animatek/G1-Emu/releases/tag/v0.1.0-alpha.30), which brings the shared memory,
G1-Emu Micro as a plugin of its own, and the fixes for dropouts in Cubase and Nuendo.

## A menu on each synth's name

Right-click a synth's name in the slot panel:

- **Rename** gives it a name of your own.
- **Release port** disconnects it and leaves its port free. A G1-Emu released this way is set aside: NME does
  not take it back by itself, and it counts as waiting.
- **Release all ports** does the same for every synth at once.
- **Switch to** lists the G1-Emu instances no editor has and the MIDI inputs no port uses. The one you choose
  comes onto the port. With more G1-Emu than NME's eight ports, this is how you choose which ones it edits.

The panel's header now shows just the count ("1/8"); the emulators left waiting are in its tooltip and in
Switch to.

## Ports and names

- **A port is kept only for a real synth.** NME kept a port for any MIDI interface you had saved, even with
  nothing behind it, so one G1-Emu always waited with "ports full". Now a port is kept only if a real G1 has
  answered on it before. A saved port with no known synth is tried for 5 seconds at start and then left free.
- **Emulator names stick.** Opening a project with many G1-Emu left some of them as "Modular" now and then.
  NME now checks the name took and sends it again if it did not.

## The Micro Modular

- **A Micro shows only bank 1**, in the synth browser and in Store to Bank, Copy and Move. The Micro has one bank
  of 99 places, and its OS refuses to store anywhere else.

## Known

- The bank transfer dialog still lists nine banks on a Micro.
- Tested in Bitwig on Linux with G1-Emu, many instances and the Micro. The full checklist was not run on a real
  G1 for this version.
- With CLAP, when the last G1-Emu of a project closes, new instances have no MIDI port of their own (a JUCE
  limit); NME still finds them through the direct link. VST3 is not affected.
- The plugin's editor does not restore its state when a DAW project reopens (#94).
- Still beta: the first-start warning stays.

The binaries are on Patreon, as before. Thanks to everyone who sent logs and reports from their DAWs.
