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
        p.blue,                 // accentInfo
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

// --- Bitwig community themes (Berikai/awesome-bitwig-themes) -----------------
// Palettes distilled from each theme's Bitwig Theme Editor file: the window
// background seeds a synthetic 5-step ramp (preserving the tint), and the
// "Panel Red/Green/Yellow/Blue/Orange/Purple" slots map onto our accents.

const PaletteSpec kGhosty = {
    juce::Colour(0xff121212), juce::Colour(0xff202020), juce::Colour(0xff323232),
    juce::Colour(0xff444444), juce::Colour(0xff636363),
    juce::Colour(0xffffffff), juce::Colour(0xffc1c1c1), juce::Colour(0xff8b8b8b),
    juce::Colour(0xfff590b2), juce::Colour(0xffc4f0a2), juce::Colour(0xffffe7b4),
    juce::Colour(0xff92deff), juce::Colour(0xfffec0ed), juce::Colour(0xffd4c0fe),
};

const PaletteSpec kMagneticRevival = {
    juce::Colour(0xff131313), juce::Colour(0xff222222), juce::Colour(0xff343434),
    juce::Colour(0xff454545), juce::Colour(0xff646464),
    juce::Colour(0xffe0e0e0), juce::Colour(0xffababab), juce::Colour(0xff7d7d7d),
    juce::Colour(0xffc0392b), juce::Colour(0xff27ae60), juce::Colour(0xfff1c40f),
    juce::Colour(0xff809bce), juce::Colour(0xffff6361), juce::Colour(0xffbc5090),
};

// MothWig: teal-night base; "Light Text" is a pale moth-yellow by design.
const PaletteSpec kMothWig = {
    juce::Colour(0xff07171e), juce::Colour(0xff0d2937), juce::Colour(0xff203a47),
    juce::Colour(0xff344b57), juce::Colour(0xff566973),
    juce::Colour(0xfffff68f), juce::Colour(0xffbbbd76), juce::Colour(0xff818b61),
    juce::Colour(0xffdc322f), juce::Colour(0xff859900), juce::Colour(0xfffff68f),
    juce::Colour(0xff268bd2), juce::Colour(0xffcb4b16), juce::Colour(0xff6c71c4),
};

// Macchiato: the Catppuccin Macchiato variant (warmer/lighter than the original Mocha).
const PaletteSpec kMacchiato = {
    juce::Colour(0xff141520), juce::Colour(0xff24273a), juce::Colour(0xff36384a),
    juce::Colour(0xff474a5a), juce::Colour(0xff666875),
    juce::Colour(0xffcad3f5), juce::Colour(0xff9ca3c1), juce::Colour(0xff747a94),
    juce::Colour(0xffed8796), juce::Colour(0xffa6da95), juce::Colour(0xffeed49f),
    juce::Colour(0xff8aadf4), juce::Colour(0xfff5a97f), juce::Colour(0xffc6a0f6),
};

const PaletteSpec kCubitwig = {
    juce::Colour(0xff202223), juce::Colour(0xff3a3d3f), juce::Colour(0xff4a4d4e),
    juce::Colour(0xff5a5c5e), juce::Colour(0xff757779),
    juce::Colour(0xffdddddd), juce::Colour(0xffafb0b1), juce::Colour(0xff888a8b),
    juce::Colour(0xffff1c40), juce::Colour(0xff4abc00), juce::Colour(0xffffde42),
    juce::Colour(0xff5ca8ee), juce::Colour(0xffff7f17), juce::Colour(0xffc36eff),
};

// "Nord Classic": the light warm-grey look of the original Clavia Nord Modular
// editor. It is a LIGHT theme (like kFrost above) — flat, no 3D bevels — with
// black text on grey module bodies, dark knobs, and the classic signal cable
// colours (from docs/RESEARCH.md). makeCanvas gives the grey base; makeNordClassic
// then overrides the LCD-blue value displays and the master-slave cable grey that
// the generic mapping doesn't capture.
const PaletteSpec kNordClassic = {
    // bg0 = canvas: cool/lavender mid-grey sampled from the original editor's
    // textured background (avg ~#80808b), deliberately darker than the modules.
    juce::Colour(0xff80808b), juce::Colour(0xffaeaeae), juce::Colour(0xffbfbfbf),
    juce::Colour(0xff8a8a8a), juce::Colour(0xff5a5a5a),
    // Text: dark across the board — on a LIGHT theme even "muted" must stay
    // readable, so fg1/fg2 are dark greys, not the mid-greys a dark theme uses.
    juce::Colour(0xff151515), juce::Colour(0xff2a2a2a), juce::Colour(0xff4c4c4c),
    juce::Colour(0xffcb4f4f), juce::Colour(0xff3f9b3f), juce::Colour(0xffe5de45),
    juce::Colour(0xff5a5fb3), juce::Colour(0xffcf7a3a), juce::Colour(0xff9a5fb3),
};

ColorScheme makeNordClassic()
{
    ColorScheme s = makeCanvas(kNordClassic);

    // Keep grid lines subtle against the darker canvas (bg1 would read too light).
    s.gridLines = juce::Colour(0xff7a7a84);

    // Knobs: medium-dark grey dial like the original — NOT tied to the (now very
    // dark) text colour, which turned the knobs near-black. Light grip for contrast.
    s.knobBase   = juce::Colour(0xff8f8f8f);
    s.knobBorder = juce::Colour(0xff4a4a4a);
    s.knobGrip   = juce::Colour(0xff202020);
    s.knobTickMark = juce::Colour(0xff4a4a4a);

    // Blue LCD-style value readouts (e.g. "327.0Hz"): deep indigo #392f7d
    // sampled from the original editor.
    s.displayBg     = juce::Colour(0xff392f7d);
    s.displayBorder = juce::Colour(0xff211a52);
    s.displayText   = juce::Colour(0xffe8ecff);

    // Envelope/LFO/filter curve boxes read on a pale ground, not the dark base.
    s.displayBgCustom     = juce::Colour(0xffbcbcbc);
    s.displayBorderCustom = juce::Colour(0xff5a5a5a);
    s.displayGrid         = juce::Colour(0xff909090);

    // Waveform icon boxes: dark glyph on light ground.
    s.iconBg = juce::Colour(0xffc4c4c4);
    s.iconFg = juce::Colour(0xff222222);

    // Master-slave cable is a neutral grey in the original (RESEARCH.md).
    s.cableMasterSlave = juce::Colour(0xffa8a8a8);

    // Faint stucco grain over the canvas, like the original editor's desktop.
    s.canvasTexture = true;

    return s;
}

// App chrome for Nord Classic: the light Win-grey (#bfbfbf) sampled from the
// original editor's toolbar/panels — same as the module bodies, so the chrome
// reads as a distinct light band against the darker lavender canvas between them.
AppThemePalette makeNordClassicApp()
{
    AppThemePalette a = makeApp(kNordClassic);
    const auto chromeGrey = juce::Colour(0xffbfbfbf);
    a.backgroundMain       = chromeGrey;
    a.backgroundPanel      = chromeGrey;
    a.backgroundSecondary  = chromeGrey;
    // Accents that stay legible on the light chrome. The canvas keeps the bright
    // cable colours; as UI accents (status text, mutator highlights, ticks) the
    // bright yellow/green wash out on light grey, so use deeper shades here.
    a.accentActive  = juce::Colour(0xff9a7a00);  // dark amber (was bright yellow)
    a.accentSuccess = juce::Colour(0xff2e7d32);  // deeper green ("Connected ...")
    a.accentWarning = juce::Colour(0xffb5651d);  // deeper amber-orange
    return a;
}

// Theme menu items use IDs 200+ in MainComponent's View menu (see getMenuForIndex
// / menuItemSelected), so the registry can grow freely.
const std::vector<EditorTheme>& themes()
{
    static const std::vector<EditorTheme> list = {
        { "Nomad",            AppTheme::palette(AppThemeId::SoftDarkGrey), createClassicTheme },
        { "Dark",             AppTheme::palette(AppThemeId::SoftDarkGrey), createDarkTheme },
        { "Deep Dark",        AppTheme::palette(AppThemeId::DeepDarkGrey), createDarkTheme },
        { "Tokyo Night",      makeApp(kTokyoNight),      []{ return makeCanvas(kTokyoNight); } },
        { "Gruvbox Dark",     makeApp(kGruvboxDark),     []{ return makeCanvas(kGruvboxDark); } },
        { "Dracula",          makeApp(kDracula),         []{ return makeCanvas(kDracula); } },
        { "Nord",             makeApp(kNord),            []{ return makeCanvas(kNord); } },
        { "Ghosty",           makeApp(kGhosty),          []{ return makeCanvas(kGhosty); } },
        { "Magnetic Revival", makeApp(kMagneticRevival), []{ return makeCanvas(kMagneticRevival); } },
        { "MothWig",          makeApp(kMothWig),         []{ return makeCanvas(kMothWig); } },
        { "Macchiato",        makeApp(kMacchiato),       []{ return makeCanvas(kMacchiato); } },
        { "Cubitwig",         makeApp(kCubitwig),        []{ return makeCanvas(kCubitwig); } },
        { "Nord Classic",     makeNordClassicApp(),      makeNordClassic },
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
