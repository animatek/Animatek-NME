# Animatek NME 0.8.0 — Truer to the Hardware

Most of the work in this release is invisible, and that's the point. 0.8.0 is a reliability
pass: it teaches the editor to behave like the Nord Modular *actually* behaves — down to how
the slot LEDs blink — and clears out a stack of patch-loading and metering bugs you reported.
If you juggle slots, dig through old patch libraries, or live in the sequencer, you'll feel
this one.

## What's new

### 🎛️ Real multi-slot behavior

The G1 has a two-level slot system, and now the editor finally models it: *enabled* slots
(steady LED, several can be lit at once) versus the single *selected* slot (blinking LED). A
plain click follows the front-panel rule — the new slot lights up, the one you're leaving
switches off unless it's pinned, and pinned slots stay put. **Ctrl+click** enables/disables a
slot without selecting it, exactly like Shift+button on the panel (and Ctrl+click in the old
3.3 editor).

Under the hood: the editor now tracks the *real* enable mask reported by the synth instead of
rewriting it with a single bit on every slot change — which used to silently switch off every
other enabled slot. Patch IDs are tracked per slot too, so a fetch or `NewPatchInSlot`
notification from a background slot no longer overwrites the slot you're working in. The slot
bar shows a hardware-style LED per slot to match.

### 💾 Your sequencer edits actually save now

Saving a patch writes the *current* values of custom controls — sequencer steps,
clock-divider displays — instead of the snapshot from when you loaded it. And custom values
are now applied *after* the whole patch is parsed, so a `CustomDump` arriving before its
`ModuleDump` no longer gets silently dropped. Translation: your edited sequences survive the
round trip.

### 📂 Legacy 2.10 patches load correctly

Old Nord Modular `2.10` patches with `[Module N]` sections now load their modules, positions,
names, parameters and cables instead of opening empty. The big one: daisy-chained cables
(~5% of all cables in real 2.10 libraries) were reading the `Ih` field wrong — bit 6
distinguishes an output source from a chained-input source — so they connected to the wrong
spot. Fixed. Legacy patches also get a purple **PCH2** tag in the browser, and detection is
shared with the loader so a modern patch that just *mentions* `[Module` in its notes won't get
misrouted.

### 📊 Correct meters and step LEDs

Fixed VU channel ordering (#12) — meters are stored in wire order like NOMAD's LightProcessor,
and the A/B channel choice happens at render time, so stereo modules read left/right correctly
and sequencer step LEDs stop flickering. Also cleaned up the overlapping dB scale digits on
the Audio In module.

### ⚡ Knobs respond instantly

The editor used to pace every outgoing MIDI message at one per 50 ms — enough to feel as a
small lag on every knob move, and to make DrumSynth preset changes "morph" over most of a
second as its 15 parameters trickled out. Messages are now sent the moment you make the
change, the way the original editor's protocol thread did it, and patch section requests
properly wait for their replies. Knobs track your hand, presets snap, and patches load
faster too.

### 🎨 New app icon

Fresh Animatek artwork across the app, window and taskbar.

## Fixed from your reports

Issues **#12, #13 and #14** are all resolved here — meter channels, custom-control values, and
legacy patch loading. Keep 'em coming. 🙏

---

*Grab 0.8.0 from the downloads below. Back up your patch libraries before upgrading, and keep
each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation exclusions.*

Thanks for supporting Animatek NME.
