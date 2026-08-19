#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <utility>
#include <vector>
#include "model/ModuleDescriptions.h"
#include "model/ModulePresets.h"
#include "model/ThemeData.h"
#include "model/Patch.h"
#include "model/PatchVariations.h"
#include "model/PatchExtras.h"
#include "model/PchFileIO.h"
#include "model/SnipFileIO.h"
#include "model/SynthSettings.h"
#include "midi/ConnectionManager.h"
#include "sync/BankTransferManager.h"
#include "sync/PatchSynchronizer.h"
#include "undo/PatchActions.h"
#include "ui/MainLayout.h"
#include "ui/EditorOptionsDialog.h"
#include "ui/PresetBrowserWindow.h"
#include "ui/KnobFloaterWindow.h"
#include "ui/KeyboardFloaterWindow.h"
#include "ui/PatchNotesFloaterWindow.h"
#include "ui/MutatorWindow.h"
#include "ui/SysexMonitorWindow.h"
#if NME_MCP_BRIDGE
#include "mcp/McpBridgeServer.h"
#endif

class SynthSettingsDialog;

class MainComponent : public juce::Component,
                      public juce::MenuBarModel
{
public:
    explicit MainComponent(juce::ApplicationProperties& props);
    ~MainComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    ModuleDescriptions& getModuleDescriptions() { return moduleDescs; }

    // Small slot-indexed accessors for McpRequestHandler (source/mcp/), which
    // needs to reach a specific slot's model/undo state without duplicating
    // MainComponent's own slot-lookup logic or requiring friend access.
    int getActiveSlot() const { return activeSlot; }
    Patch* getSlotPatch(int slot) const { return slotPatches[slot].get(); }
    const juce::File& getSlotPatchFile(int slot) const { return slotPatchFiles[slot]; }
    juce::File getPatchesFolder() const { return editorOptions.getPatchesFolder(); }
    // Write a specific slot's patch (+ .var sidecar) to disk. No UI side effects,
    // so it is safe to call from the MCP handler. Returns false if the slot is
    // empty or the write failed.
    bool saveSlotPatchToFile(int slot, const juce::File& file);
    // Upload a slot's patch to the synth and, once the upload is ACKed, store it
    // into a bank location (bankSection 0-8 = banks 1-9, position 0-98). Async:
    // returns true after initiating; on a precondition failure returns false and
    // fills `error`. For the MCP store_to_bank tool.
    bool storeSlotPatchToBank(int slot, int bankSection, int position, juce::String& error);
    juce::UndoManager& getSlotUndoManager(int slot) { return slotUndoManagers[slot]; }
    UndoContext* getSlotUndoContext(int slot) const { return slotUndoContexts[slot].get(); }
    bool isPatchTransferInProgress() const
    {
        return connectionManager.isUploadingPatch() || connectionManager.isFetchingPatch();
    }
    const juce::File& getPresetLibraryRoot() const { return editorOptions.presetLibraryRoot; }
    bool createEmptyPatchInSlot(int slot, const juce::String& name, bool activate,
                                juce::String& error);
    bool loadPatchFileIntoSlot(int slot, const juce::File& file, bool activate,
                               juce::String& error);
    void prepareSlotModuleDeletion(int slot);

private:
    // The four slot canvases. There is deliberately no "the canvas" accessor:
    // canvasFor() is for anything driven by the model (a patch arriving for a
    // given slot, a light/meter frame), activeCanvas() only for what genuinely
    // follows the user's focus (zoom, shake, the View menu). See docs/MDI_PLAN.md.
    PatchCanvasComponent& canvasFor(int slot);
    PatchCanvasComponent& activeCanvas();
    void wireSlotView(int slot);
    void handleSlotFileCommand(int slot, const juce::String& cmd);
    // For the editor-wide canvas flags (mutator mode): a canvas left out comes
    // back on screen still drawing the previous state.
    void repaintAllCanvases();

    void newPatch();
    void openPatch();
    void storePatchToBank();
    // One click on the header's store button: put the patch back where it came
    // from, no dialog. Falls back to storePatchToBank() when the editor does not
    // know a location (a new patch, or one opened from a file).
    void quickStoreToBank();
    // Tell the synth to write a slot's patch into a bank location, remember it
    // as that slot's home, and say so in the status bar.
    void sendStoreToBank(int slot, int section, int position);
    // Work out where a synth-fetched patch lives by looking its name up in the
    // bank list, for the case the synth never told us (a patch already sitting
    // in a slot when the editor connected). Only fills in a location that is
    // still unknown, and only when the name is unique across the banks.
    void inferSlotBankLocation(int slot);
    // Refresh the header's store button from the active slot's known location.
    void updateStoreLocationDisplay();
    // Fetch a bank patch into a named slot (browser double-click and its
    // right-click "Load to Slot A..D").
    void loadBankPatchIntoSlot(int section, int position, int slot);
    // Opening a .pch shows the slot chooser (issue #21); the chosen destination
    // and Local flag are then applied by loadPatchFromFile.
    void openPatchFileWithChooser(const juce::File& file);
    void loadPatchFromFile(const juce::File& file, int targetSlot, bool localOnly);
    bool replacePatchInSlot(int slot, std::unique_ptr<Patch> patch,
                            const juce::File& sourceFile, bool activate,
                            bool loadVariations, juce::String& error);
    void importSnippet();
    void importSnippetFromFile(int slot, const juce::File& file);
    void importSnippetFromFile(int slot, const juce::File& file, int targetGridX, int targetGridY);
    void saveSnippet(SnipData snip);
    void choosePresetLibraryFolder();
    void applyEditorOptions(const EditorOptions& opts);
#if NME_MCP_BRIDGE
    void setMcpBridgeEnabled(bool enabled);
    // The exact stdio command an MCP client (Claude Code, Claude Desktop,
    // etc.) needs to register mcp-bridge/server.py - searched relative to
    // the running executable so it works from any checkout location.
    juce::String getMcpBridgeCommand() const;
#endif
    void applyUiTheme(int index, bool persist);
    void toggleWireframe();
    void toggleLeftPanel();   // Ctrl+I: inspector column (issue #38)
    void toggleRightPanel();  // Ctrl+Shift+I: patch browser
    void toggleModuleIconBar();  // View menu: the module palette strip (issue #17)
    void togglePresetBrowser();
    void showPresetBrowser();
    void toggleKnobFloater();
    void toggleKeyboardFloater();
    void togglePatchNotesFloater();
    void toggleMutatorWindow();
    void toggleSysexMonitor();
    void toggleSlotOpen(int slot);  // Show/hide one slot's sub-window in the work area
    void toggleFocusMode();         // F4 (F11 alias): blow the focused sub-window up to the full area
    bool handleFloaterShortcut(const juce::KeyPress& key);  // Ctrl+1..9, T
    void showFloaterWindow(juce::DocumentWindow& window, const juce::String& settingsPrefix);
    void saveFloaterState();
    void restoreFloaterWindows();  // reopen floaters that were open last session

    // Which slots are on screen, how they are arranged, and which one has focus.
    // Kept apart from the floater state: floaters are OS windows saved in screen
    // coordinates, which mean nothing for a sub-window of the work area.
    void saveMdiLayout();
    void restoreMdiLayout();
    // Once per connection, and only once: line the open sub-windows up with the
    // slots the synth has enabled (plus the hardware-focused slot, defensively).
    // After that the windows are the user's to open and close; the enable mask
    // is a single slot most of the time and changes on every slot press, so
    // following it live would keep closing everything but one.
    void scheduleSlotWindowReconcile();
    void reconcileSlotWindowsWithSynth(const std::array<bool, 4>& enabled);
    // Starts true: the constructor opens a slot before restoreMdiLayout() runs,
    // and that fires onLayoutChanged, which would save the default layout over
    // the stored one before it was ever read. Cleared when restore finishes.
    bool restoringMdiLayout = true;
    bool syncingSlotWindows = false;
    std::array<bool, 4> lastEnabledSlots {};
    bool slotEnableStateKnown = false;
    bool slotWindowsReconciled = false;
    bool slotWindowsReconcileScheduled = false;

    void showMidiSettingsDialog();
    void showPatchSettingsDialog();
    void showSynthSettingsDialog();
    void showEditorOptionsDialog();
    void openSynthSettingsDialog();
    void showBetaWarning(bool forceShow = false);
    void showKeyboardShortcutsDialog();

    // ── Borrowing the synth's display ─────────────────────────────────────
    // The G1 shows the active slot's patch name on its own display. With the
    // option on, a dialog on screen borrows it ("ANME 0.17v") and the patch name
    // goes back when the dialog closes. The caption does not name the dialog:
    // what is worth saying on a display whose job is to tell you which patch you
    // are on is only that the editor has taken it over.
    //
    // Nothing here goes near the patch. Only the SysEx that sets the name on
    // the synth is sent, so the editor's own Patch object is untouched: nothing
    // is marked modified, nothing lands on the undo stack, and the bank-location
    // matching, which works by comparing the patch name against the bank list,
    // goes on seeing the real name. The synth's copy is its edit buffer and is
    // not written to flash (see SetPatchTitleMessage).
    void announceDialogOnSynth(juce::Component* dialog);
    void setSynthCaption();
    void clearSynthCaption();
    bool canBorrowSynthDisplay() const;
    static juce::String makeSynthCaption();

    // Which slot's name is currently borrowed, or -1 when none is. Remembered
    // rather than re-read at restore time, so switching slot while a dialog is
    // open still gives the name back to the slot it was taken from.
    int synthCaptionSlot = -1;

    // The dialogs do not share a close path, so the caption is tied to the
    // dialog component's lifetime instead: a listener that fires when it is
    // deleted, whichever way it was dismissed, including the editor quitting
    // with it still on screen.
    struct SynthCaptionWatcher : public juce::ComponentListener
    {
        explicit SynthCaptionWatcher(MainComponent& o) : owner(o) {}
        void componentBeingDeleted(juce::Component& c) override
        {
            c.removeComponentListener(this);
            if (&c != watched)
                return;
            watched = nullptr;
            owner.clearSynthCaption();
        }
        MainComponent& owner;
        juce::Component* watched = nullptr;
    };
    SynthCaptionWatcher synthCaptionWatcher { *this };
    void randomizeSlotParameters(int slot, PatchCanvasComponent& canvas, bool gaussian);  // issue #22
    void saveSlotPatch(int slot);     // Ctrl+S: save this slot, not "the" slot (issue #22)
    void saveSlotPatchAs(int slot);   // Ctrl+Shift+S, same
    juce::File suggestedSaveFileForSlot(int slot, const juce::File& folder) const;
    void initializeModule(int slot, int section, Module* module);
    void handleSnapshotClick(int index, bool isShiftClick);
    void saveSnapshot(int index);
    void recallSnapshot(int index);
    void copySnapshot(int from, int to);
    void initSnapshot(int index);
    void applySnapshot(const ParamSnapshot& snap, const juce::String& undoName);
    void refreshSnapshotUi();
    void interpolateSnapshots(int fromIndex, int toIndex, float seconds);
    // targetVariation >= 0 marks that variation active on completion; -1 = mutator audition
    void startInterpolationTo(const ParamSnapshot& snap, float seconds, int targetVariation);
    void onInterpolationTick();
    void stopInterpolation(const char* reason);

    // Morph A/B fader (editor-side software morph between two captured sounds;
    // proof-of-concept for driving an editor macro from a physical panel knob).
    void setMorphEndpoint(bool isB, int snapIndex);  // snapIndex -1 = current sound
    void assignMorphKnob(int knobIndex);   // assign a panel knob (editor->synth) to drive the fader
    void rebuildMorphPairs();
    void applyMorphPosition(float t);        // t in [0,1]; live, no undo
    void armMorphKnobLearn();
    void clearMorphKnobAssignment();
    void resetMorphAB();
    void refreshMorphUi();
    /** Redraws the Knob Floater's cells from the patch, if it is on screen. The
        floater shows knobs assigned to the morph groups as well as to module
        parameters, so anything that moves a morph value has to say so here or
        those cells sit at whatever they last read (issue #64). */
    void refreshKnobFloater();
    void handleConnectionRequest(const juce::String& inputId, const juce::String& outputId);
    void handleDisconnectionRequest();
    void onConnectionStatusChanged(const ConnectionManager::Status& status);
    void attemptAutoConnect();
    void saveMidiSettings(const juce::String& inputId, const juce::String& outputId);
    void openURL(const juce::String& url);

    juce::ApplicationProperties& appProperties;
    ModuleDescriptions moduleDescs;
    ModulePresetLibrary modulePresets;
    ThemeData themeData;
    ConnectionManager connectionManager;
    BankTransferManager bankTransfer { connectionManager, moduleDescs };
    std::unique_ptr<MainLayout> mainLayout;
    std::unique_ptr<juce::MenuBarComponent> menuBar;

    // Multi-slot state (4 slots: A/B/C/D)
    static constexpr int numSlots = 4;
    std::unique_ptr<Patch> slotPatches[numSlots];
    juce::File slotPatchFiles[numSlots];
    std::unique_ptr<PatchSynchronizer> slotSynchronizers[numSlots];
    juce::UndoManager slotUndoManagers[numSlots];
    std::unique_ptr<UndoContext> slotUndoContexts[numSlots];
    // A slot is "local" when its editor patch is not known to match the synth:
    // loaded via the Local option, or loaded/built while disconnected. Cleared
    // once the patch is uploaded to, or fetched from, the synth. Drives the
    // "LOCAL" badge in the slot bar (issue #21).
    bool slotIsLocal[numSlots] = {};
    // This slot's patch was fetched from the synth (as opposed to built here or
    // opened from a file), so looking its name up in the bank list to find where
    // it lives is a fair guess rather than an invention.
    bool slotPatchFromSynth[numSlots] = {};
    // Bank positions whose name matches this slot's patch, when there is more
    // than one: not an answer, but the shortlist the store dialog opens on.
    std::vector<std::pair<int, int>> slotBankCandidates[numSlots];
    void setSlotLocal(int slot, bool local);

    int activeSlot = 0;  // Which slot is currently displayed in the UI
    int pendingBrowserLoadSlot = -1;  // Directed browser load target, while patch data is in flight

    EditorOptions editorOptions;
    std::unique_ptr<PresetBrowserWindow> presetBrowserWindow;
    std::unique_ptr<KnobFloaterWindow> knobFloaterWindow;
    std::unique_ptr<KeyboardFloaterWindow> keyboardFloaterWindow;
    std::unique_ptr<PatchNotesFloaterWindow> patchNotesFloaterWindow;
    std::unique_ptr<MutatorWindow> mutatorWindow;
    std::unique_ptr<SysexMonitorWindow> sysexMonitorWindow;

    // Local control socket for the MCP bridge (source/mcp/, mcp-bridge/) -
    // built and started at the end of the constructor once slots/moduleDescs
    // exist, torn down first thing in the destructor.
#if NME_MCP_BRIDGE
    std::unique_ptr<McpBridgeServer> mcpBridgeServer;
#endif

    // Last-known global synth settings.
    SynthSettings cachedSynthSettings;
    bool pendingSynthSettingsDialogOpen = false;
    juce::Component::SafePointer<SynthSettingsDialog> synthSettingsDialog;

    // Convenience accessors for current slot
    std::unique_ptr<Patch>& currentPatch() { return slotPatches[activeSlot]; }
    juce::File& currentPatchFile() { return slotPatchFiles[activeSlot]; }
    std::unique_ptr<PatchSynchronizer>& currentSynchronizer() { return slotSynchronizers[activeSlot]; }
    juce::UndoManager& undoManager() { return slotUndoManagers[activeSlot]; }
    std::unique_ptr<UndoContext>& undoContext() { return slotUndoContexts[activeSlot]; }

    // bringOnScreen=false is the synth-initiated path: adopt the slot, but never
    // open a sub-window the user has closed and never yank focus mid-drag.
    void switchToSlot(int slot, bool notifySynth = true, bool bringOnScreen = true);
    // Telling the synth which slot to focus is debounced: walking focus across
    // four sub-windows must not spray SlotActivated messages down the wire.
    void notifySynthOfSlot(int slot);
    // Zero a canvas's LEDs and meters. The synth streams them for one slot at a
    // time, so the slot being left has to be blanked or it freezes lit.
    void clearLightMeterData(int slot);
    int  pendingSynthSlot = -1;
    int  synthSlotGeneration = 0;
    bool inSlotFocusChange = false;   // switchToSlot -> focusSlot -> onSlotFocused
    void updateDspLoadDisplay();

    // Module presets. wirePresetCallbacks() serves both the main window's
    // inspector and a slot window's, since the two are the same class driving
    // the same library.
    void initModulePresetLibrary();
    static juce::File appConfigFolder();
    juce::File presetsFolder() const;
    void wirePresetCallbacks(InspectorPanel& inspector, int slot);
    void recallModulePreset(int slot, int section, Module* module, int presetIndex);
    // After a save or a delete, every open inspector redraws its preset list, so
    // two windows showing the same module never disagree about what exists.
    void refreshInspectorPresets();
    void rebuildUndoContext(int slot);  // call after patch change
    // Modules an undo has just put back, filled by UndoContext::onModuleRestored
    // while the undo runs and read once it finishes: undoing a delete hands the
    // selection back rather than leaving you with nothing selected over the
    // module you just recovered.
    std::vector<std::pair<int, int>> restoredModules;   // {section, containerIndex}
    void runUndoRestoringSelection(int slot, bool redo);
    void clearSnapshots(int slot);     // call when patch changes

    // Parameter variations (8 per patch slot, persisted in a .var sidecar)
    PatchVariations variations[numSlots];

    // --- The extras library ---------------------------------------------------
    // Comments, patch notes, variations and Mutator exclusions have nowhere to
    // live on the synth, so a patch read back from it used to arrive stripped of
    // all of them. They are kept here as well, one entry per patch, found again
    // by the id a .pch carries or, for a patch off the wire, by its fingerprint.
    PatchExtrasStore patchExtras;
    juce::String slotExtrasId[numSlots];
    bool extrasDirty[numSlots] = {};
    std::unique_ptr<juce::Timer> extrasFlushTimer;

    /** Looks the slot's patch up in the library and applies whatever it finds.
        Used for patches arriving from the synth, which carry nothing. */
    void attachExtrasFromLibrary(int slot);
    /** Binds the slot to an entry and writes the patch's current extras into it,
        the file being the authority. Used when a patch is opened from disk. */
    void bindExtrasFromPatch(int slot);
    /** Notes that this slot's extras have changed; the flush timer writes them.
        Deliberately not a write per edit: variations are written through on
        every knob turn. */
    void markExtrasDirty(int slot) { if (slot >= 0 && slot < numSlots) extrasDirty[slot] = true; }
    void flushExtras(int slot);
    void flushAllExtras();
    /** A cheap summary of everything the library would store for this slot. The
        flush timer watches it, which is what catches the edits that never pass
        through a callback: an undo, a redo, or anything the MCP bridge did. */
    juce::int64 extrasRevision(int slot) const;
    juce::int64 lastExtrasRevision[numSlots] = {};

    // Interpolation state
    struct InterpolationState {
        bool active = false;
        std::vector<ParamSnapshot::Entry> from;
        std::vector<ParamSnapshot::Entry> to;
        float durationMs = 0;
        float elapsedMs = 0;
        int targetSnapshot = -1;   // -1 = not a variation (mutator audition)
        std::array<int, 4> targetMorphs { 0, 0, 0, 0 };
    };
    InterpolationState interpolation;
    std::unique_ptr<juce::Timer> interpolationTimer;

    // Morph A/B fader state (single set for the active slot; reset on patch change)
    ParamSnapshot morphA, morphB;
    struct MorphPair { int section, moduleId, paramId, aVal, bVal; };
    std::vector<MorphPair> morphPairs;     // cached differing params, rebuilt on capture
    bool morphLearnArmed = false;
    int morphKnobSection = -1, morphKnobModule = -1, morphKnobParam = -1;
    int morphKnobIndex = -1;    // physical knob (0..22) assigned as the fader carrier, -1 = none
    int morphKnobMin = 0, morphKnobMax = 127;   // range of the learned knob's param

    juce::String lastInputId;
    juce::String lastOutputId;
    int autoConnectRetries = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
