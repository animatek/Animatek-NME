# Animatek NME 0.17.0: the one where cables move

Two gestures the original editor had and this one did not: a cable can be lifted off a
connector and dropped on another one, and a patch can be dragged straight onto a slot. Around
them, four faults that only ever showed on somebody else's machine, including a Mac menu bar
with no keyboard shortcuts on it and a patch that reloaded itself every three seconds.

## 🔌 Cables can be re-routed, not just cut

Hold `Ctrl` (`Cmd` or `Alt` also work) and drag a connector that already has a cable: the cable
comes off that end and follows the pointer from the end that stays put, ready to drop on another
connector. This is what moving a patch's wiring onto a replacement module one cable at a time
needs, and it is the gesture the original editor had.

Nothing happens to the patch until you let go. The cable is only hidden from the canvas while
you carry it, so a re-route that lands nowhere legal costs nothing, sends nothing to the synth
and leaves nothing on the undo stack. Where a connector has several cables the one drawn on top
comes off first, and repeating the gesture takes the ones underneath. The move itself is one
undo step.

## 🎯 Drag a patch onto a slot to load it there

From both browsers: a patch in the synth's banks from the **Synth** tab, and a `.pch` from the
**Disk** tab or the `Ctrl+B` window. Two places accept the drop, the slot's own sub-window and
its row in the slot bar down the left side, which is the one that still works when that slot's
window is closed (it opens on the way). The target lights up while you are over it.

Each drop ends in the load that already existed, so nothing new goes to the synth. The `Ctrl+B`
preset window could not start a drag at all until now, snippets included.

## 🔤 The macOS menus have their keyboard shortcuts back

The Mac menu bar is the system's own, and it only prints a shortcut that comes from a JUCE
command manager: the hint field that the Windows and Linux menus right-align is thrown away
there. Moving the hints into that field in 0.16.0, so the Mac would stop printing a literal tab
character, took every shortcut off the Mac menus.

They are now written into the item's own text on macOS, in the symbols a Mac reads: **New Patch
⌘N**, **Save As... ⇧⌘S**, **Slot A ⌥⌘1**. They also stop saying "Ctrl" for what is Cmd there.
Nothing changes on Windows or Linux. (#74)

## 🔁 The patch stops reloading itself every few seconds

The canvas jumped back to its top-left corner on its own and the status bar flashed "loading
patch 1/13", with nobody touching anything. The G1 does not only answer an `IAm`, it announces
itself: the SysEx dumps on the report have one arriving roughly every three seconds for as long
as the synth is on. The editor read each announcement as a connection that had just come up and
re-ran its whole opening sequence, so the patch was pulled down again on top of the one being
worked in.

An announcement from a synth the editor is already talking to now changes nothing. It is still
listened to while disconnected, so a synth switched on after the editor still brings the
connection up by itself. (#73)

## 🎚️ Cable opacity moved where every platform can reach it

It was a slider inside the View menu, and a native Mac menu cannot host a custom control: JUCE
dropped it and left a blank line, so the setting was untouchable on macOS. It now sits under
**File → Editor Options**, next to the cable style, as a percentage slider. The canvas follows
it while you drag so you can see what you are choosing, and Cancel puts the old value back.

It also survives a restart now, which it never did from the menu on any platform. (#74)

## Also in this release

- **An ABCD button that puts the slot windows back in order** (#51), right of MUT in the header
  bar, its four letters drawn in the quadrants they land in. One click re-tiles the open
  sub-windows and puts the slots back into A, B, C, D order whatever order you opened them in.
  **View > Slots > Reset Slot Order (ABCD)** is the same thing.
- **Pinch to zoom on a trackpad** (#72), around the pointer, the same zoom `Ctrl`/`Cmd`+wheel
  already did.
- **The editor can show itself on the synth's own display.** Off by default; turn on **Show the
  editor on the synth display** in Editor Options and any dialog borrows the G1's display for
  `ANME 0.17v` while it is up, giving the patch name back when it closes. Nothing happens to the
  patch.
- **Uni/Bip switches say which polarity they are in** (#69). The inherited panel data labelled
  both states "Uni" on Constant, LevMult, LevAdd and the Control Sequencer.
- **A patch opened from a bank backup no longer keeps the position number in its name.**
  Loading `35 - BELLS++` stored it on the synth under that name instead of `BELLS++`.
- **The empty-canvas hint is readable on every theme** (#70) and **the status bar follows the
  theme** (#71), including when the theme changes while it is on screen.

## Known limitations

- macOS builds are unsigned. Right-click → Open the first time, or clear the quarantine
  attribute, until code signing exists.
- Stuck MIDI-IN notes are cleared with the synth's front-panel panic.
