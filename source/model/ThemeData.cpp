#include "ThemeData.h"

bool ThemeData::loadFromFile(const juce::File& xmlFile)
{
    if (!xmlFile.existsAsFile())
    {
        DBG("ThemeData: file not found: " + xmlFile.getFullPathName());
        return false;
    }
    return loadFromXml(juce::XmlDocument::parse(xmlFile));
}

bool ThemeData::loadFromXmlString(const juce::String& xmlString)
{
    return loadFromXml(juce::XmlDocument::parse(xmlString));
}

bool ThemeData::loadFromXml(std::unique_ptr<juce::XmlElement> xml)
{
    if (xml == nullptr)
    {
        DBG("ThemeData: failed to parse XML");
        return false;
    }

    // Root is <modules>, iterate <module> children
    for (auto* child = xml->getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        if (child->getTagName() == "module")
            parseModule(*child);
    }

    DBG("ThemeData: loaded " + juce::String(getModuleThemeCount()) + " module themes");
    return true;
}

const ModuleTheme* ThemeData::getModuleTheme(const juce::String& componentId) const
{
    auto it = themes.find(componentId);
    return (it != themes.end()) ? &it->second : nullptr;
}

void ThemeData::parseModule(const juce::XmlElement& moduleElem)
{
    ModuleTheme theme;
    theme.componentId = moduleElem.getStringAttribute("component-id");
    theme.width = moduleElem.getIntAttribute("width", 255);
    theme.height = moduleElem.getIntAttribute("height", 30);

    if (theme.componentId.isEmpty())
        return;

    for (auto* child = moduleElem.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        auto tag = child->getTagName();

        if (tag == "name")
            theme.name = child->getAllSubText().trim();
        else if (tag == "connector")
            parseConnector(*child, theme);
        else if (tag == "knob")
            parseKnob(*child, theme);
        else if (tag == "slider")
            parseSlider(*child, theme);
        else if (tag == "button")
            parseButton(*child, theme);
        else if (tag == "label")
            parseLabel(*child, theme);
        else if (tag == "textDisplay")
            parseTextDisplay(*child, theme);
        else if (tag == "light")
            parseLight(*child, theme);
        else if (tag == "resetButton")
            parseResetButton(*child, theme);
        else if (tag == "image")
        {
            auto href = child->getStringAttribute("xlink:href");
            if (href.contains("groupbox"))
            {
                ThemeGroupBox gb;
                gb.x = child->getIntAttribute("x");
                gb.y = child->getIntAttribute("y");
                gb.width  = child->getIntAttribute("width", 40);
                gb.height = child->getIntAttribute("height", 30);
                theme.groupBoxes.push_back(gb);
            }
            else if (href.contains("/images/"))
            {
                // Static waveform / decoration icon (e.g. wf_sine.png)
                // Only accept icons we can actually draw: wf_* waveforms and decoration-N
                juce::String iconName = href.fromLastOccurrenceOf("/", false, false)
                                            .upToLastOccurrenceOf(".", false, false);
                bool canDraw = iconName.startsWith("wf_")
                            || (iconName.startsWith("decoration-") && !iconName.contains("."))
                            || iconName.startsWith("ds-2-");
                if (canDraw)
                {
                    ThemeStaticIcon si;
                    si.iconName = iconName;
                    si.x      = child->getIntAttribute("x");
                    si.y      = child->getIntAttribute("y");
                    int sizeAttr = child->getIntAttribute("size", 0);
                    si.width  = child->getIntAttribute("width",  sizeAttr > 0 ? sizeAttr : 11);
                    si.height = child->getIntAttribute("height", sizeAttr > 0 ? sizeAttr : 9);
                    // width="-1" / height="-1" means "use natural PNG size"
                    if (si.width <= 0 || si.height <= 0)
                    {
                        struct NS { const char* name; int w, h; };
                        static const NS natural[] = {
                            {"decoration-1",  68, 16}, {"decoration-2",  91, 18},
                            {"decoration-5",  25, 11}, {"decoration-6",  25, 11},
                            {"decoration-7",  35, 16}, {"decoration-8",  59, 13},
                            {"decoration-9",  68, 13}, {"decoration-10", 50, 14},
                            {"decoration-11", 65, 16}, {"decoration-12", 64, 16},
                            {"decoration-13", 25, 19}, {"decoration-14", 68, 11},
                            {"decoration-15", 20, 14}, {"decoration-17", 18, 10},
                            {"decoration-18", 17, 19},
                            {"decoration-3",  25, 22},
                            {"ds-2-1", 16, 16}, {"ds-2-2", 16, 16},
                            {"ds-2-3", 16, 16}, {"ds-2-4", 16, 16},
                            {"ds-2-5", 16, 16}, {"ds-2-6", 16, 16},
                            {"ds-2-7", 16, 16}, {"ds-2-8", 16, 16}
                        };
                        for (auto& n : natural)
                            if (iconName == n.name) { if (si.width <= 0) si.width = n.w; if (si.height <= 0) si.height = n.h; }
                    }
                    theme.staticIcons.push_back(si);
                }
            }
        }
        else if (tag != "name"
                 && tag != "defs" && tag != "style"
                 && child->hasAttribute("x") && child->hasAttribute("width"))
        {
            // Custom display element (overdrive-display, LFODisplay, etc.)
            ThemeCustomDisplay cd;
            cd.type = tag;
            cd.x = child->getIntAttribute("x");
            cd.y = child->getIntAttribute("y");
            cd.width = child->getIntAttribute("width", 40);
            cd.height = child->getIntAttribute("height", 30);
            // Parse sub-elements: LFO (phase/shape/rate/waveform) + envelope (attack/decay/sustain/release/hold)
            for (auto* sub = child->getFirstChildElement(); sub != nullptr; sub = sub->getNextElement())
            {
                auto subTag = sub->getTagName();
                if (subTag == "phase")
                    cd.phaseComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "shape")
                    cd.shapeComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "rate")
                    cd.rateComponentId  = sub->getStringAttribute("component-id");
                else if (subTag == "waveform")
                    cd.fixedWaveform = sub->getIntAttribute("value", -1);
                else if (subTag == "attack")
                    cd.attackComponentId  = sub->getStringAttribute("component-id");
                else if (subTag == "decay")
                    cd.decayComponentId   = sub->getStringAttribute("component-id");
                else if (subTag == "sustain")
                    cd.sustainComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "release")
                    cd.releaseComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "hold")
                    cd.holdComponentId    = sub->getStringAttribute("component-id");
                else if (subTag == "inverse")
                    cd.inverseComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "l0") cd.levelIds[0] = sub->getStringAttribute("component-id");
                else if (subTag == "l1") cd.levelIds[1] = sub->getStringAttribute("component-id");
                else if (subTag == "l2") cd.levelIds[2] = sub->getStringAttribute("component-id");
                else if (subTag == "l3") cd.levelIds[3] = sub->getStringAttribute("component-id");
                else if (subTag == "t0") cd.timeIds[0] = sub->getStringAttribute("component-id");
                else if (subTag == "t1") cd.timeIds[1] = sub->getStringAttribute("component-id");
                else if (subTag == "t2") cd.timeIds[2] = sub->getStringAttribute("component-id");
                else if (subTag == "t3") cd.timeIds[3] = sub->getStringAttribute("component-id");
                else if (subTag == "t4") cd.timeIds[4] = sub->getStringAttribute("component-id");
                else if (subTag == "curve")
                    cd.curveComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "bandwidth")
                    cd.bwComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "frequency")
                    cd.freqComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "gain")
                    cd.gainComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "cutoff")
                    cd.cutoffComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "resonance")
                    cd.resonanceComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "type")
                    cd.typeComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "slope")
                    cd.slopeComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "gain-control")
                    cd.gainControlComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "overdrive")
                    cd.overdriveComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "clip")
                    cd.clipComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "symmetry")
                    cd.symmetryComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "wavewrap")
                    cd.wavewrapComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "threshold")
                    cd.thresholdComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "ratio")
                    cd.ratioComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "ref-level")
                    cd.refLevelComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "limiter")
                    cd.limiterComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "gate")
                    cd.gateComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "feedback")
                    cd.feedbackComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "peaks")
                    cd.peaksComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "spread")
                    cd.spreadComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "in-pos")
                {
                    cd.mmInX = sub->getIntAttribute("x");
                    cd.mmInY = sub->getIntAttribute("y");
                }
                else if (subTag == "out-pos")
                {
                    cd.mmOutX = sub->getIntAttribute("x");
                    cd.mmHpY  = sub->getIntAttribute("hp");
                    cd.mmBpY  = sub->getIntAttribute("bp");
                    cd.mmLpY  = sub->getIntAttribute("lp");
                }
                else if (subTag.startsWith("band") && subTag.length() <= 6)
                {
                    int idx = subTag.substring(4).getIntValue();
                    if (idx >= 0 && idx < 16)
                        cd.bandIds[idx] = sub->getStringAttribute("component-id");
                }
                else if (subTag.startsWith("step") && subTag.length() <= 6)
                {
                    int idx = subTag.substring(4).getIntValue();
                    if (idx >= 0 && idx < 16)
                        cd.noteStepIds[idx] = sub->getStringAttribute("component-id");
                }
            }
            theme.customDisplays.push_back(cd);
        }
        // Silently skip: image
    }

    themes[theme.componentId] = std::move(theme);
}

void ThemeData::parseConnector(const juce::XmlElement& elem, ModuleTheme& theme)
{
    // Outer <connector> has position/size/class, inner <connector> has component-id
    ThemeConnector tc;
    tc.x = elem.getIntAttribute("x");
    tc.y = elem.getIntAttribute("y");
    tc.size = elem.getIntAttribute("size", 13);
    tc.cssClass = elem.getStringAttribute("class");

    // Find inner <connector> child for component-id
    if (auto* inner = elem.getChildByName("connector"))
        tc.componentId = inner->getStringAttribute("component-id");

    if (tc.componentId.isNotEmpty())
        theme.connectors.push_back(tc);
}

void ThemeData::parseKnob(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeKnob tk;
    tk.x = elem.getIntAttribute("x");
    tk.y = elem.getIntAttribute("y");
    tk.size = elem.getIntAttribute("size", 21);

    if (auto* param = elem.getChildByName("parameter"))
        tk.componentId = param->getStringAttribute("component-id");

    theme.knobs.push_back(tk);
}

void ThemeData::parseSlider(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeSlider ts;
    ts.x = elem.getIntAttribute("x");
    ts.y = elem.getIntAttribute("y");
    ts.width = elem.getIntAttribute("width", 12);
    ts.height = elem.getIntAttribute("height", 50);
    ts.orientation = elem.getStringAttribute("orientation", "vertical");

    if (auto* param = elem.getChildByName("parameter"))
        ts.componentId = param->getStringAttribute("component-id");

    theme.sliders.push_back(ts);
}

// A polarity switch: two states, bipolar when the button is out and unipolar
// when it is in. The inherited theme XML labels *both* states "Uni" on every
// module that has one (Constant, LevMult, LevAdd, CtrlSeq), so the panel reads
// the same whichever way the switch is set and the only clue is the bevel. Name
// the states instead, from one place: keyed off the parameter the button drives
// rather than a list of module IDs, so a polarity switch that turns up in a
// later module or a hand-edited theme is labelled without touching this code
// or repeating the pair of strings in the XML (issue #69).
namespace
{
bool isPolarityParam(const juce::String& paramAlt)
{
    return paramAlt.equalsIgnoreCase("uni") || paramAlt.equalsIgnoreCase("unipolar");
}

// Index is the parameter value: 0 = off = bipolar, 1 = on = unipolar, which is
// what the hardware does ("in bipolar mode, button not depressed").
const std::vector<juce::String> kPolarityLabels { "Bip", "Uni" };
}

void ThemeData::parseButton(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeButton tb;
    tb.x = elem.getIntAttribute("x");
    tb.y = elem.getIntAttribute("y");
    tb.width = elem.getIntAttribute("width", 34);
    tb.height = elem.getIntAttribute("height", 16);
    tb.cyclic = (elem.getStringAttribute("cyclic", "true") == "true");
    tb.isIncrement = (elem.getStringAttribute("mode") == "increment");
    tb.landscape = (elem.getStringAttribute("landscape") == "true");
    tb.reversed = (elem.getStringAttribute("reverse") == "true");

    juce::String paramAlt;
    if (auto* param = elem.getChildByName("parameter"))
    {
        tb.componentId = param->getStringAttribute("component-id");
        paramAlt       = param->getStringAttribute("alt");
    }

    // Collect button labels indexed by btn index
    std::vector<juce::String> docLabels, docImageRefs;
    for (auto* btn = elem.getFirstChildElement(); btn != nullptr;
         btn = btn->getNextElement())
    {
        if (btn->getTagName() == "btn")
        {
            int idx = btn->getIntAttribute("index", 0);
            auto text = btn->getAllSubText().trim();

            // Extract image href if present (e.g. wf_sine, wf_saw, env_log...)
            juce::String imageRef;
            if (auto* img = btn->getChildByName("image"))
            {
                auto href = img->getStringAttribute("xlink:href");
                // Extract filename without path and extension: "./images/slice/wf_sine.png" → "wf_sine"
                imageRef = href.fromLastOccurrenceOf("/", false, false)
                               .upToLastOccurrenceOf(".", false, false);
                if (imageRef.isEmpty())
                    imageRef = href; // fallback to full href
            }

            while (static_cast<int>(tb.labels.size()) <= idx)
                tb.labels.push_back("");
            tb.labels[static_cast<size_t>(idx)] = text;

            while (static_cast<int>(tb.imageRefs.size()) <= idx)
                tb.imageRefs.push_back("");
            tb.imageRefs[static_cast<size_t>(idx)] = imageRef;

            docLabels.push_back(text);
            docImageRefs.push_back(imageRef);
        }
    }

    // Reversed vertical selectors are written top-to-bottom in document order,
    // and some blocks (LFOA/LFOB) number their btn indices the other way round
    // from the parameter value. Document order is the only consistent signal,
    // so re-key the labels by value: the first <btn> is the highest value.
    if (tb.reversed && docLabels.size() > 1)
    {
        tb.labels.assign(docLabels.rbegin(), docLabels.rend());
        tb.imageRefs.assign(docImageRefs.rbegin(), docImageRefs.rend());
    }

    // Polarity switches say which polarity they are in, whatever the XML labels
    // them (see isPolarityParam above). Increment and radio blocks are left
    // alone: this is only about the one-button toggle.
    if (tb.cyclic && !tb.isIncrement && isPolarityParam(paramAlt))
    {
        tb.labels = kPolarityLabels;
        tb.imageRefs.clear();   // the labels are the whole button
    }

    // Detect <call component="..." method="rnd"> (Vocoder Rnd button)
    if (auto* call = elem.getChildByName("call"))
    {
        tb.isCall     = true;
        tb.callMethod = call->getStringAttribute("method");
        tb.callValue  = call->getIntAttribute("value", 0);
    }

    // Multi-Env (m52) p10 sustain: original Nomad UI renders this as a small
    // display with up/down arrows rather than a cyclic button. Values are
    // "--", "L1", "L2", "L3", "L4". Convert to increment layout + custom labels.
    if (theme.componentId == "m52" && tb.componentId == "p10")
    {
        tb.isIncrement = true;
        tb.cyclic      = false;
        tb.landscape   = false;
        tb.labels = { "--", "L1", "L2", "L3", "L4" };
    }

    theme.buttons.push_back(tb);
}

void ThemeData::parseLabel(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeLabel tl;
    tl.x = elem.getIntAttribute("x");
    tl.y = elem.getIntAttribute("y");
    tl.width = elem.getIntAttribute("width", tl.width);
    tl.height = elem.getIntAttribute("height", tl.height);
    tl.centred = (elem.getStringAttribute("align") == "centre");
    tl.text = elem.getAllSubText().trim().replace("\\n", "\n");

    theme.labels.push_back(tl);
}

void ThemeData::parseTextDisplay(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeTextDisplay td;
    td.x = elem.getIntAttribute("x");
    td.y = elem.getIntAttribute("y");
    td.width = elem.getIntAttribute("width", 40);
    td.height = elem.getIntAttribute("height", 16);

    if (auto* param = elem.getChildByName("parameter"))
    {
        td.componentId = param->getStringAttribute("component-id");

        // Partial-ratio override: these modules use "value-64" / "fmtLFOHz" /
        // "fmtSemitones" in modules.xml but the original Nomad UI shows them
        // as partial ratios (1:1, 2:1...).
        //   m10-m14,m85  slave oscillator detune coarse (p2)
        //   m27-m30,m80  LFO slaves rate (p2)
        //   m34,m110     Random generator rate (p2)
        //   m106         OscSineBank per-osc coarse tune (p1/p4/p7/p10/p13/p16)
        //
        // Only some of them carry arrow steppers. The original gives the slave
        // oscillators a "Partials" row and the sine bank a spinner; the LFO
        // slaves and the two random generators have none, and drawing them
        // there put the arrows through LFOSlvA's Mono button and over the
        // bottom edge of LFOSlvC and LFOSlvE (issue #48). Every one of these
        // has a knob on the same parameter, so nothing loses its control.
        const auto& mod = theme.componentId;
        const auto& pid = td.componentId;

        static const juce::StringArray oscSineBankFreqParams { "p1","p4","p7","p10","p13","p16" };
        const bool sineBank    = (mod == "m106" && oscSineBankFreqParams.contains(pid));
        const bool slaveOsc    = (pid == "p2" && (mod == "m10" || mod == "m11" || mod == "m12" ||
                                                  mod == "m13" || mod == "m14" || mod == "m85"));
        const bool noArrows    = (pid == "p2" && (mod == "m27" || mod == "m28" || mod == "m29" ||
                                                  mod == "m30" || mod == "m80" ||
                                                  mod == "m34" || mod == "m110"));
        if (sineBank || slaveOsc || noArrows)
        {
            td.partialFormat     = true;
            td.partialArrows     = sineBank || slaveOsc;
            td.formatterOverride = "fmtPartials";
        }

        // Amplifier (m81) p1 gain: shows "x1.00" factor, fmtAmpGain is orphan
        // in nmformat.js (not referenced from modules.xml) but the Java editor
        // applies it here.
        if (mod == "m81" && pid == "p1")
            td.formatterOverride = "fmtAmpGain";

        // LevMult (m111) p1 multiplier: bipolar -127..+127, centre 0 at v=64.
        // LevAdd  (m112) p1 offset:     bipolar  -64..+64,  centre 0 at v=64.
        if (mod == "m111" && pid == "p1") td.formatterOverride = "fmtLevMult";
        if (mod == "m112" && pid == "p1") td.formatterOverride = "fmtLevAdd";

        // Pitch displays show frequency (Hz/kHz) instead of raw int / note name.
        // Originals had a "freq display units" toggle (custom p1) that we don't
        // expose yet; SpectralOsc already uses fmtOscHz directly in modules.xml.
        //   m97 MasterOsc p2:  raw int   → fmtOscHz
        //   m96 FormantOsc p2: raw int   → fmtOscHz
        //   m7  OscA p2:       fmtNote   → fmtOscHz
        //   m8  OscB p2:       fmtNote   → fmtOscHz
        if ((mod == "m97" || mod == "m96" || mod == "m7" || mod == "m8") && pid == "p2")
            td.formatterOverride = "fmtOscHz";

        // FilterA (m86) / FilterB (m87) p1 frequency: raw int → Hz/kHz.
        // FilterC already uses fmtFilterHz2 in modules.xml; A/B are 6dB filters
        // with the same coefficient layout as fmtFilterHz1 (504Hz centre).
        if ((mod == "m86" || mod == "m87") && pid == "p1")
            td.formatterOverride = "fmtFilterHz1";
    }

    theme.textDisplays.push_back(td);
}

void ThemeData::parseLight(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeLight tl;
    tl.componentId = elem.getStringAttribute("component-id");
    tl.x = elem.getIntAttribute("x");
    tl.y = elem.getIntAttribute("y");
    tl.width = elem.getIntAttribute("width", 7);
    tl.height = elem.getIntAttribute("height", 7);
    tl.type = elem.getStringAttribute("type", "led");
    if (elem.hasAttribute("ledOnValue"))
        tl.ledOnValue = elem.getIntAttribute("ledOnValue");

    theme.lights.push_back(tl);
}

void ThemeData::parseResetButton(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeResetButton rb;
    rb.x = elem.getIntAttribute("x");
    rb.y = elem.getIntAttribute("y");
    rb.width  = elem.getIntAttribute("width", 9);
    rb.height = elem.getIntAttribute("height", 6);

    if (auto* param = elem.getChildByName("parameter"))
    {
        rb.componentId = param->getStringAttribute("component-id");
        // Default value: read from descriptor if available, fallback 64
        rb.defaultValue = param->getIntAttribute("defaultValue", 64);
    }

    theme.resetButtons.push_back(rb);
}
