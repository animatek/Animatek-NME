#include "ModuleTags.h"

#include <string>
#include <unordered_map>

juce::String getModuleTags(const juce::String& moduleName)
{
    // Keyed by ModuleDescriptor::name (the short name in modules.xml)
    static const std::unordered_map<std::string, const char*> table = {
        // Morph
        { "Morph",        "morph macro control modulation" },

        // In/Out
        { "Keyboard",     "input note gate pitch velocity kbd keys" },
        { "AudioIn",      "input external line audio in" },
        { "4Output",      "output audio out main" },
        { "2Output",      "output audio out main stereo" },
        { "1Output",      "output audio out main mono" },
        { "KeyboardPatch", "input note gate kbd keys patch" },
        { "MIDIGlobal",   "input midi sync clock global" },
        { "NoteDetect",   "input note detect trigger gate" },
        { "KeybSplit",    "input note split zone range kbd" },
        { "PolyAreaIn",   "input poly voice audio bridge" },

        // Oscillator
        { "OscA",         "oscillator vco pitch audio fm sync waveform saw square sine triangle pulse pwm" },
        { "OscB",         "oscillator vco pitch audio fm sync waveform saw square sine triangle pulse pwm" },
        { "OscC",         "oscillator vco pitch audio sine fm" },
        { "OscSlvA",      "oscillator vco slave pitch audio waveform saw square sine triangle" },
        { "OscSlvB",      "oscillator vco slave pitch audio square pulse pwm" },
        { "OscSlvC",      "oscillator vco slave pitch audio saw sawtooth" },
        { "OscSlvD",      "oscillator vco slave pitch audio triangle" },
        { "OscSlvE",      "oscillator vco slave pitch audio sine" },
        { "OscSlvFM",     "oscillator vco slave pitch audio fm modulation" },
        { "Noise",        "noise generator white pink color random audio" },
        { "DrumSynth",    "drum percussion kick tom snare membrane noise click bass" },
        { "PercOsc",      "oscillator percussion drum decay click" },
        { "FormantOsc",   "oscillator formant vowel voice vocal" },
        { "MasterOsc",    "oscillator master pitch tune global" },
        { "OscSineBank",  "oscillator sine bank additive partial organ harmonics" },
        { "SpectralOsc",  "oscillator spectral harmonics additive" },

        // Sequencer
        { "NoteSeqA",     "sequencer seq step pattern sequencing note melody arpeggio" },
        { "NoteSeqB",     "sequencer seq step pattern sequencing note melody arpeggio" },
        { "EventSeq",     "sequencer seq step pattern sequencing trigger gate event rhythm drum" },
        { "CtrlSeq",      "sequencer seq step pattern sequencing control modulation automation" },

        // Control
        { "PortamentoA",  "glide portamento slew pitch control" },
        { "PortamentoB",  "glide portamento slew pitch control interrupt" },
        { "PartialGen",   "harmonics partial additive generator control" },
        { "Smooth",       "smooth slew lag glide utility control" },
        { "Constant",     "constant value offset fixed utility control" },
        { "ControlMixer", "mixer control modulation utility blend" },
        { "NoteScaler",   "note scale transpose range utility control" },
        { "NoteQuant",    "quantize note pitch utility control" },
        { "KeyQuant",     "quantize key scale pitch chord control" },
        { "NoteVelScal",  "velocity scale curve dynamics control" },

        // Mixer
        { "X-Fade",       "mixer crossfade morph blend mix" },
        { "Mixer (3)",    "mixer mix level blend audio utility" },
        { "Mixer (8)",    "mixer mix level blend audio utility" },
        { "GainControl",  "gain vca level amplitude am modulation mixer" },
        { "Pan",          "pan stereo panorama position mixer" },
        { "OnOff",        "switch mute toggle on off utility mixer" },
        { "4-1Switch",    "switch route selector utility mixer" },
        { "1-4Switch",    "switch route selector utility mixer" },
        { "Amplifier",    "amp vca gain level mixer" },
        { "LevMult",      "attenuator multiply phase invert level modulation utility" },
        { "LevAdd",       "offset bias add level utility" },
        { "1to2Fade",     "fader route blend pan mixer" },
        { "2to1Fade",     "fader route blend mix mixer" },

        // Envelope
        { "ADSR",         "envelope eg adsr attack decay sustain release gate modulation" },
        { "AD-Env",       "envelope eg ad attack decay percussive pluck modulation" },
        { "AHD",          "envelope eg ahd attack hold decay modulation" },
        { "Mod-Env",      "envelope eg modulated attack decay modulation" },
        { "Multi-Env",    "envelope eg multi stage complex modulation" },
        { "EnvFollower",  "envelope follower audio tracking dynamics sidechain" },

        // Audio
        { "Compressor",   "compressor limiter dynamics stereo audio" },
        { "Sample&Hold",  "sample hold s&h sh stepped random modulation" },
        { "Quantizer",    "quantize bitcrush lofi digital steps audio" },
        { "InvLevShift",  "invert level shift flip utility audio" },
        { "Clip",         "clip distortion saturate fuzz audio" },
        { "Overdrive",    "overdrive distortion saturation drive fuzz audio" },
        { "WaveWrap",     "wavefolder wrap fold distortion audio" },
        { "Delay",        "delay echo time audio" },
        { "Diode",        "diode rectifier distortion audio" },
        { "Shaper",       "waveshaper shape curve distortion audio" },
        { "StereoChorus", "chorus stereo ensemble width detune audio" },
        { "Phaser",       "phaser sweep stereo audio" },
        { "Expander",     "expander gate dynamics stereo audio" },
        { "RingMod",      "ring modulator am amplitude bell metallic audio" },
        { "Digitizer",    "bitcrush digitize lofi sample rate reduce audio" },

        // LFO
        { "LFOA",         "lfo modulation rate sync waveform saw square sine triangle phase" },
        { "LFOB",         "lfo modulation rate sync square pulse pwm" },
        { "LFOC",         "lfo modulation rate sync waveform saw square sine triangle" },
        { "LFOSlvA",      "lfo slave modulation rate waveform" },
        { "LFOSlvB",      "lfo slave modulation rate saw sawtooth" },
        { "LFOSlvC",      "lfo slave modulation rate sine" },
        { "LFOSlvD",      "lfo slave modulation rate square pulse pwm" },
        { "LFOSlvE",      "lfo slave modulation rate triangle" },
        { "ClkRndGen",    "random clocked stepped s&h sh generator modulation" },
        { "RndStepGen",   "random stepped generator modulation" },
        { "RndPulsGen",   "random pulse trigger gate generator rhythm" },
        { "RandomGen",    "random smooth generator modulation" },
        { "ClkGen",       "clock tempo sync master bpm generator sequencing" },
        { "PatternGen",   "pattern clocked random rhythm generator sequencing" },

        // Filter
        { "FilterA",      "filter vcf lowpass lp 6db tone" },
        { "FilterB",      "filter vcf highpass hp 6db tone" },
        { "FilterC",      "filter vcf multimode lowpass highpass bandpass lp hp bp static tone" },
        { "FilterD",      "filter vcf multimode lowpass highpass bandpass lp hp bp resonance sweep tone" },
        { "FilterE",      "filter vcf multimode lowpass highpass bandpass notch lp hp bp resonance tone" },
        { "FilterF",      "filter vcf lowpass classic ladder resonance lp 24db acid tone" },
        { "FilterBank",   "filter band fixed eq formant tone" },
        { "VocalFilter",  "filter vocal vowel formant wah tone" },
        { "EqMid",        "eq equalizer mid parametric tone filter" },
        { "EqShelving",   "eq equalizer shelf bass treble tone filter" },
        { "Vocoder",      "vocoder voice robot band analysis filter vocal" },

        // Logic
        { "PosEdgeDelay", "logic delay edge trigger gate utility" },
        { "NegEdgeDelay", "logic delay edge trigger gate utility" },
        { "LogicDelay",   "logic delay trigger gate utility" },
        { "Pulse",        "logic pulse trigger oneshot gate utility" },
        { "CompareLev",   "logic compare comparator threshold constant utility" },
        { "CompareAB",    "logic compare comparator threshold utility" },
        { "ClkDiv",       "logic clock divider division sync rhythm sequencing" },
        { "ClkDivFix",    "logic clock divider division sync rhythm sequencing fixed" },
        { "LogicInv",     "logic invert not gate utility" },
        { "LogicProc",    "logic and or xor boolean gate processor utility" },
    };

    auto it = table.find(moduleName.toStdString());
    return it != table.end() ? juce::String(it->second) : juce::String();
}
