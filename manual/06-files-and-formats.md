# 6. Files & Formats

## `.pch` patch files

Patches are standard Nord Modular **3.0 text format** `.pch` files, compatible
with the original Clavia editor and Nomad/nmedit. Anything you save in Animatek
NME loads in the originals and vice versa.

- **Legacy 2.10 patches** (the older `[Module N]` format) load transparently,
  including daisy-chained cables and correct 1/2 output routing. They are tagged
  **PCH2** in the preset browser, and the browser's **PCH2** filter toggle hides
  them when you only want current patches. Saving rewrites them in 3.0 format.
- **Patch notes** are stored in a `[Notes]` section, a Nomad/nmedit extension
  that original editors ignore harmlessly.
- **Comments** (the text notes on the canvas) are stored the same way, in a
  `[Comments]` section: one line per note with its area, grid position, size and
  text. The size is the row count on its own, or `rows x columns` once the note
  is more than one column wide. They travel with the patch when you share it, and
  an editor that does not know the section skips it.

Opening a patch asks which slot it should go to, or whether to load it **Local**
(editor only, nothing sent to the synth). See
[Working with the Synth](04-working-with-the-synth.md#opening-a-patch-choosing-where-it-goes).

## The extras library

The G1 stores modules, cables, values and names. Comments, patch notes, the
eight variations and the Mutator's exclusions are the editor's own, and there is
no room for any of them on the synth: a patch loaded from the front panel used to
arrive stripped of all of it.

The editor therefore keeps its own copy of the extras of every patch it has seen,
one small file per patch, in `extras/` beside the settings. You never have to
think about it. Load PercDetect from the synth and its comments, notes,
variations and exclusions come back on their own.

A patch is recognised by two routes:

- **By its id.** Patches saved by this editor carry one in an `[NME]` section of
  the `.pch`, so opening the file says outright which extras are its own. The id
  travels inside the file, so a patch you send to somebody else keeps its
  identity on their machine too.
- **By its fingerprint**, which is what a patch coming off the wire has to be
  recognised by. The fingerprint covers the patch name, its modules and its
  cables, and deliberately not the parameter values, so turning a knob never
  costs a patch its notes. As the patch grows the fingerprint changes, and the
  editor keeps the previous ones so the link survives editing.

The one thing it cannot tell apart is two brand new patches, which look exactly
alike. A patch you create is therefore always given an entry of its own and is
never matched against anything.

Opening a `.pch` from disk lets the file win: the comments in it are the
comments you get, and the library is brought into line with it afterwards.

## `.var` variations sidecar

The 8 per-slot patch variations (and per-module mutation exclusions) live in a
`.var` file next to the patch: `MyPatch.pch` + `MyPatch.var`. This keeps the
`.pch` byte-standard. **Keep the two files together when moving or backing up
patches**: without its sidecar a patch loads fine but loses its variations.

## `.pchp` module presets

A module preset is a named parameter snapshot of one module type, recalled from
the **Presets** section of the Inspector or the module's right-click menu (see
[Editing Patches](03-editing-patches.md#module-presets)).

Presets are stored as one `.pchp` pack per module type in the library's
`Presets/` folder. The format is plain text and meant to be edited by hand, since
transcribing the original editor's own presets is done by hand. Values are keyed
by parameter name rather than by position, so a preset that names two parameters
sets those two and leaves the rest of the module alone. Presets saved by versions
before 0.12 are migrated automatically on first run.

## Snippets

A snippet is a reusable group of modules with their cables and parameter
values, saved from a selection and imported by drag & drop. Snippets are plain
`.pch` files stored in the library's `Snippets/` folder, so they work in any
editor and you can share them like patches. Modules that can't be duplicated
(singletons like Keyboard) are filtered automatically on export.

## The preset library

The disk browser scans a configurable **preset library** folder recursively:

```
<library root>/
  Patches/    your saved patches (any folder structure you like)
  Snippets/   exported snippets
  Presets/    one .pchp pack of module presets per module type
  Banks/      Bank1 … Bank9 mirror folders from "Backup All Banks"
```

Search covers filenames; filters narrow to patches, snippets or bank backups.
Bank backups load like any other patch.

## Bank folders

**Save Bank to Disk** writes patches as `NN - Name.pch`; the `NN` prefix
records the bank position, and **Send Bank to Synth** uses it to restore
patches to their exact slots.
