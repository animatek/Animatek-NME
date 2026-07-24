#include "NewModuleMessage.h"
#include "../model/IntStream.h"
#include "../model/BitStreamWriter.h"
#include "ParameterEncoder.h"

NewModuleMessageProto::NewModuleMessageProto(int pid, int typeId, int section, int index,
                                             int xpos, int ypos, const std::string& name,
                                             const std::vector<int>& paramValues,
                                             const std::vector<int>& customValues)
    : pid_(pid)
    , typeId_(typeId)
    , section_(section)
    , index_(index)
    , xpos_(xpos)
    , ypos_(ypos)
    , name_(name)
    , paramValues_(paramValues)
    , customValues_(customValues)
{
}

std::vector<uint8_t> NewModuleMessageProto::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x1f<<2)|slot] 06
    // cc=0x1f indicates PatchPacket with both first+last bits set
    appendHeader(msg, 0x1f, slot);

    // Payload: pid + encoded patch data
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));

    // Build IntStream exactly like Java (values, not bits)
    IntStream patchData;

    // Section 1: SingleModule (type 48)
    patchData.append(48);
    patchData.append(typeId_);
    patchData.append(section_);
    patchData.append(index_);
    patchData.append(xpos_);
    patchData.append(ypos_);
    patchData.appendString(name_);

    // Section 2: CableDump (type 82)
    patchData.append(82);
    patchData.append(section_);
    patchData.append(0);  // ncables = 0

    // Section 3: ParameterDump (type 77)
    patchData.append(77);
    patchData.append(section_);
    if (!paramValues_.empty())
    {
        patchData.append(1);  // nmodules = 1
        patchData.append(index_);
        patchData.append(typeId_);
        for (int val : paramValues_)
            patchData.append(val);
    }
    else
    {
        patchData.append(0);  // nmodules = 0
    }

    // Section 4: CustomDump (type 91)
    patchData.append(91);
    patchData.append(section_);
    if (!customValues_.empty())
    {
        patchData.append(1);  // nmodules = 1
        patchData.append(index_);
        patchData.append(static_cast<int>(customValues_.size()));
        for (int val : customValues_)
            patchData.append(val);
    }
    else
    {
        patchData.append(0);  // nmodules = 0
    }

    // Section 5: NameDump (type 90)
    patchData.append(90);
    patchData.append(section_);
    patchData.append(1);  // nmodules = 1
    patchData.append(index_);
    patchData.appendString(name_);

    // Convert IntStream to BitStream using PDL2 rules
    // The PDL2 parser would interpret each IntStream value according to the schema,
    // but we know the exact structure for NewModuleMessage, so we can encode directly.

    BitStreamWriter bsw;

    // Process each value from IntStream according to NewModuleMessage structure
    const auto& vals = patchData.getValues();
    size_t pos = 0;

    // Helper to read next value
    auto nextVal = [&vals, &pos]() -> int {
        return (pos < vals.size()) ? vals[pos++] : 0;
    };

    // Section 1: SingleModule (type 48)
    // PDL2: SingleModule := type:8 section:8 index:8 xpos:8 ypos:8 String$name
    bsw.writeBits(nextVal(), 8);  // type = 48
    bsw.writeBits(nextVal(), 8);  // module type ID (8 bits!)
    bsw.writeBits(nextVal(), 8);  // section (8 bits!)
    bsw.writeBits(nextVal(), 8);  // index (8 bits!)
    bsw.writeBits(nextVal(), 8);  // xpos (8 bits!)
    bsw.writeBits(nextVal(), 8);  // ypos (8 bits!)
    // String$name (null-terminated)
    while (pos < vals.size() && vals[pos] != 0)
        bsw.writeBits(nextVal(), 8);
    bsw.writeBits(nextVal(), 8);  // null terminator
    bsw.alignToByte();

    // Section 2: CableDump (type 82)
    bsw.writeBits(nextVal(), 8);   // type = 82
    bsw.writeBits(nextVal(), 1);   // section
    bsw.writeBits(nextVal(), 15);  // ncables
    bsw.alignToByte();

    // Section 3: ParameterDump (type 77)
    bsw.writeBits(nextVal(), 8);  // type = 77
    bsw.writeBits(nextVal(), 1);  // section
    int nmodules = nextVal();
    bsw.writeBits(nmodules, 7);
    if (nmodules > 0)
    {
        bsw.writeBits(nextVal(), 7);  // index
        int moduleType = nextVal();
        bsw.writeBits(moduleType, 7);  // type

        // Get parameter bit widths for this module type
        auto bitWidths = ParameterEncoder::getParameterBitWidths(moduleType);

        if (!bitWidths.empty())
        {
            // Encode parameters with correct bit widths from PDL2
            // Use bitWidths.size() as the exact bound — do NOT check for 90/91
            // sentinel values since parameter values can legitimately be 90 or 91.
            for (size_t i = 0; i < bitWidths.size() && pos < vals.size(); ++i)
                bsw.writeBits(nextVal(), bitWidths[i]);
        }
        else
        {
            // Fallback: unknown module type, write all as 7 bits
            while (pos < vals.size() && vals[pos] != 91 && vals[pos] != 90)
                bsw.writeBits(nextVal(), 7);
        }
    }
    bsw.alignToByte();

    // Section 4: CustomDump (type 91)
    bsw.writeBits(nextVal(), 8);  // type = 91
    bsw.writeBits(nextVal(), 1);  // section
    nmodules = nextVal();
    bsw.writeBits(nmodules, 7);
    if (nmodules > 0)
    {
        bsw.writeBits(nextVal(), 7);     // index
        int nparams = nextVal();
        bsw.writeBits(nparams, 8);
        for (int i = 0; i < nparams; ++i)
            bsw.writeBits(nextVal(), 7);
    }
    bsw.alignToByte();

    // Section 5: NameDump (type 90)
    bsw.writeBits(nextVal(), 8);  // type = 90
    bsw.writeBits(nextVal(), 1);  // section
    bsw.writeBits(nextVal(), 7);  // nmodules
    bsw.writeBits(nextVal(), 8);  // index
    // Name (null-terminated string)
    while (pos < vals.size() && vals[pos] != 0)
        bsw.writeBits(nextVal(), 8);
    if (pos < vals.size())
        bsw.writeBits(nextVal(), 8);  // null terminator
    bsw.alignToByte();

    // Convert bitstream to 7-bit MIDI bytes and append
    auto encodedData = bsw.toMidiBytes();
    msg.insert(msg.end(), encodedData.begin(), encodedData.end());

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
