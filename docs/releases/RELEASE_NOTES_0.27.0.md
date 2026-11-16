# Animatek NME 0.27.0: play, browse and perform

This version comes almost entirely from your reports: a computer keyboard that plays the synth, a bank
browser you can step through, the A-B Snapshot saved with the patch, and values that read as on the
synth. Use it with **G1-Emu alpha.31** or later
(https://github.com/animatek/G1-Emu/releases/tag/v0.1.0-alpha.31).

## Play from the computer keyboard (#124)

In the standalone app, turn **Caps Lock** on and the computer keyboard plays the synth, the way Bitwig
does it:

- `A S D F G H J K L ;` are the white keys and `W E T Y U O P` the black ones, from C up to the E of the
  next octave (`;` is `Ñ` on a Spanish keyboard).
- `Z` / `X` shift the octave down / up (C1 to C7). They share the octave with the Keyboard Floater's Oct
  buttons, and the status bar shows the new one.
- The notes go through the Keyboard Floater, so Drone, Repeat, Rate and Gate work as with its own keys,
  and its keys light up when it is open. It plays with the floater closed too.

Text fields still take what you type. In a DAW nothing changes: the host has its own computer keyboard.

## The A-B Snapshot is saved with the patch (#119)

Both ends of the A-B Snapshot, the knob or control that drives the fader, and where the fader was left
are now saved with the patch, in the `.var` file beside the `.pch`. So they also travel with a slot set
and with the library: load the set and every slot comes back with its own A-B, ready to play. Each slot
also keeps its own A-B while you switch between slots, where before it was lost on every switch. The
`.pch` stays standard, and an older NME simply skips the new part.

## Banks on the Disk tab (#125)

- **BANK** now lists only the bank folders under `Banks/`, each with how many patches it holds.
- A double click (or Enter) steps into a folder and lists only its patches, by their own names.
- The folder you are in stays pinned at the top of the list: click it to go one level up.
- Pressing **BANK** again takes you back to the list of banks. The search still looks through every bank
  at once.
- A right click on a row gives **Show in folder** and **Delete from disk...** (asked first, to the trash
  where the system has one, with the patch's `.var` beside it).

## Values as the synth shows them (#122, #123)

- **The Constant** reads 0.0 to 64.0 in steps of 0.5 when it is set to Unipolar, as the original editor
  and the synth show it: on the module, the hover readout, the inspector and the knob floater. Bipolar
  still reads -64 to +64.
- **Flat knobs' arcs** start where the value's zero is: at the minimum for ClkGen's Rate, LFO rates and
  frequencies, at the zero of a signed value such as NoteQuant's range, in the middle for pulse width,
  keyboard tracking and the EQ gains, and they follow a polarity switch (Constant, LevAdd, LevMult).

## The EQs have their original names (#126)

The module bar and the right-click menu now give the two EQs the names of Clavia's module menu:
**Parametric Eq** and **Hi and lo shelving eq**. EqMid was always in the Filter tab, but under a name
nobody looked for.

## The MCP bridge comes with the editor (#99)

`server.py` lived only in the source repository, so a client kept running an old copy long after a fix.
NME now writes its own copy into its data folder on every start, and Editor Options gives the command to
register it with your MCP client. A request from a different copy is turned down with that command, so an
outdated bridge says so instead of failing in odd ways. The bridge also works again with a fresh install
(it now pins `mcp` below version 2).

## Known

- The plugin's project recall (#94, since 0.23: the slots' patches come back as LOCAL when a DAW project
  reopens) has not been tried in every DAW yet. Tell us how it behaves in yours.
- The bank transfer dialog still lists nine banks on a Micro.
- With CLAP, when the last G1-Emu of a project closes, new instances have no MIDI port of their own (a JUCE
  limit); NME still finds them through the direct link. VST3 is not affected.
- Still beta: the first-start warning stays.

The binaries are on Patreon and in the Animatek store (a one-time purchase, updates included). Thanks to
bacizone and Nocticore for the detailed reports behind most of this version.
