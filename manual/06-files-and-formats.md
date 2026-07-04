# 6. Files & Formats

## `.pch` — patch files

Patches are standard Nord Modular **3.0 text format** `.pch` files, compatible
with the original Clavia editor and Nomad/nmedit. Anything you save in Animatek
NME loads in the originals and vice versa.

- **Legacy 2.10 patches** (the older `[Module N]` format) load transparently,
  including daisy-chained cables. They are tagged **PCH2** in the preset
  browser. Saving rewrites them in 3.0 format.
- **Patch notes** are stored in a `[Notes]` section — a Nomad/nmedit extension
  that original editors ignore harmlessly.

## `.var` — variations sidecar

The 8 per-slot patch variations (and per-module mutation exclusions) live in a
`.var` file next to the patch: `MyPatch.pch` + `MyPatch.var`. This keeps the
`.pch` byte-standard. **Keep the two files together when moving or backing up
patches** — without its sidecar a patch loads fine but loses its variations.

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
  Banks/      Bank1 … Bank9 mirror folders from "Backup All Banks"
```

Search covers filenames; filters narrow to patches, snippets or bank backups.
Bank backups load like any other patch.

## Bank folders

**Save Bank to Disk** writes patches as `NN - Name.pch` — the `NN` prefix
records the bank position, and **Send Bank to Synth** uses it to restore
patches to their exact slots.
