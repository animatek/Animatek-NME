# Animatek NME 0.10.0 — Nord Classic, the MCP Bridge, and a Theme Overhaul

The biggest visual pass since the editor got themes at all, plus a genuinely new way to
work: the editor can now be driven by an AI assistant, if you want it to be.

## What's new

### 🎛️ "Nord Classic" — the original editor's look

A new light, warm-grey theme that echoes the Clavia Nord Modular editor: flat grey module
bodies over a darker lavender-grey canvas, black labels, grey knobs, indigo LCD-style value
readouts, and the classic signal-cable colours — all sampled from the original.

A subtle procedural grain now sits over the patch canvas on **every** theme, giving it a
soft paper feel instead of a flat fill. It's most noticeable on Nord Classic.

Two housekeeping notes: the old "Classic" theme (which used Nomad's own colours, not
Clavia's) is now called **"Nomad"**, and the little-used "Frost" theme has been removed.

### 🤖 MCP bridge — drive the editor from an AI assistant

The standalone editor can expose a local control channel that lets an MCP client (Claude
Code, Claude Desktop, OpenCode…) work the patch as if it were you at the canvas: adding and
moving modules, connecting and cutting cables, setting parameters, creating patches and
opening them from your preset library. Everything goes through the normal undo system, so
an assistant's edits are reviewable and undoable exactly like your own.

**It is off by default** — no port is opened unless you ask for it. Turn it on under
*Editor Options → MCP Bridge*, which also shows whether the bridge is listening and gives
you the exact command to register it with your client. It binds to `127.0.0.1` only, is off
the wire entirely when the toggle is off, and can be compiled out with
`-DNME_MCP_BRIDGE=OFF`. Standalone app only — deliberately not in the plugin, where several
instances would fight over the same port.

### 🖥️ Light themes are actually usable now

A lot of the chrome had hard-coded light greys that were invisible on a light background.
Header, status bar, slot list, inspector rows, module browser, the empty-canvas hint and
the settings dialogs all follow the palette now. The menu bar repaints when you switch
themes instead of keeping the first theme's colours, and the Theme submenu's checkmark
finally follows the theme actually in use.

Buttons, pressed labels, menu highlights, combo arrows and toggles across every dialog
dropped their unrelated green/orange/red accents for neutral palette roles, so Patch
Settings, Synth Settings, Editor Options, MIDI Settings, Store to Bank and the bank
transfer dialogs all look like one application. Editor Options also picked up the standard
`Ctrl+,` shortcut.

### 🎚️ Hardware knob-assignment map in the Inspector

A compact four-panel, 18-LED diagram mirroring the physical Nord Modular knob layout sits
beside the Assignments heading. Assigned knobs glow a bright lamp green; free ones keep the
hardware's dark unlit-lens tint. It updates immediately after edits and undo/redo, and the
Knob Floater's lamps match.

## Fixes

- **Editing one slot no longer waits on another slot's transfer.** An upload used to hold
  back parameter edits for every slot, and a fetch discarded them outright — so with a slot
  window open, turning a knob in slot A while B was transferring did nothing. Only the slot
  actually transferring is held now. A related fix stops a background slot's acknowledgement
  from overwriting the focused slot's patch identifier. Verified on hardware, including a
  patch at 100% DSP load with two slot windows open.
- **DSP load reads to one decimal** (e.g. `47.6%`), matching the original editor. The
  previous whole-number rounding could nudge a 99.5% patch up to a misleading `100%`.
- **Heavy editing sessions no longer run out of module slots.** New modules took "highest
  index + 1", so enough add/delete cycles walked that seven-bit counter past 127 and started
  producing patches the synth rejected. Lowest free index is reused now.
- **Sequencer step numbers line up** with the LEDs below them again.
- **Reversed vertical selectors show the right labels** — on blocks drawn bottom-to-top
  (LFOA/LFOB among others), the displayed label could belong to a different setting than the
  one actually selected.
- **Quick Add opens on the monitor your mouse is on**, not always the primary display.
- **Disk browser: a new PCH2 toggle** hides legacy 2.10 patches from the list.

---

*Grab 0.10.0 from the downloads below. Back up your patch libraries before upgrading, and
keep each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation
exclusions.*

Thanks for supporting Animatek NME.
