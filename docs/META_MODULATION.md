# Meta-Modulation Layer — Design Notes

Status: **design / research**. Nothing here is implemented yet. This document scopes a family
of editor-side features (Auto-Morph, snapshot banks, virtual modulators, a note bridge) and,
more importantly, draws the hard line between what the Nord Modular G1 *hardware* can do and
what only the *editor* can do. Read [RESEARCH.md](RESEARCH.md) first for the protocol basics.

---

## 1. The core idea: two worlds and one bridge

Everything below rests on one distinction.

| World | What it can host | Cost / limit |
|-------|------------------|--------------|
| **Synth DSP** | fixed firmware modules, 4 morph groups, real parameters | limited, but **runs standalone and is recordable** on the hardware |
| **Editor** | unlimited macros, modulators, snapshots, "virtual modules" | control-rate only, and **only while the editor is running and connected** |

The **bridge** between them is made of the messages the synth already exchanges with the editor:

- **Editor → synth**
  - `ParameterChange` (cc `0x17`) — set any module parameter. This is what the interpolation
    engine already streams (`ConnectionManager::sendParameter`, coalesced in `paramQueue_`).
  - `Note` (cc `0x17` sc `0x56`) — play a note on the current slot. Payload is `{onOff, note}`
    with `onOff` `0=on, 1=off`, **no velocity on the wire** (`ConnectionManager::sendNoteEvent`,
    `:898`). Used today by the Keyboard Floater.
- **Synth → editor** (what we can *sense*)
  - `ParameterChange` / `KnobChange` (sc `0x40`) — fires whenever a **physical panel knob** is
    turned; payload is identical to a parameter change: `{section, module, parameter, value}`
    (`ConnectionManager.cpp:1594`). **This is the hook for using a hardware knob as a macro source.**
  - `NMInfo` (cc `0x14`) — streamed **meters / lights / voice-count** (`NmProtocol.cpp:41`).
    Coarse, visual-feedback rate, unsolicited.

**What the synth never sends back: arbitrary DSP signal outputs.** There is no way for the
editor to read the output of a cable, an oscillator, or a clock module. The only "signals" that
leak out of the box are the LED/meter values inside `NMInfo`. This single fact decides which of
the ideas below are clean and which are hacks (see §5).

### The "dummy module" trick (documented by Clavia)

The original manual (captured in `source/help/ModuleHelpData.cpp:1241`) already describes the
technique we need:

> *"The knobs can also be set to send MIDI controller messages. If you want a knob to control
> external MIDI devices, without affecting any parameter in Nord Modular, assign the knob to a
> parameter in a 'dummy' module (that is not a part of the sound in the patch)…"*

So the sanctioned way to get a "free" control that doesn't disturb the sound is to bind a
physical knob to a **sonically inert real parameter**. We reuse exactly this: a dummy-module
parameter becomes the carrier for an editor-side macro. We never invent a parameter the synth
does not know — that is impossible (see §5).

---

## 2. Feature: Auto-Morph (A/B interpolation)

A software morph between two captured patch states, driven by a fader that can be bound to a
physical control. **Independent of the Patch Mutator's 8 variations** — it gets its own A/B pair,
though it may borrow a Mutator variation as A or B.

### Behaviour
- Capture current patch state → **A**; capture again → **B** (full `ParamSnapshot`).
- A **vertical fader** (right of the snapshot row) = morph position `t ∈ [0,1]`. `t=0` → A,
  `t=1` → B. Dragging it lerps every differing parameter and queues the changes.
- This reuses the existing interpolation matching in `startInterpolationTo`, but needs a new
  **position-driven** entry point instead of the time-driven tick:

  ```
  applyMorphPosition(from, to, t):   // t in [0,1], no timer
      for each matched (param, aVal, bVal):
          queueParameter(lerp(aVal, bVal, t))
  ```
  Cheap: a lerp plus the coalescing queue that already collapses repeated writes.

### "Decide how many knobs" (multi-axis morph)
This is the reading of the original wishlist phrase *"automated assignment to knobs, while
having the possibility to decide how much knobs should be used."* It is **not** about the 4
hardware morph groups.

- Instead of one global A→B fader, split the morph into **K independent axes** (K chosen by the
  user, unlimited — this is software).
- Each axis owns a **subset** of the differing parameters. Bucketing strategies:
  - by module category (all filter params on axis 1, all osc params on axis 2, …)
  - by delta magnitude (biggest changes first), round-robin or top-N per axis
  - manual override
- Each axis is its own fader `t_k`, so you can morph the filter section without touching the
  oscillators. K=1 is the simple global morph.

### Binding a fader to a physical knob ("Learn")
1. User arms Learn on a fader, then turns a physical panel knob (ideally one assigned to a
   **dummy module** parameter, per §1).
2. The editor catches that knob's `KnobChange` (`{section, module, parameter, value}`) and
   records `(section, module, parameter)` as the fader's source.
3. From then on, that knob's stream maps `0..127 → t = 0..1` and drives the morph.

**Honest limits:** control-rate (~30 ms, the current interpolation cadence), editor-tethered,
and it "spends" one physical knob + one dummy parameter. Cleaner sources to investigate: an
**empty morph-group knob** (if the synth reports its position) or an **external MIDI CC** the
editor listens to directly (no dummy needed). See §6.

---

## 3. Feature: Snapshot banks (up to 128)

Pure storage, editor-side, cheap. The current 8 slots (`PatchVariations::kNumSlots`) are tied to
the Mutator; snapshot banks would be a separate store so we do not disturb that.

- Scale from 8 → 32/128 snapshots with a **bank UI** (a strip of 128 buttons does not scale —
  keep the 8 fast-access slots, put banks behind them) and an optional **name** per snapshot.
- 128 is the natural ceiling because it is the MIDI value range: snapshots could be recalled by
  **Program Change** or note number.
- Sidecar format (`.var`) bumps to v2 to hold N named snapshots. The `.pch` stays 100%
  compatible with the original editors (§7).

Any snapshot can serve as A or B for Auto-Morph, and a chain A→B→C… defines a **morph path** the
master fader sweeps through.

---

## 4. Feature: Editor-side "virtual modules"

The generalisation of everything above: constructs that live **only in the editor** (in the
sidecar, never uploaded) and produce modulation by streaming `ParameterChange` (or `Note`)
messages to real targets. Same engine as the interpolation, just different sources.

Examples:
- **Virtual LFO / envelope / step-sequencer** driving one or more real parameters (a slow filter
  sweep, a per-step tweak).
- **Snapshot morpher** (Auto-Morph is a special case: a 2-point path).
- **Note bridge / editor clock** (§5).

**Hard limit — you cannot invent DSP modules.** The module set lives in the synth firmware; an
unknown module index in an uploaded `.pch` makes the synth reject it or misbehave (this is what
"leaves the synth a bit crazy" was — the box parsing a module it does not know). Virtual modules
must therefore never enter the `.pch`; they drive existing parameters from outside.

Practical ceiling: **control-rate, MIDI-bandwidth bound**. Fine for slow LFOs, macro morphs,
step tweaks; not for audio-rate or tight-timing modulation.

---

## 5. The note bridge & the DSP-signal wall

Idea: an editor module that emits notes to the synth (via `Note` sc `0x56`) — an editor clock,
arpeggiator, or trigger. Lives in the sidecar, never in the `.pch`.

### What works cleanly
An **editor-timed** note generator: the editor runs its own clock (or syncs to incoming MIDI
clock / host tempo) and calls `sendNoteOn/sendNoteOff`. Standalone, no DSP involvement. This is
the clean path and it is basically already wired — `ConnectionManager::sendNoteOn` exists and the
Keyboard Floater proves the round trip (the TX example `f0 33 5f 06 00 56 01 30 0f f7` is exactly
a `Note` on).

### What does *not* work: routing a DSP clock into an editor module
You **cannot** feed a DSP clock-generator's output signal into an editor-side bridge. The editor
never receives module output signals (§1) — a cable's value simply does not leave the box. So
"patch a DSP clock → our note module" is impossible as a direct signal route.

### The one hack: LED-driven triggering
Clock / LFO / sequencer modules have **LEDs**, and LED state *is* carried in the `NMInfo` stream
(`NmProtocol.cpp:41`). So the editor could **watch a module's LED and fire a note on each pulse**
— a genuine "DSP clock → editor → note" bridge. Caveats, to be honest:
- It is **visual-feedback rate** (tens of ms, jittery) — a loose musical clock, not tight timing.
- `NMInfo` light bytes are **not parsed today** ("real-time knob/light updates don't
  auto-refresh", RESEARCH known limitations) — we would have to decode the light payload first.

**Recommendation:** ship the editor-timed generator (clean, useful now). Treat LED-triggering as
a research experiment (§6), tempo-lockable more reliably via **external MIDI clock in**.

---

## 6. Open research questions

1. ~~Does the synth report **morph-group knob** positions to the editor?~~ **ANSWERED (yes,
   hardware-confirmed 2026-07-24).** An empty morph group is a free, inert macro carrier: assign
   a physical knob to it via the native `KnobAssignment`, and turning the knob streams the group's
   value to the editor with no audio side-effect. This is how the Morph A/B fader binds to a
   panel knob (`MainComponent::assignMorphKnob`).
2. Does the editor listen to **raw incoming MIDI CC** from an external controller? Cleanest macro
   and note-bridge sync source.
3. Decode the **`NMInfo` light payload** — which bytes map to which module LEDs, and at what
   refresh rate. Prereq for LED-triggered notes and for live meters/lights in general.
4. **SysEx bandwidth** when a multi-axis morph moves many parameters per tick — where does the
   coalescing queue saturate, and what is a safe max param count per morph.
5. Note bridge timing jitter over ALSA SysEx (relevant given the JUCE ALSA reassembly patch).

---

## 7. Persistence

All of this lives in a **sidecar** next to the `.pch` (extending the existing `.var`, or a new
`.meta`/`.morph` file). The `.pch` must stay byte-compatible with the original Clavia/nmedit
editors — no meta-modulation data ever goes into it, and none of it is uploaded to the synth as
patch content. What reaches the synth is only the *result*: streamed `ParameterChange` and `Note`
messages.

---

## 8. Suggested roadmap phasing

1. **Auto-Morph, single axis** — A/B capture + position-driven fader + reuse of the interpolation
   engine. Smallest slice, immediately useful. Decoupled from the Mutator.
2. **Physical-knob Learn** — bind the fader to a panel knob via `KnobChange` + the dummy-module
   trick. Answers "assign the morph to a physical pot".
3. **Multi-axis morph** — K user-chosen axes with parameter bucketing. Answers "decide how many
   knobs".
4. **Snapshot banks (128)** — storage + bank UI + names + Program-Change recall.
5. **Editor clock / note bridge** (editor-timed) — clean note generation; MIDI-clock sync.
6. **Research spikes** — morph-knob reporting, MIDI CC in, `NMInfo` light decode → then
   LED-triggered notes and other virtual modulators.
