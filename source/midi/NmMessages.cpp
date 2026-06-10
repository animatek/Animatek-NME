#include "NmMessages.h"
#include "../model/BitStream.h"
#include "../model/BitStreamWriter.h"

// --- IAmMessage ---
// PDL2: IAm := 0:1 sender:7 0:1 versionHigh:7 0:1 versionLow:7
// cc=0x00 is in the SysEx header, NOT repeated in payload.
// No checksum.

std::vector<uint8_t> IAmMessage::encode() const
{
    return {
        static_cast<uint8_t>(sender & 0x7F),
        static_cast<uint8_t>(versionHigh & 0x7F),
        static_cast<uint8_t>(versionLow & 0x7F)
    };
}

IAmMessage IAmMessage::decode(const uint8_t* data, size_t length)
{
    IAmMessage msg;
    if (length < 3)
        return msg;

    msg.sender       = data[0];
    msg.versionHigh  = data[1];
    msg.versionLow   = data[2];

    // Synth response (sender=1) includes identification fields
    if (msg.sender == 1 && length >= 7)
    {
        // reserved = data[3]
        msg.serial = (data[4] << 7) | data[5];
        msg.deviceId = data[6];
    }

    return msg;
}

// --- ParameterChangeMessage ---
// PDL2: Parameter := 0:1 pid:7 0:1 sc:7 ... checksum
// cc=0x13 is in the SysEx header. Payload starts with pid, sc.
// ParameterChange subcommand: sc=0x40, then section, module, parameter, value.

std::vector<uint8_t> ParameterChangeMessage::encode() const
{
    return {
        static_cast<uint8_t>(pid & 0x7F),         // patch ID from ACK
        static_cast<uint8_t>(0x40),               // sc = ParameterChange
        static_cast<uint8_t>(section & 0x7F),
        static_cast<uint8_t>(module & 0x7F),
        static_cast<uint8_t>(parameter & 0x7F),
        static_cast<uint8_t>(value & 0x7F)
    };
}

ParameterChangeMessage ParameterChangeMessage::decode(const uint8_t* data, size_t length)
{
    ParameterChangeMessage msg;
    // Payload: pid(1) + sc(1) + section + module + parameter + value [+ checksum]
    if (length < 6 || data[1] != 0x40)
    {
        msg.pid = -1;  // Mark as invalid (sc != ParameterChange)
        return msg;
    }

    msg.pid       = data[0];
    msg.section   = data[2];
    msg.module    = data[3];
    msg.parameter = data[4];
    msg.value     = data[5];
    return msg;
}

// --- AckMessage ---
// PDL2: ACK := 0:1 pid1:7 0:1 type:7 0:1 pid2:7

AckMessage AckMessage::decode(const uint8_t* data, size_t length)
{
    AckMessage msg;
    if (length < 3)
        return msg;

    msg.pid1 = data[0];
    msg.type = data[1];
    msg.pid2 = data[2];

    // Store full payload for extended ACK types
    if (length > 3)
        msg.payload.assign(data + 3, data + length);

    return msg;
}

// --- PatchMessage ---
// PDL2: PatchHandling uses cc=0x17, payload is the patch-specific data.

std::vector<uint8_t> PatchMessage::encode() const
{
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(slot & 0x7F));
    payload.insert(payload.end(), patchData.begin(), patchData.end());
    return payload;
}

PatchMessage PatchMessage::decode(const uint8_t* data, size_t length)
{
    PatchMessage msg;
    if (length < 1)
        return msg;

    msg.slot = data[0];
    if (length > 1)
        msg.patchData.assign(data + 1, data + length);
    return msg;
}

// --- ErrorMessage ---

ErrorMessage ErrorMessage::decode(const uint8_t* data, size_t length)
{
    ErrorMessage msg;
    if (length >= 1)
        msg.errorCode = data[0];
    return msg;
}

// --- NMInfoMessage ---
// PDL2: NMInfo := 0:1 pid:7 0:1 sc:7 [subcommand data] checksum

NMInfoMessage NMInfoMessage::decode(const uint8_t* payload, size_t length)
{
    NMInfoMessage msg;
    if (length < 3)  // pid + sc + checksum minimum
        return msg;

    msg.pid = payload[0];
    msg.sc  = payload[1];

    // Data is between sc and checksum (last byte)
    size_t dataStart = 2;
    size_t dataEnd = length - 1;  // checksum is last byte
    if (dataEnd > dataStart)
        msg.data.assign(payload + dataStart, payload + dataEnd);

    switch (msg.sc)
    {
        case 0x05:  // VoiceCount: 4 slots
            if (msg.data.size() >= 4)
            {
                for (int i = 0; i < 4; ++i)
                    msg.voiceCount[i] = msg.data[static_cast<size_t>(i)];
            }
            break;

        case 0x38:  // NewPatchInSlot: slot + pid
            if (msg.data.size() >= 2)
            {
                msg.newPatchSlot = msg.data[0];
                msg.newPatchPid  = msg.data[1];
            }
            break;

        case 0x39:  // LightMessage: 20 LED values (2 bits each), packed 3 per byte
            // PDL2: 0:1 startIndex:7 | [0:2 l2:2 l1:2 l0:2] x6 groups | 0:4 l19:2 l18:2
            if (msg.data.size() >= 1)
            {
                msg.lightStartIndex = msg.data[0] & 0x7f;
                int ledIdx = 0;
                for (size_t i = 1; i < msg.data.size() && ledIdx < 20; ++i)
                {
                    uint8_t b = msg.data[i];
                    if (ledIdx < 20) msg.lightValues[ledIdx++] = (b >> 0) & 0x03;
                    if (ledIdx < 20) msg.lightValues[ledIdx++] = (b >> 2) & 0x03;
                    if (ledIdx < 20) msg.lightValues[ledIdx++] = (b >> 4) & 0x03;
                }
            }
            break;

        case 0x3a:  // MeterMessage: 5 pairs of 7-bit values (b, a)
            // PDL2: 0:1 startIndex:7 | [0:1 b:7 0:1 a:7] x5
            if (msg.data.size() >= 1)
            {
                msg.meterStartIndex = msg.data[0] & 0x7f;
                for (int i = 0; i < 5 && (size_t)(1 + i*2 + 1) < msg.data.size(); ++i)
                {
                    msg.meterValuesB[i] = msg.data[static_cast<size_t>(1 + i*2)]     & 0x7f;
                    msg.meterValuesA[i] = msg.data[static_cast<size_t>(1 + i*2 + 1)] & 0x7f;
                }
            }
            break;

        default:
            break;
    }

    return msg;
}

// --- RequestPatchMessage ---
// PDL2: PatchHandling -> PatchCommand(pp=0x41) -> PatchManagerCommand(ssc=0x35) -> RequestPatch (empty)
// Sent with cc=0x17, has checksum

std::vector<uint8_t> RequestPatchMessage::encode() const
{
    return {
        0x41,   // pp = PatchManagerCommand
        0x35    // ssc = RequestPatch
    };
}

// --- LoadPatchMessage ---
// PDL2: PatchHandling -> PatchCommand(pp=0x41) -> LoadPatch(ssc=0x0a, slot:7, section:7, position:7)
// Sent with cc=0x17, has checksum

std::vector<uint8_t> LoadPatchMessage::encode() const
{
    return {
        0x41,   // pp = PatchManagerCommand
        0x0a,   // ssc = LoadPatch
        static_cast<uint8_t>(slot & 0x7F),
        static_cast<uint8_t>(section & 0x7F),
        static_cast<uint8_t>(position & 0x7F)
    };
}

// --- GetPatchMessage ---
// PDL2: PatchModification := 0:1 pid:7 0:1 sc:7 [GetPatchPartExtra: 0:1 payload:7] checksum
// Sent with cc=0x17, uses PatchModification branch (not PatchCommand)

std::vector<uint8_t> GetPatchMessage::encode() const
{
    // SC codes and optional payload bytes per section (from Java GetPatchMessage.java)
    struct SectionDef { uint8_t sc; int payload; }; // payload=-1 means none
    static const SectionDef defs[NumSections] = {
        { 0x20, 0x28 },  // Header
        { 0x4b, 0x01 },  // PolyModule
        { 0x4b, 0x00 },  // CommonModule
        { 0x53, 0x01 },  // PolyCable
        { 0x53, 0x00 },  // CommonCable
        { 0x4c, 0x01 },  // PolyParameter
        { 0x4c, 0x00 },  // CommonParameter
        { 0x66, -1   },  // MorphMap
        { 0x63, -1   },  // KnobMap
        { 0x61, -1   },  // ControlMap
        { 0x4e, 0x01 },  // PolyNameDump
        { 0x4e, 0x00 },  // CommonNameDump
        { 0x68, -1   },  // Note
    };

    auto& def = defs[section];
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(pid & 0x7F));
    data.push_back(def.sc);
    if (def.payload >= 0)
        data.push_back(static_cast<uint8_t>(def.payload));
    return data;
}

std::vector<GetPatchMessage> GetPatchMessage::forAllSections(int pid)
{
    std::vector<GetPatchMessage> msgs;
    msgs.reserve(NumSections);
    for (int i = 0; i < NumSections; ++i)
    {
        GetPatchMessage m;
        m.pid = pid;
        m.section = static_cast<Section>(i);
        msgs.push_back(m);
    }
    return msgs;
}

// --- PatchPacketMessage ---
// PDL2: PatchPacket := 0:1 command:1 pid:6 [embedded_stream] checksum
// cc=0x1c-0x1f, bit0 of cc=first, bit1=last

PatchPacketMessage PatchPacketMessage::decode(int cc, const uint8_t* payload, size_t length)
{
    PatchPacketMessage msg;
    msg.isFirst = (cc & 1) != 0;
    msg.isLast  = ((cc >> 1) & 1) != 0;

    if (length < 2)  // header byte + checksum minimum
        return msg;

    msg.command = (payload[0] >> 6) & 1;
    msg.pid     = payload[0] & 0x3F;

    // Data between header and checksum (last byte)
    if (length > 2)
        msg.patchData.assign(payload + 1, payload + length - 1);

    return msg;
}


// --- GetPatchListMessage ---
// PDL2: PatchHandling -> PatchCommand(pp=0x41) -> PatchManagerCommand(ssc=0x14)
//       -> GetPatchList(section:7, position:7)
// Sent with cc=0x17, has checksum

std::vector<uint8_t> GetPatchListMessage::encode() const
{
    return {
        0x41,                                           // pp = PatchManagerCommand
        0x14,                                           // ssc = GetPatchList
        static_cast<uint8_t>(section & 0x7F),
        static_cast<uint8_t>(position & 0x7F)
    };
}

// --- PatchListResponseMessage ---
// Decoded from ACK payload (bytes after pid1/type/pid2, before checksum).
// Format: unknown1:8 unknown2:8 unknown3:8 [?StringList] endmarker:7
// StringList: [?ListCmd] String [?StringList]
// ListCmd: code:7 + subdata depending on code
// String: null-terminated, up to 16 chars

PatchListResponseMessage PatchListResponseMessage::decode(const uint8_t* data, size_t length,
                                                          int requestSection, int requestPosition)
{
    PatchListResponseMessage msg;
    if (length < 4)  // 3 unknown bytes + endmarker minimum
        return msg;

    // Skip 3 unknown bytes (typically 6, 22, 1)
    size_t pos = 3;

    int section = requestSection;
    int position = requestPosition;

    // Read the endmarker — it's the last byte before the checksum.
    // But the checksum was already stripped by SysEx::decode, so the last byte
    // in data IS the checksum. The endmarker is the byte before it.
    // Actually — the ACK payload includes checksum as last byte.
    // We need to figure out where endmarker sits.
    //
    // Layout: [unknown1][unknown2][unknown3] [StringList...] [0:1 endmarker:7] [checksum]
    // The checksum is the LAST byte of the payload. The endmarker is just before it.
    // But wait — AckMessage::decode already stripped pid1/type/pid2, so `data` starts
    // at the PatchListResponse content. The checksum byte is included in the raw
    // ACK payload though, as the last byte (SysEx::decode strips F0/33/header/06 and F7,
    // but NOT the checksum within the payload).
    //
    // So: data[length-1] = checksum, data[length-2] = endmarker (with 0:1 prefix bit)
    size_t endmarkerIdx = length - 2;  // before checksum
    int endmarker = data[endmarkerIdx] & 0x7F;

    // Parse StringList entries between pos and endmarkerIdx
    while (pos < endmarkerIdx)
    {
        // Peek at the next byte to check for ListCmd
        uint8_t nextByte = data[pos] & 0x7F;

        if (nextByte == 0x01)  // NextPosition
        {
            pos++;  // consume code
            if (pos < endmarkerIdx)
            {
                position = data[pos] & 0x7F;
                pos++;
            }
        }
        else if (nextByte == 0x02)  // EmptyPosition
        {
            pos++;  // consume code
            position++;
            // String still follows per PDL2 grammar — fall through to read it
        }
        else if (nextByte == 0x03)  // NextSection
        {
            pos++;  // consume code
            if (pos + 1 < endmarkerIdx)
            {
                section = data[pos] & 0x7F;
                pos++;
                position = data[pos] & 0x7F;
                pos++;
            }
        }
        else if (nextByte == 0x05)  // RepeatedSection (after overwrite)
        {
            pos++;  // consume code
            if (pos + 2 < endmarkerIdx)
            {
                // The original Java parser records these fields as oldsection /
                // oldposition but does not apply them to the current list cursor.
                // Applying them shifts refreshed entries after StorePatch by one
                // visible bank location on some synth responses.
                pos++;  // skip unknown byte
                pos++;  // repeated section
                pos++;  // repeated position
            }
        }

        // Read the patch name: max 16 chars, and names of exactly 16 chars
        // carry NO null terminator. Reading past 16 merges the entry with the
        // following ones and shifts every later position in the bank.
        if (pos >= endmarkerIdx)
            break;

        std::string name;
        while (pos < endmarkerIdx && data[pos] != 0x00 && name.size() < 16)
        {
            name += static_cast<char>(data[pos]);
            pos++;
        }
        if (pos < endmarkerIdx && data[pos] == 0x00)
            pos++;  // consume the terminator only when present (<16-char names)

        PatchListEntry entry;
        entry.section = section;
        entry.position = position;
        entry.name = std::move(name);
        msg.entries.push_back(std::move(entry));

        position++;
    }

    // Calculate next section/position for continuation
    if (endmarker == 4)
    {
        msg.nextSection = -1;
        msg.nextPosition = -1;
    }
    else
    {
        if (position >= 99)
        {
            position = 0;
            section++;
        }
        if (section >= 9)
        {
            msg.nextSection = -1;
            msg.nextPosition = -1;
        }
        else
        {
            msg.nextSection = section;
            msg.nextPosition = position;
        }
    }

    return msg;
}

// --- NewModuleMessage ---
// PDL2: cc=0x1f (PatchPacket with both first+last bits), command=0, pid in payload
// Constructs 5 PDL2 sections: SingleModule, CableDump, ParameterDump, CustomDump, NameDump
// Each section is independently encoded and concatenated.

std::vector<uint8_t> NewModuleMessage::encode() const
{
    std::vector<uint8_t> result;
    BitStreamWriter bsw;

    // --- Section 1: SingleModule (type 48) ---
    // Format: type:8 Module(type:7 index:7 xpos:7 ypos:7)
    bsw.writeBits(48, 8);
    bsw.writeBits(typeId, 7);
    bsw.writeBits(index, 7);
    bsw.writeBits(xpos, 7);
    bsw.writeBits(ypos, 7);
    bsw.alignToByte();  // PDL2 "Section % 8" rule

    // --- Section 2: CableDump (type 82) ---
    // Format: type:8 section:1 ncables:15 [ncables*Cable]
    bsw.writeBits(82, 8);
    bsw.writeBits(section, 1);
    bsw.writeBits(0, 15);  // ncables = 0 (no cables for new module)
    bsw.alignToByte();

    // --- Section 3: ParameterDump (type 77) ---
    // Format: type:8 section:1 nmodules:7 [nmodules*Parameter]
    // Parameter := index:7 type:7 [type-specific param values]
    bsw.writeBits(77, 8);
    bsw.writeBits(section, 1);
    if (!parameterValues.empty())
    {
        bsw.writeBits(1, 7);  // nmodules = 1
        bsw.writeBits(index, 7);
        bsw.writeBits(typeId, 7);
        for (int val : parameterValues)
            bsw.writeBits(val, 7);
    }
    else
    {
        bsw.writeBits(0, 7);  // nmodules = 0 (no parameters)
    }
    bsw.alignToByte();

    // --- Section 4: CustomDump (type 91) ---
    // Format: type:8 section:1 nmodules:7 [nmodules*CustomModule]
    // CustomModule := index:7 nparams:8 [nparams*CustomValue:7]
    bsw.writeBits(91, 8);
    bsw.writeBits(section, 1);
    if (!customValues.empty())
    {
        bsw.writeBits(1, 7);  // nmodules = 1
        bsw.writeBits(index, 7);
        bsw.writeBits(static_cast<int>(customValues.size()), 8);
        for (int val : customValues)
            bsw.writeBits(val, 7);
    }
    else
    {
        bsw.writeBits(0, 7);  // nmodules = 0 (no custom values)
    }
    bsw.alignToByte();

    // --- Section 5: NameDump (type 90) ---
    // Format: type:8 section:1 nmodules:7 [nmodules*ModuleName]
    // ModuleName := index:8 String(16*chars:8/0)
    bsw.writeBits(90, 8);
    bsw.writeBits(section, 1);
    bsw.writeBits(1, 7);  // nmodules = 1
    bsw.writeBits(index, 8);
    bsw.writeString16(name);
    bsw.alignToByte();

    // Convert bitstream to 7-bit MIDI bytes
    auto patchData = bsw.toMidiBytes();

    // Build final message: pid:7 data:7*
    // (command=0 is implicit in PatchPacket, not sent in payload)
    result.push_back(static_cast<uint8_t>(pid & 0x7F));
    result.insert(result.end(), patchData.begin(), patchData.end());

    return result;
}

// --- RequestSynthSettingsMessage ---
// PDL2: PatchHandling(cc=0x17) -> pp=0x44 (RequestSynthSettings, no payload)
// Sent with cc=0x17, has checksum, slot=0, expects reply.
std::vector<uint8_t> RequestSynthSettingsMessage::encode() const
{
    return { 0x44, 0x02, 0x06, 0x08, 0x04 };
}

// --- SynthSettingsMessage ---
// Carried inside a PatchPacket (cc=0x1c-0x1f). The patch packet payload is:
//     command:1 pid:6 [embedded_stream] checksum
// where the synth-settings stream decodes to:
//     section.type:8 = 0x03
//     SynthSettings$data:
//       midiClockSource:1 midiVelScaleMin:7
//       ledsActive:1      midiVelScaleMax:7
//       midiClockBpm:8
//       localOn:1 keyboardMode:1 pedalPolarity:1 globalSync:5
//       masterTune:8
//       programChangeReceive:1 programChangeSend:1 knobMode:1 0:5
//       String$name (16*chars:8/0)
//       MidiChannelSlots4 | MidiChannelSlots1
//       ?ExtendedSynthSettings$extended

namespace
{
    void writeMidiChannelSlots(BitStreamWriter& w, const SynthSettings& s)
    {
        for (int i = 0; i < 4; ++i)
        {
            w.writeBits(0, 3);                            // 0:3
            w.writeBits(s.midiChannelSlot[i] & 0x1F, 5);  // midiChannelSlot:5
            w.writeBits(27, 8);                           // legacy Micro Modular marker
        }
    }

    bool readMidiChannelSlots(BitStream& bs, SynthSettings& s)
    {
        if (bs.remaining() < 8) return false;
        int slotcount = bs.readBits(8);
        if (slotcount != 1 && slotcount != 4) return false;
        s.slotcount = slotcount;

        for (int i = 0; i < 4; ++i)
        {
            if (bs.remaining() < 16) return false;
            bs.readBits(3);                                   // 0:3
            s.midiChannelSlot[i] = bs.readBits(5);
            bs.readBits(8);                                   // 27:8 / 0:8
        }
        return true;
    }

    bool readLegacyMidiChannelSlots(BitStream& bs, SynthSettings& s)
    {
        const auto start = bs.getPosition();

        for (int i = 0; i < 4; ++i)
        {
            if (bs.remaining() < 16)
            {
                bs.setPosition(start);
                return false;
            }

            const auto zero = bs.readBits(3);
            const auto channel = bs.readBits(5);
            const auto marker = bs.readBits(8);

            if (zero != 0 || marker != 27)
            {
                bs.setPosition(start);
                return false;
            }

            s.midiChannelSlot[i] = static_cast<int>(channel);
        }

        s.slotcount = 4;
        return true;
    }

    bool findAndReadMidiChannelSlots(BitStream& bs, SynthSettings& s)
    {
        const auto start = bs.getPosition();

        for (size_t pos = start; pos + 64 <= bs.getTotalBits() && pos <= start + 16 * 8; pos += 8)
        {
            bs.setPosition(pos);
            if (readLegacyMidiChannelSlots(bs, s))
                return true;
        }

        for (size_t pos = start; pos + 72 <= bs.getTotalBits() && pos <= start + 16 * 8; pos += 8)
        {
            bs.setPosition(pos);
            if (readMidiChannelSlots(bs, s))
                return true;
        }

        bs.setPosition(start);
        return false;
    }
}

std::vector<uint8_t> SynthSettingsMessage::encode(int pid) const
{
    BitStreamWriter bs;
    bs.writeBits(0x03, 8);                                    // section type = SynthSettings

    bs.writeBits(settings.midiClockSource & 0x01, 1);
    bs.writeBits(settings.midiVelScaleMin & 0x7F, 7);
    bs.writeBits(settings.ledsActive      & 0x01, 1);
    bs.writeBits(settings.midiVelScaleMax & 0x7F, 7);
    bs.writeBits(settings.midiClockBpm    & 0xFF, 8);
    bs.writeBits(settings.localOn         & 0x01, 1);
    bs.writeBits(settings.keyboardMode    & 0x01, 1);
    bs.writeBits(settings.pedalPolarity   & 0x01, 1);
    bs.writeBits(settings.globalSync      & 0x1F, 5);
    bs.writeBits(settings.masterTune      & 0xFF, 8);
    bs.writeBits(settings.programChangeReceive & 0x01, 1);
    bs.writeBits(settings.programChangeSend    & 0x01, 1);
    bs.writeBits(settings.knobMode        & 0x01, 1);
    bs.writeBits(0, 5);                                       // 0:5 padding
    bs.writeString16(settings.name);
    writeMidiChannelSlots(bs, settings);
    // Extended block intentionally omitted on send.

    auto sectionBytes = bs.toMidiBytes();

    // PatchPacket payload: 0:1 command:1 pid:6 [body]. Synth-settings replies
    // from the hardware have the same shape, with no extra sectionsEnded byte.
    std::vector<uint8_t> out;
    out.reserve(sectionBytes.size() + 1);
    out.push_back(static_cast<uint8_t>(pid & 0x3F));
    out.insert(out.end(), sectionBytes.begin(), sectionBytes.end());
    return out;
}

bool SynthSettingsMessage::decode(const std::vector<uint8_t>& patchData, SynthSettings& out)
{
    // Locate section bitstream. PatchPacketMessage::decode strips the
    // command/pid byte, so hardware replies usually start directly with the
    // packed SynthSettings section bytes.
    if (patchData.size() < 3) return false;

    // Some local callers/tests may still include legacy prefix bytes; auto-detect
    // by checking the first decoded byte for section type=0x03.
    auto trySectionAt = [&](size_t prefixBytes) -> bool
    {
        std::vector<uint8_t> body(patchData.begin() + (long) prefixBytes, patchData.end());
        BitStream bs(body);
        if (bs.remaining() < 8) return false;

        int type = bs.readBits(8);
        if (type != 0x03) return false;

        out.midiClockSource      = bs.readBits(1);
        out.midiVelScaleMin      = bs.readBits(7);
        out.ledsActive           = bs.readBits(1);
        out.midiVelScaleMax      = bs.readBits(7);
        out.midiClockBpm         = bs.readBits(8);
        out.localOn              = bs.readBits(1);
        out.keyboardMode         = bs.readBits(1);
        out.pedalPolarity        = bs.readBits(1);
        out.globalSync           = bs.readBits(5);
        out.masterTune           = bs.readBits(8);
        out.programChangeReceive = bs.readBits(1);
        out.programChangeSend    = bs.readBits(1);
        out.knobMode             = bs.readBits(1);
        bs.readBits(5);                                       // 0:5 padding
        out.name = bs.readString16();

        // If every character was replaced with '?' the offset is almost certainly
        // a false-positive type-byte match — try the next candidate offset instead.
        if (!out.name.empty() && out.name.find_first_not_of('?') == std::string::npos)
            return false;

        if (! findAndReadMidiChannelSlots(bs, out))
        {
            std::cout << "[SYNTH] Synth settings decoded without MIDI channels"
                      << " name=\"" << out.name << "\""
                      << " bitPos=" << bs.getPosition()
                      << " remaining=" << bs.remaining() << std::endl;
            return true;
        }

        std::cout << "[SYNTH] Synth settings MIDI channels:"
                  << " A=" << (out.midiChannelSlot[0] + 1)
                  << " B=" << (out.midiChannelSlot[1] + 1)
                  << " C=" << (out.midiChannelSlot[2] + 1)
                  << " D=" << (out.midiChannelSlot[3] + 1)
                  << std::endl;

        // Optional extended block
        out.hasExtended = false;
        if (bs.remaining() >= 8)
        {
            int marker = bs.readBits(8);
            if (marker == 0x07 && bs.remaining() >= 8)
            {
                bs.readBits(4);
                out.slotSelected[0] = bs.readBits(1);
                out.slotSelected[1] = bs.readBits(1);
                out.slotSelected[2] = bs.readBits(1);
                out.slotSelected[3] = bs.readBits(1);
                if (bs.remaining() >= 16 && bs.readBits(8) == 0x09)
                {
                    bs.readBits(6);
                    out.activeSlot = bs.readBits(2);
                    if (bs.remaining() >= 8 && bs.readBits(8) == 0x05 && bs.remaining() >= 32)
                    {
                        for (int i = 0; i < 4; ++i)
                            out.slotVoiceCount[i] = bs.readBits(8);
                        out.hasExtended = true;
                    }
                }
            }
        }
        return true;
    };

    if (trySectionAt(1)) return true;
    if (trySectionAt(2)) return true;
    if (trySectionAt(0)) return true;
    return false;
}
