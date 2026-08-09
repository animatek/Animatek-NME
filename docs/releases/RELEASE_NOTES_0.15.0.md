# Animatek NME 0.15.0: the module bar, and a synth that stops going quiet

The module palette comes back as a bar under the header, the frequency displays answer in
whichever units you ask for, and the three faults that made a large patch impossible to send to the
synth are fixed. This release also carries everything prepared for 0.14.0, which never went
out, so if you are coming from 0.13.0 you are getting both.

## What's new

### 🎛️ The module bar

The original editors put the module palette in a bar grouped by category, right where you are
working, and plenty of people reach for a module there rather than through a tree. It is back,
laid out the way Clavia's is: category tabs across the top, **In/Out, Osc, LFO, Env, Filter,
Mixer, Audio, Ctrl, Logic, Seq**, and the modules of the chosen category underneath, each in a
thin chip with its name.

**Drag** a chip onto a patch area, or **click** it and the module hangs off the pointer until
you click where you want it, which is the same gesture Add Module uses and lets one click
choose the area and the slot as well as the spot.

The bar remembers the tab you left it on, and **View > Module Icon Bar** turns it off if you
would rather have the pixels for the canvas.

The original shows pictograms rather than names, and so will this, once the artwork is our own
and takes the theme's colour like everything else on the canvas does.

### 📐 Frequency displays that change units

Click the frequency box on an oscillator, a slave LFO or a filter and it changes the units it
reads in, the way the original does. An absolute frequency alternates between hertz and the
note it lands on. A **slave oscillator** goes round three: the partial ratio, the interval in
semitones, and the frequency its master actually puts it at.

**Hovering shows the units the box is not displaying**, so a suboscillator can be set to
`-12(Oct)` and checked in hertz without touching anything. The choice belongs to each module
and is saved with the patch, in the same place the original keeps it.

## Fixed

### 📤 A big patch can be uploaded again

A patch with around a hundred modules always failed to upload: the synth rejected it with a
checksum error partway through and the transfer died, so a patch like `SY-1 RndBlips1` could be
downloaded but never sent back.

The bytes were right; the shape was wrong. Each of the sixteen sections was going out as one
packet, which works only while every section stays small, and the module-name section grows
with the module count. The synth is built to receive one continuous stream cut into small
packets, and that is what it gets now.

### 🔇 A failed upload no longer leaves the synth deaf

The synth waits for a packet marked as the last one before it leaves bulk-receive mode, and an
upload that simply stopped never sent one. Parked there it answered nothing at all: no
acknowledgements, no reply to the editor's handshake, not even its own idle stream. It looked
like dead hardware that needed a power cycle. It never did, and now every way out of an upload
closes the transfer properly.

### 🔄 Loading from the synth's bank refreshes the editor

Load a patch into a slot from the front panel and the editor follows again. A synth still
writing a large patch was not answering the request that follows, and the editor gave up
without a word and sat on the previous patch. It asks again now, and says so if the synth
really is not answering.

### ⚡ Buttons answer as readily as knobs

Every press of `KBT`, a mute or a waveform selector was putting the same message on the wire
twice and rebuilding the morph list and the DSP figures behind it, which walk the whole patch.
A knob pays that once per drag; a button was paying on every click. A parameter edit adds and
removes nothing, so none of it can have changed.

### 🎹 Zooming the Note Sequencer no longer retunes it

The piano roll's zoom is a display setting, but it was being sent to the synth as an ordinary
parameter, and it shares an index with the sequence's first note. Every zoom in or out moved
that note.

---

## Also in this release: everything from 0.14.0

0.14.0 was prepared but never published, so its changes arrive here for the first time:

- **Paste and Add Module hand you the modules on the pointer**, and the click that follows
  places them, which also chooses the area and the slot. Poly to Common and back, and pasting
  into Common at all, work for the first time.
- **Nothing gets buried**: dropping a module on another moves the column down to make room,
  and undo puts it back.
- **Paste and Duplicate can be undone**, which they never could.
- **Knobs answer vertical movement by default**, and circular control reads the dial rather
  than counting turns.
- **The Drum Synthesizer's 29 factory presets** ship with the editor.
- **The overlay readouts are on the View menu** as well as on their function keys, and **Cut,
  Copy, Paste and Duplicate are on the Edit menu**.

The full list is in [CHANGELOG.md](../../CHANGELOG.md).
