#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "model/ModuleDescriptions.h"
#include "model/ThemeData.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"
#include "model/SnipFileIO.h"
#include "model/SynthSettings.h"
#include "midi/ConnectionManager.h"
#include "sync/PatchSynchronizer.h"
#include "undo/PatchActions.h"
#include "ui/MainLayout.h"
#include "ui/EditorOptionsDialog.h"

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
    void saveSnippet(SnipData snip);

    void showMidiSettingsDialog();
    void showPatchSettingsDialog();
    void showSynthSettingsDialog();
    void showEditorOptionsDialog();
    void openSynthSettingsDialog();
    void showBetaWarning(bool forceShow = false);
    void randomizeParameters(bool gaussian);
    void initializeModule(int section, Module* module);
    void handleSnapshotClick(int index, bool isShiftClick);
    void saveSnapshot(int index);
    void recallSnapshot(int index);
    void interpolateSnapshots(int fromIndex, int toIndex, float seconds);
    void onInterpolationTick();
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

    // Parameter snapshots (8 slots per patch slot)
    struct ParamSnapshot {
        struct Entry { int section, moduleId, paramId, value; };
        std::vector<Entry> entries;
        bool filled = false;
    };
    ParamSnapshot snapshots[numSlots][8];  // [slot][snapshot]
    int activeSnapshotIndex[numSlots] = { -1, -1, -1, -1 };

    // Interpolation state
    struct InterpolationState {
        bool active = false;
        std::vector<ParamSnapshot::Entry> from;
        std::vector<ParamSnapshot::Entry> to;
        float durationMs = 0;
        float elapsedMs = 0;
        int targetSnapshot = -1;
    };
    InterpolationState interpolation;
    std::unique_ptr<juce::Timer> interpolationTimer;

    juce::String lastInputId;
    juce::String lastOutputId;
    int autoConnectRetries = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
