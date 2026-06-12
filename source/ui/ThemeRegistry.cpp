#include "ThemeRegistry.h"

#include <vector>

namespace
{
// Canonical colors of a third-party palette, mapped onto the editor.
// bg0 = darkest (canvas), bg2 = module/panel body, bg4 = borders/active.
struct PaletteSpec
{
    juce::Colour bg0, bg1, bg2, bg3, bg4;
    juce::Colour fg0, fg1, fg2;            // primary, secondary, muted text
    juce::Colour red, green, yellow, blue, orange, purple;
};

AppThemePalette makeApp(const PaletteSpec& p)
{
    return {
        p.bg1,                  // backgroundMain
        p.bg1,                  // backgroundPanel
        p.bg0,                  // backgroundSecondary
        p.bg3,                  // backgroundElevated
        p.bg0,                  // inputBackground
        p.bg3,                  // buttonBackground
        p.bg4,                  // buttonHover
        p.bg4.brighter(0.25f),  // buttonActive
        p.bg4,                  // borderColor
        p.bg2,                  // gridLine
        p.bg3,                  // gridLineStrong
        p.fg0,                  // textPrimary
        p.fg1,                  // textSecondary
        p.fg2,                  // textMuted
        p.yellow,               // accentActive
        p.orange,               // accentWarning
        p.green,                // accentSuccess
    };
}

ColorScheme makeCanvas(const PaletteSpec& p)
{
    // Baseline carries the structural choices (LED colors, alphas, icon styles);
    // it must run after AppTheme::setPalette so palette-derived fields match.
    ColorScheme s = createDarkTheme();

    s.gridBackground   = p.bg0;
    s.gridLines        = p.bg1;
    s.moduleBg         = p.bg2;   // opaque = override the G1 XML module colors
    s.moduleText       = p.fg0.withAlpha(0.87f);
    s.groupBoxBorder   = p.bg4;

    s.knobBase         = p.fg1;
    s.knobBorder       = p.bg4;
    s.knobGrip         = p.bg1;
    s.knobTickMark     = p.bg4;
    s.morphColor[0]    = p.red;
    s.morphColor[1]    = p.green;
    s.morphColor[2]    = p.blue;
    s.morphColor[3]    = p.yellow;

    s.connOutline      = p.bg0;
    s.connectorLine    = p.bg0;

    s.displayBg        = p.bg0;
    s.displayBorder    = p.bg4;
    s.displayText      = p.orange;

    s.buttonText       = p.fg1;
    s.buttonTextActive = p.fg0;
    s.buttonBorder     = p.bg4;
    s.resetBg          = p.bg1;
    s.resetBorder      = p.bg3;
    s.resetText        = p.fg1;

    s.cableAudio       = p.red;
    s.cableControl     = p.blue;
    s.cableLogic       = p.yellow;
    s.cableMasterSlave = p.fg2;
    s.cableUser1       = p.green;
    s.cableUser2       = p.purple;

    s.meterTrack       = p.bg3;
    s.meterBg          = p.bg0;

    s.displayCurveGreen  = p.green;
    s.displayCurveBlue   = p.blue;
    s.displayCurveWarm   = p.orange;
    s.displayCurvePurple = p.purple;
    s.displayCurveYellow = p.yellow;
    s.displayCurveRed    = p.red;

    s.iconFg           = p.fg0;
    s.snapHighlight    = p.yellow;
    s.selectionRect    = p.yellow;
    s.incrementBg      = p.bg1;
    s.incrementBorder  = p.bg4;
    s.incrementFg      = p.fg1;
    s.muteActive       = p.red;
    s.vocoderRouting   = p.green;
    s.bracketRouting   = p.fg2;
    s.slotIconActive   = p.red;
    s.slotIconInactive = p.bg4;

    return s;
}

const PaletteSpec kCatppuccinMocha = {
    juce::Colour(0xff11111b), juce::Colour(0xff181825), juce::Colour(0xff1e1e2e),
    juce::Colour(0xff313244), juce::Colour(0xff45475a),
    juce::Colour(0xffcdd6f4), juce::Colour(0xffa6adc8), juce::Colour(0xff6c7086),
    juce::Colour(0xfff38ba8), juce::Colour(0xffa6e3a1), juce::Colour(0xfff9e2af),
    juce::Colour(0xff89b4fa), juce::Colour(0xfffab387), juce::Colour(0xffcba6f7),
};

const PaletteSpec kTokyoNight = {
    juce::Colour(0xff16161e), juce::Colour(0xff1a1b26), juce::Colour(0xff24283b),
    juce::Colour(0xff292e42), juce::Colour(0xff414868),
    juce::Colour(0xffc0caf5), juce::Colour(0xffa9b1d6), juce::Colour(0xff565f89),
    juce::Colour(0xfff7768e), juce::Colour(0xff9ece6a), juce::Colour(0xffe0af68),
    juce::Colour(0xff7aa2f7), juce::Colour(0xffff9e64), juce::Colour(0xffbb9af7),
};

const PaletteSpec kGruvboxDark = {
    juce::Colour(0xff1d2021), juce::Colour(0xff282828), juce::Colour(0xff3c3836),
    juce::Colour(0xff504945), juce::Colour(0xff665c54),
    juce::Colour(0xffebdbb2), juce::Colour(0xffbdae93), juce::Colour(0xff928374),
    juce::Colour(0xfffb4934), juce::Colour(0xffb8bb26), juce::Colour(0xfffabd2f),
    juce::Colour(0xff83a598), juce::Colour(0xfffe8019), juce::Colour(0xffd3869b),
};

const PaletteSpec kDracula = {
    juce::Colour(0xff191a21), juce::Colour(0xff21222c), juce::Colour(0xff282a36),
    juce::Colour(0xff343746), juce::Colour(0xff44475a),
    juce::Colour(0xfff8f8f2), juce::Colour(0xffbfc3d1), juce::Colour(0xff6272a4),
    juce::Colour(0xffff5555), juce::Colour(0xff50fa7b), juce::Colour(0xfff1fa8c),
    juce::Colour(0xff8be9fd), juce::Colour(0xffffb86c), juce::Colour(0xffbd93f9),
};

const PaletteSpec kNord = {
    juce::Colour(0xff242933), juce::Colour(0xff2e3440), juce::Colour(0xff3b4252),
    juce::Colour(0xff434c5e), juce::Colour(0xff4c566a),
    juce::Colour(0xffeceff4), juce::Colour(0xffd8dee9), juce::Colour(0xff7b88a1),
    juce::Colour(0xffbf616a), juce::Colour(0xffa3be8c), juce::Colour(0xffebcb8b),
    juce::Colour(0xff81a1c1), juce::Colour(0xffd08770), juce::Colour(0xffb48ead),
};

// Menu item IDs 70..79 are reserved for themes in MainComponent's View menu,
// so the registry must not grow past 10 entries.
const std::vector<EditorTheme>& themes()
{
    static const std::vector<EditorTheme> list = {
        { "Classic",          AppTheme::palette(AppThemeId::SoftDarkGrey), createClassicTheme },
        { "Dark",             AppTheme::palette(AppThemeId::SoftDarkGrey), createDarkTheme },
        { "Deep Dark",        AppTheme::palette(AppThemeId::DeepDarkGrey), createDarkTheme },
        { "Catppuccin Mocha", makeApp(kCatppuccinMocha), []{ return makeCanvas(kCatppuccinMocha); } },
        { "Tokyo Night",      makeApp(kTokyoNight),      []{ return makeCanvas(kTokyoNight); } },
        { "Gruvbox Dark",     makeApp(kGruvboxDark),     []{ return makeCanvas(kGruvboxDark); } },
        { "Dracula",          makeApp(kDracula),         []{ return makeCanvas(kDracula); } },
        { "Nord",             makeApp(kNord),            []{ return makeCanvas(kNord); } },
    };
    return list;
}
}

int ThemeRegistry::count()
{
    return static_cast<int>(themes().size());
}

const EditorTheme& ThemeRegistry::get(int index)
{
    const auto& list = themes();
    if (index < 0 || index >= static_cast<int>(list.size()))
        index = 1;
    return list[static_cast<size_t>(index)];
}

juce::StringArray ThemeRegistry::names()
{
    juce::StringArray out;
    for (const auto& t : themes())
        out.add(t.name);
    return out;
}
