#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatCloseButton.h"
#include "AppTheme.h"
#include "ThemeRegistry.h"
#include <vector>

struct EditorOptions
{
    enum class CableStyle  { CurvedThick = 0, StraightThick, CurvedThin, StraightThin };
    enum class KnobControl { Horizontal = 0, Circular, Vertical };

    int         uiThemeIndex   = 6;   // index into ThemeRegistry ("Nord")
    // The saved choice is the name, so user theme files coming and going never
    // moves it to another theme; the index is only the fallback for old settings.
    juce::String uiThemeName;
    CableStyle  cableStyle     = CableStyle::CurvedThick;
    KnobControl knobControl    = KnobControl::Vertical;
    bool        autoUpload     = true;
    // Opening a .pch asks which slot to put it in (issue #21). Turning this off
    // sends every open straight to the slot on screen, which is what people who
    // always work in one slot asked for (issue #59).
    bool        askSlotOnOpen  = true;
    bool        wireframe      = false; // outline-only module rendering (theme-independent)
    // Hardware look: knobs, jacks, panels, displays and cables drawn as physical
    // hardware instead of flat shapes. Theme-independent, like wireframe, and
    // off by default so the canvas looks the way it always has until asked.
    bool        hardwareLook   = false;
    int         hardwareKnobStyle = 2;  // 0 domed cap, 1 black knurled, 2 aluminium knurled
    // Flat knobs, as on the Animatek Rack modules: a dark disc, grey rim, white
    // pointer and the value in blue on the rim. For the normal (non-hardware) look;
    // ignored while Hardware look or Wireframe is on.
    bool        flatKnobs      = false;
    // While the chosen theme sets a render style (#90), the user's own, given back
    // when a theme that sets none is chosen. Empty otherwise.
    std::optional<RenderStyle> renderBeforeTheme;

    RenderStyle renderStyle() const { return { wireframe, hardwareLook, hardwareKnobStyle, flatKnobs }; }
    void setRenderStyle(const RenderStyle& r)
    {
        wireframe = r.wireframe; hardwareLook = r.hardwareLook;
        hardwareKnobStyle = r.knobStyle; flatKnobs = r.flatKnobs;
    }
    bool        animateTiling  = true;  // slide slot sub-windows to their new tiles
    // Slot windows on startup: false = follow the synths (Port 1's come back, the others open and
    // close with their synth); true = the last session's windows, all of them, kept open.
    bool        restoreAllWindows = false;
    // Borrow the synth's own display to name the dialog that is on screen, and
    // give the patch name back when it closes. Off by default: it writes to the
    // synth's edit buffer, and an editor that dies with a dialog up leaves the
    // caption there until the patch is reloaded.
    bool        synthDisplayCaptions = false;
    // Read a note sequencer's steps as note names rather than as the numbers the
    // original editor shows (issue #76). Off by default, so a patch reads the
    // way the original does until it is asked not to.
    bool        seqNoteNames   = false;
    // Community themes (#91): look at animatek/nme-themes at most once a day on startup.
    bool        checkCommunityThemes = true;
    juce::int64 communityThemesChecked = 0;   // ms since 1970 of the last look that answered
    juce::String communityThemesSeen;         // "file:sha256" per line, the ones the user has seen
    // New version notice (#97): the latest release tag read from GitHub, kept so the
    // menu can say so before the network answers, and the one the user already opened.
    bool        checkNewVersion = true;
    juce::int64 newVersionChecked = 0;        // ms since 1970 of the last look that answered
    juce::String latestVersion;               // "X.Y.Z", empty until a look answered
    juce::String newVersionSeen;              // the latestVersion whose menu item was chosen
    bool        moduleIconBar  = true;  // the module icon bar under the header (issue #17)
    juce::String moduleIconBarCategory { "In/Out" };  // tab it was left on
    bool        mcpBridgeEnabled = false; // embedded MCP control socket (source/mcp/), if built in; opt-in so no localhost port opens unless asked
    float       cableOpacity   = 0.80f;
    int         sendRateIndex  = 1;   // index into sendRates() — synth param throughput
    juce::File  presetLibraryRoot;
    // Whole-interface zoom for big, high-resolution screens (#105): every window,
    // dialog and help text, not just the canvas. 1.0 leaves it as it always was.
    float       uiScale        = 1.0f;
    static const std::vector<float>& uiScales();   // the choices offered

    // Throttled parameter delivery presets (batch sent per tick / tick period).
    // Higher = faster Mutator/Random response but more risk of overrunning the G1.
    struct SendRate { juce::String label; int batch; int intervalMs; };
    static const std::vector<SendRate>& sendRates();

    static EditorOptions load(juce::PropertiesFile* props);
    void save(juce::PropertiesFile* props) const;
    juce::File getPatchesFolder() const;
    juce::File getSnippetsFolder() const;
    juce::File getBanksFolder() const;
    juce::File getSetsFolder() const;
    juce::File getPresetsFolder() const;
    bool ensureLibraryFolders() const;
};

// Status of the embedded MCP bridge at the moment the dialog is opened -
// computed by MainComponent (mirrors how MidiSettingsDialog is handed a
// ConnectionManager::Status snapshot at open time, not a live subscription).
enum class McpBridgeStatusKind { Disabled, Listening, Failed };

// A row of the options pages: the setting's name and what it does on the left, the switch (or the
// round radio, for a choice of one among several) on the right. The whole row is the button.
class OptionRow : public juce::ToggleButton
{
public:
    OptionRow (const juce::String& title, const juce::String& description = {}, bool radioStyle = false)
        : juce::ToggleButton (title), description_ (description), radio_ (radioStyle)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    juce::String description_;
    bool radio_;
};

class EditorOptionsDialog : public juce::Component
{
public:
    EditorOptionsDialog(const EditorOptions& current,
                        McpBridgeStatusKind mcpStatus,
                        const juce::String& mcpStatusText,
                        const juce::String& mcpCommand);

    std::function<void(const EditorOptions&)> onChange;
    // Cable opacity is the one setting you have to see to choose, so it moves
    // the canvas while the slider does, and the dialog puts the old value back
    // if it is closed any way other than OK.
    std::function<void(float)> onCableOpacityPreview;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    // Returns the dialog, so a caller can watch for it closing and can hook up
    // the live callbacks above.
    static EditorOptionsDialog* show(juce::Component* parent,
                     const EditorOptions& current,
                     McpBridgeStatusKind mcpStatus,
                     const juce::String& mcpStatusText,
                     const juce::String& mcpCommand,
                     std::function<void(const EditorOptions&)> onChangeCb);

private:
    void close();
    void apply();
    void browseLibraryRoot();
    void populateThemeSelector();

    // Single source of truth for the whole layout: places every component
    // (only if `apply` is true) and always (re)fills sectionSeparatorsY with
    // the y of each section-boundary line, so paint() can draw exactly
    // where resized() actually put things instead of a second, hand-kept-
    // in-sync list of magic numbers. Returns the total content height
    // (bottom of the OK/Cancel row + margin), used once at construction
    // time to size the dialog before the first real resized() call.
    int layoutComponents(bool apply);
    std::vector<int> sectionSeparatorsY;
    static constexpr int dialogWidth = 860;
    static constexpr int dialogHeight = 640;
    static constexpr int titleBarH = 40;
    static constexpr int sidebarW = 200;
    static constexpr int footerH = 68;

    // The pages in the sidebar, and which one is showing
    static const char* pageName (int page);
    static constexpr int numPages = 8;
    int page = 0;
    void showPage (int newPage);
    juce::Rectangle<int> pageButtonBounds (int index) const;

    // The rows that are not an OptionRow (a name and what it does, then a control on the right):
    // worked out by layoutComponents() and drawn by paint()
    struct InfoRow { int page; juce::Rectangle<int> rect; juce::String title, description; int controlW; };
    std::vector<InfoRow> infoRows;

    EditorOptions options;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    // Appearance
    juce::Label    appearanceLabel { {}, "APPEARANCE" };
    juce::Label    themeLabel      { {}, "Theme" };
    juce::ComboBox themeSelector;
    juce::Label    uiScaleLabel    { {}, "UI size" };
    juce::ComboBox uiScaleSelector;
    OptionRow hardwareLookToggle { "Hardware look", "Draws knobs, jacks, panels, displays and cables like physical hardware. Works with any theme." };
    juce::Label    knobStyleLabel { {}, "Knob style" };
    juce::ComboBox knobStyleSelector;
    OptionRow flatKnobsToggle { "Flat knobs", "A dark disc with the value arc on the rim, like the Animatek modules for VCV Rack. Not with Hardware look." };
    OptionRow wireframeToggle { "Wireframe modules", "Outlines only, whatever the theme. Handy for very big patches." };

    // Cable Style
    juce::Label    cableStyleLabel   { {}, "CABLE STYLE" };
    OptionRow cableCurvedThick   { "Curved, thick (default)", {}, true };
    OptionRow cableStraightThick { "Straight, thick", {}, true };
    OptionRow cableCurvedThin    { "Curved, thin", {}, true };
    OptionRow cableStraightThin  { "Straight, thin", {}, true };
    juce::Label    cableOpacityLabel  { {}, "Opacity" };
    juce::Slider   cableOpacitySlider;
    const float    openingOpacity = 0.80f;  // what to restore on Cancel
    bool           applied = false;

    // Knob Control
    juce::Label    knobControlLabel  { {}, "KNOB CONTROL" };
    OptionRow knobHorizontal { "Horizontal drag (default)", "Drag left and right to turn a knob.", true };
    OptionRow knobCircular   { "Circular drag", "Drag around the knob, as on a real one.", true };
    OptionRow knobVertical   { "Vertical drag", "Drag up and down to turn a knob.", true };

    // Behaviour
    juce::Label    behaviourLabel    { {}, "BEHAVIOUR" };
    OptionRow autoUploadToggle   { "Auto upload", "Send each parameter change to the synth as it is made." };
    OptionRow askSlotToggle      { "Ask which slot when opening a patch", "Off: every open goes to the slot on screen." };
    OptionRow animateTilingToggle { "Animate slot tiling", "Slide the slot windows into place when they are put back in order." };
    OptionRow restoreWindowsToggle { "Reopen every slot window of the last session", "Off: only Port 1's come back, and another synth's open when it connects. On: all of them, and they stay open." };
    OptionRow synthCaptionToggle { "Show the editor on the synth display", "Borrows the patch name while a dialog is open." };
    OptionRow communityThemesToggle { "Look for new community themes", "Once a day, reads the list in animatek/nme-themes on GitHub. Nothing is sent but the request." };
    OptionRow newVersionToggle { "Tell me when a new version of NME is out", "Checks GitHub once a day. It never downloads anything." };
    OptionRow seqNoteNamesToggle { "Read sequencer steps as note names", "NoteSeqB in pitches, NoteSeqA in intervals." };
    juce::Label    sendRateLabel     { {}, "Send speed" };
    juce::ComboBox sendRateSelector;

    // MCP Bridge
    juce::Label    mcpBridgeLabel    { {}, "MCP BRIDGE" };
    OptionRow mcpBridgeToggle { "Enable the MCP bridge", "Lets an MCP client (an AI agent, say) add modules and connect cables in the live patch, over a localhost-only socket." };
    juce::Label    mcpBridgeStatusLabel;
    juce::TextEditor mcpBridgeCommand;  // read-only, selectable - the stdio command to register a client
    juce::TextButton mcpBridgeCopyButton { "Copy" };

    // Preset Library
    juce::Label      libraryLabel { {}, "PRESET LIBRARY" };
    juce::TextEditor libraryPath;
    juce::TextButton browseLibraryButton { "Browse..." };
    std::shared_ptr<juce::FileChooser> folderChooser;

    // Buttons
    juce::TextButton okButton     { "OK" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorOptionsDialog)
};
