# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Animatek NME (Nord Modular Editor G1, formerly Nomad2026) reimplements the Nomad editor for the Clavia Nord Modular G1 synthesizer, porting from Java (Swing/JPF) to JUCE/C++17. The original editor requires JDK 8 and is incompatible with Java 9+ due to JPF 1.5.1's classloader design.

## Build Commands

```bash
# Configure (from project root, first time or after CMakeLists.txt changes)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/AnimatekNME_artefacts/Debug/AnimatekNME

# Run original Java editor (for reference/comparison)
cd nomad-0-3_2 && ../jdk8u482-b08/bin/java -jar nomad.jar
```

```bash
# Run unit tests (pure layers: patch codec, SysEx, packetizer, placement)
cmake --build build --target nme_tests && ctest --test-dir build --output-on-failure

# Before pushing: the same tests under the sanitizers. The plain build says
# nothing about undefined behaviour, and CI runs this job.
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --target nme_tests -j$(nproc)
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-asan --output-on-failure
```

Tests live in `tests/` (doctest, vendored at `libs/doctest/`); CI runs them on
every push (`.github/workflows/ci.yml`), plain and under ASan/UBSan. No linter
is configured.

## Architecture

The JUCE app is minimal scaffolding so far:
- `source/Main.cpp` — `NomadApplication` (JUCEApplication) and `MainWindow` (DocumentWindow)
- `source/MainComponent.cpp` — Root UI component (currently just a splash)
- `CMakeLists.txt` — JUCE is included as a subdirectory, app links against juce_core, juce_gui_basics, juce_gui_extra, juce_audio_basics, juce_audio_devices

JUCE lives at `JUCE/` as a git submodule (juce-framework/JUCE, 8.x). CI checks it out
with `submodules: recursive`; locally run `git submodule update --init` after a fresh clone.
`.github/workflows/build-binaries.yml` (manual `workflow_dispatch`) builds Release
binaries for Linux/Windows/macOS-universal as short-lived artifacts — binaries are
distributed via Patreon, never as public GitHub Releases. Release binaries are normally
built locally (`VERSION=x.y.z bash packaging/build-appimage.sh`) and dropped into
`#Ejecutables/<version>/`.

## Documentation Layout

- `manual/` — user manual (chapters + `07-shortcuts.md`, keep in sync with the in-app shortcuts dialog)
- `docs/` — project docs: STATUS, ROADMAP, MODULE_CHECKLIST, RELEASE_CHECKLIST, PLUGIN_ARCHITECTURE, RESEARCH, MDI_PLAN
- `docs/releases/` — per-version release notes (`RELEASE_NOTES_x.y.z.md`)
- Root keeps only README.md, CHANGELOG.md, CLAUDE.md/AGENTS.md, LICENSE

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
