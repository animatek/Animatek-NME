#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/ThemeData.h"
#include "../model/SnipFileIO.h"
#include "QuickAddPopup.h"
#include "../help/ModuleHelpPopup.h"
#include "ColorScheme.h"
#include <set>
#include <vector>
#include <map>
#include <random>

class PatchCanvas : public juce::Component,
                     public juce::DragAndDropTarget
{
public:
    // Callback types
    using ParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int value)>;
    // Fired on mouseUp after a parameter drag — carries old+new for undo
    using ParameterDragCompleteCallback = std::function<void(int section, int moduleId, int parameterId, int oldValue, int newValue)>;
    using ModuleDropCallback = std::function<void(int typeId, int section, int gridX, int gridY, const juce::String& name)>;
    using DeleteModuleCallback = std::function<void(int section, Module* module)>;
    using RenameModuleCallback = std::function<void(int section, Module* module, const juce::String& newName)>;
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

    PatchCanvas();
    ~PatchCanvas();

    void shakeCables();
    static bool handleMorphOverlayKey(const juce::KeyPress& key, juce::Component& repaintTarget);

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
    bool keyPressed(const juce::KeyPress& key) override;
    void openQuickAddAtMouse();

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);
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
    void saveSelectionAsSnippet();
    void setCableCreatedCallback(CableCallback cb) { cableCreatedCallback = std::move(cb); }
    void setCableDeletedCallback(CableCallback cb) { cableDeletedCallback = std::move(cb); }
    void setUndoCallback(std::function<void()> cb) { undoCallback = std::move(cb); }
    void setRedoCallback(std::function<void()> cb) { redoCallback = std::move(cb); }
    // File command callback: "new", "open", "save", "close"
    using FileCommandCallback = std::function<void(const juce::String&)>;
    void setFileCommandCallback(FileCommandCallback cb) { fileCommandCallback = std::move(cb); }
    void setUndoManager(juce::UndoManager* um) { undoManager = um; }

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
    void paintMorphOverlays(juce::Graphics& g, const ModuleContainer& container, int yOffset);
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
    void paintMorphOverlay(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintMorphOverlayBadge(juce::Graphics& g, juce::Rectangle<float> controlBounds,
                                juce::Rectangle<int> moduleBounds, const Parameter& param);
    juce::String getMorphOverlayText(const Parameter& param) const;
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

    // DrumSynth (m58) preset system
    struct DrumPreset
    {
        juce::String name;
        std::array<int, 15> params; // p1..p15 values (p16=mute excluded)
    };
    std::vector<DrumPreset> drumPresets;   // built-in (2) + user presets
    std::map<int, int> drumPresetState;    // containerIndex → preset index
    void initDrumPresets();
    void saveDrumPresetsToFile();
    void loadDrumPresetsFromFile();
    void applyDrumPreset(Module& m, int section, int presetIdx);
    void showDrumPresetContextMenu(Module& m, int section);

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
    std::vector<ClipboardEntry> clipboard;
    std::vector<ClipboardCable> clipboardCables;

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
    void pasteFromClipboard(juce::Point<int> mousePos);

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
    enum class MorphOverlayMode { Off, Groups, Values };
    inline static MorphOverlayMode morphOverlayMode = MorphOverlayMode::Off;

    // Editor-wide settings (shared across all PatchCanvas instances)
    inline static float cableOpacity   = 0.80f; // 0..1
    inline static int   cableStyleIdx  = 0;     // 0=CurvedThick 1=StraightThick 2=CurvedThin 3=StraightThin
    inline static int   knobControlIdx = 0;     // 0=Horizontal 1=Circular 2=Vertical
    inline static bool  autoUploadOn   = true;
    inline static bool  mutatorModeOn  = false; // Patch Mutator open: frame excluded modules

public:
    static void setCableOpacity (float v)  { cableOpacity   = juce::jlimit(0.0f, 1.0f, v); }
    static void setCableStyle   (int idx)  { cableStyleIdx  = idx; }
    static void setKnobControl  (int idx)  { knobControlIdx = idx; }
    static void setAutoUpload   (bool on)  { autoUploadOn   = on;  }
    static void setMutatorMode  (bool on)  { mutatorModeOn  = on;  }
    static float getCableOpacity()         { return cableOpacity; }

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
