# Animatek NME 0.19.0: the one that stops pretending

This cycle the editor met G1-Emu, the Nord Modular G1 emulated on its own original OS, and
being able to break a synth on purpose turned up every place where the editor believed a slot
matched the synth when it did not. Those are fixed, and the release brings slot sets, scales on
any root for the Key Quantizer, and replacing a module in place.

## 🔎 The editor says what is true about the synth

- **A patch loaded from the synth's front panel arrives whole.** It used to end in "Incomplete
  Patch Load: 0 of 13 sections" and a connection that looked dead. The G1's OS announces a
  panel load with two patch ids in a row; the editor asked for the sections with the first, and
  the synth answers a stale id with nothing. It now follows the id the load ends on.
- **A failed upload no longer looks like a successful one.** The slot turns LOCAL and stops
  sending edits to a synth slot that holds another patch.
- **A synth that stops answering is marked "not responding"**, and the editor reconnects by
  itself as soon as it speaks again.
- **A MIDI port that disappears disconnects the editor** with "MIDI port gone": closing G1-Emu
  or unplugging the interface used to leave it saying "Connected".
- **Switching synths no longer shows the previous one's patch as synced.** Slots are LOCAL until
  the new synth's patch arrives, so an edit in between cannot land on the wrong patch.

## 🎛️ Slot sets

Layered patches, one per slot, only make their sound together, and the G1 cannot store that.
**File > Save Slot Set...** saves the four slots as one sound, each slot as *include*,
*switch off* or *leave as is*; **File > Open Slot Set...** (or a double click on a **SET** in the
Disk tab) loads them back, one upload at a time, and restores which slots are on.

## 🎼 Key Quantizer scales on any root

The Scale menu opens with what the switches spell now, ticks every scale they match, and lists
every scale on every root from C to B. New scales: Phrygian Dominant (Andalusian), Double
Harmonic, Hungarian Minor, Neapolitan Minor and Major, Hirajoshi, In-sen and Diminished.

## 🔁 Replace a module with another of its family

Right-click a module, **Replace with**, and a FilterE becomes a FilterD in place, as one undo
step: position, name, the cables and values that fit, and their knob, morph and CC assignments
are kept; what does not fit is dropped and reported, never rewired to a guess.

## Also in 0.19.0

- **Reconnect** in the MIDI settings: switching between a real G1 and G1-Emu is one click.
- The **Synth Settings** dialog reads the clock source and the master tune the right way round.
  Choosing Internal used to stop the synth's master clock, and 0 cents showed as −64.
- The **keyboard floater** no longer leaves notes stuck on the synth.
- **`Ctrl+R`** leaves modules excluded from mutation alone.
- A **failed bank backup** keeps the previous one intact.
- A **store to a bank** no longer gets lost after an upload, and the bank list follows it.
- The **Disk tab** filters are words now (ALL, PATCH, SNIP, BANK, SET).
- The **MCP bridge** reads the synth back (status, events, lights, bank names), assigns knobs,
  morphs and MIDI CCs, chooses its MIDI ports and fetches a slot again.

The complete list is in `CHANGELOG.md`.

## Known limitations

- The editor's keyboard is monophonic, as the synth makes it; chords need the regular MIDI input.
- Stuck MIDI-IN notes are cleared with the synth's front-panel panic.
