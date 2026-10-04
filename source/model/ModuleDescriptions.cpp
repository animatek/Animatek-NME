#include "ModuleDescriptions.h"
#include "ModuleTags.h"
#include <algorithm>

ModuleDescriptions::ModuleDescriptions() = default;

bool ModuleDescriptions::loadFromFile(const juce::File& xmlFile)
{
    return loadFromXml(juce::XmlDocument::parse(xmlFile));
}

bool ModuleDescriptions::loadFromXmlString(const juce::String& xmlString)
{
    return loadFromXml(juce::XmlDocument::parse(xmlString));
}

bool ModuleDescriptions::loadFromXml(std::unique_ptr<juce::XmlElement> xml)
{
    if (xml == nullptr)
        return false;

    modules.clear();
    indexLookup.clear();
    nameLookup.clear();

    auto* body = xml->getChildByName("body");
    if (body == nullptr)
        return false;

    for (auto* elem = body->getFirstChildElement(); elem != nullptr; elem = elem->getNextElement())
    {
        if (elem->getTagName() == "module")
            parseModule(*elem);
    }

    return !modules.empty();
}

void ModuleDescriptions::parseModule(const juce::XmlElement& elem)
{
    ModuleDescriptor desc;
    desc.componentId = elem.getStringAttribute("component-id");
    desc.index       = elem.getIntAttribute("index", 0);
    desc.name        = elem.getStringAttribute("name");
    desc.fullname    = elem.getStringAttribute("fullname", desc.name);
    desc.category    = elem.getStringAttribute("category", "Other");
    desc.tags        = getModuleTags(desc.name);
    if (desc.category == "Seqencer") desc.category = "Sequencer";  // typo in modules.xml
    if (desc.category == "Morph" || elem.getStringAttribute("role") == "morph")
        desc.instantiable = false;

    // Parse child elements
    for (auto* child = elem.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        const auto tag = child->getTagName();

        if (tag == "parameter")
        {
            ParameterDescriptor p;
            p.componentId  = child->getStringAttribute("component-id");
            p.index        = child->getIntAttribute("index", 0);
            p.name         = child->getStringAttribute("name");
            p.paramClass   = child->getStringAttribute("class", "parameter");
            p.defaultValue = child->getIntAttribute("defaultValue", 0);
            p.minValue     = child->getIntAttribute("minValue", 0);
            p.maxValue     = child->getIntAttribute("maxValue", 127);
            p.formatter    = child->getStringAttribute("formatter");
            p.noteFormatter= child->getStringAttribute("noteFormatter");
            p.extension    = child->getStringAttribute("extension");
            p.role         = child->getStringAttribute("role");
            desc.parameters.push_back(std::move(p));
        }
        else if (tag == "connector")
        {
            ConnectorDescriptor c;
            c.componentId = child->getStringAttribute("component-id");
            c.index       = child->getIntAttribute("index", 0);
            c.name        = child->getStringAttribute("name");
            c.isOutput    = child->getStringAttribute("type") == "output";
            c.signalType  = signalTypeFromString(child->getStringAttribute("signal"));
            desc.connectors.push_back(std::move(c));
        }
        else if (tag == "light")
        {
            LightDescriptor l;
            l.componentId = child->getStringAttribute("component-id");
            l.index       = child->getIntAttribute("index", 0);
            l.name        = child->getStringAttribute("name");
            l.minValue    = child->getIntAttribute("minValue", 0);
            l.maxValue    = child->getIntAttribute("maxValue", 127);

            auto typeStr = child->getStringAttribute("type");
            if (typeStr == "led")          l.type = LightDescriptor::Led;
            else if (typeStr == "ledarray") l.type = LightDescriptor::LedArray;
            else                           l.type = LightDescriptor::Meter;

            desc.lights.push_back(std::move(l));
        }
        else if (tag == "attribute")
        {
            auto attrName = child->getStringAttribute("name");
            auto attrVal  = child->getStringAttribute("value");

            if (attrName == "cycles")        desc.cycles    = attrVal.getDoubleValue();
            else if (attrName == "prog-mem") desc.progMem   = attrVal.getDoubleValue();
            else if (attrName == "x-mem")    desc.xMem      = attrVal.getDoubleValue();
            else if (attrName == "y-mem")    desc.yMem      = attrVal.getDoubleValue();
            else if (attrName == "dyn-mem")  desc.dynMem    = attrVal.getDoubleValue();
            else if (attrName == "height")   desc.height    = attrVal.getIntValue();
            else if (attrName == "limit")    desc.limit     = attrVal.getIntValue();
            else if (attrName == "background")
                desc.background = juce::Colour::fromString("ff" + attrVal.trimCharactersAtStart("#"));
            else if (attrName == "instantiable")
                desc.instantiable = (attrVal != "false" && attrVal != "no");
        }
    }

    auto idx = modules.size();
    modules.push_back(std::move(desc));
    indexLookup[modules[idx].index] = idx;
    nameLookup[modules[idx].name] = idx;
}

const ModuleDescriptor* ModuleDescriptions::getModuleByIndex(int index) const
{
    auto it = indexLookup.find(index);
    if (it != indexLookup.end())
        return &modules[it->second];
    return nullptr;
}

const ModuleDescriptor* ModuleDescriptions::getModuleByName(const juce::String& name) const
{
    auto it = nameLookup.find(name);
    if (it != nameLookup.end())
        return &modules[it->second];
    return nullptr;
}

juce::StringArray ModuleDescriptions::getCategories() const
{
    juce::StringArray cats;
    for (auto& m : modules)
        if (m.instantiable && !cats.contains(m.category))
            cats.add(m.category);
    return cats;
}

// The order the original editor lays each category's modules out in (issue
// #81). modules.xml runs by module id, which is nothing a user would expect to
// find. A module missing from its list, as a new one in modules.xml would be,
// goes after the listed ones in the file's own order.
static const std::map<juce::String, std::vector<const char*>>& originalModuleOrder()
{
    static const std::map<juce::String, std::vector<const char*>> order {
        { "In/Out",     { "Keyboard", "KeyboardPatch", "MIDIGlobal", "AudioIn", "PolyAreaIn",
                          "1Output", "2Output", "4Output", "NoteDetect", "KeybSplit" } },
        { "Oscillator", { "MasterOsc", "OscA", "OscB", "OscC", "SpectralOsc", "FormantOsc",
                          "OscSlvA", "OscSlvB", "OscSlvC", "OscSlvD", "OscSlvE",
                          "OscSineBank", "OscSlvFM", "Noise", "PercOsc", "DrumSynth" } },
        { "LFO",        { "LFOA", "LFOB", "LFOC", "LFOSlvA", "LFOSlvB", "LFOSlvC", "LFOSlvD",
                          "LFOSlvE", "ClkGen", "ClkRndGen", "RndStepGen", "RandomGen",
                          "RndPulsGen", "PatternGen" } },
        { "Envelope",   { "ADSR", "AD-Env", "Mod-Env", "AHD", "Multi-Env", "EnvFollower" } },
        { "Filter",     { "FilterA", "FilterB", "FilterC", "FilterD", "FilterE", "FilterF",
                          "VocalFilter", "Vocoder", "FilterBank", "EqMid", "EqShelving" } },
        { "Mixer",      { "Mixer (3)", "Mixer (8)", "Amplifier", "X-Fade", "Pan", "1to2Fade",
                          "2to1Fade", "LevMult", "LevAdd", "OnOff", "4-1Switch", "1-4Switch",
                          "GainControl" } },
        { "Audio",      { "Clip", "Overdrive", "WaveWrap", "Quantizer", "InvLevShift",
                          "Sample&Hold", "Diode", "StereoChorus", "Phaser", "Delay", "Shaper",
                          "Compressor", "Expander", "RingMod", "Digitizer" } },
        { "Control",    { "Constant", "Smooth", "PortamentoA", "PortamentoB", "NoteScaler",
                          "NoteQuant", "KeyQuant", "PartialGen", "ControlMixer", "NoteVelScal" } },
        { "Logic",      { "Pulse", "PosEdgeDelay", "NegEdgeDelay", "LogicDelay", "LogicInv",
                          "LogicProc", "CompareLev", "CompareAB", "ClkDiv", "ClkDivFix" } },
        { "Seqencer",   { "EventSeq", "CtrlSeq", "NoteSeqA", "NoteSeqB" } },
    };
    return order;
}

std::vector<const ModuleDescriptor*> ModuleDescriptions::getModulesInCategory(const juce::String& category) const
{
    std::vector<const ModuleDescriptor*> result;
    for (auto& m : modules)
        if (m.instantiable && m.category == category)
            result.push_back(&m);

    const auto found = originalModuleOrder().find(category);
    if (found != originalModuleOrder().end())
    {
        const auto& names = found->second;
        const auto rank = [&names](const ModuleDescriptor* d) {
            for (size_t i = 0; i < names.size(); ++i)
                if (d->name == names[i])
                    return i;
            return names.size();
        };
        std::stable_sort(result.begin(), result.end(),
                         [&rank](const ModuleDescriptor* a, const ModuleDescriptor* b) {
                             return rank(a) < rank(b);
                         });
    }
    return result;
}

double ModuleDescriptions::getAttributeDouble(const juce::XmlElement& moduleElem, const juce::String& attrName, double defaultVal) const
{
    for (auto* child = moduleElem.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->getTagName() == "attribute" && child->getStringAttribute("name") == attrName)
            return child->getStringAttribute("value").getDoubleValue();
    return defaultVal;
}

int ModuleDescriptions::getAttributeInt(const juce::XmlElement& moduleElem, const juce::String& attrName, int defaultVal) const
{
    for (auto* child = moduleElem.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->getTagName() == "attribute" && child->getStringAttribute("name") == attrName)
            return child->getStringAttribute("value").getIntValue();
    return defaultVal;
}

juce::String ModuleDescriptions::getAttributeString(const juce::XmlElement& moduleElem, const juce::String& attrName, const juce::String& defaultVal) const
{
    for (auto* child = moduleElem.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->getTagName() == "attribute" && child->getStringAttribute("name") == attrName)
            return child->getStringAttribute("value");
    return defaultVal;
}
