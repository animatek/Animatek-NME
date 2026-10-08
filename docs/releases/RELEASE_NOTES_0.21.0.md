# Animatek NME 0.21.0: more than one synth

The release the last one promised. The editor connects several synths at once, real G1s and G1-Emu
together, each with its own four slots, the way the original editor does it. G1-Emu no longer needs
a virtual MIDI port: the editor finds it and connects it on its own.

## 🎛️ Several synths at once

- **Up to eight synths** (#84, #88): MIDI Setup has one group per port (In, Out, Enabled and the
  synth's status), and each enabled port is a synth with its own slots A B C D. The original editor
  has four ports; NME has eight, so you can run as many G1-Emu instances as your computer takes.
- **The slot panel shows one row per synth**, only for the synths that are there, under a "SYNTHS"
  header that counts them. Click a synth's name to edit it.
- Every patch window says whose slot it is: **"Slot A - Bella - animatek"**.
- **Rename a synth** with a double-click on its name in the slot panel. The name is stored in the
  synth itself, as Synth Settings does, so each G1-Emu instance keeps its own.
- Only the windows of the synths that are connected open at startup. Editor Options can bring back
  every window of the last session instead.

## 🔌 G1-Emu without MIDI ports

- **The editor finds G1-Emu and connects it by itself** (#83) over a direct local link, with no
  virtual MIDI port, driver or loopMIDI. It is what makes the G1-Emu plugin usable from the editor
  on Windows. Device > Connect to G1-Emu Automatically turns it off.
- A new G1-Emu instance that still has the factory name ("Modular"), or the name of another synth,
  gets a name of its own ("G1 Basalt", "G1 Nebula"…).
- Ports follow the emulators: when an instance closes, its row and its windows go with it.

## 🎨 Look

- **The hardware look follows the theme editor** (PR #93, by Grant): the knurled knobs take the
  theme's knob colour, jacks and cords follow the cable colours, and the step LEDs have their own
  colour.

## 🐞 Fixed

- Synth Settings: **Global Sync** was a checkbox, and pressing OK turned a G1 set to 4 quarter notes
  into 2. It is now a selector from 1 to 32 quarter notes (#92, and the missing value in #79).
- Plugin on macOS: after opening the editor, its menus replaced the DAW's own menu bar (#95).

The complete list is in `CHANGELOG.md`.

## Known limitations

- The macOS menu bar fix (#95) has not been tried on a Mac yet.
- The plugin does not yet restore its state when a DAW project is reopened (#94).
- The load bar of the slot panel adds up the module cycles of the four slots; it is an editor-side
  estimate, not a reading from the synth.
