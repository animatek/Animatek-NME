# Animatek NME Status

Current version: **0.7.0** (released 2026-06-20)

The project was renamed from **Nomad2026** to **Animatek NME — Nord Modular Editor G1** in 0.6.0.

This file tracks the current project state at a practical level. Detailed version history lives in
[CHANGELOG.md](CHANGELOG.md), and remaining work lives in [ROADMAP.md](ROADMAP.md).

## Working Now

- Native JUCE/C++ application builds and runs as a desktop editor.
- MIDI SysEx connection, handshake, patch request/load flow, patch upload, and live editor/synth sync are implemented.
- `.pch` file load/save is implemented with compatibility fixes for original editors.
- Four slot workflow is implemented for A/B/C/D with separate patch state, undo managers, synchronizers, and active hardware slot switching.
- Patch canvas editing is functional: add, delete, move, multi-select, copy/paste, duplicate, cable create/delete, QuickAdd, and context menus.
- Parameter edits, morph assignments/ranges, hardware knob assignments, and MIDI CC assignments sync to the synth.
- Patch Settings and Synth Settings dialogs are implemented and synced.
- Synth patch browser is implemented with search, hide-empty, refresh, load, copy, move, delete, and store flows.
- Bank transfers are implemented and verified against hardware: Save Bank to Disk, Send Bank to Synth, and Backup All Banks into the preset library's `Banks/` mirror folders, with progress, failure reporting, and cancellation.
- Disk preset browser is implemented with configurable preset library root, `Patches/` and `Snippets/` folders, recursive `.pch` scan, search, filters, patch load, and snippet import/drag.
- Snippet import/export works as standard `.pch` files with incremental sync and undo/redo.
- Module rendering has gone through systematic passes for oscillator/LFO, envelope, filter/EQ/vocoder, sequencer, logic, audio, mixer, control, and in/out modules.
- Module help, Help Contents, About links, bug-report link, beta warning popup, themes, canvas zoom, parameter randomization, initialization, locks, and snapshots are implemented.
- Dark-mode app icon and JUCE metadata are in place.
- Knob Floater (interactive 18-knob + pedal/switch/aftertouch overview) and Keyboard Floater (virtual keyboard with drone/repeat modes) are implemented as View-menu floating windows with position persistence.
- Thirteen rotating color themes (`Ctrl+T`) restyle the app chrome and patch canvas, with
  Nord as the default. Wireframe module mode (`Ctrl+W`) works with every theme; both choices
  persist across sessions.
- Patch Variations: the 8 header snapshots persist in a `.var` sidecar (the `.pch` stays standard), with copy/init, morph capture, and write-through of live edits.
- Patch Mutator (G2-style interactive evolution): Mother/Children/Father breeding with Mutate/Randomize/Interpolate/Cross, Quick Locks, temporary storage, variations row, per-module mutation exclusion, and timed audition morphing. Mutation uses a Gaussian distribution, oscillator pitch snaps to musical intervals, Cross has sequential/independent modes, and Output modules are excluded automatically (Sims 1991 / Dahlstedt 2004).
- SysEx Monitor floater: live TX/RX hex log of MIDI traffic for protocol debugging in console-less release builds, with zero overhead when closed.
- Parameter changes (Mutator/Randomize) stream through a single coalescing queue with a user-selectable send speed (Editor Options), preventing the synth from being overrun on large patches.
- Floaters toggle via Ctrl+5..9 (Knob, Keyboard, Patch Notes, Mutator, SysEx Monitor) and behave as normal windows.

## Known Development Context

- The app is still beta-quality. Bugs should be tracked as GitHub issues with reproduction steps.
- Experimental VST3/CLAP plugin targets exist, but the desktop app remains the main supported path.
- Linux MIDI depends on local JUCE patches for modern UMP-aware kernels and legacy MIDI devices.
- `MODULE_CHECKLIST.md` remains the detailed source of truth for per-module visual/behavior review.

## Recent Milestones

- **0.7.0**: Patch Mutator, persistent variations, SysEx Monitor,
  configurable/coalesced parameter delivery, 13 themes, wireframe mode, and hardened
  slot/patch transitions that prevent queued Mutator changes from reaching the wrong patch.
- **0.5.6**: integrated disk preset browser, configurable local preset library, contextual module help improvements, dark-mode icon, VST3/plugin stability fixes, and patch upload/serialization reliability fixes.
- **0.5.5**: synth patch browser reliability, bank operations fixes, snippet import/export hardening, and Device menu cleanup.
- **0.5.2**: module display refinements, LFO/Phaser fixes, Sample&Hold connector color correction, radio-button interaction improvements, sequencer random actions, and KeyQuantizer scale presets.

## Tracking Rule

- Use [ROADMAP.md](ROADMAP.md) for planned implementation work.
- Use [CHANGELOG.md](CHANGELOG.md) for released changes.
- Use GitHub Issues for bugs, regressions, and focused tasks.
- Keep this file as a readable snapshot of where the project stands.
