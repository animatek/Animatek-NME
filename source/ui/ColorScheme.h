#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct ColorScheme
{
    // Canvas
    juce::Colour gridBackground;
    juce::Colour gridLines;

    // Module panels
    juce::Colour moduleBorder;
    juce::Colour moduleText;
    juce::Colour groupBoxBorder;
    juce::Colour moduleBg;  // if opaque: overrides descriptor background; if transparent: use XML color

    // Knobs
    juce::Colour knobBase;           // fill color for non-morph knobs
    juce::Colour knobBorder;         // outline when no morph
    juce::Colour knobGrip;           // position indicator line
    juce::Colour knobTickMark;       // limit tick marks at +-135 degrees
    juce::Colour morphColor[4];      // morph group colors [0]=red [1]=green [2]=blue [3]=yellow
    juce::Colour lockBody;           // padlock body
    juce::Colour lockShackle;        // padlock shackle arc

    // Connectors
    juce::Colour connHole;           // dark hole inside connector
    juce::Colour connOutline;        // connector ring outline

    // Text displays
    juce::Colour displayBg;
    juce::Colour displayBorder;
    juce::Colour displayText;

    // Buttons (cyclic / toggle)
    juce::Colour buttonText;
    juce::Colour buttonTextActive;
    juce::Colour buttonBorder;

    // Reset buttons
    juce::Colour resetBg;
    juce::Colour resetBorder;
    juce::Colour resetText;

    // Reset dot (default indicator)
    juce::Colour resetDotOn;
    juce::Colour resetDotOff;

    // Cable / connector signal colors (same both themes)
    juce::Colour cableAudio;
    juce::Colour cableControl;
    juce::Colour cableLogic;
    juce::Colour cableMasterSlave;
    juce::Colour cableUser1;
    juce::Colour cableUser2;

    // LED/meter states
    juce::Colour ledOn;
    juce::Colour ledOff;
    juce::Colour ledAudioOn;
    juce::Colour ledYellow;
    juce::Colour meterLow;
    juce::Colour meterMid;
    juce::Colour meterHigh;
    juce::Colour meterTrack;
    juce::Colour meterBg;        // meter track background (dark fill behind bars)

    // Custom displays (envelopes, LFO, filter curves)
    juce::Colour displayBgCustom;
    juce::Colour displayBorderCustom;
    juce::Colour displayGrid;
    juce::Colour displayCurveGreen;
    juce::Colour displayCurveBlue;
    juce::Colour displayCurveWarm;
    juce::Colour displayCurvePurple;
    juce::Colour displayCurveYellow;
    juce::Colour displayCurveRed;

    // Static waveform icon boxes
    juce::Colour iconBg;
    juce::Colour iconFg;

    // Snap highlight (parameter snap indicator)
    juce::Colour snapHighlight;

    // Rubber-band selection rect
    juce::Colour selectionRect;
    juce::Colour selectionFill;

    // Connector/knob linking lines
    juce::Colour connectorLine;

    // Increment buttons (▲▼ spinners)
    juce::Colour incrementBg;
    juce::Colour incrementBorder;
    juce::Colour incrementFg;

    // Mute button active state
    juce::Colour muteActive;

    // Vocoder routing display
    juce::Colour vocoderRouting;

    // Filter bracket routing lines
    juce::Colour bracketRouting;

    // SlotBar synth icon
    juce::Colour slotIconActive;
    juce::Colour slotIconInactive;
};

extern const ColorScheme kClassicTheme;
extern const ColorScheme kDarkTheme;

enum class ThemeId { Classic, Dark };
