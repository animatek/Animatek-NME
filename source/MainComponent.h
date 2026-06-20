#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "model/ModuleDescriptions.h"
#include "model/ThemeData.h"
#include "model/Patch.h"
#include "model/PatchVariations.h"
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

class SynthSettingsDialog;

class MainComponent : public juce::Component,
                      public juce::MenuBarModel
{
public:
    explicit MainComponent(juce::ApplicationProperties& props);
    ~MainComponent() override;

    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    ModuleDescriptions& getModuleDescriptions() { return moduleDescs; }

private:
    void newPatch();
    void openPatch();
    void savePatch();
    void savePatchAs();
    void storePatchToBank();
    void loadPatchFromFile(const juce::File& file);
    bool savePatchToFile(const juce::File& file);
    void importSnippet();
    void importSnippetFromFile(const juce::File& file);
    void importSnippetFromFile(const juce::File& file, int targetGridX, int targetGridY);
    void saveSnippet(SnipData snip);
    void choosePresetLibraryFolder();
    void applyEditorOptions(const EditorOptions& opts);
    void applyUiTheme(int index, bool persist);
    void toggleWireframe();
    void togglePresetBrowser();
    void showPresetBrowser();
    void toggleKnobFloater();
    void toggleKeyboardFloater();
    void togglePatchNotesFloater();
    void toggleMutatorWindow();
    void toggleSysexMonitor();
    bool handleFloaterShortcut(const juce::KeyPress& key);  // Ctrl+1..8
    void showFloaterWindow(juce::DocumentWindow& window, const juce::String& settingsPrefix);
    void saveFloaterState();
    void restoreFloaterWindows();  // reopen floaters that were open last session

    void showMidiSettingsDialog();
    void showPatchSettingsDialog();
    void showSynthSettingsDialog();
    void showEditorOptionsDialog();
    void openSynthSettingsDialog();
    void showBetaWarning(bool forceShow = false);
    void showKeyboardShortcutsDialog();
    void randomizeParameters(bool gaussian);
    void initializeModule(int section, Module* module);
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
    void handleConnectionRequest(const juce::String& inputId, const juce::String& outputId);
    void handleDisconnectionRequest();
    void onConnectionStatusChanged(const ConnectionManager::Status& status);
    void attemptAutoConnect();
    void saveMidiSettings(const juce::String& inputId, const juce::String& outputId);
    void openURL(const juce::String& url);

    juce::ApplicationProperties& appProperties;
    ModuleDescriptions moduleDescs;
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

    int activeSlot = 0;  // Which slot is currently displayed in the UI
    int pendingBrowserLoadSlot = -1;  // Directed browser load target, while patch data is in flight

    EditorOptions editorOptions;
    std::unique_ptr<PresetBrowserWindow> presetBrowserWindow;
    std::unique_ptr<KnobFloaterWindow> knobFloaterWindow;
    std::unique_ptr<KeyboardFloaterWindow> keyboardFloaterWindow;
    std::unique_ptr<PatchNotesFloaterWindow> patchNotesFloaterWindow;
    std::unique_ptr<MutatorWindow> mutatorWindow;
    std::unique_ptr<SysexMonitorWindow> sysexMonitorWindow;

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

    void switchToSlot(int slot, bool notifySynth = true);
    void updateDspLoadDisplay();
    void rebuildUndoContext(int slot);  // call after patch change
    void clearSnapshots(int slot);     // call when patch changes

    // Parameter variations (8 per patch slot, persisted in a .var sidecar)
    PatchVariations variations[numSlots];

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

    juce::String lastInputId;
    juce::String lastOutputId;
    int autoConnectRetries = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
