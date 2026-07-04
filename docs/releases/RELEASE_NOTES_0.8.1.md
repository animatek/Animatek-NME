# Animatek NME 0.8.1 — Chained Cables & Speed

A quick follow-up to 0.8.0 with one classic feature back from the original editor and a
big performance pass. If 0.8.0 made the editor *truer* to the hardware, 0.8.1 makes it
feel effortless.

## What's new

### 🔗 Chained cables, like the original

You can now run a cable from one input to another, daisy-chaining a net the way the G1 and
the original editor allow — pull OscA1's Pitch input over to OscA2's Pitch, or bridge the
2 Outputs L jack across to R. The editor enforces the hardware rule (a net can only be
driven by one output — invalid targets won't light up while you drag), chained cables
survive save/load, upload, download, copy/paste and undo, and a long-standing decoding bug
that mangled these cables when fetching patches from the synth is fixed.

### 🚀 Way less CPU, way faster rendering

The editor no longer repaints the entire canvas dozens of times per second while connected.
The light/VU stream from the synth is filtered at the source (unchanged values don't even
wake the UI), only the modules whose LEDs or meters actually changed get redrawn, and cables
outside the redrawn area are skipped entirely. Big patches load and render dramatically
faster, and idle CPU usage drops to near zero.

---

*Grab 0.8.1 from the downloads below. Back up your patch libraries before upgrading, and keep
each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation exclusions.*

Thanks for supporting Animatek NME.
