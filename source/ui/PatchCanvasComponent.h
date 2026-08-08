#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/ThemeData.h"
#include "../model/SnipFileIO.h"
#include "../model/ModulePresets.h"
#include "QuickAddPopup.h"
#include "../help/ModuleHelpPopup.h"
#include "ColorScheme.h"
#include <set>
#include <vector>
#include <map>
#include <random>

class PatchCanvas : public juce::Component,
                     public juce::DragAndDropTarget,
                     private juce::Timer
{
public:
    // Hint boxes over the controls, as the original editor's function keys give:
    // F5 every parameter's value, F7 the morph group of the assigned ones. The
    // mode is editor-wide, so both areas and every window agree. Public because
    // the View menu offers the same modes: two people have now asked for the
    // cost readout without finding the F-key (issue #44).
    enum class OverlayMode { Off, MorphGroups, Values, Knobs, MidiCtrls, ModuleCosts };

    // Callback types
    using ParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int value)>;
    // Fired on mouseUp after a parameter drag — carries old+new for undo
    using ParameterDragCompleteCallback = std::function<void(int section, int moduleId, int parameterId, int oldValue, int newValue)>;
    using ModuleDropCallback = std::function<void(int typeId, int section, int gridX, int gridY, const juce::String& name)>;
    using DeleteModuleCallback = std::function<void(int section, Module* module)>;
    using RenameModuleCallback = std::function<void(int section, Module* module, const juce::String& oldName, const juce::String& newName)>;
    using ModuleSelectedCallback = std::function<void(Module* module, int section)>;
    // Module move callback for undo: section, moduleIndex, oldPos, newPos
    using ModuleMoveCallback = std::function<void(int section, int moduleIndex,
                                                   juce::Point<int> oldPos, juce::Point<int> newPos)>;
    // Cable callbacks for undo: section, outModIndex, outConnIndex, outIsOutput, inModIndex, inConnIndex, inIsOutput
    using CableCallback = std::function<void(int section, int outModIdx, int outConnIdx, bool outIsOut,
                                              int inModIdx, int inConnIdx, bool inIsOut)>;
    // section, moduleId, paramId, morphGroup (0-3, or -1=disable)
    using MorphAssignCallback = std::function<void(int section, int moduleId, int paramId, int morphGroup)>;
    // section, moduleId, paramId, span, direction
    using MorphRangeChangeCallback = std::function<void(int section, int moduleId, int paramId, int span, int direction)>;
    // section, moduleId, paramId, knobIndex (0-22, or -1=disable)
    using KnobAssignCallback = std::function<void(int section, int moduleId, int paramId, int knobIndex)>;
    // section, moduleId, paramId, midiCC (0-127, or -1=disable)
    using MidiCtrlAssignCallback = std::function<void(int section, int moduleId, int paramId, int midiCC)>;
    // Initialize module: section, module pointer
    using InitModuleCallback = std::function<void(int section, Module* module)>;
    // Snippet save: fires with SnipData after "Save as Snippet" in context menu
    using SnippetSaveCallback = std::function<void(SnipData)>;
    using SnippetDropCallback = std::function<void(const juce::File& file, int section, int gridX, int gridY)>;
    // Insert a block of modules, undoably, shifting every entry by
    // (offsetX, offsetY) and reporting where each one landed as
    // {section, containerIndex}. A section of -1 leaves each module in the area
    // it came from; naming one puts the whole block there. Paste and Duplicate
    // place modules the same way the snippet browser does, so they share this.
    using SnippetInsertCallback = std::function<std::vector<std::pair<int, int>>(
        SnipData, int section, int offsetX, int offsetY)>;

    PatchCanvas();
    ~PatchCanvas();

    void shakeCables();
    static bool handleOverlayKey(const juce::KeyPress& key, juce::Component& repaintTarget);

    void setTheme(const ColorScheme& cs) { activeScheme_ = cs; repaint(); }
    void setTheme(const ColorScheme& cs, ThemeId id) { activeScheme_ = cs; activeThemeId_ = id; repaint(); }
    const ColorScheme& getTheme() const { return activeScheme_; }
    ThemeId getThemeId() const { return activeThemeId_; }

    // Zoom
    float getZoomLevel() const { return zoomLevel; }
    void setZoomLevel(float z, juce::Point<int> anchor = {});
    void zoomToSelection();
    void resetZoom();
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void openQuickAddAtMouse();

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);
    void clearModuleSelection() { clearSelection(); repaint(); }
    // The most recently selected module in this section, or nullptr.
    Module* getSelectedModule() const { return selectedModule; }
    int     getSelectedSection() const { return selectedSection; }
    void setLightMeterData(const int lights[128], const int meters[128]);

    /** Returns list of currently selected modules as (module*, section) pairs */
    std::vector<std::pair<Module*, int>> getSelectedModules() const
    {
        std::vector<std::pair<Module*, int>> result;
        for (auto& s : selection)
            if (s.module) result.push_back({s.module, s.section});
        return result;
    }

    // Callbacks
    void setParameterChangeCallback(ParameterChangeCallback cb) { parameterChangeCallback = std::move(cb); }
    void setParameterDragCompleteCallback(ParameterDragCompleteCallback cb) { paramDragCompleteCallback = std::move(cb); }
    void setModuleDropCallback(ModuleDropCallback cb) { moduleDropCallback = std::move(cb); }
    void setDeleteModuleCallback(DeleteModuleCallback cb) { deleteModuleCallback = std::move(cb); }
    void setRenameModuleCallback(RenameModuleCallback cb) { renameModuleCallback = std::move(cb); }
    void setModuleMoveCallback(ModuleMoveCallback cb) { moduleMoveCallback = std::move(cb); }
    void setModuleSelectedCallback(ModuleSelectedCallback cb) { moduleSelectedCallback = std::move(cb); }
    void setMorphAssignCallback(MorphAssignCallback cb) { morphAssignCallback = std::move(cb); }
    void setMorphRangeChangeCallback(MorphRangeChangeCallback cb) { morphRangeChangeCallback = std::move(cb); }
    void setKnobAssignCallback(KnobAssignCallback cb) { knobAssignCallback = std::move(cb); }
    void setMidiCtrlAssignCallback(MidiCtrlAssignCallback cb) { midiCtrlAssignCallback = std::move(cb); }
    void setInitModuleCallback(InitModuleCallback cb) { initModuleCallback = std::move(cb); }
    void setSnippetSaveCallback(SnippetSaveCallback cb) { snippetSaveCallback_ = std::move(cb); }
    void setSnippetDropCallback(SnippetDropCallback cb) { snippetDropCallback_ = std::move(cb); }
    void setSnippetInsertCallback(SnippetInsertCallback cb) { snippetInsertCallback_ = std::move(cb); }
    void saveSelectionAsSnippet();
    void setCableCreatedCallback(CableCallback cb) { cableCreatedCallback = std::move(cb); }
    void setCableDeletedCallback(CableCallback cb) { cableDeletedCallback = std::move(cb); }
    void setUndoCallback(std::function<void()> cb) { undoCallback = std::move(cb); }
    void setRedoCallback(std::function<void()> cb) { redoCallback = std::move(cb); }
    // File command callback: "new", "open", "save", "close"
    using FileCommandCallback = std::function<void(const juce::String&)>;
    void setFileCommandCallback(FileCommandCallback cb) { fileCommandCallback = std::move(cb); }
    void setUndoManager(juce::UndoManager* um) { undoManager = um; }
    void setPresetLibrary(ModulePresetLibrary* library) { presetLibrary = library; repaint(); }

    // Keeps a module's own preset display box in step when the preset was
    // recalled from somewhere else, such as the Inspector's preset list.
    void notePresetRecalled(int section, int containerIndex, int presetIndex)
    {
        if (section != mySection) return;
        drumPresetState[containerIndex] = presetIndex;
        repaint();
    }

    // DragAndDropTarget interface
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    // Check if a specific parameter is currently being dragged by the user
    bool isDragging(int section, int moduleId, int parameterId) const;

    // Matches original Java editor: 255px per column, 15px per row
    static constexpr int gridX = 255;               // pixels per grid column (module width)
    static constexpr int gridY = 15;                // pixels per grid row (height unit)
    static constexpr int canvasWidth = 255 * 40;    // 40 columns
    static constexpr int canvasHeight = 15 * 256;   // legacy full height (unused when section-split)
    static constexpr int sectionHeight = 15 * 128;  // height of one voice area (128 rows)
    static constexpr int polyAreaOffsetY = 0;

    // Set which section this canvas renders (1=poly, 0=common).
    // Must be called before setPatch(). Resizes canvas to sectionHeight.
    void setSection(int s);

private:
    void paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    void paintCables(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    void paintOverlays(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    juce::Rectangle<int> getModuleBounds(const Module& m, int yOffset) const;
    juce::Point<int> getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const;

    // Theme-aware painting helpers
    // Wireframe "ink": line/text colour for a module when wireframe mode is on.
    // Classic-style themes (transparent moduleBg) use each module's own XML colour
    // so types stay distinct and legible; opaque-moduleBg themes use the light text.
    juce::Colour wireframeInk(const Module& m) const;
    void paintModuleThemed(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container);
    void paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintConnectors(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container);
    void paintLabels(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintDrumSynthExtras(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds);
    void paintKnobs(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, juce::Colour moduleBg);
    void paintSliders(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintTextDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintLights(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintCustomDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintResetButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintStaticIcons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintOverlay(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);

    // Hovering a control reads its value out in a hint box, the way the original
    // editor does, without having to hold the whole patch's readout open.
    struct HoverTarget
    {
        const Module* module = nullptr;
        juce::String componentId;
        juce::Rectangle<float> controlBounds;   // canvas coordinates
        juce::Rectangle<int>   moduleBounds;
        bool sameControlAs(const HoverTarget& o) const
        { return module == o.module && componentId == o.componentId; }
    };
    bool findControlAt(juce::Point<int> canvasPos, HoverTarget& out) const;
    bool controlBoundsFor(const Module& m, const juce::String& componentId,
                          juce::Rectangle<float>& outControl,
                          juce::Rectangle<int>& outModule) const;
    void clearHover();
    void timerCallback() override;
    void paintHoverBadge(juce::Graphics& g);
    void paintDragValueBadge(juce::Graphics& g);
    void paintModuleCostBadge(juce::Graphics& g, juce::Rectangle<int> moduleBounds,
                              const juce::String& text);

    HoverTarget hoverTarget;
    bool hoverBadgeVisible = false;
    static constexpr int hoverDelayMs = 400;
    // Module whose DSP cost was asked for by double-clicking it, as the original
    // editor answers. Cleared by the next click.
    const Module* costBadgeModule = nullptr;
    // textOverride lets the hover box say something the current overlay mode
    // wouldn't; empty means "whatever the mode reads out".
    void paintOverlayBadge(juce::Graphics& g, juce::Rectangle<float> controlBounds,
                                juce::Rectangle<int> moduleBounds, const Module& m,
                                const Parameter& param,
                                const juce::String& textOverride = {});
    juce::String getOverlayText(const Module& m, const Parameter& param) const;
    juce::String getParameterValueText(const Parameter& param) const;
    void paintModuleFallback(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds);

    // Find parameter value by component-id (e.g. "p1")
    // Non-const overload returns mutable Parameter* (used in mouseDown drag setup)
    Parameter* findParameter(Module& m, const juce::String& componentId);
    const Parameter* findParameter(const Module& m, const juce::String& componentId) const;
    // Find theme connector by component-id (e.g. "c1")
    const ThemeConnector* findThemeConnector(const ModuleTheme& theme, const juce::String& componentId) const;

    Patch* patch = nullptr;
    const ModuleDescriptions* moduleDescs = nullptr;
    const ThemeData* themeData = nullptr;
    int mySection = -1;  // -1=both (legacy), 0=common, 1=poly

    // Real-time light/meter data from synth
    int globalLightValues[128] = {};
    int globalMeterValues[128] = {};

    // Compute the global base index for a module's LEDs or meters
    // (poly modules come first, sorted by index; then common modules)
    int computeModuleLightIndex(const Module& m, int section, bool forMeters) const;

    // Per-module slot ranges in the global light/meter arrays, in wire order
    // (poly section first, then common, each sorted by container index).
    struct ModuleLightRange
    {
        const Module* mod;
        int section;
        int lightBase, lightCount;
        int meterBase, meterCount;
    };
    std::vector<ModuleLightRange> computeModuleLightRanges() const;

    // Dragging state
    struct DragState
    {
        enum Type { None, Knob, Slider, Button, NoteSeqEditor, NoteSeqScrollbar, ModuleMove, MultiModuleMove, CableCreate, RubberBand, MorphRange, CanvasPan } type = None;
        Module* module = nullptr;
        Parameter* parameter = nullptr;
        Connector* sourceConnector = nullptr;  // CableCreate: source connector
        int section = 0;  // 0=common, 1=poly (PDL2/Java convention)
        juce::Point<int> startPos;
        int startValue = 0;
        int lastSentValue = -1;  // Track last value sent to avoid duplicates
        juce::int64 lastSendTime = 0;  // Rate limiting for real-time sends
        int dragOffsetX = 0, dragOffsetY = 0;  // ModuleMove: pixel offset from module origin
        juce::Rectangle<int> customRect;       // Custom display drag rect, module-relative
        juce::String customIds[16];            // NoteSeqEditor step parameter ids
        juce::Point<int> startGridPos;          // ModuleMove: grid position before drag
    };
    DragState dragState;
    ParameterChangeCallback parameterChangeCallback;
    ParameterDragCompleteCallback paramDragCompleteCallback;
    ModuleDropCallback moduleDropCallback;
    DeleteModuleCallback deleteModuleCallback;
    RenameModuleCallback renameModuleCallback;
    ModuleMoveCallback moduleMoveCallback;
    ModuleSelectedCallback moduleSelectedCallback;
    MorphAssignCallback morphAssignCallback;
    MorphRangeChangeCallback morphRangeChangeCallback;
    KnobAssignCallback knobAssignCallback;
    MidiCtrlAssignCallback midiCtrlAssignCallback;
    InitModuleCallback initModuleCallback;
    SnippetSaveCallback snippetSaveCallback_;
    SnippetDropCallback snippetDropCallback_;
    SnippetInsertCallback snippetInsertCallback_;
    CableCallback cableCreatedCallback;
    CableCallback cableDeletedCallback;
    std::function<void()> undoCallback;
    std::function<void()> redoCallback;
    FileCommandCallback fileCommandCallback;
    juce::UndoManager* undoManager = nullptr;

    // Module drop preview
    bool showModuleDropPreview = false;
    int dropPreviewTypeId = 0;
    int dropPreviewSection = 0;
    int dropPreviewGridX = 0;
    int dropPreviewGridY = 0;

    void paintGhostOutline(juce::Graphics& g, int typeIndex, int gx, int gy) const;

    // Cable creation preview
    juce::Point<int> cablePreviewEnd;
    bool showCablePreview = false;

    // Multi-module selection
    struct SelectedModule { Module* module = nullptr; int section = 0; };
    std::vector<SelectedModule> selection;
    // For multi-move: store initial positions of all selected modules
    struct ModuleMoveState { Module* module = nullptr; int section = 0; juce::Point<int> startGridPos; };
    std::vector<ModuleMoveState> multiMoveState;

    // Rubber-band selection rect (pixel coords, during drag)
    juce::Rectangle<int> rubberBandRect;
    bool showRubberBand = false;

    // DrumSynth (m58) preset UI. The presets themselves live in the shared
    // ModulePresetLibrary, which the owner injects, so the display box, this
    // canvas's menu and the Inspector all read and write the same list.
    std::map<int, int> drumPresetState;    // containerIndex → preset index
    ModulePresetLibrary* presetLibrary = nullptr;
    const std::vector<ModulePreset>& drumPresets() const;
    // Which preset a module's box should name: whatever was last recalled into
    // it, or, failing that, the preset its values match. Worked out once per
    // module and remembered.
    int resolvedDrumPreset(const Module& m);
    void applyDrumPreset(Module& m, int section, int presetIdx);
    void showDrumPresetContextMenu(Module& m, int section);
    // The preset list is reachable both from the module's own context menu and
    // from the small preset display box, so both build and route the same menu.
    // `action` is written by the clicked row: 0 = recall, 1 = delete.
    juce::PopupMenu buildDrumPresetMenu(Module& m, std::shared_ptr<int> action);
    void handleDrumPresetMenuResult(int result, int action, Module& m, int section);
    void saveDrumPresetFromModule(Module& m);
    void deleteDrumPreset(int index);
    static constexpr int drumPresetSaveId  = 200;  // IDs >= 200 belong to the
    static constexpr int drumPresetFirstId = 201;  // DrumSynth preset menu

    // Clipboard for copy/paste
    struct ClipboardEntry
    {
        int typeIndex = 0;
        juce::String name;
        int section = 0;
        juce::Point<int> gridPos;
        std::vector<int> paramValues;
        // Cable: (srcClipIdx, srcConnIdx, dstClipIdx, dstConnIdx)
        // stored separately below
    };
    struct ClipboardCable
    {
        int srcModuleClipIdx = 0;   // index into clipboard entries
        int srcConnectorIdx = 0;    // connector index within source module
        bool srcIsOutput = true;
        int dstModuleClipIdx = 0;
        int dstConnectorIdx = 0;
        bool dstIsOutput = false;
    };
    // One clipboard for the whole editor, not one per canvas: copying in the
    // poly area and pasting into common is the same gesture as copying in one
    // slot and pasting into another, and both were impossible while every
    // canvas kept its own (issue #42). Nothing in here points at a patch, so it
    // stays valid however the patches come and go.
    inline static std::vector<ClipboardEntry> clipboard;
    inline static std::vector<ClipboardCable> clipboardCables;

    // What a click is about to drop. Paste and Add Module do not place anything
    // themselves: they hang outlines off the pointer, and you click where you
    // want them, which is how the original editor works and is what lets one
    // gesture choose the area as well as the spot (issues #42 and #36).
    struct PendingDrop
    {
        enum class Kind { None, Paste, AddModule };
        Kind kind = Kind::None;
        // Ghost outlines, in grid units relative to the block's top-left.
        struct Ghost { int typeIndex = 0; int dx = 0; int dy = 0; };
        std::vector<Ghost> ghosts;
        juce::String addName;      // AddModule: title for the new module
        int addTypeIndex = 0;
        bool active() const { return kind != Kind::None && !ghosts.empty(); }
    };
    // Defined in the .cpp: a default member initializer inside PendingDrop
    // cannot be used while PatchCanvas is still an incomplete type.
    static PendingDrop pendingDrop;
    // The canvas under the pointer, which is the one drawing the ghosts and the
    // one a click would drop on. Null while the pointer is off every canvas.
    inline static PatchCanvas* pendingHost = nullptr;
    inline static juce::Point<int> pendingGrid;

    void armPendingDrop(PendingDrop drop);
    void updatePendingGhost(juce::Point<int> canvasPos);
    void dropPendingAt(juce::Point<int> canvasPos);
    // The selected modules and the cables running between them. Copy, Duplicate
    // and Save as Snippet all want exactly this, so they read it from here.
    void collectSelection(std::vector<ClipboardEntry>& entriesOut,
                          std::vector<ClipboardCable>& cablesOut) const;
    // `normaliseToOrigin` moves the block's top-left to (0, 0), which is what
    // pasting at a chosen spot needs; leaving it alone keeps the positions the
    // modules were copied from, which is what Duplicate wants.
    static SnipData toSnip(const std::vector<ClipboardEntry>& entries,
                           const std::vector<ClipboardCable>& cables,
                           bool normaliseToOrigin);
    void selectCreated(const std::vector<std::pair<int, int>>& created);

    // Parameter context menu (right-click on knob/slider/button)
    void showParameterContextMenu(Module& m, int section, Parameter& param);

    // Selection helpers
    bool isSelected(const Module* m) const;
    void clearSelection();
    void selectModule(Module* m, int section, bool addToSelection = false);
    void updateRubberBandSelection(juce::Rectangle<int> rect);
    void showSelectionContextMenu();
    void deleteSelection();
    void duplicateSelection(bool withCables);
    void copySelectionToClipboard();
    // Paste does not place anything: it hangs the copied modules off the
    // pointer and the click that follows puts them down (issue #42).
    void beginPasteGhost();
    void beginAddModuleGhost(int typeIndex, const juce::String& name);
    // Puts the clipboard down with its top-left at this grid position.
    void pasteBlockAt(int gx, int gy);

    // Legacy single-module selection (kept for compatibility during move)
    Module* selectedModule = nullptr;
    int selectedSection = -1;

    // Connector hit-testing helpers
    struct ConnectorHit { Module* module = nullptr; Connector* connector = nullptr; int section = 0; };
    ConnectorHit findConnectorAt(juce::Point<int> pos);
    Connector* findConnectorByComponentId(Module& m, const juce::String& componentId);

    // Module overlap prevention
    bool isPositionFree(const ModuleContainer& container, const Module* exclude, int gx, int gy, int height) const;
    int findNearestFreeY(const ModuleContainer& container, const Module* exclude, int gx, int targetY, int height) const;

    // Zoom
    float zoomLevel = 1.0f;
    ColorScheme activeScheme_ = kDarkTheme;
    ThemeId activeThemeId_ = ThemeId::Dark;
    inline static OverlayMode overlayMode = OverlayMode::Off;
    // Every canvas alive right now. The overlay mode is editor-wide, so a key
    // that changes it has to repaint all of them, not only the one it reached.
    inline static std::vector<PatchCanvas*> liveCanvases;

    // Editor-wide settings (shared across all PatchCanvas instances)
    inline static float cableOpacity   = 0.80f; // 0..1
    inline static int   cableStyleIdx  = 0;     // 0=CurvedThick 1=StraightThick 2=CurvedThin 3=StraightThin
    inline static bool  autoUploadOn   = true;
    inline static bool  mutatorModeOn  = false; // Patch Mutator open: frame excluded modules

public:
    // Edit-menu commands. The keyboard has always had these; the menu needs to
    // ask whether they apply before offering them (issue #42).
    bool hasSelection() const { return !selection.empty(); }
    bool canPaste()     const { return !clipboard.empty(); }
    void cutSelection()
    {
        if (selection.empty())
            return;
        copySelectionToClipboard();
        deleteSelection();
    }
    void copySelection()      { if (!selection.empty()) copySelectionToClipboard(); }
    void pasteClipboard()     { if (!clipboard.empty()) beginPasteGhost(); }
    void duplicateSelected()  { if (!selection.empty()) duplicateSelection(true); }

    // A pending paste or add belongs to the editor, not to one canvas, so
    // Escape has to reach it from wherever the keyboard focus happens to be.
    static bool isDropPending() { return pendingDrop.active(); }
    static void cancelPendingDrop();

    static void setCableOpacity (float v)  { cableOpacity   = juce::jlimit(0.0f, 1.0f, v); }
    static void setCableStyle   (int idx)  { cableStyleIdx  = idx; }
    static void setAutoUpload   (bool on)  { autoUploadOn   = on;  }
    static void setMutatorMode  (bool on)  { mutatorModeOn  = on;  }
    static float getCableOpacity()         { return cableOpacity; }

    // The overlay mode belongs to the editor, not to one canvas, so setting it
    // has to redraw every canvas on screen. Same rule the F-keys follow.
    static OverlayMode getOverlayMode()    { return overlayMode; }
    static void setOverlayMode (OverlayMode mode)
    {
        if (overlayMode == mode)
            return;
        overlayMode = mode;
        for (auto* c : liveCanvases)
            if (c != nullptr)
                c->repaint();
    }
    // Selecting the mode that is already on turns it off, so a menu item and its
    // F-key behave the same way.
    static void toggleOverlayMode (OverlayMode mode)
    {
        setOverlayMode (overlayMode == mode ? OverlayMode::Off : mode);
    }

private:
    static constexpr float zoomMin = 0.75f;
    static constexpr float zoomMax = 3.0f;
    static constexpr float zoomStep = 0.1f;
    juce::Point<int> screenToCanvas(juce::Point<int> p) const
    {
        return { juce::roundToInt(p.x / zoomLevel), juce::roundToInt(p.y / zoomLevel) };
    }
    void updateSizeForZoom();

    static constexpr int paramSendIntervalMs = 50;  // Min interval between param sends during drag

    QuickAddPopup* activeQuickAdd = nullptr;  // nullptr when no popup open

    // Cable shake: per-connection random sag offsets for visual redistribution
    std::map<std::pair<const Connector*, const Connector*>, float> cableSagOffsets;

    // Check if a connector has a hidden (filtered) cable attached
    bool hasHiddenCable(const Connector& conn, const ModuleContainer& container) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvas)
};

class PatchCanvasComponent : public juce::Component
{
public:
    PatchCanvasComponent();

    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);
    void clearModuleSelection()
    {
        polyCanvas.clearModuleSelection();
        commonCanvas.clearModuleSelection();
    }

    // What the Inspector should be showing for this canvas: the selected module
    // and its section, or {nullptr, -1} when nothing is selected. Poly wins if
    // both sections somehow hold a selection, matching the order the two
    // canvases report through moduleSelectedCallback.
    struct Selection { Module* module = nullptr; int section = -1; };
    Selection getPrimarySelection() const
    {
        if (auto* m = polyCanvas.getSelectedModule())
            return { m, polyCanvas.getSelectedSection() };
        if (auto* m = commonCanvas.getSelectedModule())
            return { m, commonCanvas.getSelectedSection() };
        return {};
    }

    // --- Callbacks forwarded to both section canvases ---

    void setParameterChangeCallback(PatchCanvas::ParameterChangeCallback cb)
    {
        polyCanvas.setParameterChangeCallback(cb);
        commonCanvas.setParameterChangeCallback(std::move(cb));
    }

    void setParameterDragCompleteCallback(PatchCanvas::ParameterDragCompleteCallback cb)
    {
        polyCanvas.setParameterDragCompleteCallback(cb);
        commonCanvas.setParameterDragCompleteCallback(std::move(cb));
    }

    void setModuleDropCallback(PatchCanvas::ModuleDropCallback cb)
    {
        polyCanvas.setModuleDropCallback(cb);
        commonCanvas.setModuleDropCallback(std::move(cb));
    }

    void setDeleteModuleCallback(PatchCanvas::DeleteModuleCallback cb)
    {
        polyCanvas.setDeleteModuleCallback(cb);
        commonCanvas.setDeleteModuleCallback(std::move(cb));
    }

    void setRenameModuleCallback(PatchCanvas::RenameModuleCallback cb)
    {
        polyCanvas.setRenameModuleCallback(cb);
        commonCanvas.setRenameModuleCallback(std::move(cb));
    }

    void setModuleMoveCallback(PatchCanvas::ModuleMoveCallback cb)
    {
        polyCanvas.setModuleMoveCallback(cb);
        commonCanvas.setModuleMoveCallback(std::move(cb));
    }

    void setModuleSelectedCallback(PatchCanvas::ModuleSelectedCallback cb)
    {
        polyCanvas.setModuleSelectedCallback(cb);
        commonCanvas.setModuleSelectedCallback(std::move(cb));
    }

    void setMorphAssignCallback(PatchCanvas::MorphAssignCallback cb)
    {
        polyCanvas.setMorphAssignCallback(cb);
        commonCanvas.setMorphAssignCallback(std::move(cb));
    }

    void setMorphRangeChangeCallback(PatchCanvas::MorphRangeChangeCallback cb)
    {
        polyCanvas.setMorphRangeChangeCallback(cb);
        commonCanvas.setMorphRangeChangeCallback(std::move(cb));
    }

    void setKnobAssignCallback(PatchCanvas::KnobAssignCallback cb)
    {
        polyCanvas.setKnobAssignCallback(cb);
        commonCanvas.setKnobAssignCallback(std::move(cb));
    }

    void setMidiCtrlAssignCallback(PatchCanvas::MidiCtrlAssignCallback cb)
    {
        polyCanvas.setMidiCtrlAssignCallback(cb);
        commonCanvas.setMidiCtrlAssignCallback(std::move(cb));
    }

    void setInitModuleCallback(PatchCanvas::InitModuleCallback cb)
    {
        polyCanvas.setInitModuleCallback(cb);
        commonCanvas.setInitModuleCallback(std::move(cb));
    }

    void setSnippetSaveCallback(PatchCanvas::SnippetSaveCallback cb)
    {
        polyCanvas.setSnippetSaveCallback(cb);
        commonCanvas.setSnippetSaveCallback(std::move(cb));
    }

    void setSnippetDropCallback(PatchCanvas::SnippetDropCallback cb)
    {
        polyCanvas.setSnippetDropCallback(cb);
        commonCanvas.setSnippetDropCallback(std::move(cb));
    }

    void setSnippetInsertCallback(PatchCanvas::SnippetInsertCallback cb)
    {
        polyCanvas.setSnippetInsertCallback(cb);
        commonCanvas.setSnippetInsertCallback(std::move(cb));
    }

    // The two areas are separate canvases, so every command goes to whichever
    // one it applies to. The clipboard itself is shared, and paste hands the
    // block to the pointer, so which area it ends up in is decided by where you
    // click rather than by where you copied from (issue #42).
    bool hasSelection() const { return polyCanvas.hasSelection() || commonCanvas.hasSelection(); }
    bool canPaste()     const { return polyCanvas.canPaste()     || commonCanvas.canPaste(); }

    void cutSelection()
    {
        if (polyCanvas.hasSelection())   polyCanvas.cutSelection();
        if (commonCanvas.hasSelection()) commonCanvas.cutSelection();
    }
    void copySelection()
    {
        if (polyCanvas.hasSelection())   polyCanvas.copySelection();
        if (commonCanvas.hasSelection()) commonCanvas.copySelection();
    }
    void pasteClipboard()
    {
        // Either canvas can arm the ghost, since it follows the pointer from
        // one area to the other. Arming it from the one under the pointer just
        // means the outlines show up straight away.
        if (commonCanvas.isMouseOver(true)) commonCanvas.pasteClipboard();
        else                                polyCanvas.pasteClipboard();
    }
    void duplicateSelected()
    {
        if (polyCanvas.hasSelection())   polyCanvas.duplicateSelected();
        if (commonCanvas.hasSelection()) commonCanvas.duplicateSelected();
    }

    /** Aggregate selected modules from both canvases */
    std::vector<std::pair<Module*, int>> getSelectedModules() const
    {
        auto sel = polyCanvas.getSelectedModules();
        auto common = commonCanvas.getSelectedModules();
        sel.insert(sel.end(), common.begin(), common.end());
        return sel;
    }

    void setCableCreatedCallback(PatchCanvas::CableCallback cb)
    {
        polyCanvas.setCableCreatedCallback(cb);
        commonCanvas.setCableCreatedCallback(std::move(cb));
    }

    void setCableDeletedCallback(PatchCanvas::CableCallback cb)
    {
        polyCanvas.setCableDeletedCallback(cb);
        commonCanvas.setCableDeletedCallback(std::move(cb));
    }

    void setUndoCallback(std::function<void()> cb)
    {
        polyCanvas.setUndoCallback(cb);
        commonCanvas.setUndoCallback(std::move(cb));
    }

    void setRedoCallback(std::function<void()> cb)
    {
        polyCanvas.setRedoCallback(cb);
        commonCanvas.setRedoCallback(std::move(cb));
    }

    void setFileCommandCallback(PatchCanvas::FileCommandCallback cb)
    {
        polyCanvas.setFileCommandCallback(cb);
        commonCanvas.setFileCommandCallback(std::move(cb));
    }

    void setUndoManager(juce::UndoManager* um)
    {
        polyCanvas.setUndoManager(um);
        commonCanvas.setUndoManager(um);
    }

    void setPresetLibrary(ModulePresetLibrary* library)
    {
        polyCanvas.setPresetLibrary(library);
        commonCanvas.setPresetLibrary(library);
    }

    void notePresetRecalled(int section, int containerIndex, int presetIndex)
    {
        polyCanvas.notePresetRecalled(section, containerIndex, presetIndex);
        commonCanvas.notePresetRecalled(section, containerIndex, presetIndex);
    }

    void repaintCanvas()
    {
        polyCanvas.repaint();
        commonCanvas.repaint();
    }

    void setLightMeterData(const int lights[128], const int meters[128])
    {
        polyCanvas.setLightMeterData(lights, meters);
        commonCanvas.setLightMeterData(lights, meters);
    }

    void shakeCables()
    {
        polyCanvas.shakeCables();
        commonCanvas.shakeCables();
    }

    void setTheme(const ColorScheme& cs)
    {
        polyCanvas.setTheme(cs);
        commonCanvas.setTheme(cs);
    }
    void setTheme(const ColorScheme& cs, ThemeId id)
    {
        polyCanvas.setTheme(cs, id);
        commonCanvas.setTheme(cs, id);
    }
    const ColorScheme& getTheme() const { return polyCanvas.getTheme(); }
    ThemeId getThemeId() const { return polyCanvas.getThemeId(); }

    bool isDragging(int section, int moduleId, int parameterId) const
    {
        return polyCanvas.isDragging(section, moduleId, parameterId)
            || commonCanvas.isDragging(section, moduleId, parameterId);
    }

    float getZoomLevel() const { return polyCanvas.getZoomLevel(); }
    void setZoomLevel(float z, juce::Point<int> anchor = {})
    {
        polyCanvas.setZoomLevel(z, anchor);
        commonCanvas.setZoomLevel(z, anchor);
    }
    void zoomToSelection()
    {
        polyCanvas.zoomToSelection();
        commonCanvas.zoomToSelection();
    }
    void resetZoom()
    {
        polyCanvas.resetZoom();
        commonCanvas.resetZoom();
    }

private:
    // Poly (section 1) area — top panel
    juce::Viewport polyViewport;
    PatchCanvas polyCanvas;

    // Common (section 0) area — bottom panel
    juce::Viewport commonViewport;
    PatchCanvas commonCanvas;

    // Draggable divider between the two panels
    juce::StretchableLayoutManager layout;
    juce::StretchableLayoutResizerBar resizerBar { &layout, 1, false };

    static constexpr int labelHeight = 18;   // "POLY" / "COMMON" header strip
    static constexpr int resizerThick = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvasComponent)
};
