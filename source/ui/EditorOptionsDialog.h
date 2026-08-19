#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatCloseButton.h"
#include "AppTheme.h"
#include <vector>

struct EditorOptions
{
    enum class CableStyle  { CurvedThick = 0, StraightThick, CurvedThin, StraightThin };
    enum class KnobControl { Horizontal = 0, Circular, Vertical };

    int         uiThemeIndex   = 6;   // index into ThemeRegistry ("Nord")
    CableStyle  cableStyle     = CableStyle::CurvedThick;
    KnobControl knobControl    = KnobControl::Vertical;
    bool        autoUpload     = true;
    // Opening a .pch asks which slot to put it in (issue #21). Turning this off
    // sends every open straight to the slot on screen, which is what people who
    // always work in one slot asked for (issue #59).
    bool        askSlotOnOpen  = true;
    bool        wireframe      = false; // outline-only module rendering (theme-independent)
    bool        animateTiling  = true;  // slide slot sub-windows to their new tiles
    // Borrow the synth's own display to name the dialog that is on screen, and
    // give the patch name back when it closes. Off by default: it writes to the
    // synth's edit buffer, and an editor that dies with a dialog up leaves the
    // caption there until the patch is reloaded.
    bool        synthDisplayCaptions = false;
    bool        moduleIconBar  = true;  // the module icon bar under the header (issue #17)
    juce::String moduleIconBarCategory { "In/Out" };  // tab it was left on
    bool        mcpBridgeEnabled = false; // embedded MCP control socket (source/mcp/), if built in; opt-in so no localhost port opens unless asked
    float       cableOpacity   = 0.80f;
    int         sendRateIndex  = 1;   // index into sendRates() — synth param throughput
    juce::File  presetLibraryRoot;

    // Throttled parameter delivery presets (batch sent per tick / tick period).
    // Higher = faster Mutator/Random response but more risk of overrunning the G1.
    struct SendRate { juce::String label; int batch; int intervalMs; };
    static const std::vector<SendRate>& sendRates();

    static EditorOptions load(juce::PropertiesFile* props);
    void save(juce::PropertiesFile* props) const;
    juce::File getPatchesFolder() const;
    juce::File getSnippetsFolder() const;
    juce::File getBanksFolder() const;
    juce::File getPresetsFolder() const;
    bool ensureLibraryFolders() const;
};

// Status of the embedded MCP bridge at the moment the dialog is opened -
// computed by MainComponent (mirrors how MidiSettingsDialog is handed a
// ConnectionManager::Status snapshot at open time, not a live subscription).
enum class McpBridgeStatusKind { Disabled, Listening, Failed };

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
    static constexpr int dialogWidth = 560;

    EditorOptions options;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    // Appearance
    juce::Label    appearanceLabel { {}, "APPEARANCE" };
    juce::Label    themeLabel      { {}, "Theme" };
    juce::ComboBox themeSelector;

    // Cable Style
    juce::Label    cableStyleLabel   { {}, "CABLE STYLE" };
    juce::ToggleButton cableCurvedThick   { "Curved (thick, default)" };
    juce::ToggleButton cableStraightThick { "Straight (thick)" };
    juce::ToggleButton cableCurvedThin    { "Curved (thin)" };
    juce::ToggleButton cableStraightThin  { "Straight (thin)" };
    juce::Label    cableOpacityLabel  { {}, "Opacity" };
    juce::Slider   cableOpacitySlider;
    const float    openingOpacity = 0.80f;  // what to restore on Cancel
    bool           applied = false;

    // Knob Control
    juce::Label    knobControlLabel  { {}, "KNOB CONTROL" };
    juce::ToggleButton knobHorizontal { "Horizontal drag (default)" };
    juce::ToggleButton knobCircular   { "Circular drag" };
    juce::ToggleButton knobVertical   { "Vertical drag" };

    // Behaviour
    juce::Label    behaviourLabel    { {}, "BEHAVIOUR" };
    juce::ToggleButton autoUploadToggle   { "Auto Upload  (send parameter changes to synth immediately)" };
    juce::ToggleButton askSlotToggle      { "Ask which slot when opening a patch  (off: open into the current slot)" };
    juce::ToggleButton wireframeToggle    { "Wireframe modules  (outline only, works with any theme)" };
    juce::ToggleButton animateTilingToggle { "Animate Slot Tiling  (slide sub-windows into place)" };
    juce::ToggleButton synthCaptionToggle { "Show the editor on the synth display  (borrows the patch name while a dialog is open)" };
    juce::Label    sendRateLabel     { {}, "Send speed" };
    juce::ComboBox sendRateSelector;

    // MCP Bridge
    juce::Label    mcpBridgeLabel    { {}, "MCP BRIDGE" };
    juce::ToggleButton mcpBridgeToggle { "Enable MCP bridge" };
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
