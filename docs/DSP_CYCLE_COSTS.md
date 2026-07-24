# DSP cycle costs — measuring the real per-module load (issue #18)

## The problem

ANME's Load meters (PVA / E) are a **client-side estimate**: for each module in a
voice area we sum a per-module `cycles` value from `modules.xml`, and that sum *is*
the percentage (0–100 scale, no voice-count scaling — the real editor doesn't scale
either). The synth does **not** report its true DSP load over the protocol, so an
estimate is all any editor can show offline.

The `cycles` values in `modules.xml` come from the nmedit project. They are slightly
**too high** compared to the real Clavia editor's firmware accounting — every
nmedit-derived editor (Nomad 0.3, 0.4, ANME) inherits the same numbers, so they all
over-read by the same ~1–2 %.

### Evidence (measured on hardware against the original Clavia editor)

| Patch | Original editor | ANME / nmedit sum | Excess |
|-------|-----------------|-------------------|--------|
| MorgBass02      | 99.5 % | 100.5 % | **+1.0** |
| SY-1 RndBlips1  | 97.5 % | 99.67 % | **+2.17** |

The excess is not a constant, so it is a **per-module** data error, not a formula bug.
ANME sums the published values faithfully (verified: an independent offline sum of the
`.pch` gives exactly what ANME displays).

**Note:** the error is in the *safe* direction — the estimate never *under*-reports a
saturated patch, so a genuinely-100 % patch still reads ≥ 100 %.

### A lead

The mono output module **1Output** (type 5) carries `cycles = 1.28125`, which looks too
high for an output. Subtracting 1.0 per 1Output instance reproduces the original almost
exactly: MorgBass (1×) → 99.5 (exact), SY-1 (2×) → 97.67 (≈ 97.5). Not confirmed for the
other output/module types — that's what the measurement effort below is for.

## The fix: measure each module, one by one

The only way to get correct values is to measure each module in isolation on the real
Clavia editor connected to hardware, and record what it costs. This is exactly what the
original authors must have done — with some errors, as shown above.

### Live crowd-sourced sheet

The measurements are collected in a shared Google Sheet:

- **View (public, read-only):**
  https://docs.google.com/spreadsheets/d/e/2PACX-1vTjvaa-Qjq1bWuZSUrX-HP3NvBzcRAUlAhCXcSXI1idAwTskEFAaVJOa0jep8dbq6wNpTIVW3YZNsii/pubhtml
- **Contribute (edit):**
  https://docs.google.com/spreadsheets/d/13Kvlt3cO2nU5--zyeup1RH1EA2hiJYC2vtDYH5bbK2w/edit?usp=sharing

The Google Sheet is the **live** copy (where contributors add values). `docs/module-resource-costs.csv`
in this repo is the seed and the version-controlled snapshot — periodically re-export the
sheet back onto it so the correction is tracked in git before it lands in `modules.xml`.

### Columns

| Column | Meaning |
|--------|---------|
| `type` | module type id (= `index` in `modules.xml`, = 2nd column of a `.pch` `[ModuleDump]` line) |
| `name` / `fullname` / `category` | from `modules.xml` |
| `nomad_cycles` | the current (nmedit) value ANME uses today — reference, to compare against |
| `measured_cycles` | the real cost you read from the original editor (fill this in) |
| `measured_by` | who measured it (for a public sheet) |
| `notes` | anything odd (e.g. cost depends on a mode/parameter) |

### How to measure one module

1. Original Clavia editor, empty patch, connected to a Nord (or in the editor's own
   estimate if it matches the synth).
2. Note the PVA/E reading (should be ~0).
3. Add **one** instance of the module to the poly voice area.
4. The increase in PVA is that module's cost → `measured_cycles`.
5. Remove it; repeat for the next module. Do a couple of modules twice to confirm the
   cost is per-instance and constant.

Fill in as many as you can each session. Once enough rows are trustworthy, we replace
the `cycles` values in `modules.xml` (or add an override table) and the meters match the
original.

## Status

Issue #18 is **not** an ANME code bug — it is inherited data inaccuracy, over-reading in
the safe direction. Closed as such; the long-term correction is this measurement sheet,
filled in gradually.
