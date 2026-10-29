# Animatek NME 0.25.0: the G1-Mods modules, two ways to work without the panels, and a log

NME now knows the new module types of a **G1-Mods** OS, and a handful of things asked for by people using
it with a Micro Modular and G1-Emu: two views that clear the screen, a lock for the slot question, one key
for the cables, and a log file for bug reports.

## The G1-Mods modules

[G1-Mods](https://animatek.github.io/G1-Mods/) builds an OS with module types Clavia never made: a
Turing Machine, a flip-flop, a scale quantizer, a plucked string and more. **From this version NME offers
them**, to a synth that has that OS only (it says version 3.09 when NME connects), in the module bar, the
menus and Quick Add. A G1 with Clavia's OS, or a Micro, sees exactly what it saw before. Patches that use the
new modules open anywhere, but are only sent to a synth that has them. The status bar says **MODDED OS**.

**NME 0.25 is the first version that knows them**: with an older one, a G1-Mods OS connects but its modules
are not offered. G1-Mods' install guide walks through building the OS and installing it with
**Device > Update Synth OS**.

## Working without the panels

- **Zen mode (F6)** closes the Inspector and the Patch browser and zooms every module to fill the area. Press it
  again and the panels and the zoom come back.
- **Performance mode (Shift+F6)** goes further: no module bar, no status bar and no cables, only the modules and
  the header with the macros and snapshots, for playing a patch you have finished. **Esc** leaves either view.
- **C hides every cable colour at once**, or shows them all; in the header, press a cable colour and swipe across
  the others to set them the same way. Folded cables have "Show / hide all cables" in their menu.
- **Ctrl+L locks opening to the active slot**: patches opened from disk go straight into it with no question,
  until you unlock it. It is the editor option "Ask which slot when opening a patch", one key away. A Micro,
  which has one slot, is never asked.

## The Micro Modular

- The inspector's knob panel shows the Micro's **three knobs** (it showed the G1's eighteen), and the Knob menu
  of a parameter or a morph lists those three and After touch.
- **Loading a slot set** into a Micro no longer ends with a false warning.

## A log file for every run

NME wrote what it did to a console that Windows does not have, so bug reports had nothing to attach. Now every run
writes a log with a time stamp on each line, kept for the last ten runs. **Help > Open Log Folder** opens it:
`%APPDATA%\AnimatekNME\Logs` on Windows. When you report a problem, attach the log of the run where it happened.

## Fixed

- **The Synth tab no longer shows empty banks** after a Refresh or a connection hiccup: the list keeps what it had
  and only replaces what the new read reached; the read waits for the synth's silence, not a fixed 10 seconds (#112).
- **The status bar names the patch on screen** after loading from disk, and no longer keeps an old error (#113).
- **Patch Mutator:** right-click on a slot offers **Save Current Sound Here**, which works on an empty one (#114).
- **Update Synth OS** says "this is a whole 512 KB ROM, for G1-Emu" instead of "too big" when you pick the wrong file.

## Known

- Saving the A-B snapshot inside a slot set is not done yet (#119).
- The Knob Floater still shows all eighteen knobs on a Micro (#111), and the button of the Micro has no assignment
  yet: waiting to hear which knobs the Micro's three are.
- The plugin's editor does not restore its state when a DAW project reopens (#94).

Thanks to everyone who reported these, in particular bacizone.
