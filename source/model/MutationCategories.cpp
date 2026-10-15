#include "MutationCategories.h"

namespace
{
// "step 3" / "ctrl 12" / "note 7" → value fader; "step count" / "step" → not
bool isPrefixPlusNumber(const juce::String& name, const juce::String& prefix)
{
    if (!name.startsWith(prefix)) return false;
    auto rest = name.substring(prefix.length()).trim();
    return rest.isNotEmpty() && rest.containsOnly("0123456789");
}
}

const char* mutCategoryName(MutCategory cat)
{
    switch (cat)
    {
        case MutCategory::OscFreq:  return "OscFreq";
        case MutCategory::OscFine:  return "OscFine";
        case MutCategory::Envelope: return "Envelope";
        case MutCategory::SeqValue: return "SeqValue";
        case MutCategory::SeqEvent: return "SeqEvent";
        case MutCategory::Delays:   return "Delays";
        case MutCategory::Effects:  return "Effects";
    }
    return "?";
}

bool mutCategoryMatches(MutCategory cat,
                        const ModuleDescriptor& module,
                        const ParameterDescriptor& param)
{
    if (param.paramClass != "parameter")
        return false;

    const auto pname = param.name.toLowerCase();

    switch (cat)
    {
        case MutCategory::OscFreq:
            // "freq coarse" (OscA/B, Spectral), "pitch coarse" (OscC),
            // "pitch" (Master/Formant/Percussion Osc)
            return module.category == "Oscillator"
                && (pname == "freq coarse" || pname == "pitch coarse" || pname == "pitch");

        case MutCategory::OscFine:
            return module.category == "Oscillator"
                && (pname == "freq fine" || pname == "pitch fine" || pname == "pitchfine");

        case MutCategory::Envelope:
            return module.category == "Envelope";

        case MutCategory::SeqValue:
            // NoteSeqA "step N", CtrlSeq "ctrl N", NoteSeqB "note N"
            // (sic: modules.xml spells the category "Seqencer")
            return module.category == "Seqencer"
                && (isPrefixPlusNumber(pname, "step ")
                    || isPrefixPlusNumber(pname, "ctrl ")
                    || isPrefixPlusNumber(pname, "note "));

        case MutCategory::SeqEvent:
            // EventSeq "seq 1, step N" / "seq 2, step N" and gate toggles
            return module.category == "Seqencer"
                && (pname.startsWith("seq ") || pname.startsWith("gate"));

        case MutCategory::Delays:
            return module.name.containsIgnoreCase("Delay");

        case MutCategory::Effects:
            // G1's closest match to the G2 FX group is the Audio category
            return module.category == "Audio" && !module.name.containsIgnoreCase("Delay");
    }
    return false;
}

int getCategoryForParam(const ModuleDescriptor& module, const ParameterDescriptor& param)
{
    for (int i = 0; i < kNumMutCategories; ++i)
        if (mutCategoryMatches(static_cast<MutCategory>(i), module, param))
            return i;
    return -1;
}

// The G2's mutator gives every parameter a shape, and leaves the ones that set up
// the patch alone (G2-Emu/NOTES.md, "El Patch Mutator de Clavia"). The G1's
// modules.xml carries no such shape, so these rules give one by name, following
// what the G2 does with the same kind of control.
MutCurve getMutationCurve(const ModuleDescriptor& module, const ParameterDescriptor& param)
{
    if (param.paramClass != "parameter")
        return MutCurve::Fixed;

    const auto n = param.name.toLowerCase().trim();
    const int span = param.maxValue - param.minValue;
    if (span <= 0)
        return MutCurve::Fixed;

    // Set-up switches: what the module is, not how it sounds. The G2 never moves them.
    static const juce::StringArray fixedNames {
        "mute", "bypass", "on/off", "active", "act", "mon", "monitor", "record", "play",
        "pause", "loop", "mono", "unipolar", "uni", "invert", "inv", "inv 1", "inv 2",
        "keyboard assignment", "destination", "dest", "gain control",
        "sidechain activation", "sidechain monitor", "high freq emphasis", "-6db",
        "+6db", "-3oct", "shift", "edit position", "currentstep", "step count",
        "stepcount", "cont", "partials", "quant off", "sampling off", "lfo",
        "low delta", "col", "sym", "in", "out", "in level", "inlevel",
        "in amplification", "limiter", "ref level", "pad", "reset", "trig", "kbg",
    };
    if (fixedNames.contains(n)
        || n.startsWith("mode")
        || (n.startsWith("osc") && n.endsWith(" mute"))
        || (n.endsWith("kbt") && span <= 4)
        || (n == "range" && span <= 4))
        return MutCurve::Fixed;

    // FM amounts, modulated or not, are fine near zero, where FM goes from colour to noise.
    if (n.startsWith("fm"))
        return MutCurve::FineLow;

    // Any other modulation amount moves evenly, as the G2's "… M" knobs do.
    if (n.endsWith(" mod") || n.contains(" mod ") || n.startsWith("mod"))
        return MutCurve::Linear;

    if (n.contains("fine") || n.contains("coarse") || n.contains("detune")
        || n.endsWith("tune") || n == "pitch" || n == "transpose"
        || n.startsWith("pan") || n == "left gain" || n == "right gain"
        || n.contains("decay") || n == "d"
        || juce::CharacterFunctions::isDigit(n[0])                // graphic EQ bands: "50", "1.3"…
        || (n == "gain" && module.name.containsIgnoreCase("eq")))
        return MutCurve::Centered;

    if (n.startsWith("attack") || n == "a" || n.startsWith("release") || n == "hold"
        || n == "h" || n.startsWith("time") || n.startsWith("feedback") || n == "ratio"
        || n == "filter sweep" || n.startsWith("glide") || n.startsWith("portamento"))
        return MutCurve::FineLow;

    if ((n.startsWith("osc") && n.endsWith(" level")) || n == "click" || n == "noise"
        || n == "mlevel" || n == "slevel")
        return MutCurve::FineHigh;

    return MutCurve::Linear;
}
