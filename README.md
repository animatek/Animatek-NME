# Animatek NME — Nord Modular Editor G1

Animatek NME (formerly Nomad2026) is a modern native editor for the **Clavia Nord Modular G1**
synthesizer. It is a JUCE/C++ reimplementation inspired by the original Java Nomad editor, built
to run on current macOS, Windows, and Linux systems without requiring an old Java runtime.

> Nord Modular is a trademark of Clavia DMI AB. This project is an independent,
> community-developed editor and is not affiliated with or endorsed by Clavia.

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
https://github.com/animatek/Animatek-NME/issues

## Building

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/animatek/Animatek-NME.git
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
./build/AnimatekNME_artefacts/Debug/AnimatekNME
```

Run on macOS:

```bash
build/AnimatekNME_artefacts/Debug/AnimatekNME.app/Contents/MacOS/AnimatekNME
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
