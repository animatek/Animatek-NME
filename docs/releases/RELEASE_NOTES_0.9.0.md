# Animatek NME 0.9.0 — Slot Window Polish & Reliability Fixes

A follow-up to 0.8.2 — one UI improvement for the new pop-out slot windows, and a batch
of correctness fixes found during a review pass right after that release shipped.

## What's new

### 🧰 Collapsible inspector in slot pop-out windows

A slot's own window can now hide its Inspector panel (morph/knob/MIDI-CC assignments)
so the canvas gets the window's full width — handy once you've got a window sized
narrow. Click the thin arrow strip at the canvas's left edge, or press `Ctrl+I` while
the window is focused, to toggle it back and forth.

### 🩹 Reliability fixes from a post-0.8.2 review pass

- A stale internal flag could let a real patch fetch get mistaken for an abandoned
  background prefetch and get corrupted.
- Opening a `.pch` file while viewing a slot that didn't have hardware focus could
  silently upload it to the wrong physical slot.
- A queued batch of parameter changes for one slot (e.g. a Mutator audition in a
  background window) could be wiped out by an unrelated fetch on a different slot.
- Morph keyboard (velocity/note) assignment could target the wrong slot.
- Importing a legacy 2.10 patch with a morph keyboard assignment set could silently
  corrupt that assignment — an over-broad string match was catching it alongside the
  intended output-routing fix from 0.8.2.

---

*Grab 0.9.0 from the downloads below. Back up your patch libraries before upgrading, and
keep each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation
exclusions.*

Thanks for supporting Animatek NME.
