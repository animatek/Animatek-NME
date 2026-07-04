# 5. Tools & Floaters

Five floating windows live in the View menu (or `Ctrl+5`–`Ctrl+9`). They are
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

- **DRONE** — latches notes until released.
- **REPEAT** — pulses the held note (Rate 100–500 ms, Gate 20–400 ms).

Notes are sent through the editor protocol, so they work over the same USB/DIN
connection as everything else.

## Patch Notes (`Ctrl+7`)

A resizable monospaced notepad bound to the active slot's patch. Notes are
stored in the `.pch` file's `[Notes]` section (a Nomad/nmedit extension that
original Clavia editors simply ignore) — no sidecar files.

## Patch Mutator (`Ctrl+8`)

A G2-style interactive sound breeder. A **Mother** and **Father** sound flank a
row of **Children**; from there you can:

- **Mutate** — gaussian variation around a sound (oscillator pitches snap to
  musical intervals),
- **Randomize** — fresh random settings,
- **Interpolate** — blend Mother and Father,
- **Cross** — genetic crossover (sequential or independent modes).

Click a sound to audition it on the synth. Locked parameters, excluded modules
(right-click a module → exclude from mutation) and Output modules are never
touched. A temporary storage row keeps favorites, and the variations row links
to the 8 per-slot variations. Keyboard control is fast — see the
[shortcuts](07-shortcuts.md#patch-mutator-window-focused).

## SysEx Monitor (`Ctrl+9`)

A live TX/RX hex log of all MIDI traffic between editor and synth — the tool to
grab when something doesn't sync and you want to see why (or to attach to a bug
report). Zero overhead when closed; works in release builds without a console.
