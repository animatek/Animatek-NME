# Nomad2026

Nomad2026 is a modern native editor for the **Clavia Nord Modular G1** synthesizer.
It is a JUCE/C++ reimplementation inspired by the original Java Nomad editor, built
to run on current macOS, Windows, and Linux systems without requiring an old Java runtime.

## What It Does

Nomad2026 lets you edit Nord Modular G1 patches from a modern desktop application:

- Connect to the synth over MIDI SysEx, auto-detect ports, and keep editor/synth state in sync.
- Load, edit, save, and store `.pch` patches.
- Work with all four hardware slots, each with independent patch state and undo history.
- Build patches visually with modules, cables, parameters, morphs, hardware knob assignments, and MIDI CC mappings.
- Browse synth memory and local disk presets from the integrated right-side browser.
- Save and import snippets as reusable `.pch` module groups.
- Use contextual module help based on the original Nord Modular Editor documentation.

## Main Features

- Native JUCE desktop application.
- Pixel-oriented module canvas with Poly/Common areas.
- Module browser, QuickAdd, drag and drop, copy/paste, duplicate, multi-selection, and undo/redo.
- Real-time parameter, cable, module, morph, knob, and MIDI controller synchronization.
- Patch settings and synth settings dialogs.
- Synth patch browser with bank/slot operations.
- Disk preset browser with configurable preset library folder, recursive `.pch` scanning, search, and patch/snippet filters.
- Randomize, initialize, parameter locks, snapshots, cable visibility tools, canvas zoom, and module help.
- Dark and classic themes.
- Experimental VST3/CLAP plugin targets.

## Documentation

- [STATUS.md](STATUS.md) - current implementation status and what is considered working now.
- [ROADMAP.md](ROADMAP.md) - remaining real implementation work.
- [CHANGELOG.md](CHANGELOG.md) - version history outside the README.
- [MODULE_CHECKLIST.md](MODULE_CHECKLIST.md) - module rendering and behavior verification.
- [RESEARCH.md](RESEARCH.md) - protocol, `.pch` format, PDL2, and original editor research.
- [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md) - parked notes for the experimental plugin direction.

Issues are the preferred place to track bugs and concrete follow-up work:
https://github.com/animatek/Nomad2026/issues

## Building

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/animatek/Nomad2026.git
```

If the repo is already cloned:

```bash
git submodule update --init --recursive
```

Configure and build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

Run on Linux/Windows:

```bash
./build/Nomad2026_artefacts/Debug/Nomad2026
```

Run on macOS:

```bash
build/Nomad2026_artefacts/Debug/Nomad2026.app/Contents/MacOS/Nomad2026
```

macOS universal binary:

```bash
cmake -B build-universal -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal -j$(sysctl -n hw.logicalcpu)
```

Windows release build:

```bash
cmake -B build-win-release -G "Visual Studio 17 2022" -A x64
cmake --build build-win-release --config Release
```

Requires JUCE via the included `JUCE/` submodule and a C++17 compiler.

## Experimental Plugin Build

The editor can also be built as VST3/CLAP plugin targets. This path is experimental.

```bash
cmake --build build --target Nomad2026Plugin_VST3 Nomad2026Plugin_CLAP -j$(nproc)
```

Install locally:

```bash
cp -r build/Nomad2026Plugin_artefacts/Debug/VST3/Nomad2026.vst3 ~/.vst3/
cp build/Nomad2026Plugin_artefacts/Debug/CLAP/Nomad2026.clap ~/.clap/
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
