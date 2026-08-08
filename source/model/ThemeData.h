#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <map>
#include <memory>

struct ThemeConnector
{
    juce::String componentId;  // e.g. "c1", "c2"
    int x = 0, y = 0;
    int size = 13;
    juce::String cssClass;     // e.g. "cAUDIO", "cCONTROL"
};

struct ThemeKnob
{
    juce::String componentId;  // e.g. "p1", "p2"
    int x = 0, y = 0;
    int size = 21;
};

struct ThemeSlider
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 12, height = 50;
    juce::String orientation;  // "vertical" or "horizontal"
};

struct ThemeButton
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 34, height = 16;
    bool cyclic = true;                // cyclic=toggle, !cyclic=radio selector
    bool isIncrement = false;          // mode="increment" (arrow buttons)
    bool landscape = false;            // landscape="true" (horizontal layout)
    std::vector<juce::String> labels;     // indexed by btn index
    std::vector<juce::String> imageRefs;  // image href per btn index (e.g. "wf_sine", "wf_saw")
    bool reversed = false;     // vertical radio buttons: render index 0 at bottom
    bool isCall = false;       // button triggers a call action, not a parameter change
    juce::String callMethod;   // e.g. "rnd", "shift", "invert", "min", "max"
    int callValue = 0;         // e.g. shift amount (-2..+2) for "shift" method
};

struct ThemeLabel
{
    int x = 0, y = 0;
    int width = 120, height = 12;   // drawing box; defaults leave text left-aligned and roomy
    bool centred = false;           // align="centre" centres the text inside width
    juce::String text;
};

struct ThemeTextDisplay
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 40, height = 16;

    // Optional formatter name override (fmtXxx). When empty, the value is
    // formatted using the descriptor's formatter from modules.xml.
    // Used to match original Nomad UI for cases where modules.xml formatter
    // (e.g. "value-64" on slave oscs) doesn't match the UI (partial ratio).
    juce::String formatterOverride;

    // Reads as a partial ratio ("1:1", "2:1") rather than through the
    // descriptor's own formatter.
    bool partialFormat = false;

    // Draws the arrow steppers under the display, and hit-tests them. Separate
    // from partialFormat: the LFO slaves and the random generators show the
    // ratio but have no arrows in the original, where the value is set with the
    // knob beside the display (issue #48).
    bool partialArrows = false;
};

struct ThemeLight
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 7, height = 7;
    juce::String type;      // "led" or "meter"
    int ledOnValue = -1;    // if >= 0: LED activates when paired meter reaches this value
};

struct ThemeGroupBox
{
    int x = 0, y = 0, width = 40, height = 30;
};

struct ThemeResetButton
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 9, height = 6;
    int defaultValue = 64;   // value at which the indicator lights green
};

struct ThemeCustomDisplay
{
    juce::String type;              // tag name, e.g. "overdrive-display", "LFODisplay"
    int x = 0, y = 0;
    int width = 40, height = 30;
    // LFODisplay sub-element component IDs (empty = not present)
    juce::String phaseComponentId;   // <phase component-id="pN"> → phase param
    juce::String shapeComponentId;   // <shape component-id="pN"> → waveform shape param
    juce::String rateComponentId;    // <rate component-id="pN">  → rate param (cycles scaling)
    int fixedWaveform = -1;          // <waveform value="N"> → fixed shape (-1 = dynamic)
    // Envelope display sub-element component IDs
    juce::String attackComponentId;  // <attack component-id="pN">
    juce::String decayComponentId;   // <decay component-id="pN">
    juce::String sustainComponentId; // <sustain component-id="pN">
    juce::String releaseComponentId; // <release component-id="pN">
    juce::String holdComponentId;    // <hold component-id="pN"> (AHD)
    juce::String inverseComponentId; // <inverse component-id="pN"> (Mod-Env INV button)
    // Multi-Env display sub-element component IDs
    juce::String levelIds[4];        // <l0..l3 component-id="pN">
    juce::String timeIds[5];         // <t0..t4 component-id="pN">
    juce::String curveComponentId;   // <curve component-id="pN"> (curve type)
    juce::String bwComponentId;      // <bandwidth component-id="pN"> (EqMid)
    juce::String bandIds[16];        // <band0..band15 component-id="pN"> (Vocoder)
    juce::String noteStepIds[16];    // <step0..step15 component-id="pN"> (NoteSeqB)
    // EQ display sub-element component IDs (eq-mid-display, eq-shelving-display)
    juce::String freqComponentId;    // <frequency component-id="pN">
    juce::String gainComponentId;    // <gain component-id="pN">
    // Filter display sub-element component IDs (filter-e-display, filter-f-display)
    juce::String cutoffComponentId;       // <cutoff component-id="pN">
    juce::String resonanceComponentId;    // <resonance component-id="pN">
    juce::String typeComponentId;         // <type component-id="pN">
    juce::String slopeComponentId;        // <slope component-id="pN">
    juce::String gainControlComponentId;  // <gain-control component-id="pN">
    // Distortion/dynamics display sub-element component IDs
    juce::String overdriveComponentId;   // <overdrive  component-id="pN">
    juce::String clipComponentId;        // <clip        component-id="pN">
    juce::String symmetryComponentId;    // <symmetry   component-id="pN">
    juce::String wavewrapComponentId;    // <wavewrap   component-id="pN">
    juce::String thresholdComponentId;   // <threshold  component-id="pN"> (compressor/expander)
    juce::String ratioComponentId;       // <ratio       component-id="pN">
    juce::String refLevelComponentId;    // <ref-level  component-id="pN"> (compressor)
    juce::String limiterComponentId;     // <limiter     component-id="pN"> (compressor)
    juce::String gateComponentId;        // <gate        component-id="pN"> (expander)
    juce::String feedbackComponentId;    // <feedback    component-id="pN"> (phaser)
    juce::String peaksComponentId;       // <peaks       component-id="pN"> (phaser)
    juce::String spreadComponentId;      // <spread      component-id="pN"> (phaser)
    // Multimode-routing bracket (FilterC / FilterD)
    int mmInX = 0, mmInY = 0;            // audio input connector centre
    int mmOutX = 0;                       // output connector centre x (HP/BP/LP share same x)
    int mmHpY = 0, mmBpY = 0, mmLpY = 0; // HP / BP / LP output connector centres y
};

struct ThemeStaticIcon
{
    juce::String iconName;     // e.g. "wf_sine", "wf_saw", "wf_tri", "wf_square"
    int x = 0, y = 0;
    int width = 11, height = 9;
};

struct ModuleTheme
{
    juce::String componentId;  // e.g. "m62"
    int width = 255;
    int height = 30;
    juce::String name;

    std::vector<ThemeConnector> connectors;
    std::vector<ThemeKnob> knobs;
    std::vector<ThemeSlider> sliders;
    std::vector<ThemeButton> buttons;
    std::vector<ThemeLabel> labels;
    std::vector<ThemeTextDisplay> textDisplays;
    std::vector<ThemeLight> lights;
    std::vector<ThemeCustomDisplay> customDisplays;
    std::vector<ThemeResetButton> resetButtons;
    std::vector<ThemeGroupBox> groupBoxes;
    std::vector<ThemeStaticIcon> staticIcons;
};

class ThemeData
{
public:
    bool loadFromFile(const juce::File& xmlFile);
    bool loadFromXmlString(const juce::String& xmlString);

    const ModuleTheme* getModuleTheme(const juce::String& componentId) const;

    int getModuleThemeCount() const { return static_cast<int>(themes.size()); }

private:
    bool loadFromXml(std::unique_ptr<juce::XmlElement> xml);
    void parseModule(const juce::XmlElement& moduleElem);
    void parseConnector(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseKnob(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseSlider(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseButton(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseLabel(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseTextDisplay(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseLight(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseResetButton(const juce::XmlElement& elem, ModuleTheme& theme);

    std::map<juce::String, ModuleTheme> themes;
};

