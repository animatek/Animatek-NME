# Animatek NME — Nord Modular Editor G1

Animatek NME (formerly Nomad2026) is a modern native editor for the **Clavia Nord Modular G1**
synthesizer. It is a JUCE/C++ reimplementation inspired by the original Java Nomad editor, built
to run on current macOS, Windows, and Linux systems without requiring an old Java runtime.

## Status and how to get it

**Animatek NME is in active beta** (current version 0.17.0, see
[CHANGELOG.md](CHANGELOG.md)). There are two ways to get it, and both give you the
same application:

- **Ready-to-run builds** for macOS, Windows, and Linux are available to patrons at
  [patreon.com/animatek](https://www.patreon.com/c/animatek). Patreon pays for the
  hardware, the testing, and the per-platform packaging.
- **Building it yourself is free and fully supported.** Everything needed is in this
  repository: no missing pieces, no disabled features, no separate "pro" branch. See
  [Building](#building) below.

That is why the repo has no GitHub Releases: binaries are distributed through Patreon,
while the code stays GPLv3 for anyone who wants to compile it, fork it, or read it.

> Nord Modular is a trademark of Clavia DMI AB. This project is an independent,
> community-developed editor and is not affiliated with or endorsed by Clavia.

![Animatek NME editor](https://animatek.net/wp-content/uploads/2026/06/Editor_animatek.png)

## What It Does

Animatek NME lets you edit Nord Modular G1 patches from a modern desktop application:

- Connect to the synth over MIDI SysEx, auto-detect ports, and keep editor/synth state in sync.
- Load, edit, save, and store `.pch` patches.
- Work with all four hardware slots, each with independent patch state and undo history.
- Build patches visually with modules, cables, parameters, morphs, hardware knob assignments, and MIDI CC mappings.
- Browse synth memory and local disk presets from the integrated right-side browser.
- Save and import snippets as reusable `.pch` module groups.
- Transfer whole banks: save a synth bank to a folder, send a folder of patches to a bank,
  or mirror-backup all 9 banks into the preset library in one action.
- Breed new sounds interactively with the **Patch Mutator** (mutate / randomize / interpolate
  / cross), and store up to 8 patch variations per slot.
- Watch MIDI traffic live with the **SysEx Monitor** for protocol debugging.
- Use contextual module help based on the original Nord Modular Editor documentation.

## Main Features

- Native JUCE desktop application.
- Pixel-oriented module canvas with Poly/Common areas.
- Module browser, QuickAdd, drag and drop, copy/paste, duplicate, multi-selection, and undo/redo.
- Real-time parameter, cable, module, morph, knob, and MIDI controller synchronization.
- Patch settings and synth settings dialogs.
- Synth patch browser with bank/slot operations.
- Bank transfer tools (Device menu): Save Bank to Disk, Send Bank to Synth with overwrite
  warning and clean stop on failure, and Backup All Banks to the preset library
  (`Banks/Bank1`-`Bank9` mirror folders), all with progress and cancellation.
- Disk preset browser with configurable preset library folder, recursive `.pch` scanning, search, and patch/snippet/bank filters. Bank backups load like any patch.
- Randomize, initialize, parameter locks, snapshots, cable visibility tools, canvas zoom, and module help.
- **Patch Mutator** (G2-style interactive sound breeder) with Gaussian mutation, harmonic
  oscillator mutation, crossover, interpolation, Quick Locks, and 8 persistent patch variations.
- **SysEx Monitor** floater for live TX/RX MIDI logging (works in console-less release builds).
- Configurable synth **send speed** (parameter throughput) to balance responsiveness vs. reliability.
- 13 color themes (cycled with `Ctrl+T`), with Nord as the default, plus a persistent
  wireframe module mode (`Ctrl+W`).
- Experimental VST3/CLAP plugin targets.

## Documentation

- [manual/](manual/README.md) - the user manual: installation, interface tour, patch
  editing, working with the synth, tools, file formats, shortcuts, and troubleshooting.
- [CHANGELOG.md](CHANGELOG.md) - version history.
- [docs/](docs/) - project documentation: [status](docs/STATUS.md),
  [roadmap](docs/ROADMAP.md), [release notes](docs/releases/),
  [module checklist](docs/MODULE_CHECKLIST.md),
  [protocol/format research](docs/RESEARCH.md),
  [release checklist](docs/RELEASE_CHECKLIST.md), and
  [plugin architecture notes](docs/PLUGIN_ARCHITECTURE.md).

Issues are the preferred place to track bugs and concrete follow-up work:
https://github.com/animatek/Animatek-NME/issues

## Building

You need CMake 3.22 or newer, a C++17 compiler, and Git. JUCE comes with the repository
as a submodule, so there is nothing else to download by hand.

On Debian/Ubuntu, the development packages JUCE needs are:

```bash
sudo apt-get install -y build-essential cmake git \
  libasound2-dev libjack-jackd2-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libfreetype6-dev libfontconfig1-dev
```

On macOS, Xcode command line tools and CMake. On Windows, Visual Studio 2022 with the
C++ desktop workload.

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/animatek/Animatek-NME.git
```

If the repo is already cloned:

```bash
git submodule update --init --recursive
```

Configure and build. Use `Release` for everyday use and `Debug` when developing:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run on Linux/Windows:

```bash
./build/AnimatekNME_artefacts/Release/AnimatekNME
```

Run on macOS:

```bash
build/AnimatekNME_artefacts/Release/AnimatekNME.app/Contents/MacOS/AnimatekNME
```

macOS universal binary:

```bash
cmake -B build-universal -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal -j$(sysctl -n hw.logicalcpu)
```

The macOS build targets macOS 10.13 High Sierra or newer by default. It contains
both Intel and Apple Silicon code when configured with the universal command
above.

Windows release build:

```bash
cmake -B build-win-release -G "Visual Studio 17 2022" -A x64
cmake --build build-win-release --config Release
```

If the build fails or the app cannot see your MIDI interface, open an
[issue](https://github.com/animatek/Animatek-NME/issues): building from source is meant to
work, and a broken build is a bug.

## Experimental Plugin Build

The editor can also be built as VST3/CLAP plugin targets. This path is experimental.

```bash
cmake --build build --target AnimatekNMEPlugin_VST3 AnimatekNMEPlugin_CLAP -j$(nproc)
```

Install locally:

```bash
cp -r "build/AnimatekNMEPlugin_artefacts/Debug/VST3/Animatek NME.vst3" ~/.vst3/
cp "build/AnimatekNMEPlugin_artefacts/Debug/CLAP/Animatek NME.clap" ~/.clap/
```

Plugin builds require the `clap-juce-extensions` submodule.

## Linux MIDI Note

The local JUCE copy includes patches to `JUCE/modules/juce_audio_devices/native/juce_Midi_linux.cpp`
for modern Linux kernels with UMP MIDI support:

1. Synchronous endpoint cache in the ALSA client constructor.
2. Legacy bytestream send path for non-UMP MIDI ports.

These patches are required on Linux systems where unpatched JUCE reports no MIDI devices or sends
UMP packets to legacy MIDI interfaces.

## Credits

This project is a reimplementation based on the work of the original nmedit/Nomad developers:

| Person | Contribution |
|--------|--------------|
| Marcus Andersson | Reverse-engineered the Nord Modular MIDI protocol; C++ and Java protocol libraries |
| Christian Schneider | Nomad Java editor v0.2/v0.3 |
| Ian Hoogeboom | Nomad v0.4 update and macOS compatibility |
| Jan Punter | Nord Modular patch file format documentation |
| Jelle Herold | Original project founder |
| Stefan Keel | Module SVG icon designs |
| Tobias Weinald | Splash screen artwork |

## Original Project

- Website: https://nmedit.sourceforge.net/
- Source v0.3: https://github.com/wesen/nmedit
- Source v0.4: https://github.com/Airell/nmedit

## License

This project is licensed under the [GNU General Public License v3](LICENSE), upgraded from v2
for JUCE AGPLv3 compatibility.
