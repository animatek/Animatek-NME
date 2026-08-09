#pragma once

#include "Patch.h"
#include "BitStreamWriter.h"
#include <vector>
#include <cstdint>
#include <string>

/**
 * PatchSerializer - Serializes a Patch object to 7-bit MIDI binary format
 * for uploading to the Nord Modular G1 synthesizer.
 *
 * Produces individual PDL2 sections (one type per section) suitable for
 * wrapping as PatchPacket SysEx messages.
 */
class PatchSerializer
{
public:
    PatchSerializer() = default;

    /**
     * Serialize a patch to individual PDL2 sections for upload.
     *
     * Returns 16 sections in Java Patch2BitstreamBuilder order:
     *   [0]  PatchName (type 55)
     *   [1]  Header (type 33)
     *   [2]  ModuleDump poly (type 74, section=1)
     *   [3]  ModuleDump common (type 74, section=0)
     *   [4]  NoteDump (type 105)
     *   [5]  CableDump poly (type 82, section=1)
     *   [6]  CableDump common (type 82, section=0)
     *   [7]  ParameterDump poly (type 77, section=1)
     *   [8]  ParameterDump common (type 77, section=0)
     *   [9]  MorphMap (type 101)
     *   [10] KnobMap (type 98)
     *   [11] ControlMap (type 96)
     *   [12] CustomDump poly (type 91, section=1)
     *   [13] CustomDump common (type 91, section=0)
     *   [14] NameDump poly (type 90, section=1)
     *   [15] NameDump common (type 90, section=0)
     *
     * Sections come back as plain 8-bit bytes, NOT 7-bit MIDI encoded: the
     * upload sends them as one continuous stream chopped into fixed-size
     * packets, and each packet carries its own 7-bit encoding, so the encoding
     * can only happen once the packet boundaries are known (issue #39).
     *
     * @param patch The patch to serialize
     * @return Vector of 16 sections (raw 8-bit binary data)
     */
    std::vector<std::vector<uint8_t>> serializeForUpload(const Patch& patch);

    /**
     * Serialize a patch to the 13 sections used by the download protocol
     * (for round-trip fidelity when storing/loading .pch files).
     *
     * LIMITATION: Currently only serializes empty/init patches. This is used
     * for StorePatch and Delete operations. For full upload, use serializeForUpload().
     *
     * @param patch The patch to serialize
     * @return Vector of 13 sections (raw 8-bit binary data)
     */
    std::vector<std::vector<uint8_t>> serialize(const Patch& patch);

private:
    // Section serializers
    std::vector<uint8_t> serializePatchName(const Patch& patch);
    std::vector<uint8_t> serializeHeaderAndName(const Patch& patch);  // for legacy serialize()
    std::vector<uint8_t> serializeHeader(const Patch& patch);
    std::vector<uint8_t> serializeModuleDump(const Patch& patch, int section);
    std::vector<uint8_t> serializeCableDump(const Patch& patch, int section);
    std::vector<uint8_t> serializeParameterDump(const Patch& patch, int section);
    std::vector<uint8_t> serializeMorphMap(const Patch& patch);
    std::vector<uint8_t> serializeKnobMap(const Patch& patch);
    std::vector<uint8_t> serializeControlMap(const Patch& patch);
    std::vector<uint8_t> serializeCustomDump(const Patch& patch, int section);
    std::vector<uint8_t> serializeNameDump(const Patch& patch, int section);
    std::vector<uint8_t> serializeNoteDump(const Patch& patch);

    // Helper: find the containerIndex of the module owning a connector
    int findModuleContainerIndex(const ModuleContainer& container, const Connector* conn) const;

    // Helper: compute bit width needed for a max value (matches PatchParser::bitWidth)
    static int bitWidth(int maxValue);
};
