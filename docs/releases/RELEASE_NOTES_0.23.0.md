# Animatek NME 0.23.0: the new look, and a Mutator that mutates like the G2's

The whole interface is redrawn in a flat, dark style with one amber accent. It is the same editor,
with the same shortcuts and the same files, drawn from scratch: calmer, with more room, and with
the controls where you look for them.

## The Patch Mutator, rebuilt

The G2's own mutator was studied and its ideas rebuilt (no code of theirs):

- Every parameter has its own **shape**: finer near the bottom (attack, release, times), near the top
  (an oscillator's levels) or around the middle (tune, detune, pan), and a change is drawn evenly in that shape.
- **Set-up switches are never mutated** (on/off, mute, bypass, modes, KBT and LFO range switches, input
  levels, step counts); waveforms, sequencer steps and key-quantiser notes still are, as on the G2.
- Changes **bounce back from the edges**; Probability and Range are tied by the G2's curve and Range stops
  at 50%; no more octave jumps; Randomize keeps each parameter's shape and leaves the switches.
- **Mother and Father start as the patch.** The boxes draw the sound as a path, and stored sounds follow
  the patch when a module is added, deleted or replaced.

## Banks straight to the synth's memory

Save Bank to Disk and Send Bank to Synth talk to the synth's flash directly, as Clavia's editor does, with
no slot used on the way; banks are saved with a `.pchList`. Four MCP tools drive them.

## What you will notice in the new look

- **A new header bar.** The patch name in a field you can type in, the four macro dials redrawn, the cable
  colours between the dials and the snapshots, the **A/B crossfade** between the snapshots and
  **MUTATOR**, and heart and Patreon buttons at the far right.
- **A module bar with a rule** between the categories and the module chips, and room round the ABCD button.
- **A new inspector.** It shows the module you select and only its own knobs, MIDI CCs and macros; what is
  empty is hidden.
- **Both browsers redone.** The Synth tab shows each bank as a heading, with the locations as pills; the
  Disk tab lists the whole library in a third of the time and no longer freezes the editor while it reads.
  "Hide empty" and "Hide 2.10" are remembered.
- **Every menu, dialog and floater restyled**: Editor Options with a list of pages, MIDI Setup with a card per
  port, Patch and Synth Settings, the slot and location dialogs, the Mutator, the Knob Floater (with the
  same knob the modules wear and the value as the parameter formats it), the keyboard, notes and SysEx windows.
- **A status bar with buttons.** An **i** opens and closes the inspector, a **lens** the browser, a little
  **keyboard** opens MIDI Setup and a **cogwheel** the options. Ctrl+I, Ctrl+Shift+I, Ctrl+M and Ctrl+, are
  unchanged.
- **Keyboard Shortcuts** is a window with the groups at the left, the keys as key caps and a filter that
  searches every group.
- **One Themes window** (View > Themes...) with pages for the themes, the colour editor, the community themes and the
  file options, in place of the long submenu. **Ctrl+T cycles the theme** even with a menu open. Opening MIDI Setup, Editor Options or Patch/Synth
  Settings again closes it instead of opening a second one.

## Also in this version

- **The plugin keeps the slots' patches in the DAW project** and restores them when it reopens (#94).
- **UI size** in Editor Options scales every window, menu and text for 4K screens (#105).
- **Banks go to and from the synth's memory directly**, as in Clavia's editor.
- **The Patch Mutator mutates like the G2's.**
- **Store to Bank is confirmed by the synth** and goes to the right synth when two are connected (#109).
- **Daisy-chained cables keep the colour of their source** (#108, thanks teezdalien).
- **Fixed:** the Disk tab now lists every `.pch` under the library folder (#106); Ctrl+B opens the right
  panel if it is folded (#107); the module cost badge shows whole (#104); a cable keeps its stored colour
  (#100); no display no longer crashes NME at start (#101); a refused MCP connection says where the
  bridge is turned on (#102); a saved `.pch` has the same header line as Clavia's (#103); dialogs and
  floaters no longer hide behind the main window (#98); adding and deleting modules fast no longer gets a
  patch rejected.

## Honest notes

- The new look was checked on Linux, with a G1-Emu. Windows and macOS builds of it have had little use:
  if a menu, a font or the scaling looks off on your system, please say so.
- Only the standalone editor was tested against hardware after the redesign; the first G1 sessions with it
  are the ones that count.
- The first-start warning still says this is a beta, and it is.

The complete list is in `CHANGELOG.md`.
