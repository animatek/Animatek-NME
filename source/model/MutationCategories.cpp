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
