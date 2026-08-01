# Animatek NME 0.11.0: Slot windows come alive, safer uploads, more MCP reach

A round of multi-window polish and hardware-safety fixes, plus more control for MCP
clients driving the editor.

## What's new

### 🪟 Slot windows now track the synth live and take Randomize/Save

A popped-out slot window used to be edit-only: turning a physical knob (or watching a
light or meter move) on the front panel animated the main window's canvas but never the
slot window's, even when it was showing the very slot you were touching. Now the live
fan-out from the synth (parameter values, morph knobs, lights and meters) reaches the
focused slot's own window too, so it animates just like the main one.

Two global commands also work per-window now: **Ctrl+R / Ctrl+Shift+R** (Randomize,
simple/Gaussian) and **Ctrl+S / Ctrl+Shift+S** (Save / Save As) act on that window's slot,
honouring its own module selection. The top settings bar (macros, CPU and voice meters)
stays on the main window only, matching the original Nord Modular editor.

### 📂 Slot chooser when opening a patch, with a "Local" option

Opening a `.pch` used to silently target the active slot and, when connected, always
upload, so there was no way to just look at a patch without overwriting the synth's
current slot, and no way to load into a slot you hadn't visited yet.

Opening a patch (File > Open and both preset browsers) now shows a chooser listing A/B/C/D
with the patch currently in each slot, defaulting to the active one, plus a **Local**
option that loads into the editor only, without uploading. Slots whose editor patch isn't
known to match the synth (loaded Local, or loaded/built while disconnected) carry a
**LOCAL** badge in the slot bar; it clears once the patch is uploaded to, or fetched from,
the synth.

### 🤖 The MCP bridge can now persist and rename

Building a patch through the bridge used to leave it stranded in memory. Three new tools
close the loop:

- **`save_patch`** writes a slot's patch (and its `.var` sidecar) to a `.pch` on disk.
- **`store_to_bank`** (bank 1–9, position 1–99) uploads a slot's patch and writes it to the
  synth's internal memory once the upload is acknowledged.
- **`rename_module`** renames a module as an undoable operation. Modules could be created
  with a name but never renamed from the bridge before.

The browsing tools are also far lighter for clients that aren't sitting on the source tree:
`describe_module_type` fetches a single type, `list_module_types` is compact with a category
filter, and `list_modules` reports each module's connectors and takes a `container_index`
filter. A new `mutate_patch` runs the editor's own Mutator as one undoable, throttled step.

## Fixes

- **Rapid Voices changes no longer corrupt the synth slot (issue #28).** Each voice change
  re-uploads the whole patch, and pressing the arrows quickly fired overlapping uploads that
  interleaved on the wire, leaving the slot corrupt (it read back as a patch named "Error"
  with no modules). Voice-change uploads are now debounced and coalesced, and a new upload
  never starts while one is still in flight. Verified on hardware.
- **The front-panel Voices arrows actually reach the synth now (issue #25).** They updated
  the on-screen number but sent nothing, so the voice count never changed unless you went
  through Ctrl+P Patch Settings.
- **Renaming a module is undoable (issue #23).** From the canvas menu, the Inspector, or a
  slot window. It previously changed the title outside the undo system, so Ctrl+Z couldn't
  take it back.
- **The Windows build is fixed (issue #24).** An MSVC-only parse error in the Editor Options
  dialog's revert timer broke the Windows build while Linux and macOS were fine.
- **The Patch Mutator no longer wrecks pitch and cutoff.** Mutate and Randomize slammed
  oscillator frequency, slave detune, filter cutoff and some LFO rates to almost nothing,
  because those parameters were read against the wrong internal range. Present since the
  Mutator shipped in 0.7.0; Interpolate and Cross were unaffected.

---

*Grab 0.11.0 from the downloads below. Back up your patch libraries before upgrading, and
keep each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation
exclusions.*

Thanks for supporting Animatek NME.
