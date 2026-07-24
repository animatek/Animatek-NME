#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <cstdint>

#include "../model/SynthSettings.h"

// Message command IDs (cc field in SysEx header)
namespace NmCmd
{
    constexpr uint8_t IAm             = 0x00;
    constexpr uint8_t ParameterChange = 0x13;
    constexpr uint8_t NMInfo          = 0x14;
    constexpr uint8_t ACK             = 0x16;
    constexpr uint8_t PatchHandling   = 0x17;
    // PatchPacket uses cc=0x1c-0x1f, bits encode first/last flags
    constexpr uint8_t PatchPacketBase = 0x1c;
}

struct IAmMessage
{
    int sender = 0;       // 0 = PC, 1 = Synth
    int versionHigh = 3;
    int versionLow = 3;
    int serial = -1;
    int deviceId = -1;

    std::vector<uint8_t> encode() const;
    static IAmMessage decode(const uint8_t* data, size_t length);
};

struct ParameterChangeMessage
{
    int pid = 0;         // patch ID from ACK
    int section = 0;     // 0 = common voice area, 1 = poly voice area
    int module = 0;
    int parameter = 0;
    int value = 0;

    std::vector<uint8_t> encode() const;
    static ParameterChangeMessage decode(const uint8_t* data, size_t length);
};

struct AckMessage
{
    int pid1 = 0;
    int type = 0;
    int pid2 = 0;
    std::vector<uint8_t> payload;  // Full payload for extended ACK types (0x13, 0x15, etc.)

    static AckMessage decode(const uint8_t* data, size_t length);
};

struct PatchMessage
{
    int slot = 0;
    std::vector<uint8_t> patchData;

    std::vector<uint8_t> encode() const;
    static PatchMessage decode(const uint8_t* data, size_t length);
};

struct ErrorMessage
{
    int errorCode = 0;

    static ErrorMessage decode(const uint8_t* data, size_t length);
};

// NMInfo (cc=0x14): status messages from synth
// PDL2: 0:1 pid:7 0:1 sc:7 [subcommand data] checksum
struct NMInfoMessage
{
    int pid = 0;
    int sc = 0;
    std::vector<uint8_t> data;  // subcommand-specific data (after pid+sc, before checksum)

    // VoiceCount (sc=0x05): 4 voice counts, one per slot
    int voiceCount[4] = {0, 0, 0, 0};
    // NewPatchInSlot (sc=0x38)
    int newPatchSlot = -1;
    int newPatchPid = -1;
    // LightMessage (sc=0x39): 20 LED values (2-bit each, 0-3), startIndex into global LED array
    int lightStartIndex = -1;
    int lightValues[20] = {};
    // MeterMessage (sc=0x3A): 5 pairs of 7-bit values, startIndex into global meter array.
    // Each meter/led-array module owns one (B, A) pair. Stereo modules: first
    // light (left) reads A, second (right) reads B. Modules with a single
    // light (sequencer led-arrays, gain-reduction meters) get their value on B.
    int meterStartIndex = -1;
    int meterValuesA[5] = {};   // second byte of each pair (odd global slot)
    int meterValuesB[5] = {};   // first byte of each pair (even global slot)

    static NMInfoMessage decode(const uint8_t* payload, size_t length);
};

// RequestPatch: sent via PatchHandling (cc=0x17)
// PDL2: pp=0x41, ssc=0x35, no additional data, with checksum
struct RequestPatchMessage
{
    int slot = 0;

    std::vector<uint8_t> encode() const;
};

// LoadPatch: load a patch from bank to slot (cc=0x17)
// PDL2: pp=0x41, ssc=0x0a, slot:7 section:7 position:7
struct LoadPatchMessage
{
    int slot = 0;      // Target slot (0-3)
    int section = 0;   // Bank section (0-8)
    int position = 0;  // Position within bank (0-98)

    std::vector<uint8_t> encode() const;
};

// GetPatch: sent via PatchHandling (cc=0x17) using PatchModification format
// PDL2: pid:7 sc:7 [payload:7] checksum
// Requests a specific patch section from the synth after receiving ACK with patchId
struct GetPatchMessage
{
    enum Section
    {
        Header = 0,
        PolyModule, CommonModule,
        PolyCable, CommonCable,
        PolyParameter, CommonParameter,
        MorphMap, KnobMap, ControlMap,
        PolyNameDump, CommonNameDump,
        Note,
        NumSections
    };

    int pid = 0;         // patch ID from ACK
    Section section = Header;

    std::vector<uint8_t> encode() const;

    // Generate all 13 GetPatch messages for a given pid
    static std::vector<GetPatchMessage> forAllSections(int pid);
};

// PatchPacket (cc=0x1c-0x1f): patch data stream from synth
// cc bits: bit0=first, bit1=last
struct PatchPacketMessage
{
    bool isFirst = false;
    bool isLast = false;
    int command = 0;    // 0:1 command:1 pid:6
    int pid = 0;
    std::vector<uint8_t> patchData;

    static PatchPacketMessage decode(int cc, const uint8_t* payload, size_t length);
};

// GetPatchList: request patch names from synth flash memory
// PDL2: PatchHandling(cc=0x17) -> PatchCommand(pp=0x41) -> GetPatchList(ssc=0x14)
struct GetPatchListMessage
{
    int section = 0;   // Bank index (0-8)
    int position = 0;  // Position within bank (0-98)

    std::vector<uint8_t> encode() const;
};

// PatchListEntry: one patch name with its location
struct PatchListEntry
{
    int section = 0;   // Bank index (0-8)
    int position = 0;  // Position within bank (0-98)
    std::string name;
};

// PatchListResponse: comes inside ACK message (type=0x13 or 0x15)
// PDL2: unknown[3] + StringList chain + endmarker
struct PatchListResponseMessage
{
    std::vector<PatchListEntry> entries;
    int nextSection = -1;   // -1 = done, else section for next request
    int nextPosition = -1;  // Position for next request

    static PatchListResponseMessage decode(const uint8_t* data, size_t length,
                                            int requestSection, int requestPosition);
};

// NewModule: add a new module to the synth's current patch
// PDL2: Uses cc=0x1f (PatchPacket with both first+last bits), command=0
// Payload contains 5 PDL2 sections: SingleModule(48), CableDump(82), ParameterDump(77), CustomDump(91), NameDump(90)
struct NewModuleMessage
{
    int pid = 0;           // Patch ID from ACK
    int typeId = 0;        // Module type ID (3-113)
    int section = 0;       // 0 = common, 1 = poly
    int index = 0;         // Module index (unique within section)
    int xpos = 0;          // Grid X position
    int ypos = 0;          // Grid Y position
    std::string name;      // Module name (max 16 chars)
    std::vector<int> parameterValues;  // Parameter values (default values from descriptor)
    std::vector<int> customValues;     // Custom values (usually empty)

    std::vector<uint8_t> encode() const;
};

// RequestSynthSettings: ask synth for its global settings.
// PDL2: PatchHandling(cc=0x17) with pp=0x44 (RequestSynthSettings)
// Sent with cc=0x17, has checksum, slot=0, expects reply.
struct RequestSynthSettingsMessage
{
    std::vector<uint8_t> encode() const;
};

// SynthSettings: 17 global parameters + 4 MIDI channels + optional extended block.
// Sent and received as a PatchPacket-style envelope (cc=0x1c-0x1f) carrying a
// section with type=0x03 followed by the bit-packed SynthSettings$data.
struct SynthSettingsMessage
{
    SynthSettings settings;

    // Encode a PatchPacket payload: pid + 7-bit-encoded SynthSettings section.
    // Caller adds the SysEx envelope and checksum.
    std::vector<uint8_t> encode(int pid) const;

    // Decode the patchData carried by a PatchPacketMessage with a SynthSettings
    // section (already stripped of cc header, command/pid byte, and checksum).
    // Returns false if the payload is not a SynthSettings section.
    static bool decode(const std::vector<uint8_t>& patchData, SynthSettings& out);
};
