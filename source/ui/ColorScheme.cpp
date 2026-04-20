#include "ColorScheme.h"

static ColorScheme makeClassicTheme()
{
    ColorScheme s;
    // Canvas
    s.gridBackground  = juce::Colour(0xff12122a);
    s.gridLines       = juce::Colour(0xff1a1a3a);
    // Module panels
    s.moduleBorder    = juce::Colour(0x44000000);
    s.moduleText      = juce::Colours::black.withAlpha(0.7f);
    s.groupBoxBorder  = juce::Colour(0x44000000);
    s.moduleBg        = juce::Colour();  // transparent = use XML/descriptor color (preserve Classic behavior)
    // Knobs
    s.knobBase        = juce::Colour(0xff989898);
    s.knobBorder      = juce::Colour(0xff666666);
    s.knobGrip        = juce::Colours::white;
    s.knobTickMark    = juce::Colour(0xff333333);
    s.morphColor[0]   = juce::Colour(0xffCB4F4F);  // red
    s.morphColor[1]   = juce::Colour(0xff9AC899);  // green
    s.morphColor[2]   = juce::Colour(0xff5A5FB3);  // blue
    s.morphColor[3]   = juce::Colour(0xffE5DE45);  // yellow
    s.lockBody        = juce::Colour(0xffE0C030);
    s.lockShackle     = juce::Colour(0xffC0A020);
    // Connectors
    s.connHole        = juce::Colour(0xff111111);
    s.connOutline     = juce::Colour(0xff222222);
    // Text displays
    s.displayBg       = juce::Colour(0xff2A2560);
    s.displayBorder   = juce::Colour(0xff181440);
    s.displayText     = juce::Colour(0xff4A3FA0);
    // Buttons
    s.buttonText       = juce::Colour(0xff333333);
    s.buttonTextActive = juce::Colour(0xff111111);
    s.buttonBorder     = juce::Colour(0xff222222);
    // Reset buttons
    s.resetBg          = juce::Colour(0xff2a2a2a);
    s.resetBorder      = juce::Colour(0xff444444);
    s.resetText        = juce::Colour(0xffaaaaaa);
    // Reset dot
    s.resetDotOn       = juce::Colour(0xff44cc44);
    s.resetDotOff      = juce::Colour(0xff2a4a2a);
    // Cable signal colors
    s.cableAudio       = juce::Colour(0xffCB4F4F);
    s.cableControl     = juce::Colour(0xff5A5FB3);
    s.cableLogic       = juce::Colour(0xffE5DE45);
    s.cableMasterSlave = juce::Colour(0xffA8A8A8);
    s.cableUser1       = juce::Colour(0xff9AC899);
    s.cableUser2       = juce::Colour(0xffBB00D7);
    // LEDs/meters
    s.ledOn            = juce::Colour(0xffff2200);
    s.ledOff           = juce::Colour(0xff333333);
    s.ledAudioOn       = juce::Colour(0xffff6644);
    s.ledYellow        = juce::Colour(0xffffdd44).withAlpha(0.8f);
    s.meterLow         = juce::Colour(0xff22cc44);
    s.meterMid         = juce::Colour(0xffddcc00);
    s.meterHigh        = juce::Colour(0xffee2200);
    s.meterTrack       = juce::Colour(0xff555555);
    s.meterBg          = juce::Colour(0xff222222);
    // Custom displays
    s.displayBgCustom     = juce::Colour(0xff1a1a2e);
    s.displayBorderCustom = juce::Colour(0xff444466);
    s.displayGrid         = juce::Colour(0xff333355);
    s.displayCurveGreen   = juce::Colour(0xff55cc55);
    s.displayCurveBlue    = juce::Colour(0xff55aaff);
    s.displayCurveWarm    = juce::Colour(0xffff8844);
    s.displayCurvePurple  = juce::Colour(0xffaa55cc);
    s.displayCurveYellow  = juce::Colour(0xffaaaa55);
    s.displayCurveRed     = juce::Colour(0xffcc5555);
    // Static icons
    s.iconBg              = juce::Colour(0x44000000);
    s.iconFg              = juce::Colours::white;
    // Snap / selection
    s.snapHighlight       = juce::Colour(0xffE5DE45);
    s.selectionRect       = juce::Colours::yellow;
    s.selectionFill       = juce::Colour(0x33ffffff);
    // Connector lines
    s.connectorLine       = juce::Colour(0xff1a1a1a);
    // Increment buttons
    s.incrementBg         = juce::Colour(0xff3a3a3a);
    s.incrementBorder     = juce::Colour(0xff555555);
    s.incrementFg         = juce::Colour(0xffcccccc);
    // Mute
    s.muteActive          = juce::Colour(0xffcc4444);
    // Vocoder
    s.vocoderRouting      = juce::Colour(0xff00cc44);
    // Filter bracket
    s.bracketRouting      = juce::Colour(0xff888888);
    // SlotBar
    s.slotIconActive      = juce::Colour(0xffcc3333);
    s.slotIconInactive    = juce::Colour(0xff555577);
    return s;
}

static ColorScheme makeDarkTheme()
{
    ColorScheme s;
    // Canvas
    s.gridBackground  = juce::Colour(0xff111111);
    s.gridLines       = juce::Colour(0xff1c1c2a);
    // Module panels
    s.moduleBorder    = juce::Colour(0x44000000);
    s.moduleText      = juce::Colours::white.withAlpha(0.85f);
    s.groupBoxBorder  = juce::Colour(0xff3a3d42);
    s.moduleBg        = juce::Colour(0xff2D3033);  // uniform dark panel for all modules
    // Knobs (semi-flat option B)
    s.knobBase        = juce::Colour(0xffb8b8b8);
    s.knobBorder      = juce::Colour(0xff55585C);
    s.knobGrip        = juce::Colour(0xff3a3a3a);
    s.knobTickMark    = juce::Colour(0xff55585C);
    s.morphColor[0]   = juce::Colour(0xffCB4F4F);
    s.morphColor[1]   = juce::Colour(0xff9AC899);
    s.morphColor[2]   = juce::Colour(0xff5A5FB3);
    s.morphColor[3]   = juce::Colour(0xffE5DE45);
    s.lockBody        = juce::Colour(0xffE0C030);
    s.lockShackle     = juce::Colour(0xffC0A020);
    // Connectors
    s.connHole        = juce::Colour(0xff111111);
    s.connOutline     = juce::Colour(0xff333333);
    // Text displays
    s.displayBg       = juce::Colour(0xff1a1a1a);
    s.displayBorder   = juce::Colour(0xff55585C);
    s.displayText     = juce::Colour(0xffF37F15);
    // Buttons
    s.buttonText       = juce::Colour(0xffaaaaaa);
    s.buttonTextActive = juce::Colour(0xffffffff);
    s.buttonBorder     = juce::Colour(0xff55585C);
    // Reset buttons
    s.resetBg          = juce::Colour(0xff2a2a2a);
    s.resetBorder      = juce::Colour(0xff444444);
    s.resetText        = juce::Colour(0xffaaaaaa);
    // Reset dot
    s.resetDotOn       = juce::Colour(0xff44cc44);
    s.resetDotOff      = juce::Colour(0xff1a2a22);
    // Cable signal colors (identical to Classic)
    s.cableAudio       = juce::Colour(0xffCB4F4F);
    s.cableControl     = juce::Colour(0xff5A5FB3);
    s.cableLogic       = juce::Colour(0xffE5DE45);
    s.cableMasterSlave = juce::Colour(0xffA8A8A8);
    s.cableUser1       = juce::Colour(0xff9AC899);
    s.cableUser2       = juce::Colour(0xffBB00D7);
    // LEDs/meters
    s.ledOn            = juce::Colour(0xffff2200);
    s.ledOff           = juce::Colour(0xff333333);
    s.ledAudioOn       = juce::Colour(0xffff6644);
    s.ledYellow        = juce::Colour(0xffffdd44).withAlpha(0.8f);
    s.meterLow         = juce::Colour(0xff22cc44);
    s.meterMid         = juce::Colour(0xffddcc00);
    s.meterHigh        = juce::Colour(0xffee2200);
    s.meterTrack       = juce::Colour(0xff3a3a3a);
    s.meterBg          = juce::Colour(0xff1a1a1a);
    // Custom displays
    s.displayBgCustom     = juce::Colour(0xff1a1a2e);
    s.displayBorderCustom = juce::Colour(0xff444466);
    s.displayGrid         = juce::Colour(0xff333355);
    s.displayCurveGreen   = juce::Colour(0xff2DDCA3);
    s.displayCurveBlue    = juce::Colour(0xff55aaff);
    s.displayCurveWarm    = juce::Colour(0xffff8844);
    s.displayCurvePurple  = juce::Colour(0xffaa55cc);
    s.displayCurveYellow  = juce::Colour(0xffaaaa55);
    s.displayCurveRed     = juce::Colour(0xffcc5555);
    // Static icons
    s.iconBg              = juce::Colour(0x44000000);
    s.iconFg              = juce::Colours::white;
    // Snap / selection
    s.snapHighlight       = juce::Colour(0xffE5DE45);
    s.selectionRect       = juce::Colour(0xff4444ff);
    s.selectionFill       = juce::Colour(0x22ffffff);
    // Connector lines
    s.connectorLine       = juce::Colour(0xff2a2a2a);
    // Increment buttons
    s.incrementBg         = juce::Colour(0xff2a2a2a);
    s.incrementBorder     = juce::Colour(0xff55585C);
    s.incrementFg         = juce::Colour(0xffcccccc);
    // Mute
    s.muteActive          = juce::Colour(0xffcc4444);
    // Vocoder
    s.vocoderRouting      = juce::Colour(0xff2DDCA3);
    // Filter bracket
    s.bracketRouting      = juce::Colour(0xff888888);
    // SlotBar
    s.slotIconActive      = juce::Colour(0xffcc3333);
    s.slotIconInactive    = juce::Colour(0xff555577);
    return s;
}

const ColorScheme kClassicTheme = makeClassicTheme();
const ColorScheme kDarkTheme    = makeDarkTheme();
