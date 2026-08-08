# 5. Tools & Floaters

Five floating windows live in the View menu (or `Ctrl+5`–`Ctrl+9`), plus two
extras at the end of this chapter. They are
normal windows: move them to a second display, resize the resizable ones, and
the editor remembers where they were.

## Knob Floater (`Ctrl+5`)

An interactive overview of the 18 hardware knobs plus pedal, switch and
aftertouch. Each knob shows its assignment LED and the module/parameter it
controls; the knobs are fully interactive (edit + sync + undo, morphs
included). Right-click a knob to reassign it to a free slot.

## Keyboard Floater (`Ctrl+6`)

A virtual keyboard with octave navigation for playing the synth without a MIDI
keyboard. Two performance modes:

- **DRONE**: latches notes until released.
- **REPEAT**: pulses the held note (Rate 100–500 ms, Gate 20–400 ms).

Notes are sent through the editor protocol, so they work over the same USB/DIN
connection as everything else.

## Patch Notes (`Ctrl+7`)

A resizable monospaced notepad bound to the active slot's patch. Notes are
stored in the `.pch` file's `[Notes]` section (a Nomad/nmedit extension that
original Clavia editors simply ignore). There are no sidecar files.

## Patch Mutator (`Ctrl+8`)

A G2-style interactive sound breeder. A **Mother** and **Father** sound flank a
row of **Children**; from there you can:

- **Mutate**: gaussian variation around a sound (oscillator pitches snap to
  musical intervals),
- **Randomize**: fresh random settings,
- **Interpolate**: blend Mother and Father,
- **Cross**: genetic crossover (sequential or independent modes).

Click a sound to audition it on the synth. Locked parameters, excluded modules
(right-click a module → exclude from mutation) and Output modules are never
touched. A temporary storage row keeps favorites, and the variations row links
to the 8 per-slot variations. Keyboard control is fast; see the
[shortcuts](07-shortcuts.md#patch-mutator-window-focused).

## SysEx Monitor (`Ctrl+9`)

A live TX/RX hex log of all MIDI traffic between editor and synth, the tool to
grab when something doesn't sync and you want to see why (or to attach to a bug
report). Zero overhead when closed; works in release builds without a console.

## Slot sub-windows

Not a floater, but the other way to get more than one thing on screen: the four
slots are tiled sub-windows inside the main window, each with its own canvas,
selection and undo history. `Ctrl+Shift+1`–`Ctrl+Shift+4` shows and hides them
(`Cmd+Alt+1`–`Cmd+Alt+4` on macOS).
See [Working with the Synth](04-working-with-the-synth.md#the-four-slots-on-screen).

## The MCP bridge

An optional, off-by-default local channel that lets an AI assistant edit the
patch through the editor's own undo system. See
[The MCP Bridge](09-mcp-bridge.md).
