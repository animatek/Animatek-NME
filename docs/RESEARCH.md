# Nomad2026 - Research & Documentation

## Overview

Nomad is an open-source editor for the **Nord Modular G1** synthesizer, originally developed
between 2002-2008 and updated in 2019. This document compiles all technical research for
reimplementing the editor in JUCE/C++ as a universal application.

---

## 1. Project History & Versions

### Version 0.3 (2008) - Original
- **Developers**: Christian Schneider (editor), Marcus Andersson (protocol libraries)
- **Tech**: Java 5+, Java Swing, JPF (Java Plugin Framework) 1.5.1
- **Source**: SourceForge CVS -> GitHub (wesen/nmedit)
- **Status**: Pre-release update 2 (2008-03-11)

### Version 0.4 (2019) - Updated
- **Developer**: Ian Hoogeboom (GitHub: Airell)
- **Tech**: Java 8 (specifically 8u202), CoreMidi4J for macOS
- **Source**: https://github.com/Airell/nmedit (fork of wesen/nmedit)
- **Changes**:
  - Fixed MIDI issues
  - Added macOS compatibility (10.12-10.15)
  - Added CoreMidi4J library for native macOS MIDI
  - Distributed as macOS .app bundle and Windows .zip
- **Known Issues**:
  - Real-time knob/light updates don't auto-refresh
  - Module deletion unreliable
  - Knob window alignment depends on module naming
  - OpenJDK 9+ does NOT work (JPF incompatibility)

### Credits
| Person | Role |
|--------|------|
| Marcus Andersson (Sweden) | Reverse-engineered Nord Modular MIDI protocol; C++ and Java protocol libs |
| Christian Schneider (Germany) | Nomad Java editor (v0.2, v0.3) |
| Ian Hoogeboom | Nomad v0.4, maintained GitHub fork |
| Jan Punter (Netherlands) | Nord Modular patch file format documentation |
| Jelle Herold | Project founder, SourceForge infrastructure |
| Stefan Keel (Seattle, WA) | Designed all 110 module SVG icons |
| Tobias Weinald (Germany) | Nomad splash screen |

---

## 2. Architecture

### Library Components

| Library | Language | Purpose |
|---------|----------|---------|
| `libppf` | C/C++ | Patch processing framework |
| `libnmPatch` | C/C++ | Patch data handling |
| `libnmProtocol` | C/C++ | Nord Modular communication protocol |
| `libPDL` | C/C++ | Patch Description Language parser |
| `jnmProtocol` / `jnmProtocol2` | Java | Protocol implementation (v1 and v2) |
| `jPDL` / `jPDL2` | Java | PDL parser (v1 and v2) |
| `jPatch` | Java | Patch data model |
| `jSynth` | Java | Synthesizer abstraction layer |
| `jTheme` | Java | UI theming |
| `nmutils` | Java | Utility functions |
| `codecs` | PDL2 files | Binary format definitions for MIDI and patch files |
| `nordmodular` | Java | Nord Modular specific implementation |
| `patchmodifier` | Java | Patch modification tools |

### JPF Plugin Architecture (nomad-0-3_2)
```
plugins/
  net.sf.nmedit.nomad.core    -> Core application (menu, services, UI framework)
  net.sf.nmedit.nordmodular   -> Nord Modular device support
  net.sf.nmedit.jsynth        -> Synth abstraction UI
  net.sf.nmedit.jtheme        -> Theme/module rendering
  net.sf.nmedit.jpatch        -> Patch library
  net.sf.nmedit.jpdl2         -> PDL parser
  net.sf.nmedit.nmutils       -> Utilities
  com.jgoodies.forms           -> JGoodies Forms layout
  com.jgoodies.looks           -> JGoodies Look & Feel
  org.apache.xerces            -> XML parser
  org.apache.xmlgraphics.batik -> SVG rendering (Batik)
  org.mozilla.rhino            -> JavaScript engine
  org.jdom                     -> JDOM XML
  net.sf.cssparser             -> CSS parser
  org.w3.sac                   -> W3C SAC (CSS API)
```

### jSynth Abstraction Layer
| Class | Purpose |
|-------|---------|
| `Synthesizer` / `AbstractSynthesizer` | Top-level synth abstraction |
| `Slot` / `AbstractSlot` | Patch slot management (4 slots NM, 1 Micro) |
| `Bank` / `AbstractBank` / `BankManager` | Patch bank storage |
| `Port` / `AbstractPort` / `PortManager` | MIDI port abstraction |
| `MidiPortSupport` / `DefaultMidiPorts` | MIDI I/O handling |
| `RemotePatch` / `PatchInfo` | Remote patch state tracking |
| `ComStatus` | Connection status management |

---

## Hardware Specifications (Nord Modular G1)

### DSP & Processing
- **4 x Motorola 56303 DSP** chips (standard), expandable to **8 DSPs** via Voice Expansion Board
- **96 kHz** internal sample rate, **24-bit** processing
- DAC: 18-bit / 96 kHz | ADC: 16-bit / 48 kHz
- Each patch is confined to **one DSP** — cannot span multiple DSPs (G1 limitation vs G2)
- 4 DSPs maps naturally to 4 slots, but DSP resources are shared dynamically — not a fixed 1:1 assignment
- With the Voice Expansion (8 DSPs), you still have 4 slots but more DSP headroom per slot
- Polyphony is dynamic: simple patches up to ~32 voices, complex patches may use a full DSP per voice

### Variants
| Model | DSPs | Slots | Knobs | Keys | Notes |
|-------|------|-------|-------|------|-------|
| Nord Modular Keyboard | 4 (exp. to 8) | 4 | 23 assignable | 25 (velocity) | Full-size unit |
| Nord Modular Rack | 4 (exp. to 8) | 4 | 23 assignable | — | Same as Key minus keyboard |
| Nord Micro Modular | 1 (fixed) | 1 | 4 + 1 button | — | Half-rack, monotimbral |

### Memory & Storage
- **9 banks x 99 patches** = 891 slots (storage depends on patch complexity)
- Patches stored in flash memory (no battery backup needed)
- Performance slots can combine up to 4 patches (one per slot)
- Micro Modular: reduced storage capacity

### I/O
- Audio: stereo in/out (line-level), headphone out
- MIDI: standard In/Out + dedicated "PC" In/Out for editor SysEx traffic
- Pedal inputs: sustain + expression (Key/Rack only)
- All knobs and parameters send/receive MIDI CC

### Firmware History
- OS v2.1, v3.0: added modules, Mac OS 8.6+ support
- **OS v3.03** (2000, final): added 16-band vocoder, 14-band filter bank, ring modulator, new EQ modules
- Device types in protocol: `0x00` = Keyboard, `0x01` = Rack, `0x02` = Micro Modular

### G1 vs G2 Limitations
- No MIDI Out or CV/Gate output modules (G2 added these)
- No DSP-intensive effects (reverb, long delays) — G2 added them
- Single-DSP per patch — G2 could allocate multiple DSPs per patch
- G1 multi-DSP usage requires multiple slots + physical audio cables between them

---

## 3. Nord Modular MIDI Protocol (v3.03)

### SysEx Envelope
```
0xF0  0x33  0x06  [cc:5][slot:2]  [payload...]  0xF7
```
- Manufacturer ID: `0x33` (Clavia)
- Device type: `0x06`
- `cc`: 5-bit command code
- `slot`: 2-bit slot selector (0-3)
- All data fields use **7-bit encoding** (MSB always 0) for MIDI compatibility
- Checksums: `sum_of_bytes % 128`

### Command Codes (cc field)

| cc | Message Type | Purpose |
|----|-------------|---------|
| 0x00 | IAm | Device identification / handshake |
| 0x13 | Parameter | Real-time parameter changes |
| 0x14 | NMInfo | Synth state feedback (voices, slots, lights, meters) |
| 0x16 | ACK | Command acknowledgments |
| 0x17 | PatchHandling | Patch editing operations |
| 0x1c-0x1f | PatchPacket | Multi-packet patch transfer (first/last flags in cc) |

### Device Identification (IAm)
- **PC sends**: `sender=0, versionHigh=3, versionLow=3`
- **Synth responds**: `sender=1` + serial number + device ID
- Device types: `0x00` = NM Keyboard, `0x01` = NM Rack, `0x02` = Micro Modular

### Parameter Change Message
```
cc=0x13, sc=0x40: section:7 module:7 parameter:7 value:7
```
- section: 0=common voice area, 1=poly voice area (verified from Java Format.java)
- module: module index (0-127)
- parameter: parameter index (0-127)
- value: parameter value (0-127)

### Cable Connection Message
```
cc=0x17, sc=0x50: section color module1 type1 connector1 module2 type2 connector2
```

### New Module Message
Encoded as `cc=0x1f`, containing embedded patch sections:
- Section 48 (SingleModule): type, section, index, xpos, ypos, name
- Section 82 (CableDump): empty cable section
- Section 77 (ParameterDump): default parameter values
- Section 91 (CustomDump): custom values
- Section 90 (NameDump): module name

### Note Message (virtual keyboard)
```
cc=0x17, payload: pid 0x56 onOff note    (onOff: 0 = note-on, 1 = note-off)
```
Plays/releases notes on the slot's patch through the PC port (which ignores plain MIDI
channel messages). No velocity on the wire. Captured from the original Clavia editor's
keyboard floater and hardware-verified (the editor also sends off+on pairs when changing
notes in drone mode). Sniffing technique: the Wine editor exposes an ALSA seq client
("WINE midi driver"), so `aseqdump -p <client>:1` shows its TX directly.

Hardware-tested dead ends (2026-06-11), do not retry:
- `NoteEvent` (sc=0x41, vel/onoff/note) is **incoming-only** — the synth reports keyboard
  notes with it but refuses it as input with error 5 ("no slot focused"), even right after
  an `ActivateSlot` (PatchCommand 0x41/0x09); that attempt left the synth in an error state.
- CC 120/123 (all sound/notes off) on any channel are ignored by the PC port.
- The front-panel panic is internal: it emits nothing on the PC port and has no protocol
  equivalent, so stuck MIDI IN notes cannot be cleared by an editor.

### NMInfo Subcommands (sc field)

| sc | Message | Content |
|----|---------|---------|
| 0x05 | VoiceCount | 4x 7-bit voice counts per slot |
| 0x07 | SlotsSelected | 4 slot selection bits |
| 0x09 | SlotActivated | Active slot index |
| 0x25 | KnobAssignment | Knob-to-parameter mapping |
| 0x27 | SetPatchTitle | Null-terminated string (16 chars max) |
| 0x39 | Lights | 20x 2-bit LED state values |
| 0x3a | Meters | 5 pairs of 7-bit values |
| 0x40 | KnobChange | section/module/parameter/value |
| 0x7e | Error | 7-bit error code |

### Protocol Behavior
- Request/response pattern with **3-second timeout**
- ACK messages confirm operations (type=0x7f for completion)
- Patch data split across multiple SysEx messages with first/last flags
- All data uses 7-bit encoding for MIDI compatibility
- Checksum = `sum(all bytes from F0 through last payload byte) % 128` — **includes F0 and 0x33**
- Not all messages have checksums: IAm (cc=0x00) has none; Parameter (0x13), PatchHandling (0x17) do

### Patch Retrieval Flow

To retrieve the full patch currently loaded in a slot:

**Step 1** — Send `RequestPatch` (cc=0x17, pp=0x41, ssc=0x35). Synth ACKs with a `patchId`.

**Step 2** — Send 13 `GetPatch` messages (cc=0x17, PatchModification: pid + section code + payload). The synth responds to each with a `PatchPacket` (cc=0x1c–0x1f, where the 2 LSBs encode first/last packet flags).

| # | Section | sc | Description |
|---|---------|-----|-------------|
| 1 | Header | 0x20 or 0x28 | Voice config, portamento, octave shift |
| 2 | ModuleDump (poly) | 0x4b, section=1 | Module list for poly voice area |
| 3 | ModuleDump (common) | 0x4b, section=0 | Module list for common voice area |
| 4 | CableDump (poly) | 0x53, section=1 | Cable connections (poly) |
| 5 | CableDump (common) | 0x53, section=0 | Cable connections (common) |
| 6 | ParameterDump (poly) | 0x4c, section=1 | All parameter values (poly) |
| 7 | ParameterDump (common) | 0x4c, section=0 | All parameter values (common) |
| 8 | MorphMap | 0x66 | Morph assignments |
| 9 | KnobMapDump | 0x63 | 23 knob-to-parameter assignments |
| 10 | ControlMapDump | 0x61 | MIDI CC-to-parameter mappings |
| 11 | NameDump (poly) | 0x4e, section=1 | Module names (poly) |
| 12 | NameDump (common) | 0x4e, section=0 | Module names (common) |
| 13 | NoteDump | 0x68 | Note event data |

**Important**: Each PatchPacket response is independently 7-bit encoded — you cannot concatenate raw bytes across packets. Decode each packet separately. The first PatchPacket (Header request) contains both Header (type=33) and PatchName2 (type=39).

### Section Binary Formats (from PDL2 spec)

**ParameterDump** — all parameter values for one voice area:
```
ParameterDump := section:1 nmodules:7 nmodules*Parameter
Parameter     := index:7 type:7 <type-specific params>
```
Each module's parameters are keyed by `type` (77 distinct layouts, types 3–127). **Parameter bit widths vary per module type** — e.g., OscA (type 7) uses `[7,7,7,7,2,7,7,7,7,1]` bits. You must build a lookup table from the PDL2 spec; writing all params as 7-bit will silently corrupt data.

**KnobMapDump** — which parameters are assigned to the 23 physical knobs:
```
KnobMapDump    := Knob * 23
Knob           := assigned:1 assigned*KnobAssignment
KnobAssignment := section:2 module:7 parameter:7
```
Each knob is either unassigned (1 bit = 0) or assigned (1 + 2 + 7 + 7 = 17 bits) to a specific section/module/parameter.

**ControlMapDump** — MIDI CC number to parameter mappings:
```
ControlMapDump := ncontrols:7 ncontrols*Control
Control        := control:7 section:2 module:7 parameter:7
```

**MorphMap** — morph source assignments:
```
MorphMap := 4*MorphValue  KeyboardAssignment  nentries:7 nentries*MorphEntry
MorphValue := value:7
MorphEntry := section:2 module:7 parameter:7 morphIndex:2 range:8
```

### 7-Bit Encoding

All patch data is 7-bit encoded for MIDI compatibility. Every 7 bytes of original data are preceded by 1 MSB byte containing the high bits:
```
Encoded: [MSB] [b0] [b1] [b2] [b3] [b4] [b5] [b6]
MSB bits: bit6=b0.7, bit5=b1.7, bit4=b2.7, ..., bit0=b6.7
```
Decode by restoring the MSB of each byte from the corresponding MSB bit. Each PatchPacket must be decoded independently.

---

## 4. Patch File Format (.pch)

### Section Types

| Type ID | Section | Content |
|---------|---------|---------|
| 3 | SynthSettings | MIDI channels, clock, tuning, keyboard mode |
| 33 | Header | Voice config, portamento, octave shift, range params |
| 39 | PatchName2 | Patch name string |
| 48 | SingleModule | Individual module definition |
| 55 | PatchName | Patch name with 3 padding bytes |
| 74 | ModuleDump | All modules (section:1, count:7, then Module records) |
| 77 | ParameterDump | Module parameters by type (77 defined types) |
| 82 | CableDump | Cable connections |
| 90 | NameDump | Module name assignments |
| 91 | CustomDump | Custom module data |
| 96 | ControlMapDump | MIDI CC to parameter mappings |
| 98 | KnobMapDump | 23 knob assignments |
| 101 | MorphMap | 4 morph values + keyboard assignments + morph entries |
| 105 | NoteDump | Note event data |

### Data Formats
- **Module Record**: `type:7 index:7 xpos:7 ypos:7`
- **Cable Record**: `color:3 source:7 inputOutput:6 type:1 destination:7 input:6`
- **String Format**: 16 x 8-bit characters, null-terminated
- **Parameter Types**: 77 distinct layouts (types 3-127) per module type

---

## 5. Module System

### Module Definitions (modules.xml)
```xml
<module component-id="m7" index="7" name="OscA"
       fullname="Multiple Oscillator A" category="Oscillator">
  <connector component-id="c1" index="0" name="Sync"
             type="input" signal="master-slave" />
  <connector component-id="c6" index="0" name="Out"
             type="output" signal="audio" />
  <parameter component-id="p1" index="0" name="FreqCoarse"
             class="parameter" defaultValue="64" formatter="..." />
  <attribute name="cycles" type="int" value="..." />
</module>
```

### Signal Types (7 types with color coding)

| Signal | Color | Hex |
|--------|-------|-----|
| audio | red | #CB4F4F |
| control | blue | #5A5FB3 |
| logic | yellow | #E5DE45 |
| master-slave | gray | #A8A8A8 |
| user1 | - | - |
| user2 | - | - |
| none | - | - |

### Module Categories
Oscillators, Filters, Envelopes, Effects, Audio Processing, Control/Logic,
Sequencers, Keyboard/Input, Output, Compressor/Expander

### Module Attributes (CPU/Memory)
`cycles`, `x-mem`, `y-mem`, `prog-mem`, `dyn-mem`, `zero-page`, `height`, `background`

### Parameter Classes
- `parameter` - Standard controllable parameter
- `morph` - Morphing-capable parameter
- `custom` - UI-only parameter

---

## 6. PDL2 (Protocol/Patch Description Language)

Domain-specific grammar for defining binary data structures.

### Key Files
| File | Purpose |
|------|---------|
| `libs/codecs/midi.pdl2` | Complete MIDI protocol binary format spec |
| `libs/codecs/patch.pdl2` | Complete patch file binary format spec |
| `libs/jpdl2/format/pdl2.jflex` | JFlex lexer specification |
| `libs/jpdl2/format/pdl2.byaccj` | Byacc/J parser grammar |

PDL2 uses bit-level field specifications: `field_name:bit_width` with conditional
parsing via `switch` and optional fields via `?` prefix.

---

## 7. Key Source Files for JUCE Reimplementation

| Path (relative to nmedit/) | Purpose |
|----------------------------|---------|
| `libs/codecs/midi.pdl2` | Complete MIDI protocol binary format |
| `libs/codecs/patch.pdl2` | Complete patch file binary format |
| `libs/nordmodular/data/module-descriptions/modules.xml` | All 110+ module definitions |
| `libs/nordmodular/data/module-descriptions/transformations.xml` | Module-to-UI component mappings |
| `libs/jnmprotocol2/src/` | 50 Java files: all protocol messages |
| `libs/jpatch/src/` | 40+ Java files: patch data model |
| `libs/jsynth/src/` | 21 Java files: synthesizer abstraction |
| `libs/libnmprotocol/` | C/C++ protocol implementation |
| `libs/libnmpatch/` | C/C++ patch handling |
| `libs/libpdl/` | C/C++ PDL parser |
| `libs/libppf/` | C/C++ patch processing framework |

---

## 8. External Links

- **Nomad 0.4**: https://oracle48.nl/Nomad/index.html
- **NMedit Project**: https://nmedit.sourceforge.net/
- **Developer Docs**: https://nmedit.sourceforge.net/developer.html
- **Community Forum**: https://www.electro-music.com/forum/forum-138.html
- **SourceForge**: https://sourceforge.net/projects/nmedit/
- **GitHub (wesen)**: https://github.com/wesen/nmedit
- **GitHub (Airell/0.4)**: https://github.com/Airell/nmedit
- **CoreMidi4J**: https://github.com/DerekCook/CoreMidi4J

---

## 9. Build Notes

### Running Original (v0.3)
```bash
cd /mnt/SPEED/CODE/Nomad2026/nomad-0-3_2
/mnt/SPEED/CODE/Nomad2026/jdk8u482-b08/bin/java -jar nomad.jar
```
- Requires Java 8 (JPF 1.5.1 incompatible with Java 9+)
- JDK 8 portable installed at: `jdk8u482-b08/`

### Original Build System
- C/C++ libraries: Autotools (configure, make)
- Java libraries: Apache Ant
- Dependencies: TCL 8.x, Flex, Bison, SWIG, SCons (for legacy C/C++)
