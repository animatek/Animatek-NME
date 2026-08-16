# Animatek NME 0.16.0: the one that went inwards

Most of what changed in this release is not something you can point at on screen, which is an
odd thing to say about a release. Big patches draw far faster, a family of crashes cannot come
back, and the project has tests and a build robot for the first time. Nine issues are closed,
and five more faults were found by a test pass written specifically to go looking for them.

## ⚡ Big patches draw far faster

Every time the canvas painted a module it worked out, from scratch and twice over, where every
module in the patch keeps its LEDs and meters. That is a table with a sort and a lookup per
module, rebuilt once per module, per redraw. On a hundred-module patch it came to roughly
**18 milliseconds of pure bookkeeping on every single redraw** — and while the meters are moving
the synth asks for redraws several times a second.

The table is now worked out once and reused until the patch's structure actually changes, which
takes that 18 ms down to about **14 microseconds**. Cables no longer rebuild a lookup of every
connector in the patch on each redraw either, and resting the pointer on a knob stopped
redrawing the whole canvas on every mouse movement for the sake of the two little nudge arrows.

The difference is widest exactly where it was worst: a large patch with the meters running.

## 🧠 The editor stops holding on to modules it has deleted

Selecting a module, hovering one, pointing at a knob or reading a DSP cost all left the editor
holding the module itself in memory. Deleting it, or undoing the add that made it, left every
one of those pointing at nothing. That is the crash macOS users saw in 0.15.0 and Linux users
never did (#61), and 0.15.1 kept it at bay by sweeping all of them before each redraw.

Modules are now identified by **where they live in the patch** rather than by where they sit in
memory, so the question cannot be got wrong and the sweep is gone. The whole family of faults
behind that crash is closed off rather than patched over.

It behaves better as well: deleting a module and undoing it now gives you the selection back,
where before you had to find the module again and re-pick it.

## ✅ Tests, and a robot that runs them

The project had no tests. It has 41 now, covering the layers where a quiet regression costs the
most: the patch codec (a patch is serialized, parsed back and serialized again, and the bytes
must match), the SysEx envelope, the upload packetizer, and module placement. They run in
hundredths of a second, need no synth and no screen, and **GitHub builds the app and runs them on
every push**, once normally and once under the sanitizers.

It earned its place on the first day: the sanitizer job caught undefined behaviour in code
written that same afternoon, which the ordinary build compiled and every test passed over.

## 🎛️ The Inspector lists every parameter, and edits them

Selecting a module fills a **Parameters** section at the top of the Inspector with every control
it has, each as its name and the figure it reads in its own units. Drag a value to walk it, or
**double-click and type one in**: `440Hz`, `C#3` and `-12(Oct)` all land where you mean them to,
and a bare number picks the nearest step. Whatever the module wears as a button is a button here
too, carrying the same lettering its face does.

Its sections fold away and stay folded, so a module with thirty parameters and no assignments
does not push its presets off the bottom.

## ↕️ Nudge arrows, and `+` / `-`

A knob packs its whole range into a few pixels, so landing on an exact frequency by dragging one
is guesswork. Resting the pointer on a knob or slider now pops the same two little buttons the
original editor shows: one step down, one step up, and holding either repeats. `+` and `-` do
the same to whatever the pointer is over (#66), and the whole run is a single undo step. The
morph dials in the header bar have them too.

## 📝 Comments on the patch

A text note you can drop into any empty space, from the module bar's ANME group. It holds its
rectangle of the grid like a module does, so modules make room for it and it makes room for
them, and it never reaches the synth. Notes are part of a selection like anything else: they
copy, paste, move and delete with the modules around them.

## 💾 Where a patch came from, and where it goes back

A store button sits next to the patch name showing the bank location the patch on screen came
from, so putting it back is one click. The Store Patch to Bank dialog opens on the patch's own
location instead of bank 1, Save Patch As opens with the filename already typed in, and opening
a patch can stop asking which slot every time (#59, in Editor Options).

A patch loaded from the synth also gets its comments, notes, variations and Mutator exclusions
back: the synth has nowhere to keep them, so the editor recognises the patch and hands them over.

## ⌨️ Shortcuts that do not collide

The DSP cost overlay moved to `F3` and focus mode to `F4` (#55): F10 belongs to the menu bar on
Windows and some Linux desktops, and macOS keeps F11 for Show Desktop. The old keys still work.
On macOS wireframe moved to `Cmd+Shift+W`, because the naked `Cmd+W` is the system's own close
window and was firing both ways at once. Menu shortcuts now sit right-aligned in their own
column (#56) instead of riding inside the item text, which the macOS menu bar was printing
literally, tab character and all.

## 🩹 Fixed

- **Pressing A, B, C or D on the synth brings that slot up on screen.** The buttons looked dead:
  the editor followed the front panel only into a sub-window that already happened to be open.
- **Nothing can be dragged off the canvas.** A module dragged past the bottom or the last column
  still existed but could not be seen or grabbed again.
- **A module can no longer be buried under another** at the bottom of a column (#54). A drop or
  paste with no room is refused rather than stacking two things on the same rows.
- **Nudging a selection into an edge stops it as a block**, instead of clamping each module
  separately and piling them onto the same row.
- **Select All takes the text notes too.**
- **Undoing a delete gives the selection back.**
- **Closing the editor with the slot chooser open no longer leaks it**, along with the same
  fault in the store-location dialog.
- **Error messages leave when their moment does** (#65), and any status message can be dismissed
  by clicking it.
- **Module help drops the `$Contents` scraper junk** (#57) and its description follows the theme
  instead of staying near-white (#58).
- **A sequencer's Clr button parks every step at its default** (#53): a CtrlSeq fader returns to
  centre, not to zero.
- **The Inspector's section headings share one readable colour** (#68) instead of three chosen
  against a dark panel, which left them too faint on Nord Classic and the light themes.
- **Renaming a module reaches the synth**, so storing to a bank right after a rename no longer
  saves the old name.
- **The Knob Floater follows the morph dials** (#64), and knobs assigned to a morph group are
  listed in the Inspector (#63).

## 📖 Under the hood

`PatchCanvasComponent.cpp` had grown to 9,330 lines and is now six files, one per job, which is
a pure move: sort every line of the old file and of the new ones, diff them, and only the header
comments and includes differ. The upload packetizer moved into its own module so the 166-byte
packet rule and the packet that frees a stuck synth are held there by tests rather than by
memory.

## Known limitations

- Stuck MIDI-IN notes are cleared with the synth's front-panel panic.
- macOS builds are unsigned; Gatekeeper needs **Open Anyway** in System Settings > Privacy &
  Security the first time.

## Thanks

To **Nocticore** for the reports behind several of these, and for the kind of detail that turns a
bug into a diagnosis rather than a hunt.
