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

Opening a patch asks which slot it should go to, or whether to load it **Local**
(editor only, nothing sent to the synth). See
[Working with the Synth](04-working-with-the-synth.md#opening-a-patch-choosing-where-it-goes).

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
