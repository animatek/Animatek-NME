#include "ThemeFile.h"

namespace
{
struct AppField    { const char* key; juce::Colour AppThemePalette::* member; };
struct CanvasField { const char* key; juce::Colour ColorScheme::*     member; };

const AppField kAppFields[] = {
    { "backgroundMain", &AppThemePalette::backgroundMain },
    { "backgroundPanel", &AppThemePalette::backgroundPanel },
    { "backgroundSecondary", &AppThemePalette::backgroundSecondary },
    { "backgroundElevated", &AppThemePalette::backgroundElevated },
    { "inputBackground", &AppThemePalette::inputBackground },
    { "buttonBackground", &AppThemePalette::buttonBackground },
    { "buttonHover", &AppThemePalette::buttonHover },
    { "buttonActive", &AppThemePalette::buttonActive },
    { "borderColor", &AppThemePalette::borderColor },
    { "gridLine", &AppThemePalette::gridLine },
    { "gridLineStrong", &AppThemePalette::gridLineStrong },
    { "textPrimary", &AppThemePalette::textPrimary },
    { "textSecondary", &AppThemePalette::textSecondary },
    { "textMuted", &AppThemePalette::textMuted },
    { "accentActive", &AppThemePalette::accentActive },
    { "accentWarning", &AppThemePalette::accentWarning },
    { "accentSuccess", &AppThemePalette::accentSuccess },
    { "accentInfo", &AppThemePalette::accentInfo },
};

const CanvasField kCanvasFields[] = {
    { "gridBackground", &ColorScheme::gridBackground },
    { "gridLines", &ColorScheme::gridLines },
    { "moduleBorder", &ColorScheme::moduleBorder },
    { "moduleText", &ColorScheme::moduleText },
    { "groupBoxBorder", &ColorScheme::groupBoxBorder },
    { "moduleBg", &ColorScheme::moduleBg },
    { "knobBase", &ColorScheme::knobBase },
    { "knobBorder", &ColorScheme::knobBorder },
    { "knobGrip", &ColorScheme::knobGrip },
    { "knobTickMark", &ColorScheme::knobTickMark },
    { "lockBody", &ColorScheme::lockBody },
    { "lockShackle", &ColorScheme::lockShackle },
    { "connHole", &ColorScheme::connHole },
    { "connOutline", &ColorScheme::connOutline },
    { "displayBg", &ColorScheme::displayBg },
    { "displayBorder", &ColorScheme::displayBorder },
    { "displayText", &ColorScheme::displayText },
    { "buttonText", &ColorScheme::buttonText },
    { "buttonTextActive", &ColorScheme::buttonTextActive },
    { "buttonBorder", &ColorScheme::buttonBorder },
    { "resetBg", &ColorScheme::resetBg },
    { "resetBorder", &ColorScheme::resetBorder },
    { "resetText", &ColorScheme::resetText },
    { "resetDotOn", &ColorScheme::resetDotOn },
    { "resetDotOff", &ColorScheme::resetDotOff },
    { "cableAudio", &ColorScheme::cableAudio },
    { "cableControl", &ColorScheme::cableControl },
    { "cableLogic", &ColorScheme::cableLogic },
    { "cableMasterSlave", &ColorScheme::cableMasterSlave },
    { "cableUser1", &ColorScheme::cableUser1 },
    { "cableUser2", &ColorScheme::cableUser2 },
    { "ledOn", &ColorScheme::ledOn },
    { "ledOff", &ColorScheme::ledOff },
    { "ledAudioOn", &ColorScheme::ledAudioOn },
    { "ledYellow", &ColorScheme::ledYellow },
    { "meterLow", &ColorScheme::meterLow },
    { "meterMid", &ColorScheme::meterMid },
    { "meterHigh", &ColorScheme::meterHigh },
    { "meterTrack", &ColorScheme::meterTrack },
    { "meterBg", &ColorScheme::meterBg },
    { "displayBgCustom", &ColorScheme::displayBgCustom },
    { "displayBorderCustom", &ColorScheme::displayBorderCustom },
    { "displayGrid", &ColorScheme::displayGrid },
    { "displayCurveGreen", &ColorScheme::displayCurveGreen },
    { "displayCurveBlue", &ColorScheme::displayCurveBlue },
    { "displayCurveWarm", &ColorScheme::displayCurveWarm },
    { "displayCurvePurple", &ColorScheme::displayCurvePurple },
    { "displayCurveYellow", &ColorScheme::displayCurveYellow },
    { "displayCurveRed", &ColorScheme::displayCurveRed },
    { "iconBg", &ColorScheme::iconBg },
    { "iconFg", &ColorScheme::iconFg },
    { "snapHighlight", &ColorScheme::snapHighlight },
    { "selectionRect", &ColorScheme::selectionRect },
    { "selectionFill", &ColorScheme::selectionFill },
    { "connectorLine", &ColorScheme::connectorLine },
    { "incrementBg", &ColorScheme::incrementBg },
    { "incrementBorder", &ColorScheme::incrementBorder },
    { "incrementFg", &ColorScheme::incrementFg },
    { "muteActive", &ColorScheme::muteActive },
    { "vocoderRouting", &ColorScheme::vocoderRouting },
    { "bracketRouting", &ColorScheme::bracketRouting },
    { "slotIconActive", &ColorScheme::slotIconActive },
    { "slotIconInactive", &ColorScheme::slotIconInactive },
    { "knobArc", &ColorScheme::knobArc },
};

// Writes every field of `fields` that `obj` carries as a readable colour into `dst`.
template <class Obj, class Fields>
void readColours(const juce::var& json, const Fields& fields, Obj& dst)
{
    if (auto* o = json.getDynamicObject())
        for (const auto& f : fields)
        {
            const auto v = o->getProperty(f.key);
            juce::Colour c;
            if (v.isString() && ThemeFile::colourFromString(v.toString(), c))
                dst.*(f.member) = c;
        }
}

template <class Obj, class Fields>
juce::var writeColours(const Fields& fields, const Obj& src)
{
    auto* o = new juce::DynamicObject();
    for (const auto& f : fields)
        o->setProperty(f.key, ThemeFile::colourToString(src.*(f.member)));
    return juce::var(o);
}
}

juce::String ThemeFile::colourToString(juce::Colour c)
{
    const auto hex = juce::String::toHexString(static_cast<int>(c.getARGB())).paddedLeft('0', 8);
    return "#" + (c.getAlpha() == 0xff ? hex.substring(2) : hex);
}

bool ThemeFile::colourFromString(const juce::String& s, juce::Colour& out)
{
    auto t = s.trim();
    if (t.startsWithChar('#'))
        t = t.substring(1);
    if ((t.length() != 6 && t.length() != 8) || !t.containsOnly("0123456789abcdefABCDEF"))
        return false;
    const auto v = static_cast<juce::uint32>(t.getHexValue64());
    out = juce::Colour(t.length() == 6 ? (0xff000000u | v) : v);
    return true;
}

juce::String ThemeFile::toJson(const EditorTheme& theme, const juce::String& baseName)
{
    // The canvas factories read the active app palette, so make it this theme's.
    const auto saved = AppTheme::palette();
    AppTheme::setPalette(theme.app);
    const ColorScheme cs = theme.makeCanvas();
    AppTheme::setPalette(saved);

    auto* root = new juce::DynamicObject();
    root->setProperty("format", kFormat);
    root->setProperty("name", theme.name);
    root->setProperty("base", baseName.isNotEmpty() ? baseName : theme.name);
    root->setProperty("app", writeColours(kAppFields, theme.app));

    auto canvas = writeColours(kCanvasFields, cs);
    juce::Array<juce::var> morph;
    for (const auto& c : cs.morphColor)
        morph.add(colourToString(c));
    canvas.getDynamicObject()->setProperty("morphColor", morph);
    canvas.getDynamicObject()->setProperty("canvasTexture", cs.canvasTexture);
    root->setProperty("canvas", canvas);

    return juce::JSON::toString(juce::var(root), false);
}

bool ThemeFile::fromJson(const juce::String& text, const EditorTheme& fallbackBase,
                         EditorTheme& out, juce::String& error)
{
    juce::var json;
    if (juce::JSON::parse(text, json).failed() || json.getDynamicObject() == nullptr)
    {
        error = "not a JSON object";
        return false;
    }
    if (static_cast<int>(json.getProperty("format", 0)) != kFormat)
    {
        error = "unknown theme file format";
        return false;
    }
    const auto name = json.getProperty("name", {}).toString().trim();
    if (name.isEmpty())
    {
        error = "the theme has no name";
        return false;
    }

    // The file's own "base" when it names a built-in theme, else the fallback.
    const auto* named = ThemeRegistry::findBuiltin(json.getProperty("base", {}).toString());
    const EditorTheme& base = named != nullptr ? *named : fallbackBase;

    AppThemePalette app = base.app;
    readColours(json.getProperty("app", {}), kAppFields, app);

    // The base's canvas, built against the base's own palette, then the overrides.
    const auto saved = AppTheme::palette();
    AppTheme::setPalette(base.app);
    ColorScheme cs = base.makeCanvas();
    AppTheme::setPalette(saved);

    const auto canvasJson = json.getProperty("canvas", {});
    readColours(canvasJson, kCanvasFields, cs);
    if (const auto* morph = canvasJson.getProperty("morphColor", {}).getArray())
        for (int i = 0; i < 4 && i < morph->size(); ++i)
        {
            juce::Colour c;
            if ((*morph)[i].isString() && colourFromString((*morph)[i].toString(), c))
                cs.morphColor[i] = c;
        }
    if (canvasJson.getProperty("canvasTexture", {}).isBool())
        cs.canvasTexture = static_cast<bool>(canvasJson.getProperty("canvasTexture", {}));

    out.name = name;
    out.app = app;
    out.makeCanvas = [cs] { return cs; };
    return true;
}

juce::File ThemeFile::userFolder()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "AnimatekNME";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    return options.getDefaultFile().getParentDirectory().getChildFile("themes");
}

std::vector<EditorTheme> ThemeFile::loadFolder(const juce::File& folder, const EditorTheme& fallbackBase)
{
    std::vector<EditorTheme> found;
    if (!folder.isDirectory())
        return found;

    for (const auto& f : folder.findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        EditorTheme t;
        juce::String error;
        if (fromJson(f.loadFileAsString(), fallbackBase, t, error))
            found.push_back(std::move(t));
        else
            juce::Logger::writeToLog("Theme file " + f.getFileName() + " skipped: " + error);
    }
    std::sort(found.begin(), found.end(),
              [](const EditorTheme& a, const EditorTheme& b) { return a.name.compareIgnoreCase(b.name) < 0; });
    return found;
}
