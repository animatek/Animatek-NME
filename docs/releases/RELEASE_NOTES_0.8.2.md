# Animatek NME 0.8.2 — Multi-Window Editing & Reliability

A big one. The headline is working on two or more patches side by side, like the
original editor — but most of this release is about the editor telling you the truth
about what's actually on the synth, especially on CPU-heavy patches where things used
to quietly go wrong.

## What's new

### 🪟 Pop-out windows for editing 2+ slots at once

Right-click a slot row (A–D) to open that slot's patch in its own window — cables,
modules, parameters, morph/knob/MIDI-CC assignment, renaming and undo/redo all work
independently there, right alongside the main window's tabs (which keep working exactly
as before). This isn't just a second view: edits made in a background slot's window land
on the synth correctly even without front-panel focus, confirmed on real hardware. When
the synth's own focus changes to a slot with a window open, that window comes forward
and its title marks it "- Focused" — the same idea as the original editor highlighting
the active patch's title bar.

### 🐢 Fixed cables and modules disappearing on CPU-heavy patches

Two separate bugs were causing this, both fixed:

- A rack running at 99–100% DSP load answers patch downloads slowly. The editor used to
  time out and silently keep whatever partial data had arrived — missing cables, a
  desynced rack, and saved `.pch` files with invalid cable connections. It now
  re-requests only the sections that didn't arrive and warns clearly if a load is still
  incomplete after retrying, instead of pretending everything is fine.
- On Linux, a SysEx message larger than one MIDI packet (common on complex patches — a
  patch with 65+ cables routinely needs this) was silently truncated by JUCE's MIDI
  input layer after the first chunk. Patched the vendored JUCE library to reassemble
  chunked SysEx properly; verified on hardware with previously-broken patches loading
  with every cable and module intact.

### ⚡ Instant slot switching

Changing between slots A–D no longer re-downloads the patch when the editor already has
a model that matches what's on the synth. Connecting also now quietly fetches every
enabled slot's patch in the background (not just the focused one), so the first switch
to any of them is instant too — matching how the original editor behaved.

### 🎨 Morph highlighting on every control

4-1 selector switches, toggles, increment buttons and sliders assigned to a morph group
now show the group's color, the same way knobs already did.

### 🔈 Legacy 2.10 patches play correctly

Old Nord Modular 2.10 patch files store their output routing differently than the
current format; importing them was silently sending every legacy patch's audio to
outputs 3/4 instead of 1/2. All 857 known factory patches now import with correct
routing and actually make sound.

### Also in this release

- The preset browser sometimes only showed the first few banks after connecting
  (a name-list fetch getting interrupted by the initial patch load) — it now resumes
  automatically instead of staying partial.
- The console log marks each patch load with a clear boundary line, for easier bug
  reports.

---

*Grab 0.8.2 from the downloads below. Back up your patch libraries before upgrading, and
keep each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation
exclusions.*

Thanks for supporting Animatek NME.
