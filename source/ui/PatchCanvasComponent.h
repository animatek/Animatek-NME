#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/ThemeData.h"
#include "../model/LightMeterLayout.h"
#include "../model/SnipFileIO.h"
#include "../model/ModulePresets.h"
#include "QuickAddPopup.h"
#include "../help/ModuleHelpPopup.h"
#include "ColorScheme.h"
#include "ValueSpinner.h"
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
    // A UI-only ("custom" class) parameter changed: display units, sequencer
    // zoom. These live in the patch but never on the wire — their index collides
    // with a real parameter's, so sending one would edit the wrong thing.
    using CustomParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int oldValue, int newValue)>;
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

    // --- Editor text notes (comments) ---
    // Add one at a grid position; fires with the section and grid cell clicked.
    using CommentAddCallback = std::function<void(int section, int gridX, int gridY)>;
    using CommentMoveCallback = std::function<void(int commentId, juce::Point<int> oldPos,
                                                   juce::Point<int> newPos)>;
    using CommentTextCallback = std::function<void(int commentId, const juce::String& oldText,
                                                   const juce::String& newText)>;
    using CommentDeleteCallback = std::function<void(int commentId)>;
    // Creates a note with a size and a text already decided, inside whatever
    // undo transaction is open rather than starting one: that is what pasting
    // and duplicating need, so the whole block undoes in a single step.
    using CommentCreateCallback = std::function<void(int section, int gridX, int gridY,
                                                     int width, int height,
                                                     const juce::String& text)>;
    // Rectangles in grid units: (column, row, columns, rows). A corner drag can
    // move the left edge as well as the size, so the whole rectangle travels.
    using CommentResizeCallback = std::function<void(int commentId,
                                                     juce::Rectangle<int> oldRect,
                                                     juce::Rectangle<int> newRect)>;

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
    // Trackpad pinch (macOS, and any platform JUCE feeds magnify events from).
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;

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
    void setCommentAddCallback(CommentAddCallback cb) { commentAddCallback = std::move(cb); }
    void setCommentMoveCallback(CommentMoveCallback cb) { commentMoveCallback = std::move(cb); }
    void setCommentTextCallback(CommentTextCallback cb) { commentTextCallback = std::move(cb); }
    void setCommentDeleteCallback(CommentDeleteCallback cb) { commentDeleteCallback = std::move(cb); }
    void setCommentCreateCallback(CommentCreateCallback cb) { commentCreateCallback = std::move(cb); }
    void setCommentResizeCallback(CommentResizeCallback cb) { commentResizeCallback = std::move(cb); }

    Module* getSelectedModule() const { return resolve(selectedRef); }
    int     getSelectedSection() const { return selectedRef.section; }
    void setLightMeterData(const int lights[128], const int meters[128]);

    /** Returns list of currently selected modules as (module*, section) pairs.
        Modules the patch no longer owns are simply absent, so a caller cannot
        be handed a pointer to something that has been deleted. */
    std::vector<std::pair<Module*, int>> getSelectedModules() const
    {
        std::vector<std::pair<Module*, int>> result;
        for (auto& ref : selection)
            if (auto* m = resolve(ref))
                result.push_back({ m, ref.section });
        return result;
    }

    // Callbacks
    void setParameterChangeCallback(ParameterChangeCallback cb) { parameterChangeCallback = std::move(cb); }
    void setParameterDragCompleteCallback(ParameterDragCompleteCallback cb) { paramDragCompleteCallback = std::move(cb); }
    void setCustomParameterChangeCallback(CustomParameterChangeCallback cb) { customParameterChangeCallback = std::move(cb); }
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
    /** The module a reference names, or nullptr when it has been deleted.
        Every read of a stored reference goes through here. */
    Module* resolve(ModuleRef ref) const
    {
        return patch != nullptr ? patch->getModule(ref) : nullptr;
    }
    /** A reference to a module on this canvas. */
    ModuleRef refTo(const Module& m) const { return Patch::refTo(m, mySection); }

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
    void paintModuleThemed(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container,
                           const LightMeterLayout::ModuleSlots* lightSlots);
    void paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintConnectors(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container);
    void paintLabels(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintDrumSynthExtras(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds);
    void paintKnobs(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, juce::Colour moduleBg);
    void paintSliders(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintTextDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    /** `slots` is where this module's LEDs and meters live in the synth's
        global arrays, or null for a module that owns none. Handed in rather
        than looked up: see lightRangeTable(). */
    void paintLights(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme,
                     const LightMeterLayout::ModuleSlots* slots);
    void paintCustomDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintResetButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintStaticIcons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintOverlay(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);

    // Hovering a control reads its value out in a hint box, the way the original
    // editor does, without having to hold the whole patch's readout open.
    struct HoverTarget
    {
        ModuleRef module;
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
    /** Drops the drag if the module it is on has been destroyed under it. The
        one thing here still held by pointer; see the definition. */
    void dropDragIfModuleGone();
    void timerCallback() override;
    void paintHoverBadge(juce::Graphics& g);
    void paintDragValueBadge(juce::Graphics& g);
    void paintModuleCostBadge(juce::Graphics& g, juce::Rectangle<int> moduleBounds,
                              const juce::String& text);

    HoverTarget hoverTarget;
    bool hoverBadgeVisible = false;
    static constexpr int hoverDelayMs = 400;

    // ── Nudge arrows ─────────────────────────────────────────────────────────
    // Hovering a knob or a slider pops the two little buttons the original
    // editor puts under it (see ValueSpinner). All this end keeps is which
    // control they belong to; the buttons themselves are the shared widget, so
    // the morph dials in the header bar behave exactly the same way.
    struct SpinnerTarget
    {
        ModuleRef module;
        juce::String componentId;
        juce::Rectangle<float> control;        // canvas coordinates
        juce::Rectangle<int>   moduleBounds;
    };
    ValueSpinner spinner { *this };
    SpinnerTarget spinnerTarget;
    int spinnerValueBeforePress = 0;   // so a whole press is one undo step

    bool findSpinnerAt(juce::Point<int> canvasPos, SpinnerTarget& out);
    void updateSpinner(juce::Point<int> canvasPos);
    void clearSpinner();
    /** True when the click landed on an arrow and has been dealt with, in which
        case nothing underneath it should see the click. */
    bool spinnerMouseDown(juce::Point<int> canvasPos);
    void spinnerStep(int delta);
    void spinnerRelease();

    // ── Stepping from the keyboard (issue #66) ───────────────────────────────
    // `+` and `-` step whatever the pointer is over, which is the same control
    // the nudge arrows are showing under, so both gestures share a target and a
    // step. Held down, the key repeats and the whole run is one undo step,
    // closed when the key comes back up.
    ModuleRef    keyStepModule;
    juce::String keyStepComponentId;
    int          keyStepStartValue = 0;
    void beginKeyStep();
    void endKeyStep();
    bool keyStateChanged(bool isKeyDown) override;
    // Module whose DSP cost was asked for by double-clicking it, as the original
    // editor answers. Cleared by the next click.
    ModuleRef costBadgeModule;
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

    // ── Frequency display units (issue #30) ──────────────────────────────────
    // The original rotates the units of an oscillator's, slave LFO's or
    // filter's frequency box when it is clicked. The choice is stored in the
    // patch, as the module's "freq display units" custom parameter — a UI-only
    // parameter that is never sent to the synth.
    struct FreqUnit
    {
        juce::String formatter;  // ValueFormatters name, or "@slaveHz"
        juce::String name;       // shown in the hover readout
    };
    /** The units parameter this display cycles, or nullptr if it has none. */
    const Parameter* freqUnitsParamFor(const Module& m, const juce::String& displayComponentId) const;
    Parameter*       freqUnitsParamFor(Module& m, const juce::String& displayComponentId);
    /** Units in cycling order, for a display known to have a units parameter. */
    juce::Array<FreqUnit> freqUnitsFor(const Module& m, const juce::String& displayComponentId,
                                       const juce::String& baseFormatter) const;
    /** The display's text in one particular unit. */
    juce::String formatInFreqUnit(const Module& m, const Parameter& valueParam,
                                  const FreqUnit& unit) const;
    // Find theme connector by component-id (e.g. "c1")
    const ThemeConnector* findThemeConnector(const ModuleTheme& theme, const juce::String& componentId) const;

    Patch* patch = nullptr;
    const ModuleDescriptions* moduleDescs = nullptr;
    const ThemeData* themeData = nullptr;
    int mySection = -1;  // -1=both (legacy), 0=common, 1=poly

    // Real-time light/meter data from synth
    int globalLightValues[128] = {};
    int globalMeterValues[128] = {};

    // Where each module's LEDs and meters sit in the synth's two 128-slot
    // arrays. Working the table out costs a sort and a theme lookup per module,
    // and it was being done twice per module per paint plus once per light
    // frame from the synth: quadratic in the module count, several times a
    // second, for something that only changes when the patch's structure does.
    // It is cached here and validated against a fingerprint of that structure,
    // so no caller has to remember to invalidate it (undo and the MCP bridge
    // both mutate the patch without going through this class). The table and
    // the fingerprint live in the model, where they are unit tested.
    const LightMeterLayout::Table& lightRangeTable() const;
    mutable LightMeterLayout::Table lightRangeCache_;
    mutable bool lightRangeCacheValid_ = false;

    /** Repaints one rectangle given in canvas (unzoomed) coordinates, which is
        what all the painting code works in. */
    void repaintCanvasArea(juce::Rectangle<int> canvasArea);
    /** Repaints just this note, for the changes that only affect one: hover
        grips, selection. A no-op for an id that is not on this canvas. */
    void repaintComment(int commentId);

    // --- Editor text notes ---
    juce::Rectangle<int> getCommentBounds(const PatchComment& c) const;
    PatchComment* getCommentAt(juce::Point<int> canvasPos);
    void paintComments(juce::Graphics& g);
    void showCommentEditor(int commentId);       // modal text editor for one note
    void showCommentContextMenu(int commentId);
    /** The note's text, laid out to fill its panel: bold, centred, and as big as
        fits. Shared by the note itself and by the outline that precedes it. */
    void drawCommentText(juce::Graphics& g, const juce::String& text,
                         juce::Rectangle<int> bounds, juce::Colour colour) const;
    // Which corner grip the pointer is over, so a drag there resizes the note
    // instead of moving it. Both bottom corners pull: the right one takes the
    // right edge with it, the left one the left edge.
    enum class CommentGrip { None, BottomLeft, BottomRight };
    CommentGrip commentGripAt(const PatchComment& c, juce::Point<int> canvasPos) const;
    static constexpr int commentGripSize = 14;   // canvas pixels, square
    // Text notes are selected alongside modules, not instead of them: the rubber
    // band catches both, and a selection holding some of each moves, copies and
    // deletes as one. They are kept in their own list because a note is not a
    // Module and has no business pretending to be one in the patch model, where
    // everything else ends up on the wire.
    std::vector<int> selectedCommentIds;
    bool isCommentSelected(int id) const;
    void selectComment(int id, bool addToSelection);
    void clearCommentSelection() { selectedCommentIds.clear(); }
    /** The one selected note, or nullptr when none or several are: editing text
        and reading a size are single-note affairs. */
    PatchComment* soleSelectedComment();
    int dragCommentId = -1;
    juce::Point<int> dragCommentStartPos;  // grid position when the drag began
    int dragCommentOffsetX = 0, dragCommentOffsetY = 0;
    // Resize drag: the rectangle the note had when the grip was grabbed, and
    // which grip is being pulled.
    juce::Rectangle<int> dragCommentStartRect;
    CommentGrip dragCommentGrip = CommentGrip::None;
    CommentGrip hoverCommentGrip = CommentGrip::None;
    int hoverCommentId = -1;

    // Dragging state
    struct DragState
    {
        enum Type { None, Knob, Slider, Button, NoteSeqEditor, NoteSeqScrollbar, ModuleMove, MultiModuleMove, CableCreate, CableReroute, RubberBand, MorphRange, CanvasPan, CommentMove, CommentResize } type = None;
        Module* module = nullptr;
        Parameter* parameter = nullptr;
        Connector* sourceConnector = nullptr;  // CableCreate: source connector
        // CableCreate that began by lifting an existing cable off a connector
        // (#67). The drop then belongs in the transaction the lift opened, so a
        // single undo puts the cable back where it came from.
        bool rerouting = false;
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
    CustomParameterChangeCallback customParameterChangeCallback;
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
    CommentAddCallback commentAddCallback;
    CommentMoveCallback commentMoveCallback;
    CommentTextCallback commentTextCallback;
    CommentDeleteCallback commentDeleteCallback;
    CommentCreateCallback commentCreateCallback;
    CommentResizeCallback commentResizeCallback;
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

    void paintGhostOutline(juce::Graphics& g, int typeIndex, int gx, int gy,
                           int gw = 1, int gh = 1) const;

    // Cable creation preview
    juce::Point<int> cablePreviewEnd;
    bool showCablePreview = false;

    // Multi-module selection, by reference rather than by pointer: a selection
    // outlives the delete, the undo and the patch reload that destroy modules
    // under it (issue #61).
    std::vector<ModuleRef> selection;
    // For multi-move: store initial positions of all selected modules
    struct ModuleMoveState { ModuleRef ref; juce::Point<int> startGridPos; };
    std::vector<ModuleMoveState> multiMoveState;
    // Notes travel with the modules in a multi-move, so they need the same
    // record of where they started.
    struct CommentMoveState { int id = -1; juce::Point<int> startGridPos; };
    std::vector<CommentMoveState> commentMoveState;

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
    // Text notes ride in the same clipboard as the modules, in grid coordinates
    // like theirs, so a block copied with a note keeps the note where it sat
    // relative to the modules it belongs to.
    struct ClipboardComment
    {
        int section = 0;
        juce::Point<int> gridPos;
        int width = 1, height = 2;
        juce::String text;
    };
    inline static std::vector<ClipboardEntry> clipboard;
    inline static std::vector<ClipboardCable> clipboardCables;
    inline static std::vector<ClipboardComment> clipboardComments;
    /** The top-left grid cell of whatever is on the clipboard, modules and notes
        together, which is what a paste is positioned from. */
    static juce::Point<int> clipboardOrigin();

    // What a click is about to drop. Paste and Add Module do not place anything
    // themselves: they hang outlines off the pointer, and you click where you
    // want them, which is how the original editor works and is what lets one
    // gesture choose the area as well as the spot (issues #42 and #36).
    struct PendingDrop
    {
        enum class Kind { None, Paste, AddModule, AddComment };
        Kind kind = Kind::None;
        // Ghost outlines, in grid units relative to the block's top-left. A
        // typeIndex of commentGhost means the outline is a text note, which has
        // no descriptor to take its size from.
        static constexpr int commentGhost = -1;
        // w and h are in grid units and are read only for comment ghosts; a
        // module's size comes from its descriptor.
        struct Ghost { int typeIndex = 0; int dx = 0; int dy = 0; int w = 1; int h = 1; };
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
    // Whichever canvas the pointer is over, in any slot window, or null. Asked
    // rather than waiting to be told, so the outlines can be shown the moment a
    // command arms them instead of only once the mouse moves.
    static PatchCanvas* canvasUnderPointer();
    static juce::Point<int> pointerScreenPosition();
    // The selected modules and the cables running between them. Copy, Duplicate
    // and Save as Snippet all want exactly this, so they read it from here.
    void collectSelection(std::vector<ClipboardEntry>& entriesOut,
                          std::vector<ClipboardCable>& cablesOut) const;
    // `origin` is subtracted from every position, which is how pasting at a
    // chosen spot moves the block's top-left to (0, 0). Passing (0, 0) keeps the
    // positions the modules were copied from, which is what Duplicate wants. The
    // origin is given rather than worked out here because a block can hold text
    // notes too, and they have their say in where its top-left is.
    static SnipData toSnip(const std::vector<ClipboardEntry>& entries,
                           const std::vector<ClipboardCable>& cables,
                           juce::Point<int> origin);


    // Parameter context menu (right-click on knob/slider/button)
    void showParameterContextMenu(Module& m, int section, Parameter& param);

    // Selection helpers
    bool isSelected(const Module* m) const;
    void clearSelection();
    void selectModule(Module* m, int section, bool addToSelection = false);
    void updateRubberBandSelection(juce::Rectangle<int> rect);
    /** Starts dragging everything that is selected, modules and notes alike. */
    void beginMultiMove(juce::Point<int> pos);
    void showSelectionContextMenu();
    void deleteSelection();
    void duplicateSelection(bool withCables);
    void copySelectionToClipboard();
    // Paste does not place anything: it hangs the copied modules off the
    // pointer and the click that follows puts them down (issue #42).
    void beginPasteGhost();
    void beginAddModuleGhost(int typeIndex, const juce::String& name);
    void beginAddCommentGhost();
    // Puts the clipboard down with its top-left at this grid position.
    void pasteBlockAt(int gx, int gy);

    // The one module the Inspector follows: the most recently selected.
    ModuleRef selectedRef;

    // Connector hit-testing helpers
    struct ConnectorHit { Module* module = nullptr; Connector* connector = nullptr; int section = 0; };
    ConnectorHit findConnectorAt(juce::Point<int> pos);
    // True when at least one cable is plugged into `conn`, which is what decides
    // whether a modifier+drag re-routes or starts a new cable (#67).
    static bool connectorHasCable(const ModuleContainer& container, const Connector* conn);

    // The cable a re-route is carrying. Nothing has happened to the patch while
    // this is set: the cable is only hidden from the canvas so it looks lifted.
    // The model, and therefore the synth, is not touched until the drop lands,
    // so a re-route that comes to nothing sends nothing at all.
    //
    // Held both ways on purpose. The two pointers are what the painter skips and
    // what the net walk steps over, and they are only read inside the one drag
    // that set them. The indices are what the undo actions and the synth
    // messages are addressed by, and they still name the same cable if the patch
    // moved underneath: the same reason ModuleRef exists.
    struct LiftedCable
    {
        int section = -1;   // -1 = nothing lifted
        Connector* out = nullptr;
        Connector* in  = nullptr;
        int outModIndex = 0, outConnIndex = 0; bool outIsOutput = false;
        int inModIndex  = 0, inConnIndex  = 0; bool inIsOutput  = false;
        bool isValid() const { return section >= 0; }
    };
    LiftedCable liftedCable;

    // Picks the cable a re-route will carry and returns the connector at its far
    // end, which the drag then anchors to. Reads the patch; does not change it.
    ConnectorHit noteCableToLift(ModuleContainer& container, int section, Connector* conn);
    // Performs the move: takes the lifted cable off and puts the new one on, in
    // that order, both on the model and on the undo stack.
    void commitLiftedCableMove(ModuleContainer& container, Connector* outConn, Connector* inConn);
    // True when this pair is the cable that was lifted, i.e. the drop landed
    // back where the drag started and there is nothing to do.
    bool sameAsLiftedCable(int outModIndex, const Connector* out,
                           int inModIndex,  const Connector* in) const;
    Connector* findConnectorByComponentId(Module& m, const juce::String& componentId);

    // Module overlap prevention. The area form is the general one: a text note
    // can be several columns wide, so "is this spot free" is a rectangle
    // question, and both modules and notes can be in the way of either.
    bool isAreaFree(int gx, int gw, int gy, int gh,
                    const Module* excludeModule, int excludeCommentId) const;
    int findNearestFreeYForArea(int gx, int gw, int targetY, int gh,
                                const Module* excludeModule, int excludeCommentId) const;
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
    // A selected text note counts as a selection everywhere the edit commands
    // are concerned: it can be cut, copied, duplicated and deleted like a module.
    /** Selects exactly these {section, containerIndex} modules, dropping
        whatever was selected before. Used after a paste and after an undo
        that puts deleted modules back. */
    void selectCreated(const std::vector<std::pair<int, int>>& created);

    bool hasSelection() const { return !selection.empty() || !selectedCommentIds.empty(); }
    bool canPaste()     const { return !clipboard.empty() || !clipboardComments.empty(); }
    void cutSelection()
    {
        if (!hasSelection())
            return;
        copySelectionToClipboard();
        deleteSelection();
    }
    void copySelection()      { if (hasSelection()) copySelectionToClipboard(); }
    void pasteClipboard()     { if (canPaste()) beginPasteGhost(); }
    void duplicateSelected()  { if (hasSelection()) duplicateSelection(true); }

    // A pending paste or add belongs to the editor, not to one canvas, so
    // Escape has to reach it from wherever the keyboard focus happens to be.
    static bool isDropPending() { return pendingDrop.active(); }
    static void cancelPendingDrop();
    // Puts a pending block down where the pointer is, without needing a click.
    // Adding a module with Enter is meant to be quick, so Enter finishes it.
    static bool dropPendingAtPointer();
    // Hands a module to the pointer from outside any canvas — what a click on
    // the module icon bar does (issue #17). Same gesture as Add Module.
    static void beginAddModuleDrop(int typeIndex, const juce::String& name);
    // The same gesture for the editor's own text note, which the module bar
    // offers under ANME: it has no descriptor, so it needs its own entry point.
    static void beginAddCommentDrop();
    // The size a fresh note arrives at, in grid units. One column wide like a
    // module, two rows tall, and the corner grips take it from there.
    static constexpr int commentDefaultWidth  = 1;
    static constexpr int commentDefaultHeight = 2;

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
    // Zooms to `newZoom`, keeping the canvas point under the pointer where it
    // is. Shared by Ctrl+wheel and the trackpad pinch, which differ only in how
    // they arrive at the new level.
    void zoomAnchoredToPointer(float newZoom, const juce::MouseEvent& e);

    static constexpr int paramSendIntervalMs = 50;  // Min interval between param sends during drag

    QuickAddPopup* activeQuickAdd = nullptr;  // nullptr when no popup open

    // Cable shake: per-connection random sag offsets for visual redistribution
    std::map<std::pair<const Connector*, const Connector*>, float> cableSagOffsets;

    // Scratch buffer for paintCables' connector-to-module lookup, kept as a
    // member purely so its capacity survives between paints. Rebuilt from the
    // live modules on every paint and never read outside one: see the comment
    // there for why this must not become a cache.
    mutable std::vector<std::pair<const Connector*, const Module*>> cableOwnerScratch_;

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
    Selection getPrimarySelection()
    {
        // Resolved fresh from the patch, so a module deleted since it was
        // selected comes back as nothing rather than as a dangling pointer.
        if (auto* m = polyCanvas.getSelectedModule())
            return { m, polyCanvas.getSelectedSection() };
        if (auto* m = commonCanvas.getSelectedModule())
            return { m, commonCanvas.getSelectedSection() };
        return {};
    }

    // --- Callbacks forwarded to both section canvases ---

    /** Hands one callback to both section canvases. Written out by hand, each
        of these was six lines that passed the callback to the first canvas and
        moved it into the second: get that order the wrong way round in a
        copy-paste and the second canvas silently gets an empty callback. Here
        the order is written once. */
    template <typename Setter, typename Callback>
    void toBothCanvases(Setter setter, Callback cb)
    {
        (polyCanvas.*setter)(cb);
        (commonCanvas.*setter)(std::move(cb));
    }

    void setParameterChangeCallback(PatchCanvas::ParameterChangeCallback cb)
    { toBothCanvases(&PatchCanvas::setParameterChangeCallback, std::move(cb)); }

    void setParameterDragCompleteCallback(PatchCanvas::ParameterDragCompleteCallback cb)
    { toBothCanvases(&PatchCanvas::setParameterDragCompleteCallback, std::move(cb)); }

    void setCustomParameterChangeCallback(PatchCanvas::CustomParameterChangeCallback cb)
    { toBothCanvases(&PatchCanvas::setCustomParameterChangeCallback, std::move(cb)); }

    void setModuleDropCallback(PatchCanvas::ModuleDropCallback cb)
    { toBothCanvases(&PatchCanvas::setModuleDropCallback, std::move(cb)); }

    void setDeleteModuleCallback(PatchCanvas::DeleteModuleCallback cb)
    { toBothCanvases(&PatchCanvas::setDeleteModuleCallback, std::move(cb)); }

    void setRenameModuleCallback(PatchCanvas::RenameModuleCallback cb)
    { toBothCanvases(&PatchCanvas::setRenameModuleCallback, std::move(cb)); }

    void setCommentAddCallback(PatchCanvas::CommentAddCallback cb)
    { toBothCanvases(&PatchCanvas::setCommentAddCallback, std::move(cb)); }

    void setCommentMoveCallback(PatchCanvas::CommentMoveCallback cb)
    { toBothCanvases(&PatchCanvas::setCommentMoveCallback, std::move(cb)); }

    void setCommentTextCallback(PatchCanvas::CommentTextCallback cb)
    { toBothCanvases(&PatchCanvas::setCommentTextCallback, std::move(cb)); }

    void setCommentDeleteCallback(PatchCanvas::CommentDeleteCallback cb)
    { toBothCanvases(&PatchCanvas::setCommentDeleteCallback, std::move(cb)); }

    void setCommentResizeCallback(PatchCanvas::CommentResizeCallback cb)
    { toBothCanvases(&PatchCanvas::setCommentResizeCallback, std::move(cb)); }

    void setCommentCreateCallback(PatchCanvas::CommentCreateCallback cb)
    { toBothCanvases(&PatchCanvas::setCommentCreateCallback, std::move(cb)); }

    void setModuleMoveCallback(PatchCanvas::ModuleMoveCallback cb)
    { toBothCanvases(&PatchCanvas::setModuleMoveCallback, std::move(cb)); }

    void setModuleSelectedCallback(PatchCanvas::ModuleSelectedCallback cb)
    { toBothCanvases(&PatchCanvas::setModuleSelectedCallback, std::move(cb)); }

    void setMorphAssignCallback(PatchCanvas::MorphAssignCallback cb)
    { toBothCanvases(&PatchCanvas::setMorphAssignCallback, std::move(cb)); }

    void setMorphRangeChangeCallback(PatchCanvas::MorphRangeChangeCallback cb)
    { toBothCanvases(&PatchCanvas::setMorphRangeChangeCallback, std::move(cb)); }

    void setKnobAssignCallback(PatchCanvas::KnobAssignCallback cb)
    { toBothCanvases(&PatchCanvas::setKnobAssignCallback, std::move(cb)); }

    void setMidiCtrlAssignCallback(PatchCanvas::MidiCtrlAssignCallback cb)
    { toBothCanvases(&PatchCanvas::setMidiCtrlAssignCallback, std::move(cb)); }

    void setInitModuleCallback(PatchCanvas::InitModuleCallback cb)
    { toBothCanvases(&PatchCanvas::setInitModuleCallback, std::move(cb)); }

    void setSnippetSaveCallback(PatchCanvas::SnippetSaveCallback cb)
    { toBothCanvases(&PatchCanvas::setSnippetSaveCallback, std::move(cb)); }

    void setSnippetDropCallback(PatchCanvas::SnippetDropCallback cb)
    { toBothCanvases(&PatchCanvas::setSnippetDropCallback, std::move(cb)); }

    void setSnippetInsertCallback(PatchCanvas::SnippetInsertCallback cb)
    { toBothCanvases(&PatchCanvas::setSnippetInsertCallback, std::move(cb)); }

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

    /** Aggregate selected modules from both canvases. Each is resolved from
        the patch as the list is built, so nothing deleted can come back in it. */
    std::vector<std::pair<Module*, int>> getSelectedModules()
    {
        auto sel = polyCanvas.getSelectedModules();
        auto common = commonCanvas.getSelectedModules();
        sel.insert(sel.end(), common.begin(), common.end());
        return sel;
    }

    void setCableCreatedCallback(PatchCanvas::CableCallback cb)
    { toBothCanvases(&PatchCanvas::setCableCreatedCallback, std::move(cb)); }

    void setCableDeletedCallback(PatchCanvas::CableCallback cb)
    { toBothCanvases(&PatchCanvas::setCableDeletedCallback, std::move(cb)); }

    void setUndoCallback(std::function<void()> cb)
    { toBothCanvases(&PatchCanvas::setUndoCallback, std::move(cb)); }

    void setRedoCallback(std::function<void()> cb)
    { toBothCanvases(&PatchCanvas::setRedoCallback, std::move(cb)); }

    void setFileCommandCallback(PatchCanvas::FileCommandCallback cb)
    { toBothCanvases(&PatchCanvas::setFileCommandCallback, std::move(cb)); }

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

    /** Selects exactly the modules an undo has just put back, in whichever
        area each of them lives. Both areas are told, so a block that spanned
        the two comes back selected as one again. */
    void selectRestored(const std::vector<std::pair<int, int>>& modules)
    {
        polyCanvas.selectCreated(modules);
        commonCanvas.selectCreated(modules);
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
