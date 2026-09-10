# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project

Nomad2026 reimplements the Nomad editor for the Clavia Nord Modular G1 synthesizer, porting from Java (Swing/JPF) to JUCE/C++17. The original editor requires JDK 8 and is incompatible with Java 9+ due to JPF 1.5.1's classloader design.

## Build Commands

```bash
# Configure (from project root, first time or after CMakeLists.txt changes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/Nomad2026_artefacts/Debug/Nomad2026

# Run original Java editor (for reference/comparison)
cd nomad-0-3_2 && ../jdk8u482-b08/bin/java -jar nomad.jar
```

No test framework is set up yet. No linter is configured.

## Architecture

The JUCE app is minimal scaffolding so far:
- `source/Main.cpp` — `NomadApplication` (JUCEApplication) and `MainWindow` (DocumentWindow)
- `source/MainComponent.cpp` — Root UI component (currently just a splash)
- `CMakeLists.txt` — JUCE is included as a subdirectory, app links against juce_core, juce_gui_basics, juce_gui_extra, juce_audio_basics, juce_audio_devices

JUCE lives at `JUCE/` as a local copy (not a submodule).

## Reference Material

All reverse-engineering docs live in `docs/RESEARCH.md` — protocol spec, patch format, module system, PDL2 grammar, and architecture of the original editor. This is the primary reference for implementation work.

### Key data sources (gitignored, present locally)
| Path | Content |
|------|---------|
| `nmedit/libs/codecs/midi.pdl2` | Binary spec for MIDI SysEx protocol |
| `nmedit/libs/codecs/patch.pdl2` | Binary spec for .pch patch files |
| `nmedit/libs/nordmodular/data/module-descriptions/modules.xml` | All 110+ module definitions (params, connectors, signals) |
| `nmedit/libs/nordmodular/data/module-descriptions/transformations.xml` | Module-to-UI component mappings |
| `nmedit/libs/jnmprotocol2/src/` | Java protocol messages (reference implementation) |
| `nmedit/libs/jpatch/src/` | Java patch data model |
| `nmedit/libs/libnmprotocol/` | C/C++ protocol implementation |

### Protocol essentials
- SysEx envelope: `F0 33 06 [cc:5][slot:2] [payload] F7`
- All data 7-bit encoded, checksums = `sum % 128`
- 3-second request/response timeout
- Key commands: IAm(0x00), Parameter(0x13), NMInfo(0x14), ACK(0x16), PatchHandling(0x17)

## Environment Notes

- Filesystem is NTFS at `/mnt/SPEED` — shell scripts need `bash -c` wrappers, no exec bit support
- The Write tool may produce CRLF line endings on this filesystem — verify with `xxd` when binary-sensitive
- Fish shell escapes `!` in shebangs — use python or heredocs when generating shell scripts
- `nmedit/`, `nomad-0-3_2/`, and `jdk8u482-b08/` are gitignored (local reference only)

## Slash Commands

Use `/nomad` with subcommands for quick reference:
- `/nomad protocol` — SysEx protocol summary
- `/nomad modules [name]` — Browse/search module definitions
- `/nomad patch` — Patch file format reference
- `/nomad run` — Launch original editor
- `/nomad source <keyword>` — Search original Java/C++ source
- `/nomad arch` — Architecture overview
- `/nomad status` — Project roadmap progress

## The shared CODE changelog

On top of this repo's `CHANGELOG.md`, **every meaningful change also gets an entry in
`/mnt/SPEED/CODE/CHANGELOG.md`**: the shared log across the seven projects, and what the panel's
`Cambios` page shows. Without that line the change does not exist outside this repo — which is
exactly what happened until 2026-09-10, when the panel only read the `Animatek.net` changelog.

That entry is the summary: what changed, the real verification, the agent, and the commit or the
path to this repo's changelog. Technical detail stays here and is not duplicated.

- The path is a **symlink** to the canonical Obsidian note `00 - Sistema/CHANGELOG - CODE.md`.
  Resolve it and edit the target; never replace it with a separate file or start a second copy.
  If it is unavailable, report the blocker instead of inventing another location.
- Write under the local date (Europe/Madrid), newest first, one section per project. Re-read the
  current dated block first and patch only your own entry: several agents write there.
- Uncommitted work is marked explicitly as `cambio local, sin commit`.
- The panel picks it up in the next morning's ingest. To see it now:
  `Animatek.net/panel/ingesta/actualizar.sh`.
- **Do not read the whole thing just to write in it.** It grows ~13 KB a day and is split by
  month: the current month in the note, closed months in `CHANGELOG - CODE/AAAA-MM.md` next to
  it. Query it with `cambios` — `cambios buscar "morph"`, `cambios de NME`, `cambios ver
  2026-09-10` — which asks the database and returns the entry, not the file.
