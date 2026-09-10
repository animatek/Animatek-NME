#include "NewModuleMessage.h"
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

    // Payload: pid + encoded patch data.
    // PatchPacket := 0:1 command:1 pid:6 -- only six bits for the pid here, and
    // bit 6 is the command flag, which the bulk upload sets and an edit leaves
    // at 0 (see UploadPacketizer::frame). NewModule is the one edit message on
    // cc=0x1f; move/delete/rename/cables ride cc=0x17, where the pid is seven
    // bits wide. Masking with 0x7F would turn a pid of 64 or more into
    // command=1 and the synth would read the packet as a bulk-upload stream.
    msg.push_back(static_cast<uint8_t>(pid_ & 0x3F));

    // Encode each byte-aligned PDL2 section directly. Values are not MIDI
    // bytes yet: SingleModule fields and CustomValue both use all eight bits.
    BitStreamWriter bsw;

    // SingleModule := type:8 section:8 index:8 xpos:8 ypos:8 String$name
    bsw.writeBits(48, 8);
    bsw.writeBits(typeId_, 8);
    bsw.writeBits(section_, 8);
    bsw.writeBits(index_, 8);
    bsw.writeBits(xpos_, 8);
    bsw.writeBits(ypos_, 8);
    bsw.writeString16(name_);
    bsw.alignToByte();

    // CableDump := section:1 ncables:15
    bsw.writeBits(82, 8);
    bsw.writeBits(section_, 1);
    bsw.writeBits(0, 15);
    bsw.alignToByte();

    // ParameterDump := section:1 nmodules:7 [index:7 type:7 params]
    bsw.writeBits(77, 8);
    bsw.writeBits(section_, 1);
    bsw.writeBits(paramValues_.empty() ? 0 : 1, 7);
    if (!paramValues_.empty())
    {
        bsw.writeBits(index_, 7);
        bsw.writeBits(typeId_, 7);
        const auto widths = ParameterEncoder::getParameterBitWidths(typeId_);
        for (size_t i = 0; i < paramValues_.size(); ++i)
            bsw.writeBits(paramValues_[i], i < widths.size() ? widths[i] : 7);
    }
    bsw.alignToByte();

    // CustomDump := section:1 nmodules:7 [index:7 nparams:8 values:8*]
    bsw.writeBits(91, 8);
    bsw.writeBits(section_, 1);
    bsw.writeBits(customValues_.empty() ? 0 : 1, 7);
    if (!customValues_.empty())
    {
        bsw.writeBits(index_, 7);
        bsw.writeBits(static_cast<uint32_t>(customValues_.size()), 8);
        for (int value : customValues_)
            bsw.writeBits(value, 8);
    }
    bsw.alignToByte();

    // NameDump := section:1 nmodules:7 [index:8 String$name]
    bsw.writeBits(90, 8);
    bsw.writeBits(section_, 1);
    bsw.writeBits(1, 7);
    bsw.writeBits(index_, 8);
    bsw.writeString16(name_);
    bsw.alignToByte();

    // Convert bitstream to 7-bit MIDI bytes and append
    auto encodedData = bsw.toMidiBytes();
    msg.insert(msg.end(), encodedData.begin(), encodedData.end());

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
