#include "ParameterEncoder.h"
#include <iostream>
#include <set>

std::vector<int> ParameterEncoder::getParameterBitWidths(int moduleType)
{
    // PDL2 Param<N> structures from patch.pdl2
    // Returns the bit width for each parameter in order

    switch (moduleType)
    {
        // Param3 - Out (module 3)
        case 3:
            // level:7
            return {7};

        // Param4 - Out2 (module 4)
        case 4:
            // level:7 destination:2 mute:1
            return {7, 2, 1};

        // Param5 - Out4 (module 5)
        case 5:
            // level:7 destination:3 mute:1
            return {7, 3, 1};

        // Param7 - OscA (module 7)
        case 7:
            // freqCoarse:7 freqFine:7 freqKbt:7 pulseWidth:7 waveform:2
            // pitchMod1:7 pitchMod2:7 fmaMod:7 pwMod:7 mute:1
            return {7, 7, 7, 7, 2, 7, 7, 7, 7, 1};

        // Param8 - OscB (module 8)
        case 8:
            // freqCoarse:7 freqFine:7 freqKbt:7 waveform:2
            // pitchMod1:7 pitchMod2:7 fmaMod:7 pulseWidth:7 Mute:1
            return {7, 7, 7, 2, 7, 7, 7, 7, 1};

        // Param9 - OscSlvA (module 9)
        case 9:
            // pitchCoarse:7 pitchFine:7 pitchKbt:1 pitchModAmount:7 fma:7 mute:1
            return {7, 7, 1, 7, 7, 1};

        // Param10 - OscSlvB (module 10)
        case 10:
            // detuneCoarse:7 detuneFine:7 pulseWidth:7 pwMod:7 mute:1
            return {7, 7, 7, 7, 1};

        // Param11 - OscSlvC (module 11)
        case 11:
            // detuneCoarse:7 detuneFine:7 fmaMod:7 mute:1
            return {7, 7, 7, 1};

        // Param12 - OscSlvD (module 12)
        case 12:
            // detuneCoarse:7 detuneFine:7 fmaMod:7 mute:1
            return {7, 7, 7, 1};

        // Param13 - OscC (module 13)
        case 13:
            // detuneCoarse:7 detuneFine:7 fmaMod:7 mute:1
            return {7, 7, 7, 1};

        // Param14 - OscD (module 14)
        case 14:
            // detuneCoarse:7 detuneFine:7 shape:2 fmaMod:7 mute:1
            return {7, 7, 2, 7, 1};

        // Param15 - NoteSeqA (module 15)
        case 15:
            // step1:7..step16:7 stepCount:7 editPosition:5 record:1 pause:1 active:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                    7, 5, 1, 1, 1};

        // Param16 - EnvD (module 16)
        case 16:
            // time:7
            return {7};

        // Param17 - EventSeq (module 17)
        case 17:
            // stepcount:7 active:1 gate1:1 gate2:1
            // seq1step1:1..seq1step16:1 seq2step1:1..seq2step16:1
            return {7, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

        // Param18 - Xfade (module 18)
        case 18:
            // modulation:7 crossfade:7
            return {7, 7};

        // Param19 - Mix3-1 (module 19)
        case 19:
            // inSense1:7 inSense2:7 inSense3:7
            return {7, 7, 7};

        // Param20 - ADSR (module 20)
        case 20:
            // attackShape:2 attack:7 decay:7 sustain:7 release:7 invert:1
            return {2, 7, 7, 7, 7, 1};

        // Param21 - Compressor (module 21)
        case 21:
            // attack:7 release:7 threshold:6 ratio:7 refLevel:6 limiter:5 act:1 mon:1 bypass:1
            return {7, 7, 6, 7, 6, 5, 1, 1, 1};

        // Param22 - ClipAmp (module 22)
        case 22:
            // range:7
            return {7};

        // Param23 - EnvADSR (module 23)
        case 23:
            // attack:7 decay:7 sustain:7 release:7 attackMod:7 decayMod:7 sustainMod:7 releaseMod:7 invert:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 1};

        // Param24 - LFOA (module 24)
        case 24:
            // rate:7 range:2 waveform:3 rateMod:7 mono:1 rateKbt:7 phase:7 mute:1
            return {7, 2, 3, 7, 1, 7, 7, 1};

        // Param25 - LFOB (module 25)
        case 25:
            // rate:7 range:2 phase:7 rateMod:7 mono:1 rateKbt:7 pwMod:7 pulseWidth:7
            return {7, 2, 7, 7, 1, 7, 7, 7};

        // Param26 - LFOC (module 26)
        case 26:
            // rate:7 range:2 waveform:3 rateMod:7 mono:1 mute:1
            return {7, 2, 3, 7, 1, 1};

        // Param40 - Mixer8 (module 40)
        case 40:
            // inSense1:7 inSense2:7 inSense3:7 inSense4:7 inSense5:7 inSense6:7 inSense7:7 inSense8:7 attenuate:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 1};

        // Param44 - GainControl (module 44)
        case 44:
            // shift:1
            return {1};

        // Param47 - Pan (module 47)
        case 47:
            // panMod:7 pan:7
            return {7, 7};

        // Param49 - FilterD (module 49)
        case 49:
            // freq:7 kbt:7 resonance:7 freqMod:7
            return {7, 7, 7, 7};

        // Param50 - FilterC (module 50)
        case 50:
            // freq:7 resonance:7 gainControl:1
            return {7, 7, 1};

        // Param51 - FilterE (module 51)
        case 51:
            // filterType:2 gainControl:1 frequencyMod:7 frequency:7 kbt:7 resonanceMod:7 resonance:7 slope:1 frequencyMod2:7 bypass:1
            return {2, 1, 7, 7, 7, 7, 7, 1, 7, 1};

        // Param52 - Multi-Env (module 52)
        case 52:
            // level1:7 level2:7 level3:7 level4:7 time1:7 time2:7 time3:7 time4:7 time5:7 sustain:3 curve:2
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 2};

        // Param90 - NoteSeqB (module 90)
        case 90:
            // note1:7..note16:7 step:7 currentstep:5 record:1 play:1 loop:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                    7, 5, 1, 1, 1};

        // Param27 - ClkRndA (module 27)
        case 27:
            return {7};

        // Param28 - ClkRndB (module 28)
        case 28:
            return {7};

        // Param29 - ClkRndC (module 29)
        case 29:
            return {7};

        // Param30 - ClkRndD (module 30)
        case 30:
            return {7};

        // Param31 - Noise (module 31)
        case 31:
            return {7};

        // Param32 - EqShelving (module 32)
        case 32:
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 1};

        // Param33 - Porta (module 33)
        case 33:
            return {1, 1};

        // Param34 - ClkGen (module 34)
        case 34:
            return {7};

        // Param35 - RndStepGen (module 35)
        case 35:
            return {7};

        // Param36 - DigDelay (module 36)
        case 36:
            return {7};

        // Param37 - DigDelay2 (module 37)
        case 37:
            return {7};

        // Param38 - DelayA (module 38)
        case 38:
            return {7};

        // Param39 - DelayB (module 39)
        case 39:
            return {7};

        // Param43 - Constant (module 43)
        case 43:
            return {7, 1};

        // Param45 - Vocoder (module 45)
        case 45:
            return {4, 4, 4, 7, 7, 7, 7, 7, 7};

        // Param46 - EnvAHD (module 46)
        case 46:
            return {7, 7, 7, 7, 7, 7};

        // Param48 - Reverb (module 48)
        case 48:
            return {7};

        // Param54 - BitCrusher (module 54)
        case 54:
            return {4};

        // Param57 - Logic (module 57)
        case 57:
            return {2, 1};

        // Param58 - DrumSynth (module 58)
        case 58:
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 2, 7, 7, 7, 7, 1};

        // Param59 - Mix1-1 (module 59)
        case 59:
            return {7};

        // Param61 - Clip (module 61)
        case 61:
            return {7, 7, 1};

        // Param62 - Overdrive (module 62)
        case 62:
            return {7, 7};

        // Param64 - Delay (module 64)
        case 64:
            return {7};

        // Param66 - Mix2-1 (module 66)
        case 66:
            return {1, 7, 1, 7, 1};

        // Param67 - NoteDetect (module 67)
        case 67:
            return {7};

        // Param68 - PulseClock (module 68)
        case 68:
            return {7, 1};

        // Param69 - ClkDiv (module 69)
        case 69:
            return {7};

        // Param71 - EnvFollower (module 71)
        case 71:
            return {7, 7};

        // Param72 - Transpose (module 72)
        case 72:
            return {7};

        // Param73 - CompLev (module 73)
        case 73:
            return {2};

        // Param74 - WaveWrap (module 74)
        case 74:
            return {7, 7};

        // Param75 - NoteQuant (module 75)
        case 75:
            return {7, 7};

        // Param76 - KeyboardPatch (module 76)
        case 76:
            return {1};

        // Param78 - Flanger (module 78)
        case 78:
            return {7, 7};

        // Param79 - Mix4-1 (module 79)
        case 79:
            return {2, 7, 7, 7, 7, 1};

        // Param80 - LFOShpA (module 80)
        case 80:
            return {7, 7, 3, 1, 1};

        // Param81 - Amplifier (module 81)
        case 81:
            return {7};

        // Param82 - CompSig (module 82)
        case 82:
            return {2};

        // Param83 - Mux (module 83)
        case 83:
            return {3};

        // Param84 - AD (module 84)
        case 84:
            return {7, 7, 1};

        // Param85 - OscE (module 85)
        case 85:
            return {7, 7, 1, 7, 1};

        // Param86 - FilterA (module 86)
        case 86:
            return {7};

        // Param87 - FilterB (module 87)
        case 87:
            return {7};

        // Param88 - Mix2-1B (module 88)
        case 88:
            return {2, 7, 1};

        // Param91 - CtrlSeq (module 91)
        case 91:
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                    7, 1, 1};

        // Param92 - FilterF (module 92)
        case 92:
            return {7, 7, 7, 7, 7, 2, 1};

        // Param94 - Phaser (module 94)
        case 94:
            return {7, 7, 1};

        // Param95 - PercOsc (module 95)
        case 95:
            return {7, 7, 7, 1, 7, 7, 1};

        // Param96 - OscSineBank (module 96)
        case 96:
            return {7, 7, 1, 1, 7, 7, 7};

        // Param97 - OscStringA (module 97)
        case 97:
            return {7, 7, 7, 7, 7};

        // Param98 - NoteScaler (module 98)
        case 98:
            return {7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

        // Param99 - ArpGen (module 99)
        case 99:
            return {7, 7, 1, 7, 1};

        // Param100 - NoteRange (module 100)
        case 100:
            return {7, 7};

        // Param102 - Phaser2 (module 102)
        case 102:
            return {7, 1, 7, 7, 7, 7, 3, 7, 1, 7, 7};

        // Param103 - EqPeak (module 103)
        case 103:
            return {7, 7, 7, 1, 7};

        // Param104 - EqShelf (module 104)
        case 104:
            return {7, 7, 1, 1, 7};

        // Param105 - Compressor2 (module 105)
        case 105:
            return {7, 7, 7, 7, 7, 7, 1, 1, 1};

        // Param106 - OscSine (module 106)
        case 106:
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                    1, 1, 1, 1, 1, 1};

        // Param107 - OscDualSaw (module 107)
        case 107:
            return {7, 7, 7, 1, 7, 7, 7, 7, 1, 1};

        // Param108 - Vocoder16 (module 108)
        case 108:
            return {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                    7, 1, 1};

        // Param110 - RndPulse (module 110)
        case 110:
            return {7};

        // Param111 - Multiplier (module 111)
        case 111:
            return {7, 1};

        // Param112 - Offset (module 112)
        case 112:
            return {7, 1};

        // Param113 - FadeIn (module 113)
        case 113:
            return {7};

        // Param114 - FadeOut (module 114)
        case 114:
            return {7};

        // Param115 - KeyVelScale (module 115)
        case 115:
            return {7, 6, 7, 6};

        // Param117 - RingMod (module 117)
        case 117:
            return {7, 7};

        // Param118 - SampleHold (module 118)
        case 118:
            return {4, 7, 7, 1, 1};

        // Param127 - Amplify (module 127)
        case 127:
            return {1};

        default:
            // Unknown module type — will fall back to all-7-bit encoding in caller.
            // Log once per unknown type to help identify missing entries.
            static std::set<int> warnedTypes;
            if (warnedTypes.find(moduleType) == warnedTypes.end())
            {
                warnedTypes.insert(moduleType);
                std::cout << "[ParameterEncoder] WARNING: unknown module type " << moduleType
                          << ", falling back to 7-bit encoding (add entry to fix)" << std::endl;
            }
            return {};
    }
}

std::vector<int> ParameterEncoder::getEncodingInfo(int moduleType)
{
    return getParameterBitWidths(moduleType);
}
