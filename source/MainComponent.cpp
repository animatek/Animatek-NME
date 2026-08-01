#include "MainComponent.h"
#include "model/PatchParser.h"
#include "model/PchFileIO.h"
#include "ui/BankTransferDialog.h"
#include "ui/MidiSettingsDialog.h"
#include "ui/PatchLocationDialog.h"
#include "ui/AboutDialog.h"
#include "ui/SlotSelectDialog.h"
#include "ui/PatchSettingsDialog.h"
#include "ui/SynthSettingsDialog.h"
#include "ui/AppTheme.h"
#include "ui/ThemeRegistry.h"
#include "model/Mutator.h"
#include "model/MutationCategories.h"
#include "protocol/StorePatchMessage.h"
#include "protocol/MorphKeyboardAssignmentMessage.h"
#include "BinaryData.h"
#include <iostream>
#include <set>
#include <climits>

// ─── Cable opacity slider for View menu ──────────────────────────────────────
class CableOpacitySlider : public juce::PopupMenu::CustomComponent
{
public:
    CableOpacitySlider()
        : juce::PopupMenu::CustomComponent(false)  // false = don't auto-dismiss on click
    {
        slider.setRange(0.0, 1.0, 0.01);
        slider.setValue(static_cast<double>(PatchCanvas::getCableOpacity()),
                        juce::dontSendNotification);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 18);
        slider.setColour(juce::Slider::textBoxTextColourId,       AppTheme::palette().accentWarning);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, AppTheme::palette().inputBackground);
        slider.setColour(juce::Slider::textBoxOutlineColourId,    AppTheme::palette().borderColor);
        slider.setColour(juce::Slider::thumbColourId,             AppTheme::palette().accentActive);
        slider.setColour(juce::Slider::trackColourId,             AppTheme::palette().buttonActive);
        slider.onValueChange = [this]
        {
            PatchCanvas::setCableOpacity(static_cast<float>(slider.getValue()));
            if (auto* topComp = juce::Desktop::getInstance().getComponent(0))
                topComp->repaint();
        };
        addAndMakeVisible(slider);
    }

    void getIdealSize(int& w, int& h) override { w = 230; h = 28; }

    void resized() override
    {
        auto b = getLocalBounds().reduced(2, 2);
        const int labelW = 90;
        slider.setBounds(b.withTrimmedLeft(labelW));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(AppTheme::palette().textSecondary);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Cable Opacity", getLocalBounds().withTrimmedRight(getWidth() - 90).reduced(6, 0),
                   juce::Justification::centredLeft);
    }

private:
    juce::Slider slider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CableOpacitySlider)
};

MainComponent::MainComponent(juce::ApplicationProperties &props)
    : appProperties(props) {
  editorOptions = EditorOptions::load(appProperties.getUserSettings());
  AppTheme::setPalette(ThemeRegistry::get(editorOptions.uiThemeIndex).app);
  editorOptions.ensureLibraryFolders();
  PatchCanvas::setCableStyle   (static_cast<int>(editorOptions.cableStyle));
  PatchCanvas::setKnobControl  (static_cast<int>(editorOptions.knobControl));
  PatchCanvas::setAutoUpload   (editorOptions.autoUpload);
  PatchCanvas::setCableOpacity (editorOptions.cableOpacity);
  {
    const auto& rates = EditorOptions::sendRates();
    const int ri = juce::jlimit(0, static_cast<int>(rates.size()) - 1, editorOptions.sendRateIndex);
    connectionManager.setParamSendRate(rates[static_cast<size_t>(ri)].batch,
                                       rates[static_cast<size_t>(ri)].intervalMs);
  }
  setWantsKeyboardFocus(true);

  // Helper: find a data file by probing multiple locations regardless of CWD.
  // Searches CWD, next to the executable, and up to 5 parent dirs of the
  // executable.
  auto findDataFile = [](const juce::String &relativePath) -> juce::File {
    // 1. Relative to CWD (works when launched from terminal in project root)
    auto f =
        juce::File::getCurrentWorkingDirectory().getChildFile(relativePath);
    if (f.existsAsFile())
      return f;

    // 2. Relative to the executable, going up 1..5 parent directories
    auto exeDir =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory();
    for (int i = 0; i < 5; ++i) {
      f = exeDir.getChildFile(relativePath);
      if (f.existsAsFile())
        return f;
      exeDir = exeDir.getParentDirectory();
    }

    return {}; // not found
  };

  QuickAddPopup::setSharedSettings(appProperties.getUserSettings());

  // Load module descriptions — prefer embedded BinaryData, fall back to disk
  if (BinaryData::modules_xmlSize > 0)
    moduleDescs.loadFromXmlString(juce::String(BinaryData::modules_xml, BinaryData::modules_xmlSize));
  else {
    auto xmlPath = findDataFile("nmedit/libs/nordmodular/data/module-descriptions/modules.xml");
    if (xmlPath.existsAsFile()) moduleDescs.loadFromFile(xmlPath);
    else DBG("WARNING: modules.xml not found!");
  }

  DBG("Loaded " + juce::String(moduleDescs.getModuleCount()) + " module descriptions");

  // Load classic theme — prefer embedded BinaryData, fall back to disk
  if (BinaryData::classictheme_xmlSize > 0)
    themeData.loadFromXmlString(juce::String(BinaryData::classictheme_xml, BinaryData::classictheme_xmlSize));
  else {
    auto themePath = findDataFile("nmedit/libs/nordmodular/data/classic-theme/classic-theme.xml");
    if (themePath.existsAsFile()) themeData.loadFromFile(themePath);
    else DBG("WARNING: classic-theme.xml not found!");
  }

  DBG("Loaded " + juce::String(themeData.getModuleThemeCount()) + " module themes");

  // Menu bar
  menuBar = std::make_unique<juce::MenuBarComponent>(this);
  addAndMakeVisible(menuBar.get());

  // Main layout
  mainLayout = std::make_unique<MainLayout>(moduleDescs);
  {
    auto canvasScheme = ThemeRegistry::get(editorOptions.uiThemeIndex).makeCanvas();
    canvasScheme.wireframe = editorOptions.wireframe;
    mainLayout->setTheme(canvasScheme);
  }
  addAndMakeVisible(mainLayout.get());

  // Wire connection manager status updates to UI
  connectionManager.setStatusCallback(
      [this](const ConnectionManager::Status &status) {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync(
            [safeThis, status]() { if (safeThis) safeThis->onConnectionStatusChanged(status); });
      });

  connectionManager.setVoiceCountCallback([this](const int voiceCounts[4]) {
    int total =
        voiceCounts[0] + voiceCounts[1] + voiceCounts[2] + voiceCounts[3];
    int c0 = voiceCounts[0], c1 = voiceCounts[1], c2 = voiceCounts[2], c3 = voiceCounts[3];
    DBG("[DSP] VoiceCount: " + juce::String(c0) + " " + juce::String(c1) + " "
        + juce::String(c2) + " " + juce::String(c3) + " total=" + juce::String(total));
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync(
        [safeThis, total, c0, c1, c2, c3]() {
          if (!safeThis) return;
          safeThis->mainLayout->getStatusBar().setVoiceCount(total);
          safeThis->mainLayout->getHeaderBar().setSynthDspLoad(c0, c1, c2, c3);
        });
  });

  // Wire canvas selection to inspector
  mainLayout->getCanvas().setModuleSelectedCallback(
      [this](Module* module, int section) {
        if (module)
          mainLayout->getInspector().setModule(module, section);
        else
          mainLayout->getInspector().clearModule();
      });

  // Wire inspector name changes (undoable)
  mainLayout->getInspector().onNameChanged =
      [this](int section, Module* module, const juce::String& oldName, const juce::String& newName) {
    if (!module || !currentPatch() || !undoContext()) return;
    undoManager().beginNewTransaction("Rename Module");
    undoManager().perform(new RenameModuleAction(
        *undoContext(), section, module->getContainerIndex(), oldName, newName));
  };

  // Wire inspector morph group remove
  mainLayout->getInspector().onMorphGroupChanged = [this](int section, Module* module,
                                                           int paramIndex, int morphGroup) {
    if (!currentPatch() || !module || !undoContext()) return;
    int moduleId = module->getContainerIndex();
    int oldGroup = -1, oldRange = 0;
    for (auto& ma : currentPatch()->morphAssignments)
        if (ma.section == section && ma.module == moduleId && ma.param == paramIndex)
        { oldGroup = ma.morph; oldRange = ma.range; break; }
    undoManager().beginNewTransaction("Morph Assign");
    undoManager().perform(new MorphAssignAction(*undoContext(), section, moduleId, paramIndex, morphGroup, oldGroup, oldRange));
  };

  // Wire inspector morph range change
  mainLayout->getInspector().onMorphRangeChanged = [this](int section, Module* module,
                                                           int paramIndex, int span, int dir) {
    if (!module || !currentPatch() || !undoContext()) return;
    int moduleId = module->getContainerIndex();
    int newRange = (dir == 0) ? span : -span;
    int oldRange = 0;
    for (auto& ma : currentPatch()->morphAssignments)
        if (ma.section == section && ma.module == moduleId && ma.param == paramIndex)
        { oldRange = ma.range; break; }
    undoManager().beginNewTransaction("Morph Range");
    undoManager().perform(new MorphRangeChangeAction(*undoContext(), section, moduleId, paramIndex, oldRange, newRange));
  };

  // Wire knob/CC removal from inspector X buttons
  mainLayout->getInspector().onKnobRemoved = [this](int section, int moduleId, int paramId, int /*knobIndex*/) {
    if (!currentPatch() || !undoContext()) return;
    int prevKnob = -1;
    for (int k = 0; k < 23; ++k)
    {
        auto& ka = currentPatch()->knobAssignments[static_cast<size_t>(k)];
        if (ka.assigned && ka.section == section && ka.module == moduleId && ka.param == paramId)
        { prevKnob = k; break; }
    }
    if (prevKnob < 0) return;
    undoManager().beginNewTransaction("Knob Deassign");
    undoManager().perform(new KnobAssignAction(*undoContext(), section, moduleId, paramId, -1, prevKnob));
  };

  mainLayout->getInspector().onMidiCtrlRemoved = [this](int section, int moduleId, int paramId, int /*midiCC*/) {
    if (!currentPatch() || !undoContext()) return;
    int prevCtrl = -1;
    for (auto& ca : currentPatch()->ctrlAssignments)
        if (ca.section == section && ca.module == moduleId && ca.param == paramId)
        { prevCtrl = ca.control; break; }
    if (prevCtrl < 0) return;
    undoManager().beginNewTransaction("MIDI CC Deassign");
    undoManager().perform(new MidiCtrlAssignAction(*undoContext(), section, moduleId, paramId, -1, prevCtrl));
  };

  // Wire patch list updates to patch browser panel
  connectionManager.setPatchListCallback([this](const std::vector<std::string>& names) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, names]() {
      if (!safeThis) return;
      safeThis->mainLayout->getPatchBrowser().setPatchList(names);
      safeThis->mainLayout->getPatchBrowser().setLoadingState(false);
    });
  });

  connectionManager.setSynthSettingsCallback([this](const SynthSettings& settings) {
    cachedSynthSettings = settings;
    if (!settings.name.empty())
      mainLayout->getHeaderBar().setSynthName(juce::String(settings.name));
    if (synthSettingsDialog != nullptr)
      synthSettingsDialog->setSettings(settings);
    if (pendingSynthSettingsDialogOpen)
    {
      pendingSynthSettingsDialogOpen = false;
      openSynthSettingsDialog();
    }
  });

  // Wire patch browser callbacks
  mainLayout->getPatchBrowser().onPatchDoubleClicked = [this](int section, int position) {
    const int targetSlot = activeSlot;
    pendingBrowserLoadSlot = targetSlot;
    mainLayout->getSlotBar().setCurrentTab(targetSlot);

    const char* slotLetters[] = {"A", "B", "C", "D"};
    std::cout << "[MAIN] Loading patch from browser: section=" << section
              << " pos=" << position
              << " targetSlot=" << slotLetters[targetSlot] << std::endl;

    connectionManager.loadPatchFromBank(section, position, targetSlot);
    mainLayout->getHeaderBar().setCurrentLocation(section, position);
    mainLayout->getPatchBrowser().setLoadedPatch(section, position);
  };

  mainLayout->getPatchBrowser().onRefreshRequested = [this]() {
    mainLayout->getPatchBrowser().setLoadingState(true);
    connectionManager.requestPatchList();
  };

  mainLayout->getPatchBrowser().onPatchDelete = [this](int section, int position) {
    const auto& patchList = connectionManager.getPatchList();
    int index = section * 99 + position;
    juce::String patchName = (index < static_cast<int>(patchList.size()) && !patchList[index].empty())
                              ? patchList[index] : "--";
    auto* dialog = new juce::AlertWindow("Delete Patch",
        "Delete \"" + patchName + "\" from location " +
        juce::String((section + 1) * 100 + position + 1) + "?",
        juce::MessageBoxIconType::WarningIcon);
    dialog->addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainComponent> safeThis(this);
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, section, position](int result) {
          if (safeThis != nullptr && result == 1)
              safeThis->connectionManager.deletePatchInBank(section, position);
        }), true);
  };

  mainLayout->getPatchBrowser().onPatchCopy = [this](int section, int position) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    PatchLocationDialog::show(this, "Copy Patch", connectionManager.getPatchList(), true, activeSlot,
      [safeThis, section, position](const PatchLocationDialog::Result& r) {
        if (safeThis != nullptr && r.confirmed)
          safeThis->connectionManager.copyPatchInBank(section, position, r.section, r.position, r.slot);
      });
  };

  mainLayout->getPatchBrowser().onPatchMove = [this](int section, int position) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    PatchLocationDialog::show(this, "Move Patch", connectionManager.getPatchList(), true, activeSlot,
      [safeThis, section, position](const PatchLocationDialog::Result& r) {
        if (safeThis != nullptr && r.confirmed)
          safeThis->connectionManager.movePatchInBank(section, position, r.section, r.position, r.slot);
      });
  };

  connectionManager.setPatchDataCallback(
      [this](const std::vector<std::vector<uint8_t>> &sections, int targetSlot) {
        DBG("Patch data received: " + juce::String(sections.size()) +
            " sections — parsing...");

        PatchParser parser(moduleDescs);
        auto patch = parser.parse(sections);

        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis, p = std::move(patch), targetSlot]() mutable {
          if (!safeThis) return;
          if (targetSlot < 0 || targetSlot >= numSlots)
            return;

          // Store patch in the correct slot
          safeThis->slotSynchronizers[targetSlot].reset();

          // If replacing the active slot, clear UI refs BEFORE destroying old patch
          if (targetSlot == safeThis->activeSlot) {
            safeThis->mainLayout->getInspector().clearModule();
            if (safeThis->knobFloaterWindow)
              safeThis->knobFloaterWindow->setPatch(nullptr);
            if (safeThis->patchNotesFloaterWindow)
              safeThis->patchNotesFloaterWindow->setPatch(nullptr);
          }
          // Same, for an open SlotWindow: its inspector may hold a Module*
          // into the patch object that's about to be destroyed below.
          if (safeThis->slotWindows[targetSlot])
            safeThis->slotWindows[targetSlot]->getContent().getInspector().clearModule();

          safeThis->slotPatches[targetSlot] = std::move(p);
          if (safeThis->slotPatches[targetSlot]) {
            if (safeThis->connectionManager.isConnected()) {
              safeThis->slotSynchronizers[targetSlot] = std::make_unique<PatchSynchronizer>(
                  *safeThis->slotPatches[targetSlot], safeThis->connectionManager, targetSlot);
            }

            safeThis->slotUndoManagers[targetSlot].clearUndoHistory();
            safeThis->rebuildUndoContext(targetSlot);
            safeThis->clearSnapshots(targetSlot);

            // If this is the currently viewed slot, update the UI
            if (targetSlot == safeThis->activeSlot) {
              safeThis->mainLayout->getCanvas().setPatch(safeThis->currentPatch().get(), &safeThis->moduleDescs, &safeThis->themeData);
              safeThis->mainLayout->getHeaderBar().setPatch(safeThis->currentPatch().get());
              safeThis->mainLayout->getInspector().setPatch(safeThis->currentPatch().get());
              if (safeThis->knobFloaterWindow)
                safeThis->knobFloaterWindow->setPatch(safeThis->currentPatch().get());
              if (safeThis->patchNotesFloaterWindow)
                safeThis->patchNotesFloaterWindow->setPatch(safeThis->currentPatch().get());
              safeThis->updateDspLoadDisplay();
              safeThis->mainLayout->getStatusBar().setConnectionStatus(
                  "Connected - " + safeThis->currentPatch()->getName(), true);

              int ls = safeThis->connectionManager.getLastLoadedSection();
              int lp = safeThis->connectionManager.getLastLoadedPosition();
              if (ls >= 0 && lp >= 0)
                  safeThis->mainLayout->getPatchBrowser().setLoadedPatch(ls, lp);
            }

            // Re-point an open SlotWindow at the new patch object too — it
            // has its own canvas/header/inspector, independent of whether
            // this slot is the one currently active in the main window.
            if (safeThis->slotWindows[targetSlot]) {
              auto& swContent = safeThis->slotWindows[targetSlot]->getContent();
              swContent.getCanvas().setPatch(safeThis->slotPatches[targetSlot].get(),
                                             &safeThis->moduleDescs, &safeThis->themeData);
              swContent.getHeaderBar().setPatch(safeThis->slotPatches[targetSlot].get());
              swContent.getInspector().setPatch(safeThis->slotPatches[targetSlot].get());
              safeThis->updateSlotWindowDspLoad(targetSlot);
            }

            // Update slot bar with patch name. This patch came from the synth,
            // so the slot is in sync — clear any stale LOCAL badge (issue #21).
            safeThis->mainLayout->getSlotBar().setSlotName(targetSlot, safeThis->slotPatches[targetSlot]->getName());
            safeThis->setSlotLocal(targetSlot, false);

            const char* slotLetters[] = {"A", "B", "C", "D"};
            std::cout << "[SYNC] Patch loaded into slot " << slotLetters[targetSlot]
                      << ": " << safeThis->slotPatches[targetSlot]->getName().toStdString() << std::endl;
          }

          if (safeThis->pendingBrowserLoadSlot == targetSlot)
            safeThis->pendingBrowserLoadSlot = -1;
        });
      });

  // Patch fetch progress in the status bar — some patches take a few seconds
  // to stream from the synth and the UI should show something is happening.
  connectionManager.setPatchLoadProgressCallback([this](int done, int total) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, done, total]() {
      if (!safeThis) return;
      auto& statusBar = safeThis->mainLayout->getStatusBar();
      if (done >= total)
        statusBar.clearProgress();
      else
        statusBar.setProgress(total > 0 ? static_cast<double>(done) / total : 0.0,
                              "Loading patch " + juce::String(done) + "/" + juce::String(total));
    });
  });

  // A fetch that still misses sections after the automatic retries delivers a
  // patch without cables/parameters — editing or saving it would silently
  // corrupt the user's work, so warn loudly (issue #15).
  connectionManager.setPatchLoadIncompleteCallback([this](int slot, int received, int total) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, slot, received, total]() {
      if (!safeThis) return;
      safeThis->mainLayout->getStatusBar().clearProgress();
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::WarningIcon, "Incomplete Patch Load",
          "Slot " + juce::String::charToString(static_cast<char>('A' + slot)) + " received only "
          + juce::String(received) + " of " + juce::String(total)
          + " patch sections from the synth (it may be too busy).\n\n"
          "Cables or parameters may be missing. Reload the patch from the synth "
          "before editing or saving it.");
    });
  });

  // Wire parameter changes from canvas to synth (user turns knob in editor)
  mainLayout->getCanvas().setParameterChangeCallback(
      [this](int section, int moduleId, int parameterId, int value) {
        connectionManager.sendParameter(activeSlot, section, moduleId, parameterId, value);
        if (knobFloaterWindow && knobFloaterWindow->isVisible())
          knobFloaterWindow->refresh();
      });

  // Wire parameter drag complete for undo (fires once on mouseUp with old+new)
  mainLayout->getCanvas().setParameterDragCompleteCallback(
      [this](int section, int moduleId, int parameterId, int oldValue, int newValue) {
        if (!undoContext()) return;
        undoManager().beginNewTransaction("Parameter Change");
        undoManager().perform(new ParameterChangeAction(*undoContext(), section, moduleId, parameterId, oldValue, newValue));
      });

  // Wire module drops from browser to patch model
  mainLayout->getCanvas().setModuleDropCallback(
      [this](int typeId, int section, int gridX, int gridY, const juce::String& name) {
        if (!currentPatch() || !undoContext()) return;
        if (!undoManager().perform(new AddModuleAction(*undoContext(), section, typeId, gridX, gridY, name)))
          mainLayout->getStatusBar().setConnectionStatus(
            "Failed to add module - check synth memory/limits", true);
        updateDspLoadDisplay();
      });

  // Wire module delete from canvas context menu
  mainLayout->getCanvas().setDeleteModuleCallback(
      [this](int section, Module* module) {
        if (!currentPatch() || !undoContext() || !module) return;
        undoManager().perform(new DeleteModuleAction(*undoContext(), section, module));
        updateDspLoadDisplay();
      });

  // Wire module move undo from canvas
  mainLayout->getCanvas().setModuleMoveCallback(
      [this](int section, int moduleIndex, juce::Point<int> oldPos, juce::Point<int> newPos) {
        if (!currentPatch() || !undoContext()) return;
        undoManager().perform(new MoveModuleAction(*undoContext(), section, moduleIndex, oldPos, newPos));
      });

  // Wire module rename from canvas context menu (undoable)
  mainLayout->getCanvas().setRenameModuleCallback(
      [this](int section, Module* module, const juce::String& oldName, const juce::String& newName) {
        if (!module || !currentPatch() || !undoContext()) return;
        undoManager().beginNewTransaction("Rename Module");
        undoManager().perform(new RenameModuleAction(
            *undoContext(), section, module->getContainerIndex(), oldName, newName));
      });

  // Wire morph group assignment from parameter context menu
  mainLayout->getCanvas().setMorphAssignCallback(
      [this](int section, int moduleId, int paramId, int morphGroup) {
        if (!currentPatch() || !undoContext()) return;
        // Find previous assignment
        int oldGroup = -1, oldRange = 0;
        for (auto& ma : currentPatch()->morphAssignments)
            if (ma.section == section && ma.module == moduleId && ma.param == paramId)
            { oldGroup = ma.morph; oldRange = ma.range; break; }
        undoManager().beginNewTransaction("Morph Assign");
        undoManager().perform(new MorphAssignAction(*undoContext(), section, moduleId, paramId, morphGroup, oldGroup, oldRange));
      });

  // Wire zero morph / Ctrl+drag (MorphRangeChange) from canvas
  mainLayout->getCanvas().setMorphRangeChangeCallback(
      [this](int section, int moduleId, int paramId, int span, int direction) {
        if (!currentPatch() || !undoContext()) return;
        int newSignedRange = (direction == 0) ? span : -span;
        int oldSignedRange = 0;
        for (auto& ma : currentPatch()->morphAssignments)
            if (ma.section == section && ma.module == moduleId && ma.param == paramId)
            { oldSignedRange = ma.range; break; }
        undoManager().beginNewTransaction("Morph Range");
        undoManager().perform(new MorphRangeChangeAction(*undoContext(), section, moduleId, paramId, oldSignedRange, newSignedRange));
      });

  // Wire knob assignment from parameter context menu
  mainLayout->getCanvas().setKnobAssignCallback(
      [this](int section, int moduleId, int paramId, int knobIndex) {
        if (!currentPatch() || !undoContext()) return;
        int prevKnob = -1;
        for (int k = 0; k < 23; ++k)
        {
            auto& ka = currentPatch()->knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == section && ka.module == moduleId && ka.param == paramId)
            { prevKnob = k; break; }
        }
        if (knobIndex == prevKnob) return; // no-op
        undoManager().beginNewTransaction("Knob Assign");
        undoManager().perform(new KnobAssignAction(*undoContext(), section, moduleId, paramId, knobIndex, prevKnob));
      });

  // Wire MIDI controller assignment from parameter context menu
  mainLayout->getCanvas().setMidiCtrlAssignCallback(
      [this](int section, int moduleId, int paramId, int midiCC) {
        if (!currentPatch() || !undoContext()) return;
        int prevCtrl = -1;
        for (auto& ca : currentPatch()->ctrlAssignments)
            if (ca.section == section && ca.module == moduleId && ca.param == paramId)
            { prevCtrl = ca.control; break; }
        if (midiCC == prevCtrl) return; // no-op
        undoManager().beginNewTransaction("MIDI CC Assign");
        undoManager().perform(new MidiCtrlAssignAction(*undoContext(), section, moduleId, paramId, midiCC, prevCtrl));
      });

  // Wire morph knob assignments from header bar (same logic as canvas params)
  mainLayout->getHeaderBar().setKnobAssignCallback(
      [this](int section, int moduleId, int paramId, int knobIndex) {
        if (!currentPatch() || !undoContext()) return;
        int prevKnob = -1;
        for (int k = 0; k < 23; ++k)
        {
            auto& ka = currentPatch()->knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == section && ka.module == moduleId && ka.param == paramId)
            { prevKnob = k; break; }
        }
        if (knobIndex == prevKnob) return;
        undoManager().beginNewTransaction("Knob Assign");
        undoManager().perform(new KnobAssignAction(*undoContext(), section, moduleId, paramId, knobIndex, prevKnob));
      });
  mainLayout->getHeaderBar().setMidiCtrlAssignCallback(
      [this](int section, int moduleId, int paramId, int midiCC) {
        if (!currentPatch() || !undoContext()) return;
        int prevCtrl = -1;
        for (auto& ca : currentPatch()->ctrlAssignments)
            if (ca.section == section && ca.module == moduleId && ca.param == paramId)
            { prevCtrl = ca.control; break; }
        if (midiCC == prevCtrl) return;
        undoManager().beginNewTransaction("MIDI CC Assign");
        undoManager().perform(new MidiCtrlAssignAction(*undoContext(), section, moduleId, paramId, midiCC, prevCtrl));
      });

  // Wire morph keyboard assignment (velocity/note) from header bar
  mainLayout->getHeaderBar().setKeyboardAssignCallback(
      [this](int morphIndex, int keyboard) {
        if (!currentPatch()) return;
        currentPatch()->morphKeyboard[static_cast<size_t>(morphIndex)] = keyboard;
        if (connectionManager.isConnected()) {
          // Must address activeSlot, not the hardware-focused slot — every
          // sibling callback here was already updated to do this; this one
          // was missed (found in code review).
          MorphKeyboardAssignmentMessage msg(
              connectionManager.getPatchId(activeSlot), morphIndex, keyboard);
          auto sysex = msg.toSysEx(activeSlot);
          connectionManager.sendAckedSysEx(sysex);
        }
      });

  // Wire cable creation undo from canvas
  mainLayout->getCanvas().setCableCreatedCallback(
      [this](int section, int outModIdx, int outConnIdx, bool outIsOut,
             int inModIdx, int inConnIdx, bool inIsOut) {
        if (!currentPatch() || !undoContext()) return;
        undoManager().perform(new AddCableAction(*undoContext(), section,
            outModIdx, outConnIdx, outIsOut, inModIdx, inConnIdx, inIsOut, true));
      });

  // Wire cable deletion undo from canvas
  mainLayout->getCanvas().setCableDeletedCallback(
      [this](int section, int outModIdx, int outConnIdx, bool outIsOut,
             int inModIdx, int inConnIdx, bool inIsOut) {
        if (!currentPatch() || !undoContext()) return;
        undoManager().perform(new DeleteCableAction(*undoContext(), section,
            outModIdx, outConnIdx, outIsOut, inModIdx, inConnIdx, inIsOut, true));
      });

  // Wire morph knob changes from header bar to synth
  // Morphs use section=2 (morph section), module=1 (morph module),
  // parameter=0-3
  mainLayout->getHeaderBar().setMorphChangeCallback(
      [this](int morphIndex, int value) {
        connectionManager.sendParameter(activeSlot, 2, 1, morphIndex, value);
      });

  // Wire the front-panel Voices arrows to the synth. The G1 keeps the voice
  // count in the patch header, so — exactly like the Ctrl+P Patch Settings
  // dialog — the change only reaches the synth by re-uploading the patch.
  // Without this the arrows updated the on-screen number but sent nothing
  // (issue #25). PatchHeaderBar has already applied the new value to the
  // header before invoking this callback.
  //
  // Each voice increment re-uploads the whole patch, so pressing the arrows
  // rapidly used to fire overlapping uploads: a second uploadPatch() clobbered
  // the first's in-flight section/ACK state, the SysEx sections interleaved,
  // and the synth's slot ended up corrupt (name="Error", 0 modules) with a
  // "sc=0x7e code=6" warning (issue #28). Coalesce instead: debounce the arrow
  // presses and never start an upload while one is still in flight — the same
  // generation-guard pattern rebuildUndoContext() uses for its sync upload.
  mainLayout->getHeaderBar().setVoiceChangeCallback(
      [this, voiceUploadGen = std::make_shared<int>(0)](int voices) {
        if (!currentPatch()) return;
        mainLayout->getStatusBar().showMessage("Voices: " + juce::String(voices), 2000);
        if (!connectionManager.isConnected()) return;

        const int gen = ++(*voiceUploadGen);
        auto capturedGen = voiceUploadGen;
        juce::Component::SafePointer<MainComponent> safeThis(this);

        // Self-rescheduling: waits out both the user's rapid presses and any
        // upload still in flight, then sends exactly one upload for the final
        // voice count. `attempt` holds itself so it can re-arm the timer; the
        // cycle is released as soon as a terminal branch runs (upload sent, or
        // superseded/disconnected — no further timer scheduled).
        auto attempt = std::make_shared<std::function<void()>>();
        *attempt = [safeThis, capturedGen, gen, attempt]() {
          if (!safeThis || *capturedGen != gen) return;  // superseded by a newer press
          if (!safeThis->connectionManager.isConnected() || !safeThis->currentPatch()) return;
          if (safeThis->connectionManager.isUploadingPatch()) {
            // An upload (this slot's or another's) is mid-flight — try again shortly.
            juce::Timer::callAfterDelay(80, *attempt);
            return;
          }
          safeThis->connectionManager.uploadPatch(
              safeThis->connectionManager.getCurrentSlot(), *safeThis->currentPatch());
        };
        juce::Timer::callAfterDelay(200, *attempt);
      });

  // Wire patch name changes to send to synth
  mainLayout->getHeaderBar().setNameChangeCallback(
      [this](const juce::String& newName) {
        if (!currentPatch() || !undoContext()) return;
        juce::String oldName = currentPatch()->getName();
        if (oldName == newName) return;
        undoManager().beginNewTransaction("Rename Patch");
        undoManager().perform(new RenamePatchAction(*undoContext(), oldName, newName));
        mainLayout->getSlotBar().setSlotName(activeSlot, newName);
      });

  // Wire quick save button
  mainLayout->getHeaderBar().setQuickSaveCallback(
      [this]() {
        std::cout << "[MAIN] Quick save triggered" << std::endl;
        // Get current location from header bar
        auto& headerBar = mainLayout->getHeaderBar();
        int section = headerBar.currentSection;
        int position = headerBar.currentPosition;

        if (section < 0 || position < 0)
        {
          juce::AlertWindow::showMessageBoxAsync(
              juce::MessageBoxIconType::WarningIcon,
              "Quick Save",
              "No save location set. Load a patch from the browser first.");
          return;
        }

        // Save patch to synth at the tracked location
        int displayLocation = (section + 1) * 100 + position + 1;
        int slot = connectionManager.getCurrentSlot();

        // Show "Saving..." message
        mainLayout->getStatusBar().showMessage("Quick saving to location " + juce::String(displayLocation) + "...", 0);

        StorePatchMessage msg(slot, section, position);
        auto sysex = msg.toSysEx(slot);
        connectionManager.sendRawSysEx(sysex);

        std::cout << "[MAIN] Quick saved to location " << displayLocation << std::endl;

        // Show success message (auto-hide after 3 seconds)
        mainLayout->getStatusBar().showMessage("Quick saved to location " + juce::String(displayLocation), 3000);
      });

  // Wire initialize module callback
  mainLayout->getCanvas().setInitModuleCallback(
      [this](int section, Module* module) {
        initializeModule(section, module);
      });

  // Wire snippet save callback
  mainLayout->getCanvas().setSnippetSaveCallback(
      [this](SnipData snip) { saveSnippet(std::move(snip)); });
  mainLayout->getCanvas().setSnippetDropCallback(
      [this](const juce::File& file, int, int gridX, int gridY) {
        importSnippetFromFile(file, gridX, gridY);
      });

  // Wire cable visibility toggles to repaint the canvas
  mainLayout->getHeaderBar().setCableVisibilityCallback(
      [this]() { mainLayout->getCanvas().repaintCanvas(); });

  // Wire real-time light/meter data from synth
  connectionManager.setLightMeterCallback(
      [this](const int lights[128], const int meters[128]) {
          std::array<int,128> l, me;
          std::copy(lights,  lights  + 128, l.begin());
          std::copy(meters, meters + 128, me.begin());
          juce::Component::SafePointer<MainComponent> safeThis(this);
          juce::MessageManager::callAsync([safeThis, l, me]() mutable {
              if (!safeThis) return;
              safeThis->mainLayout->getCanvas().setLightMeterData(l.data(), me.data());
              // Live fan-out (issue #22): the synth streams lights/meters for the
              // focused slot, which the editor keeps in sync with activeSlot. If
              // that slot also has its own pop-out window open, drive its canvas
              // too so its LEDs/meters animate like the main one's.
              const int focused = safeThis->activeSlot;
              if (safeThis->slotWindows[focused] && safeThis->slotWindows[focused]->isVisible())
                  safeThis->slotWindows[focused]->getContent().getCanvas().setLightMeterData(
                      l.data(), me.data());
          });
      });

  // Wire shake cables button
  mainLayout->getHeaderBar().setShakeCablesCallback(
      [this]() { mainLayout->getCanvas().shakeCables(); });

  // Wire snapshot buttons
  mainLayout->getHeaderBar().setSnapshotClickCallback(
      [this](int index, bool isShift) { handleSnapshotClick(index, isShift); });
  mainLayout->getHeaderBar().setSnapshotInterpolateCallback(
      [this](int fromIdx, int toIdx, float secs) { interpolateSnapshots(fromIdx, toIdx, secs); });
  mainLayout->getHeaderBar().setSnapshotCopyCallback(
      [this](int fromIdx, int toIdx) { copySnapshot(fromIdx, toIdx); });
  mainLayout->getHeaderBar().setSnapshotInitCallback(
      [this](int index) { initSnapshot(index); });
  mainLayout->getHeaderBar().setMutatorButtonCallback(
      [this]() { toggleMutatorWindow(); });

  // Wire the Morph A/B fader
  mainLayout->getHeaderBar().setMorphFaderCallback(
      [this](float pos) { applyMorphPosition(pos); });
  mainLayout->getHeaderBar().setMorphSetEndpointCallback(
      [this](bool isB, int snapIndex) { setMorphEndpoint(isB, snapIndex); });
  mainLayout->getHeaderBar().setMorphAssignKnobCallback(
      [this](int knobIndex) { assignMorphKnob(knobIndex); });
  mainLayout->getHeaderBar().setMorphLearnCallback(
      [this]() { armMorphKnobLearn(); });
  mainLayout->getHeaderBar().setMorphClearKnobCallback(
      [this]() { clearMorphKnobAssignment(); });
  mainLayout->getInspector().onMorphFaderKnobRemove =
      [this]() { clearMorphKnobAssignment(); };

  // Wire undo/redo keyboard shortcuts from canvas
  mainLayout->getCanvas().setUndoCallback([this]() { undoManager().undo(); updateDspLoadDisplay(); });
  mainLayout->getCanvas().setRedoCallback([this]() { undoManager().redo(); updateDspLoadDisplay(); });
  mainLayout->getCanvas().setUndoManager(&undoManager());

  // Wire file command shortcuts (Ctrl+N/O/S/W) from canvas
  mainLayout->getCanvas().setFileCommandCallback([this](const juce::String& cmd) {
    if (cmd == "new")   newPatch();
    else if (cmd == "open")  openPatch();
    else if (cmd == "save")  savePatch();
    else if (cmd == "saveAs") savePatchAs();
    else if (cmd == "patchSettings") showPatchSettingsDialog();
    else if (cmd == "synthSettings") showSynthSettingsDialog();
    else if (cmd == "presetBrowser") togglePresetBrowser();
    else if (cmd == "randomize") randomizeParameters(false);
    else if (cmd == "randomizeGaussian") randomizeParameters(true);
    else if (cmd.startsWith("slot")) switchToSlot(cmd.getTrailingIntValue());
    else if (cmd.startsWith("floater")) {
      switch (cmd.getTrailingIntValue()) {
        case 0: toggleKnobFloater(); break;
        case 1: toggleKeyboardFloater(); break;
        case 2: togglePatchNotesFloater(); break;
        case 3: toggleMutatorWindow(); break;
      }
    }
  });

  // Wire parameter changes from synth to editor (user turns knob on hardware)
  connectionManager.setParameterChangeCallback([this](int section, int moduleId,
                                                      int parameterId,
                                                      int value) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, section, moduleId, parameterId,
                                     value]() {
      if (!safeThis) return;
      if (safeThis->currentPatch() == nullptr)
        return;

      // Morph A/B fader: Learn a physical panel knob as the drive source.
      // Panel-knob turns arrive here as (section, module, parameter, value).
      if (safeThis->morphLearnArmed) {
        safeThis->morphKnobSection = section;
        safeThis->morphKnobModule = moduleId;
        safeThis->morphKnobParam = parameterId;
        safeThis->morphKnobMin = 0;
        safeThis->morphKnobMax = 127;
        if (auto* mod = safeThis->currentPatch()->getContainer(section).getModuleByIndex(moduleId))
          if (auto* p = mod->getParameter(parameterId))
            if (auto* pd = p->getDescriptor()) {
              safeThis->morphKnobMin = pd->minValue;
              safeThis->morphKnobMax = pd->maxValue;
            }
        safeThis->morphLearnArmed = false;
        safeThis->refreshMorphUi();
        safeThis->mainLayout->getStatusBar().showMessage(
            "Morph fader assigned to panel knob (sec " + juce::String(section)
            + " mod " + juce::String(moduleId) + " par " + juce::String(parameterId) + ")", 3000);
        return;
      }
      // Learned knob drives the morph position.
      if (parameterId == safeThis->morphKnobParam && moduleId == safeThis->morphKnobModule
          && section == safeThis->morphKnobSection) {
        int span = juce::jmax(1, safeThis->morphKnobMax - safeThis->morphKnobMin);
        float t = juce::jlimit(0.0f, 1.0f,
                               static_cast<float>(value - safeThis->morphKnobMin) / span);
        safeThis->mainLayout->getHeaderBar().setMorphFaderPos(t);
        safeThis->applyMorphPosition(t);
        return;
      }

      // Morph section (section=2, module=1, parameter=0-3)
      if (section == 2 && moduleId == 1 && parameterId >= 0 &&
          parameterId < 4) {
        safeThis->currentPatch()->morphValues[static_cast<size_t>(parameterId)] = value;
        safeThis->mainLayout->getHeaderBar().repaint();
        if (safeThis->knobFloaterWindow && safeThis->knobFloaterWindow->isVisible())
          safeThis->knobFloaterWindow->refresh();
        safeThis->mirrorLiveUpdateToSlotWindow();  // issue #22 fan-out
        return;
      }

      // Skip if the user is currently dragging this exact parameter (avoid
      // fighting the user)
      if (safeThis->mainLayout->getCanvas().isDragging(section, moduleId, parameterId))
        return;

      auto &container = safeThis->currentPatch()->getContainer(section);
      auto *module = container.getModuleByIndex(moduleId);
      if (module == nullptr)
        return;

      auto *param = module->getParameter(parameterId);
      if (param == nullptr)
        return;

      // Only update + repaint if value actually changed (prevents unnecessary
      // repaints)
      if (param->getValue() != value) {
        param->setValue(value);
        safeThis->mainLayout->getCanvas().repaintCanvas();
        if (safeThis->knobFloaterWindow && safeThis->knobFloaterWindow->isVisible())
          safeThis->knobFloaterWindow->refresh();
        safeThis->mirrorLiveUpdateToSlotWindow();  // issue #22 fan-out
      }
    });
  });

  connectionManager.setSynthErrorCallback([this](int errorCode) {
    juce::String description;
    if (errorCode == 4)
      description = "checksum error";
    else if (errorCode == 5)
      description = "no slot focused";
    else
      description = "unknown";

    mainLayout->getStatusBar().showMessage(
        "ERROR: Synth error code " + juce::String(errorCode)
        + " (" + description + ") — check console for details", 8000);
  });

  // Wire toolbar buttons
  mainLayout->onMidiSettingsClicked = [this]() { showMidiSettingsDialog(); };

  // Chevron strips go through the same path as Ctrl+I / Ctrl+Shift+I so the
  // status bar reports the change either way.
  mainLayout->onPanelToggleRequested = [this](bool left) {
    if (left) toggleLeftPanel(); else toggleRightPanel();
  };
  mainLayout->onLibraryFolderClicked = [this]() { choosePresetLibraryFolder(); };
  mainLayout->onStoreToBankClicked = [this]() { storePatchToBank(); };
  mainLayout->getDiskPresetBrowser().setLibraryRoot(editorOptions.presetLibraryRoot);
  mainLayout->getDiskPresetBrowser().onPatchChosen = [this](const juce::File& file) {
    openPatchFileWithChooser(file);
  };
  mainLayout->getDiskPresetBrowser().onSnippetChosen = [this](const juce::File& file) {
    importSnippetFromFile(file);
  };
  // Wire bug report button on header bar
  mainLayout->getHeaderBar().setReportBugCallback([this]() {
    openURL("https://github.com/animatek/Animatek-NME/issues");
  });

  // Wire slot tab changes (user clicks tab)
  mainLayout->onSlotChanged = [this](int slot) {
    switchToSlot(slot);
  };

  // Right-click a slot row: pop it out into its own window
  mainLayout->onSlotWindowRequested = [this](int slot) {
    toggleSlotWindow(slot);
  };

  // Wire slot enable state (fixed LEDs on hardware; several can be on at once)
  connectionManager.setSlotsEnabledCallback([this](const std::array<bool, 4>& enabled) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, enabled]() {
      if (safeThis)
        safeThis->mainLayout->getSlotBar().setSlotsEnabled(enabled);
    });
  });

  // Clicking a slot's LED toggles its enable state on the synth (like holding
  // the slot button on the hardware). The LED updates when the synth confirms.
  mainLayout->getSlotBar().onSlotEnableToggled = [this](int slot) {
    if (connectionManager.isConnected())
      connectionManager.setSlotEnabled(slot, !connectionManager.isSlotEnabled(slot));
  };

  // Wire synth slot changes (user presses slot button on hardware)
  connectionManager.setSlotChangedCallback([this](int slot) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, slot]() {
      if (!safeThis) return;
      if (safeThis->pendingBrowserLoadSlot >= 0 && slot != safeThis->pendingBrowserLoadSlot) {
        std::cout << "[SLOT] Ignoring stale slot change " << slot
                  << " during browser load to slot " << safeThis->pendingBrowserLoadSlot << std::endl;
        return;
      }
      safeThis->mainLayout->getSlotBar().setCurrentTab(slot);
      safeThis->switchToSlot(slot, /*notifySynth=*/false);
      safeThis->updateSlotWindowFocusIndicators();
    });
  });

  setSize(1280, 800);

  // Auto-connect after UI is set up (with delay to let ALSA enumerate devices)
  {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(500, [safeThis]() { if (safeThis) safeThis->attemptAutoConnect(); });

    // Show beta warning dialog (after UI is ready)
    juce::Timer::callAfterDelay(800, [safeThis]() { if (safeThis) safeThis->showBetaWarning(); });

    // Reopen floater windows that were open last session
    juce::Timer::callAfterDelay(600, [safeThis]() { if (safeThis) safeThis->restoreFloaterWindows(); });
  }

#if NME_MCP_BRIDGE
  mcpBridgeServer = std::make_unique<McpBridgeServer>(*this);
  if (editorOptions.mcpBridgeEnabled)
    mcpBridgeServer->start();
#endif

#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(this);
#endif
}

MainComponent::~MainComponent() {
#if NME_MCP_BRIDGE
  // Stop accepting/handling MCP requests before anything else they could
  // touch (Patch/UndoManager/etc.) starts being torn down.
  mcpBridgeServer.reset();
#endif

  // Stop interpolation timer before anything else
  if (interpolationTimer)
    interpolationTimer->stopTimer();
  interpolationTimer.reset();

  // Disconnect MIDI and clear all async callbacks to prevent post-destruction
  // dispatches (fixes crash-on-close in plugin builds)
  connectionManager.disconnect();
  connectionManager.setStatusCallback(nullptr);
  connectionManager.setVoiceCountCallback(nullptr);
  connectionManager.setPatchDataCallback(nullptr);
  connectionManager.setParameterChangeCallback(nullptr);
  connectionManager.setSynthErrorCallback(nullptr);
  connectionManager.setSlotChangedCallback(nullptr);
  connectionManager.setSlotsEnabledCallback(nullptr);
  connectionManager.setUploadCompleteCallback(nullptr);
  connectionManager.setLightMeterCallback(nullptr);
  connectionManager.setPatchListCallback(nullptr);
  connectionManager.setPatchLoadProgressCallback(nullptr);
  connectionManager.setPatchLoadIncompleteCallback(nullptr);

  // Tear down UI before members are destroyed
#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
  menuBar.reset();
  saveFloaterState();
  knobFloaterWindow.reset();
  keyboardFloaterWindow.reset();
  mainLayout.reset();
}

void MainComponent::paint(juce::Graphics& g) {
  // Themed backdrop, notably behind the menu-bar strip: the menu bar draws its
  // background semi-transparently, so without this the bar washes out to a fixed
  // grey that never follows the theme.
  g.fillAll(AppTheme::palette().backgroundMain);
}

void MainComponent::resized() {
  auto area = getLocalBounds();

#if !JUCE_MAC
  menuBar->setBounds(area.removeFromTop(24));
#endif

  mainLayout->setBounds(area);
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
  if (key == juce::KeyPress(',', juce::ModifierKeys::commandModifier, 0))
  {
    showEditorOptionsDialog();
    return true;
  }
  if (key == juce::KeyPress('b', juce::ModifierKeys::commandModifier, 0))
  {
    togglePresetBrowser();
    return true;
  }
  if (key == juce::KeyPress('t', juce::ModifierKeys::commandModifier, 0))
  {
    applyUiTheme(editorOptions.uiThemeIndex + 1, true);
    mainLayout->getStatusBar().showMessage(
        "Theme: " + ThemeRegistry::get(editorOptions.uiThemeIndex).name, 2500);
    return true;
  }
  if (key == juce::KeyPress('w', juce::ModifierKeys::commandModifier, 0))
  {
    toggleWireframe();
    return true;
  }
  // Ctrl+I hides the left panel, Ctrl+Shift+I the right one (issue #38).
  // Both cases of the key code are matched: the shifted variant reports the
  // upper-case one on some platforms.
  if (key.getModifiers().isCommandDown()
      && (key.getKeyCode() == 'i' || key.getKeyCode() == 'I'))
  {
    if (key.getModifiers().isShiftDown())
      toggleRightPanel();
    else
      toggleLeftPanel();
    return true;
  }
  if (handleFloaterShortcut(key))
    return true;
  return mainLayout != nullptr
      && PatchCanvas::handleMorphOverlayKey(key, mainLayout->getCanvas());
}

juce::StringArray MainComponent::getMenuBarNames() {
  return {"File", "Edit", "View", "Device", "Help", "About"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex,
                                               const juce::String &) {
  juce::PopupMenu menu;

  if (menuIndex == 0) // File
  {
    menu.addItem(1, "New Patch\tCtrl+N");
    menu.addItem(2, "Open...\tCtrl+O");
    menu.addSeparator();
    menu.addItem(3, "Save\tCtrl+S");
    menu.addItem(4, "Save As...\tCtrl+Shift+S");
    menu.addSeparator();
    menu.addItem(5, "Import Snippet...", currentPatch() != nullptr);
    menu.addItem(6, "Preset Browser...\tCtrl+B");
    menu.addSeparator();
    menu.addItem(8, "Patch Settings...\tCtrl+P", currentPatch() != nullptr);
    menu.addItem(9, "Synth Settings...\tCtrl+G");
    menu.addItem(11, "Editor Options...\tCtrl+,");
    menu.addSeparator();
    menu.addItem(10, "Quit\tCtrl+Q");
  } else if (menuIndex == 1) // Edit
  {
    menu.addItem(20, "Undo " + undoManager().getUndoDescription() + "\tCtrl+Z",
                 undoManager().canUndo(), false);
    menu.addItem(21, "Redo " + undoManager().getRedoDescription() + "\tCtrl+Shift+Z",
                 undoManager().canRedo(), false);
    menu.addSeparator();
    bool hasPatch = (currentPatch() != nullptr);
    menu.addItem(22, "Randomize (Simple)\tCtrl+R", hasPatch);
    menu.addItem(23, "Randomize (Gaussian)\tCtrl+Shift+R", hasPatch);
  } else if (menuIndex == 2) // View
  {
    float zoom = mainLayout->getCanvas().getZoomLevel();
    menu.addItem(60, "Zoom In\tCtrl++");
    menu.addItem(61, "Zoom Out\tCtrl+-");
    menu.addItem(62, "Reset Zoom (100%)\tShift+Z");
    menu.addItem(63, "Zoom to Selection\tZ", !mainLayout->getCanvas().isDragging(0, 0, 0));  // always enabled
    menu.addSeparator();
    juce::String zoomLabel = "Zoom: " + juce::String(juce::roundToInt(zoom * 100)) + "%";
    menu.addItem(-1, zoomLabel, false);
    menu.addSeparator();
    menu.addItem(64, "Shake Cables\tS");
    menu.addSeparator();
    menu.addCustomItem(65, std::make_unique<CableOpacitySlider>());
    menu.addSeparator();
    juce::PopupMenu themeMenu;
    for (int i = 0; i < ThemeRegistry::count(); ++i)  // ids 200+ reserved for themes
      themeMenu.addItem(200 + i, ThemeRegistry::get(i).name, true,
                        i == editorOptions.uiThemeIndex);
    menu.addSubMenu("Theme\tCtrl+T", themeMenu);
    menu.addItem(66, "Wireframe Modules\tCtrl+W", true, editorOptions.wireframe);
    menu.addSeparator();
    menu.addItem(67, "Inspector Panel\tCtrl+I", true, mainLayout->isLeftPanelVisible());
    menu.addItem(68, "Patch Browser\tCtrl+Shift+I", true, mainLayout->isRightPanelVisible());
    menu.addSeparator();
    menu.addItem(80, "Knob Floater\tCtrl+5", true,
                 knobFloaterWindow != nullptr && knobFloaterWindow->isVisible());
    menu.addItem(81, "Keyboard Floater\tCtrl+6", true,
                 keyboardFloaterWindow != nullptr && keyboardFloaterWindow->isVisible());
    menu.addItem(82, "Patch Notes\tCtrl+7", true,
                 patchNotesFloaterWindow != nullptr && patchNotesFloaterWindow->isVisible());
    menu.addItem(83, "Patch Mutator\tCtrl+8", true,
                 mutatorWindow != nullptr && mutatorWindow->isVisible());
    menu.addItem(84, "SysEx Monitor\tCtrl+9", true,
                 sysexMonitorWindow != nullptr && sysexMonitorWindow->isVisible());
  }
  else if (menuIndex == 3) // Device
  {
    menu.addItem(30, "MIDI Settings...");
    menu.addSeparator();
    bool connected = connectionManager.isConnected();
    menu.addItem(31, "Request Patch from Synth", connected);
    menu.addItem(32, "Send Controller Snapshot", connected);
    menu.addItem(33, "Store to Bank...", connected);
    menu.addSeparator();
    menu.addItem(34, "Save Bank to Disk...", connected);
    menu.addItem(35, "Send Bank to Synth...", connected);
    menu.addItem(36, "Backup All Banks to Library...", connected);
  }
  else if (menuIndex == 4) // Help
  {
    menu.addItem(45, "Keyboard Shortcuts...", true);
    menu.addSeparator();
    menu.addItem(40, "Nord Modular Forum", true);
    menu.addItem(41, "Nord Modular Facebook Group", true);
    menu.addItem(42, "Nord Modular Patches Archive", true);
    menu.addSeparator();
    menu.addItem(43, "Report a Bug...", true);
    menu.addItem(44, "Show Beta Warning...", true);
  }
  else if (menuIndex == 5) // About
  {
    menu.addItem(54, "About Animatek NME...", true);
    menu.addSeparator();
    menu.addItem(53, "Animatek NME Website", true);
    menu.addItem(50, "Support the Project (Patreon)", true);
    menu.addItem(51, "Source Code (GitHub)", true);
    menu.addItem(52, "animatek.net", true);
  }

  return menu;
}

void MainComponent::openURL(const juce::String& url) {
  // Try JUCE's built-in method first
  if (juce::URL(url).launchInDefaultBrowser())
    return;

  // Fallback: use platform-specific commands
#if JUCE_LINUX
  juce::String command = "xdg-open \"" + url + "\" &";
#elif JUCE_MAC
  juce::String command = "open \"" + url + "\"";
#elif JUCE_WINDOWS
  juce::String command = "start \"\" \"" + url + "\"";
#else
  return;  // Unknown platform
#endif

  system(command.toRawUTF8());
}

void MainComponent::menuItemSelected(int menuItemID, int) {
  switch (menuItemID) {
  case 1:
    newPatch();
    break;
  case 2:
    openPatch();
    break;
  case 3:
    savePatch();
    break;
  case 4:
    savePatchAs();
    break;
  case 5:
    importSnippet();
    break;
  case 6:
    togglePresetBrowser();
    break;
  case 8:
    showPatchSettingsDialog();
    break;
  case 9:
    showSynthSettingsDialog();
    break;
  case 11:
    showEditorOptionsDialog();
    break;
  case 10:
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
    break;
  case 20:
    undoManager().undo();
    updateDspLoadDisplay();
    break;
  case 21:
    undoManager().redo();
    updateDspLoadDisplay();
    break;
  case 22:
    randomizeParameters(false);
    break;
  case 23:
    randomizeParameters(true);
    break;
  case 30:
    showMidiSettingsDialog();
    break;
  case 31:
    connectionManager.requestPatch(connectionManager.getCurrentSlot());
    break;
  case 32:
    connectionManager.sendControllerSnapshot();
    mainLayout->getStatusBar().showMessage(
        "Controller snapshot sent - the synth emits assigned CC values on its DIN MIDI OUT (not the PC port)", 5000);
    break;
  case 33:
    storePatchToBank();
    break;
  case 34:
    BankTransferDialog::show(this, BankTransferDialog::Mode::SaveToDisk,
                             bankTransfer, connectionManager);
    break;
  case 35:
    BankTransferDialog::show(this, BankTransferDialog::Mode::SendToSynth,
                             bankTransfer, connectionManager);
    break;
  case 36:
    if (editorOptions.presetLibraryRoot == juce::File()) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::InfoIcon, "Backup All Banks",
          "Set a preset library folder first (File > Editor Options).");
    } else {
      editorOptions.ensureLibraryFolders();
      BankTransferDialog::show(this, BankTransferDialog::Mode::BackupAllBanks,
                               bankTransfer, connectionManager,
                               editorOptions.getBanksFolder());
    }
    break;

  // Help menu
  case 40:  // Nord Modular Forum
    openURL("https://electro-music.com/forum/forum-43.html");
    break;
  case 41:  // Facebook Group
    openURL("https://www.facebook.com/groups/218654441592104");
    break;
  case 42:  // Patches Archive
    openURL("https://electro-music.com/nm_classic/");
    break;
  case 43:  // Report Bug
    openURL("https://github.com/animatek/Animatek-NME/issues");
    break;
  case 44:  // Show Beta Warning
    showBetaWarning(true);
    break;
  case 45:  // Keyboard Shortcuts
    showKeyboardShortcutsDialog();
    break;

  // About menu
  case 54:  // About box
    AboutDialog::show(this, [this](const juce::String& url) { openURL(url); });
    break;
  case 53:  // Animatek NME website
    openURL("https://animatek.net/animatek-nme-eng/");
    break;
  case 50:  // Patreon
    openURL("https://www.patreon.com/collection/2038913");
    break;
  case 51:  // GitHub
    openURL("https://github.com/animatek/Animatek-NME/");
    break;
  case 52:  // animatek.net
    openURL("https://animatek.net/");
    break;

  // View menu
  case 60:  // Zoom In
    mainLayout->getCanvas().setZoomLevel(mainLayout->getCanvas().getZoomLevel() + 0.1f);
    break;
  case 61:  // Zoom Out
    mainLayout->getCanvas().setZoomLevel(mainLayout->getCanvas().getZoomLevel() - 0.1f);
    break;
  case 62:  // Reset Zoom
    mainLayout->getCanvas().resetZoom();
    break;
  case 63:  // Zoom to Selection
    mainLayout->getCanvas().zoomToSelection();
    break;
  case 64:  // Shake Cables
    mainLayout->getCanvas().shakeCables();
    break;
  case 66:  // Wireframe Modules
    toggleWireframe();
    break;
  case 67:  // Inspector Panel
    toggleLeftPanel();
    break;
  case 68:  // Patch Browser
    toggleRightPanel();
    break;
  case 80:  // Knob Floater
    toggleKnobFloater();
    break;
  case 81:  // Keyboard Floater
    toggleKeyboardFloater();
    break;
  case 82:  // Patch Notes Floater
    togglePatchNotesFloater();
    break;
  case 83:  // Patch Mutator
    toggleMutatorWindow();
    break;
  case 84:  // SysEx Monitor
    toggleSysexMonitor();
    break;

  default:
    if (menuItemID >= 200 && menuItemID < 200 + ThemeRegistry::count())
      applyUiTheme(menuItemID - 200, true);
    break;
  }
}

void MainComponent::switchToSlot(int slot, bool notifySynth) {
  if (slot < 0 || slot >= numSlots)
    return;

  if (slot == activeSlot) {
    mainLayout->getSlotBar().setCurrentTab(slot);
    if (notifySynth && connectionManager.isConnected()
        && connectionManager.getCurrentSlot() != slot) {
      stopInterpolation("synth slot realignment");
      connectionManager.selectSlot(slot);
    }
    return;
  }

  // Interpolation entries reference module indices in the previously active patch.
  // Stop before activeSlot changes, otherwise the next timer tick could apply those
  // entries to the new slot and queue them for the wrong synth patch.
  stopInterpolation("slot switch");

  activeSlot = slot;
  mainLayout->getSlotBar().setCurrentTab(slot);

  // Tell synth to switch active slot (skip when the synth itself initiated the change)
  if (notifySynth && connectionManager.isConnected())
    connectionManager.selectSlot(slot);

  // Clear inspector (points into old slot's patch)
  mainLayout->getInspector().clearModule();
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(nullptr);
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(nullptr);

  if (currentPatch()) {
    mainLayout->getCanvas().setPatch(currentPatch().get(), &moduleDescs, &themeData);
    mainLayout->getHeaderBar().setPatch(currentPatch().get());
    mainLayout->getInspector().setPatch(currentPatch().get());
    if (knobFloaterWindow)
      knobFloaterWindow->setPatch(currentPatch().get());
    if (patchNotesFloaterWindow)
      patchNotesFloaterWindow->setPatch(currentPatch().get());
    mainLayout->getCanvas().setUndoManager(&undoManager());
    updateDspLoadDisplay();
    if (mutatorWindow) {
      mutatorWindow->getPanel().clearAll();  // snapshots referenced the old slot's patch
      mutatorWindow->getPanel().setVariations(variations[activeSlot]);
    }
    resetMorphAB();  // A/B captures belong to the previous slot's patch
    refreshSnapshotUi();

    const char* slotNames[] = {"A", "B", "C", "D"};
    mainLayout->getStatusBar().setConnectionStatus(
        juce::String("Slot ") + slotNames[slot] + " - " + currentPatch()->getName(),
        connectionManager.isConnected());
  } else {
    mainLayout->getCanvas().setPatch(nullptr, nullptr, nullptr);
    mainLayout->getHeaderBar().setPatch(nullptr);
    mainLayout->getInspector().setPatch(nullptr);
    updateDspLoadDisplay();

    const char* slotNames[] = {"A", "B", "C", "D"};
    mainLayout->getStatusBar().setConnectionStatus(
        juce::String("Slot ") + slotNames[slot] + " - empty",
        connectionManager.isConnected());

    // If connected, the synth echoes our SlotActivated command as a
    // notification, whose handler auto-requests the patch with clean
    // sequencing (same path as a front-panel slot change). Requesting here
    // immediately raced the slot-command ACKs: the first ACK back (for
    // SlotsSelected) was mistaken for the patch-request ACK and the fetch
    // derailed. Keep only a delayed fallback in case the echo never comes.
    if (connectionManager.isConnected()) {
      juce::Component::SafePointer<MainComponent> safeThis(this);
      juce::Timer::callAfterDelay(500, [safeThis, slot]() {
        if (!safeThis) return;
        if (safeThis->activeSlot != slot) return;
        if (safeThis->currentPatch() != nullptr) return;
        if (!safeThis->connectionManager.isConnected()) return;
        if (safeThis->connectionManager.isFetchingPatch()) return;
        std::cout << "[SLOT] No SlotActivated echo - fallback patch request for slot "
                  << slot << std::endl;
        safeThis->connectionManager.requestPatch(slot);
      });
    }
  }

  std::cout << "[SLOT] Switched to slot " << slot << std::endl;
}

bool MainComponent::replacePatchInSlot(int slot, std::unique_ptr<Patch> patch,
                                       const juce::File& sourceFile, bool activate,
                                       bool loadVariations, juce::String& error) {
  if (slot < 0 || slot >= numSlots) {
    error = "slot must be 0-3 (A-D)";
    return false;
  }
  if (!patch) {
    error = "The replacement patch is invalid";
    return false;
  }
  if (connectionManager.isUploadingPatch() || connectionManager.isFetchingPatch()) {
    error = "A patch transfer is already in progress; retry when it completes";
    return false;
  }
  if (!connectionManager.isAckedQueueIdle()) {
    error = "A structural edit is still waiting for acknowledgement; retry when it completes";
    return false;
  }

  if (slot == activeSlot)
    stopInterpolation("MCP patch replacement");

  // Every synchronizer and editor surface stores pointers into its Patch.
  // Detach them before replacing the owning unique_ptr.
  slotSynchronizers[slot].reset();
  if (slot == activeSlot) {
    mainLayout->getInspector().clearModule();
    if (knobFloaterWindow)
      knobFloaterWindow->setPatch(nullptr);
    if (patchNotesFloaterWindow)
      patchNotesFloaterWindow->setPatch(nullptr);
  }
  if (slotWindows[slot])
    slotWindows[slot]->getContent().getInspector().clearModule();

  slotPatches[slot] = std::move(patch);
  slotPatchFiles[slot] = sourceFile;
  clearSnapshots(slot);

  if (loadVariations && sourceFile.existsAsFile()) {
    if (loadVarFile(variations[slot], sourceFile.withFileExtension("var"))) {
      for (auto& [section, moduleIndex] : variations[slot].mutationExcluded)
        if (auto* module = slotPatches[slot]->getContainer(section).getModuleByIndex(moduleIndex))
          module->setExcludedFromMutation(true);
    }
  }

  slotUndoManagers[slot].clearUndoHistory();
  if (connectionManager.isConnected()) {
    // Upload messages carry their destination slot in the SysEx envelope, so
    // changing hardware focus first is unnecessary and introduces competing ACKs.
    connectionManager.uploadPatch(slot, *slotPatches[slot]);
    slotSynchronizers[slot] = std::make_unique<PatchSynchronizer>(
        *slotPatches[slot], connectionManager, slot);
  }
  rebuildUndoContext(slot);

  if (activate && slot != activeSlot)
    switchToSlot(slot, false);

  if (slot == activeSlot) {
    mainLayout->getCanvas().setPatch(slotPatches[slot].get(), &moduleDescs, &themeData);
    mainLayout->getHeaderBar().setPatch(slotPatches[slot].get());
    mainLayout->getInspector().setPatch(slotPatches[slot].get());
    if (sourceFile == juce::File())
      mainLayout->getHeaderBar().clearCurrentLocation();
    if (knobFloaterWindow)
      knobFloaterWindow->setPatch(slotPatches[slot].get());
    if (patchNotesFloaterWindow)
      patchNotesFloaterWindow->setPatch(slotPatches[slot].get());
    refreshSnapshotUi();
    updateDspLoadDisplay();
  }

  if (slotWindows[slot]) {
    auto& content = slotWindows[slot]->getContent();
    content.getCanvas().setPatch(slotPatches[slot].get(), &moduleDescs, &themeData);
    content.getHeaderBar().setPatch(slotPatches[slot].get());
    content.getInspector().setPatch(slotPatches[slot].get());
    updateSlotWindowDspLoad(slot);
  }

  mainLayout->getSlotBar().setSlotName(slot, slotPatches[slot]->getName());
  // Synced when we just uploaded it; otherwise it lives in the editor only.
  setSlotLocal(slot, !connectionManager.isConnected());
  mainLayout->getStatusBar().showMessage(
      (sourceFile.existsAsFile() ? "Loaded: " + sourceFile.getFileName()
                                 : "New patch: " + slotPatches[slot]->getName()),
      3000);
  return true;
}

bool MainComponent::createEmptyPatchInSlot(int slot, const juce::String& name,
                                           bool activate, juce::String& error) {
  auto patch = std::make_unique<Patch>();
  if (name.isNotEmpty())
    patch->setName(name);
  return replacePatchInSlot(slot, std::move(patch), {}, activate, false, error);
}

bool MainComponent::loadPatchFileIntoSlot(int slot, const juce::File& file,
                                          bool activate, juce::String& error) {
  if (!file.existsAsFile()) {
    error = "Patch file does not exist: " + file.getFullPathName();
    return false;
  }
  if (!file.hasFileExtension(".pch")) {
    error = "Patch file must have a .pch extension";
    return false;
  }

  PchFileIO io(moduleDescs);
  auto patch = io.readFile(file);
  if (!patch) {
    error = "Failed to parse patch: " + file.getFullPathName();
    return false;
  }
  return replacePatchInSlot(slot, std::move(patch), file, activate, true, error);
}

void MainComponent::prepareSlotModuleDeletion(int slot) {
  if (slot == activeSlot) {
    mainLayout->getCanvas().clearModuleSelection();
    mainLayout->getInspector().clearModule();
  }
  if (slot >= 0 && slot < numSlots && slotWindows[slot]) {
    auto& content = slotWindows[slot]->getContent();
    content.getCanvas().clearModuleSelection();
    content.getInspector().clearModule();
  }
}

void MainComponent::newPatch() {
  stopInterpolation("new patch");

  // CRITICAL: Destroy synchronizer BEFORE replacing patch
  currentSynchronizer().reset();
  mainLayout->getInspector().clearModule();
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(nullptr);
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(nullptr);

  currentPatch() = std::make_unique<Patch>();
  currentPatchFile() = juce::File();
  clearSnapshots(activeSlot);
  mainLayout->getCanvas().setPatch(currentPatch().get(), &moduleDescs, &themeData);
  mainLayout->getHeaderBar().setPatch(currentPatch().get());
  mainLayout->getInspector().setPatch(currentPatch().get());
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(currentPatch().get());
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(currentPatch().get());
  mainLayout->getHeaderBar().clearCurrentLocation();
  mainLayout->getSlotBar().setSlotName(activeSlot, currentPatch()->getName());
  mainLayout->getStatusBar().setConnectionStatus("New Patch", false);
  updateDspLoadDisplay();

  if (connectionManager.isConnected()) {
    // Upload empty patch to synth so it resets too
    connectionManager.uploadPatch(activeSlot, *currentPatch());
    currentSynchronizer() = std::make_unique<PatchSynchronizer>(*currentPatch(), connectionManager, activeSlot);
  }

  undoManager().clearUndoHistory();
  rebuildUndoContext(activeSlot);
}


void MainComponent::openPatch() {
  auto startFolder = editorOptions.getPatchesFolder();
  auto chooser = std::make_shared<juce::FileChooser>(
      "Open Patch", startFolder.exists() ? startFolder : juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result.existsAsFile())
          openPatchFileWithChooser(result);
      });
}

void MainComponent::savePatch() {
  if (currentPatch() == nullptr)
    return;

  if (currentPatchFile().existsAsFile()) {
    savePatchToFile(currentPatchFile());
  } else {
    savePatchAs();
  }
}

void MainComponent::savePatchAs() {
  if (currentPatch() == nullptr)
    return;

  auto startFolder = editorOptions.getPatchesFolder();
  auto chooser = std::make_shared<juce::FileChooser>(
      "Save Patch As", startFolder.exists() ? startFolder : juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::saveMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result != juce::File()) {
          auto file = result.hasFileExtension(".pch")
                          ? result
                          : result.withFileExtension("pch");
          if (savePatchToFile(file))
            currentPatchFile() = file;
        }
      });
}

// Ctrl+S from a SlotWindow (issue #22): save that window's own slot, not the
// active one. Saves to the slot's known file if it has one, otherwise prompts.
void MainComponent::saveSlotPatch(int slot) {
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr) return;
  if (slotPatchFiles[slot].existsAsFile()) {
    bool ok = saveSlotPatchToFile(slot, slotPatchFiles[slot]);
    mainLayout->getStatusBar().showMessage(
        ok ? "Saved: " + slotPatchFiles[slot].getFileName()
           : "ERROR:Failed to save: " + slotPatchFiles[slot].getFileName(),
        ok ? 3000 : 5000);
  } else {
    saveSlotPatchAs(slot);
  }
}

void MainComponent::saveSlotPatchAs(int slot) {
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr) return;

  auto startFolder = editorOptions.getPatchesFolder();
  auto chooser = std::make_shared<juce::FileChooser>(
      "Save Patch As", startFolder.exists() ? startFolder : juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::saveMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser, slot](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result == juce::File()) return;
        auto file = result.hasFileExtension(".pch") ? result
                                                     : result.withFileExtension("pch");
        bool ok = saveSlotPatchToFile(slot, file);
        if (ok) slotPatchFiles[slot] = file;
        mainLayout->getStatusBar().showMessage(
            ok ? "Saved: " + file.getFileName()
               : "ERROR:Failed to save: " + file.getFileName(),
            ok ? 3000 : 5000);
      });
}

void MainComponent::loadPatchFromFile(const juce::File &file, int targetSlot, bool localOnly) {
  PchFileIO io(moduleDescs);
  auto patch = io.readFile(file);

  if (patch == nullptr) {
    mainLayout->getStatusBar().showMessage("ERROR:Failed to load: " + file.getFileName(), 5000);
    return;
  }

  // Route the load to the chosen destination tab (issue #21). Done only after a
  // successful read so a bad file never yanks you to another slot.
  if (targetSlot >= 0 && targetSlot < numSlots && targetSlot != activeSlot)
    switchToSlot(targetSlot);

  std::cout << "===== LOAD PATCH: slot=" << static_cast<char>('A' + activeSlot)
            << (localOnly ? " (LOCAL)" : "")
            << " source=file \"" << file.getFileName() << "\" =====" << std::endl;

  stopInterpolation("disk patch load");

  // CRITICAL: Destroy synchronizer BEFORE replacing patch
  currentSynchronizer().reset();
  // Clear inspector before replacing patch — its currentModule points into the old patch
  mainLayout->getInspector().clearModule();
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(nullptr);
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(nullptr);

  currentPatch() = std::move(patch);
  currentPatchFile() = file;
  clearSnapshots(activeSlot);

  // Load variations sidecar if present (keeps the .pch itself 100% standard)
  if (loadVarFile(variations[activeSlot], file.withFileExtension("var"))) {
    refreshSnapshotUi();
    for (auto& [sec, modIdx] : variations[activeSlot].mutationExcluded)
      if (auto* mod = currentPatch()->getContainer(sec).getModuleByIndex(modIdx))
        mod->setExcludedFromMutation(true);
    std::cout << "[VAR] Loaded variations sidecar: "
              << file.withFileExtension("var").getFileName() << std::endl;
  }
  mainLayout->getCanvas().setPatch(currentPatch().get(), &moduleDescs, &themeData);
  mainLayout->getHeaderBar().setPatch(currentPatch().get());
  mainLayout->getInspector().setPatch(currentPatch().get());
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(currentPatch().get());
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(currentPatch().get());
  mainLayout->getSlotBar().setSlotName(activeSlot, currentPatch()->getName());
  mainLayout->getStatusBar().showMessage("Loaded: " + file.getFileName(), 3000);
  updateDspLoadDisplay();

  // Local load (issue #21): keep the patch in the editor only — no upload, no
  // synchronizer that would push edits onto a synth slot holding a different
  // patch. Mark the slot LOCAL so the divergence from the synth is visible.
  if (!localOnly && connectionManager.isConnected()) {
    // Send loaded patch to synth so it plays immediately. Must be activeSlot
    // (the tab that just received this file), not getCurrentSlot() (whatever
    // slot happens to have hardware focus) — those can differ since slot
    // switching stopped forcing hardware focus to follow every tab change;
    // uploading to the wrong one silently overwrote an unrelated slot while
    // the synchronizer below stayed correctly bound to activeSlot (found in
    // code review).
    int slot = activeSlot;
    connectionManager.selectSlot(slot);
    std::cout << "[FILE] Focusing synth slot " << slot << " before disk patch upload" << std::endl;
    mainLayout->getStatusBar().showMessage(
        "Uploading " + file.getFileName() + " to synth...", 0);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    connectionManager.setUploadCompleteCallback([safeThis, slot, fileName = file.getFileName()]() {
      if (!safeThis)
        return;

      safeThis->connectionManager.setUploadCompleteCallback(nullptr);

      const char* slotNames[] = {"A", "B", "C", "D"};
      juce::String slotName = (slot >= 0 && slot < 4) ? slotNames[slot] : juce::String(slot);
      safeThis->mainLayout->getStatusBar().showMessage(
          "Uploaded " + fileName + " to synth slot " + slotName, 3000);
    });

    juce::Timer::callAfterDelay(200, [safeThis, slot]() {
      if (!safeThis || !safeThis->connectionManager.isConnected() || !safeThis->currentPatch())
        return;

      safeThis->connectionManager.uploadPatch(slot, *safeThis->currentPatch());
      std::cout << "[FILE] Uploading loaded patch to synth slot " << slot << std::endl;
    });

    currentSynchronizer() = std::make_unique<PatchSynchronizer>(
        *currentPatch(), connectionManager, activeSlot);
    std::cout << "[SYNC] Patch synchronizer enabled after file load" << std::endl;
  }

  setSlotLocal(activeSlot, localOnly || !connectionManager.isConnected());

  undoManager().clearUndoHistory();
  rebuildUndoContext(activeSlot);
}

// Show the slot chooser (issue #21), then load into the chosen destination.
// Used by every user-initiated open (File > Open and both preset browsers).
void MainComponent::openPatchFileWithChooser(const juce::File &file) {
  std::array<juce::String, 4> names;
  for (int i = 0; i < numSlots; ++i)
    names[static_cast<size_t>(i)] =
        slotPatches[i] ? slotPatches[i]->getName() : juce::String();

  juce::Component::SafePointer<MainComponent> safeThis(this);
  SlotSelectDialog::show(
      this, "Open \"" + file.getFileNameWithoutExtension() + "\" into...",
      names, activeSlot,
      [safeThis, file](const SlotSelectDialog::Result &r) {
        if (!safeThis || !r.confirmed)
          return;
        safeThis->loadPatchFromFile(file, r.slot, r.local);
      });
}

// A slot's editor patch has diverged from (local) or rejoined (synced) the synth.
void MainComponent::setSlotLocal(int slot, bool local) {
  if (slot < 0 || slot >= numSlots)
    return;
  slotIsLocal[slot] = local;
  mainLayout->getSlotBar().setSlotLocal(slot, local);
}

void MainComponent::storePatchToBank() {
  if (!connectionManager.isConnected()) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "Not Connected",
        "Please connect to the Nord Modular first.");
    return;
  }
  if (currentPatch() == nullptr) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "No Patch",
        "Please load a patch first.");
    return;
  }
  if (!connectionManager.isPatchListLoaded()) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "Patch List Not Loaded",
        "Please wait for the patch list to finish loading.");
    return;
  }

  int slot = connectionManager.getCurrentSlot();
  juce::Component::SafePointer<MainComponent> safeThis(this);

  PatchLocationDialog::show(this, "Store Patch to Bank",
    connectionManager.getPatchList(), true, slot,
    [safeThis](const PatchLocationDialog::Result& result) {
      if (safeThis == nullptr || !result.confirmed) return;

      int location = (result.section + 1) * 100 + result.position + 1;
      safeThis->mainLayout->getStatusBar().showMessage(
          "Storing to bank location " + juce::String(location) + "...", 0);

      StorePatchMessage msg(result.slot, result.section, result.position);
      safeThis->connectionManager.sendRawSysEx(msg.toSysEx(result.slot));

      const char* slotNames[] = {"A", "B", "C", "D"};
      safeThis->mainLayout->getStatusBar().showMessage(
          "Stored slot " + juce::String(slotNames[result.slot])
          + " to bank location " + juce::String(location), 3000);
    });
}

bool MainComponent::storeSlotPatchToBank(int slot, int bankSection, int position,
                                         juce::String &error) {
  if (!connectionManager.isConnected()) {
    error = "Not connected to a Nord Modular";
    return false;
  }
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr) {
    error = "No patch loaded in slot " + juce::String(slot);
    return false;
  }
  if (!connectionManager.isPatchListLoaded()) {
    error = "Patch list not loaded yet";
    return false;
  }
  if (bankSection < 0 || bankSection > 8 || position < 0 || position > 98) {
    error = "bank must be 1-9 and position 1-99";
    return false;
  }

  // Upload the editor patch to the synth slot first so the bank stores exactly
  // this patch, then send the store once the synth ACKs the upload.
  juce::Component::SafePointer<MainComponent> safeThis(this);
  connectionManager.setUploadCompleteCallback(
      [safeThis, slot, bankSection, position]() {
        if (!safeThis) return;
        safeThis->connectionManager.setUploadCompleteCallback(nullptr);
        StorePatchMessage msg(slot, bankSection, position);
        safeThis->connectionManager.sendRawSysEx(msg.toSysEx(slot));
        int location = (bankSection + 1) * 100 + position + 1;
        safeThis->mainLayout->getStatusBar().showMessage(
            "Stored to bank location " + juce::String(location) + " (MCP)", 3000);
      });
  connectionManager.uploadPatch(slot, *slotPatches[slot]);
  return true;
}

bool MainComponent::saveSlotPatchToFile(int slot, const juce::File &file) {
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr)
    return false;

  PchFileIO io(moduleDescs);
  if (!io.writeFile(*slotPatches[slot], file))
    return false;

  auto& vars = variations[slot];
  vars.mutationExcluded.clear();
  for (int sec : {0, 1})
    for (auto& mod : slotPatches[slot]->getContainer(sec).getModules())
      if (mod && mod->isExcludedFromMutation())
        vars.mutationExcluded.emplace_back(sec, mod->getContainerIndex());
  if (vars.anyFilled() || !vars.mutationExcluded.empty())
    saveVarFile(vars, file.withFileExtension("var"));

  return true;
}

bool MainComponent::savePatchToFile(const juce::File &file) {
  bool ok = saveSlotPatchToFile(activeSlot, file);
  mainLayout->getStatusBar().showMessage(
      ok ? "Saved: " + file.getFileName()
         : "ERROR:Failed to save: " + file.getFileName(),
      ok ? 3000 : 5000);
  return ok;
}

void MainComponent::showPatchSettingsDialog() {
  if (currentPatch() == nullptr)
    return;

  PatchSettingsDialog::show(this, currentPatch()->getHeader(),
      [this](const PatchSettingsDialog::Result& r)
      {
        auto& h = currentPatch()->getHeader();
        h.voices = r.voices;
        h.velRangeMin = r.velRangeMin;
        h.velRangeMax = r.velRangeMax;
        h.keyRangeMin = r.keyRangeMin;
        h.keyRangeMax = r.keyRangeMax;
        h.pedalMode = r.pedalMode;
        h.bendRange = r.bendRange;
        h.portamento = r.portamento;
        h.portamentoTime = r.portamentoTime;
        h.octaveShift = r.octaveShift;
        h.voiceRetriggerPoly = r.voiceRetriggerPoly ? 1 : 0;
        h.voiceRetriggerCommon = r.voiceRetriggerCommon ? 1 : 0;

        mainLayout->getHeaderBar().repaint();
        mainLayout->getStatusBar().showMessage("Patch settings updated", 2000);

        // Upload full patch to synth if connected
        if (connectionManager.isConnected())
          connectionManager.uploadPatch(connectionManager.getCurrentSlot(), *currentPatch());
      });
}

void MainComponent::showSynthSettingsDialog() {
  if (connectionManager.isConnected())
  {
    pendingSynthSettingsDialogOpen = true;
    connectionManager.requestSynthSettings();
    mainLayout->getStatusBar().showMessage("Requesting synth settings...", 1200);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(2500, [safeThis]() {
      if (safeThis != nullptr && safeThis->pendingSynthSettingsDialogOpen)
      {
        safeThis->pendingSynthSettingsDialogOpen = false;
        safeThis->openSynthSettingsDialog();
      }
    });
    return;
  }

  openSynthSettingsDialog();
}

void MainComponent::openSynthSettingsDialog() {
  synthSettingsDialog = SynthSettingsDialog::show(this, cachedSynthSettings,
      [this](const SynthSettings& s)
      {
        cachedSynthSettings = s;
        mainLayout->getHeaderBar().setSynthName(juce::String(s.name));
        mainLayout->getStatusBar().showMessage("Synth settings updated", 2000);
        if (connectionManager.isConnected())
          connectionManager.sendSynthSettings(s);
      });
}

void MainComponent::showMidiSettingsDialog() {
  MidiSettingsDialog::show(
      this, lastInputId, lastOutputId, connectionManager.getStatus(),
      [this](const juce::String &inputId, const juce::String &outputId) {
        handleConnectionRequest(inputId, outputId);
      },
      [this]() { handleDisconnectionRequest(); });
}

void MainComponent::showEditorOptionsDialog() {
#if NME_MCP_BRIDGE
  McpBridgeStatusKind mcpStatus = McpBridgeStatusKind::Disabled;
  juce::String mcpStatusText = "Disabled";
  if (editorOptions.mcpBridgeEnabled && mcpBridgeServer) {
    if (mcpBridgeServer->isListening()) {
      mcpStatus = McpBridgeStatusKind::Listening;
      mcpStatusText = "Listening on 127.0.0.1:" + juce::String(mcpBridgeServer->getPort());
    } else {
      mcpStatus = McpBridgeStatusKind::Failed;
      mcpStatusText = "Failed to bind port " + juce::String(mcpBridgeServer->getPort());
    }
  }
  juce::String mcpCommand = getMcpBridgeCommand();
#else
  McpBridgeStatusKind mcpStatus = McpBridgeStatusKind::Disabled;
  juce::String mcpStatusText = "Not built with MCP bridge support";
  juce::String mcpCommand;
#endif
  EditorOptionsDialog::show(this, editorOptions, mcpStatus, mcpStatusText, mcpCommand,
                            [this](const EditorOptions& opts) {
    applyEditorOptions(opts);
  });
}

void MainComponent::applyEditorOptions(const EditorOptions& opts) {
  editorOptions = opts;
  auto libraryOk = editorOptions.ensureLibraryFolders();
  editorOptions.save(appProperties.getUserSettings());
  PatchCanvas::setCableStyle   (static_cast<int>(opts.cableStyle));
  PatchCanvas::setKnobControl  (static_cast<int>(opts.knobControl));
  PatchCanvas::setAutoUpload   (opts.autoUpload);
  PatchCanvas::setCableOpacity (opts.cableOpacity);
  applyUiTheme(editorOptions.uiThemeIndex, false);

  // Synth parameter send speed (Mutator/Random throughput)
  const auto& rates = EditorOptions::sendRates();
  const int ri = juce::jlimit(0, static_cast<int>(rates.size()) - 1, opts.sendRateIndex);
  connectionManager.setParamSendRate(rates[static_cast<size_t>(ri)].batch,
                                     rates[static_cast<size_t>(ri)].intervalMs);

  if (editorOptions.presetLibraryRoot != juce::File()) {
    if (mainLayout)
      mainLayout->getDiskPresetBrowser().setLibraryRoot(editorOptions.presetLibraryRoot);

    if (libraryOk)
      mainLayout->getStatusBar().showMessage(
          "Preset library: " + editorOptions.presetLibraryRoot.getFullPathName(), 4000);
    else
      mainLayout->getStatusBar().showMessage(
          "ERROR: Could not create Patches/Snippets folders", 5000);
  }

#if NME_MCP_BRIDGE
  setMcpBridgeEnabled(editorOptions.mcpBridgeEnabled);
#endif
}

#if NME_MCP_BRIDGE
void MainComponent::setMcpBridgeEnabled(bool enabled) {
  if (!mcpBridgeServer) return;
  if (enabled && !mcpBridgeServer->isRunning())
    mcpBridgeServer->start();
  else if (!enabled && mcpBridgeServer->isRunning())
    mcpBridgeServer->stop();
}

juce::String MainComponent::getMcpBridgeCommand() const {
  auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
  for (int i = 0; i < 6; ++i) {
    auto serverPy = dir.getChildFile("mcp-bridge/server.py");
    if (serverPy.existsAsFile()) {
      auto venvPython = dir.getChildFile("mcp-bridge/.venv/bin/python");
      juce::String pythonCmd = venvPython.existsAsFile() ? venvPython.getFullPathName()
                                                          : juce::String("python3");
      return pythonCmd + " " + serverPy.getFullPathName();
    }
    dir = dir.getParentDirectory();
  }
  return {};
}
#endif

void MainComponent::applyUiTheme(int index, bool persist) {
  const int n = ThemeRegistry::count();
  index = ((index % n) + n) % n;
  editorOptions.uiThemeIndex = index;

  const auto& theme = ThemeRegistry::get(index);
  AppTheme::setPalette(theme.app);
  auto canvasScheme = theme.makeCanvas();
  canvasScheme.wireframe = editorOptions.wireframe;
  mainLayout->setTheme(canvasScheme);
  mainLayout->applyTheme();
  if (presetBrowserWindow)
    presetBrowserWindow->applyTheme();
  if (knobFloaterWindow)
    knobFloaterWindow->applyTheme();
  if (keyboardFloaterWindow)
    keyboardFloaterWindow->applyTheme();
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->applyTheme();
  for (auto& sw : slotWindows)
    if (sw) { sw->getContent().setTheme(canvasScheme); sw->repaint(); }
  mainLayout->repaint();

  // The menu bar lives outside mainLayout, so it isn't repainted above and would
  // otherwise keep the first theme's colours. Repaint our themed backdrop behind
  // it and re-query the bar's LookAndFeel so both the strip and its text update.
  repaint();
  if (menuBar)
    menuBar->sendLookAndFeelChange();

  // Refresh the menu bar so the Theme submenu's checkmark follows the change
  // (the native macOS menu keeps the stale tick otherwise). Covers every path:
  // View menu, Ctrl+T cycle, and the Editor Options dialog.
  menuItemsChanged();

  if (persist)
    editorOptions.save(appProperties.getUserSettings());
}

void MainComponent::toggleWireframe() {
  editorOptions.wireframe = !editorOptions.wireframe;
  // Re-apply the current theme so the canvas rebuilds with the new flag; persist.
  applyUiTheme(editorOptions.uiThemeIndex, true);
  if (mainLayout)
    mainLayout->getStatusBar().showMessage(
        editorOptions.wireframe ? "Wireframe: on" : "Wireframe: off", 2000);
}

void MainComponent::toggleLeftPanel() {
  if (!mainLayout) return;
  const bool show = !mainLayout->isLeftPanelVisible();
  mainLayout->setLeftPanelVisible(show);
  // Name the way back: with the panel gone so is the slot bar, and the message
  // is the only place the shortcut is visible without opening a menu.
  mainLayout->getStatusBar().showMessage(
      show ? "Inspector panel shown" : "Inspector panel hidden (Ctrl+I to show)",
      2500);
}

void MainComponent::toggleRightPanel() {
  if (!mainLayout) return;
  const bool show = !mainLayout->isRightPanelVisible();
  mainLayout->setRightPanelVisible(show);
  mainLayout->getStatusBar().showMessage(
      show ? "Patch browser shown" : "Patch browser hidden (Ctrl+Shift+I to show)",
      2500);
}

void MainComponent::choosePresetLibraryFolder() {
  auto start = editorOptions.presetLibraryRoot == juce::File()
      ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
      : editorOptions.presetLibraryRoot;

  auto chooser = std::make_shared<juce::FileChooser>(
      "Choose Preset Library Folder", start);

  juce::Component::SafePointer<MainComponent> safeThis(this);
  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectDirectories,
      [safeThis, chooser](const juce::FileChooser& fc) {
        if (safeThis == nullptr) return;

        auto folder = fc.getResult();
        if (folder == juce::File()) return;

        auto opts = safeThis->editorOptions;
        opts.presetLibraryRoot = folder;
        safeThis->applyEditorOptions(opts);
        safeThis->mainLayout->getDiskPresetBrowser().setLibraryRoot(folder);
        if (safeThis->presetBrowserWindow)
          safeThis->presetBrowserWindow->setLibraryRoot(folder);
      });
}

void MainComponent::togglePresetBrowser() {
  mainLayout->getDiskPresetBrowser().setLibraryRoot(editorOptions.presetLibraryRoot);
  mainLayout->showDiskPresetBrowser();
}

void MainComponent::showPresetBrowser() {
  if (!presetBrowserWindow)
  {
    presetBrowserWindow = std::make_unique<PresetBrowserWindow>();

    presetBrowserWindow->onPatchChosen = [this](const juce::File& file) {
      openPatchFileWithChooser(file);
    };
    presetBrowserWindow->onSnippetChosen = [this](const juce::File& file) {
      importSnippetFromFile(file);
    };
    presetBrowserWindow->onChooseLibraryFolder = [this]() {
      choosePresetLibraryFolder();
    };
  }

  presetBrowserWindow->setLibraryRoot(editorOptions.presetLibraryRoot);

  auto* top = getTopLevelComponent();
  auto screen = top != nullptr ? top->localAreaToGlobal(top->getLocalBounds())
                               : juce::Rectangle<int>(100, 100, 1280, 800);
  presetBrowserWindow->setTopLeftPosition(
      screen.getX() + (screen.getWidth() - presetBrowserWindow->getWidth()) / 2,
      screen.getY() + (screen.getHeight() - presetBrowserWindow->getHeight()) / 2);
  presetBrowserWindow->addToDesktop(juce::ComponentPeer::windowHasDropShadow |
                                    juce::ComponentPeer::windowHasTitleBar);
  presetBrowserWindow->setVisible(true);
  presetBrowserWindow->toFront(true);
}

void MainComponent::showFloaterWindow(juce::DocumentWindow& window,
                                      const juce::String& settingsPrefix) {
  auto* settings = appProperties.getUserSettings();
  const int savedX = settings ? settings->getIntValue(settingsPrefix + "X", INT_MIN) : INT_MIN;
  const int savedY = settings ? settings->getIntValue(settingsPrefix + "Y", INT_MIN) : INT_MIN;

  if (settings && window.isResizable()) {
    const int savedW = settings->getIntValue(settingsPrefix + "W", 0);
    const int savedH = settings->getIntValue(settingsPrefix + "H", 0);
    if (savedW > 0 && savedH > 0)
      window.setSize(savedW, savedH);
  }

  if (savedX != INT_MIN && savedY != INT_MIN) {
    // Clamp to the display the floater was last on (falls back to the nearest
    // display if the monitor layout changed)
    auto userArea = juce::Desktop::getInstance().getDisplays()
                        .getDisplayForPoint({ savedX, savedY })->userArea;
    window.setTopLeftPosition(
        juce::jlimit(userArea.getX(), userArea.getRight() - window.getWidth(), savedX),
        juce::jlimit(userArea.getY(), userArea.getBottom() - window.getHeight(), savedY));
  } else {
    auto* top = getTopLevelComponent();
    auto screen = top != nullptr ? top->localAreaToGlobal(top->getLocalBounds())
                                 : juce::Rectangle<int>(100, 100, 1280, 800);
    window.setTopLeftPosition(
        screen.getX() + (screen.getWidth() - window.getWidth()) / 2,
        screen.getY() + (screen.getHeight() - window.getHeight()) / 2);
  }

  // DocumentWindow adds itself to the desktop with its own style flags;
  // passing manual flags to addToDesktop trips a TopLevelWindow assertion.
  window.setVisible(true);
  window.toFront(true);
  saveFloaterState();
}

void MainComponent::saveFloaterState() {
  auto* settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  auto save = [settings](juce::DocumentWindow* window, const juce::String& prefix) {
    if (window == nullptr) return;
    settings->setValue(prefix + "X", window->getX());
    settings->setValue(prefix + "Y", window->getY());
    settings->setValue(prefix + "Open", window->isVisible());
    if (window->isResizable()) {
      settings->setValue(prefix + "W", window->getWidth());
      settings->setValue(prefix + "H", window->getHeight());
    }
  };
  save(knobFloaterWindow.get(), "knobFloater");
  save(keyboardFloaterWindow.get(), "keyboardFloater");
  save(patchNotesFloaterWindow.get(), "patchNotesFloater");
  save(mutatorWindow.get(), "mutatorFloater");
  save(sysexMonitorWindow.get(), "sysexMonitor");
  for (int i = 0; i < numSlots; ++i)
    save(slotWindows[i].get(), "slotWindow" + juce::String::charToString(static_cast<char>('A' + i)));
  settings->saveIfNeeded();
}

void MainComponent::restoreFloaterWindows() {
  auto* settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  if (settings->getBoolValue("knobFloaterOpen", false))
    toggleKnobFloater();
  if (settings->getBoolValue("keyboardFloaterOpen", false))
    toggleKeyboardFloater();
  if (settings->getBoolValue("patchNotesFloaterOpen", false))
    togglePatchNotesFloater();
  if (settings->getBoolValue("mutatorFloaterOpen", false))
    toggleMutatorWindow();
  if (settings->getBoolValue("sysexMonitorOpen", false))
    toggleSysexMonitor();
  for (int i = 0; i < numSlots; ++i)
    if (settings->getBoolValue("slotWindow" + juce::String::charToString(static_cast<char>('A' + i)) + "Open", false))
      toggleSlotWindow(i);
}

void MainComponent::toggleKnobFloater() {
  if (knobFloaterWindow && knobFloaterWindow->isVisible()) {
    knobFloaterWindow->setVisible(false);
    saveFloaterState();
    return;
  }

  if (!knobFloaterWindow) {
    knobFloaterWindow = std::make_unique<KnobFloaterWindow>();
    knobFloaterWindow->onClosed = [this]() { saveFloaterState(); };
    knobFloaterWindow->onGlobalKey =
        [this](const juce::KeyPress& k) { return handleFloaterShortcut(k); };

    // Same path as the canvas parameter callbacks (live change + undo on drag end)
    knobFloaterWindow->onParameterChanged =
        [this](int section, int moduleId, int parameterId, int value) {
          connectionManager.sendParameter(activeSlot, section, moduleId, parameterId, value);
          mainLayout->getCanvas().repaintCanvas();
        };
    knobFloaterWindow->onParameterDragComplete =
        [this](int section, int moduleId, int parameterId, int oldValue, int newValue) {
          if (!undoContext()) return;
          undoManager().beginNewTransaction("Parameter Change");
          undoManager().perform(new ParameterChangeAction(
              *undoContext(), section, moduleId, parameterId, oldValue, newValue));
        };
    knobFloaterWindow->onMorphChanged = [this](int morphIndex, int value) {
      connectionManager.sendParameter(activeSlot, 2, 1, morphIndex, value);
      mainLayout->getHeaderBar().repaint();
    };
    knobFloaterWindow->onReassignRequested = [this](int fromKnob, int toKnob) {
      if (!currentPatch() || !undoContext()) return;
      const auto& ka = currentPatch()->knobAssignments[static_cast<size_t>(fromKnob)];
      if (!ka.assigned) return;
      undoManager().beginNewTransaction("Knob Reassign");
      undoManager().perform(new KnobAssignAction(
          *undoContext(), ka.section, ka.module, ka.param, toKnob, fromKnob));
    };
  }

  knobFloaterWindow->applyTheme();
  knobFloaterWindow->setPatch(currentPatch().get());
  showFloaterWindow(*knobFloaterWindow, "knobFloater");
}

void MainComponent::toggleKeyboardFloater() {
  if (keyboardFloaterWindow && keyboardFloaterWindow->isVisible()) {
    keyboardFloaterWindow->allNotesOff();
    keyboardFloaterWindow->setVisible(false);
    saveFloaterState();
    return;
  }

  if (!keyboardFloaterWindow) {
    keyboardFloaterWindow = std::make_unique<KeyboardFloaterWindow>();
    keyboardFloaterWindow->onClosed = [this]() { saveFloaterState(); };
    keyboardFloaterWindow->onGlobalKey =
        [this](const juce::KeyPress& k) { return handleFloaterShortcut(k); };

    keyboardFloaterWindow->onNoteOn = [this](int note, int velocity) {
      connectionManager.sendNoteOn(note, velocity);
    };
    keyboardFloaterWindow->onNoteOff = [this](int note) {
      connectionManager.sendNoteOff(note);
    };
  }

  keyboardFloaterWindow->applyTheme();
  showFloaterWindow(*keyboardFloaterWindow, "keyboardFloater");
}

void MainComponent::togglePatchNotesFloater() {
  if (patchNotesFloaterWindow && patchNotesFloaterWindow->isVisible()) {
    patchNotesFloaterWindow->setVisible(false);
    saveFloaterState();
    return;
  }

  if (!patchNotesFloaterWindow) {
    patchNotesFloaterWindow = std::make_unique<PatchNotesFloaterWindow>();
    patchNotesFloaterWindow->onClosed = [this]() { saveFloaterState(); };
    patchNotesFloaterWindow->onGlobalKey =
        [this](const juce::KeyPress& k) { return handleFloaterShortcut(k); };
  }

  patchNotesFloaterWindow->applyTheme();
  patchNotesFloaterWindow->setPatch(currentPatch().get());
  showFloaterWindow(*patchNotesFloaterWindow, "patchNotesFloater");
}

bool MainComponent::handleFloaterShortcut(const juce::KeyPress& key) {
  if (!key.getModifiers().isCommandDown() || key.getModifiers().isShiftDown())
    return false;
  int code = key.getKeyCode();
  if (code == 127) code = '8';  // X11 legacy: Ctrl+8 arrives as DEL (0x7F)
  switch (code) {
    case '1': case '2': case '3': case '4':
      switchToSlot(code - '1');
      return true;
    case '5': toggleKnobFloater(); return true;
    case '6': toggleKeyboardFloater(); return true;
    case '7': togglePatchNotesFloater(); return true;
    case '8': toggleMutatorWindow(); return true;
    case '9': toggleSysexMonitor(); return true;
    case 't':
      // Ctrl+T cycles the theme from the main window (MainComponent::
      // keyPressed) — forward it here too so it also works while a floater
      // or slot window has keyboard focus instead of only the main one.
      applyUiTheme(editorOptions.uiThemeIndex + 1, true);
      mainLayout->getStatusBar().showMessage(
          "Theme: " + ThemeRegistry::get(editorOptions.uiThemeIndex).name, 2500);
      return true;
    default: return false;
  }
}

void MainComponent::toggleMutatorWindow() {
  std::cout << "[MUT] toggleMutatorWindow, visible="
            << (mutatorWindow && mutatorWindow->isVisible()) << std::endl;
  if (mutatorWindow && mutatorWindow->isVisible()) {
    mutatorWindow->setVisible(false);
    PatchCanvas::setMutatorMode(false);
    mainLayout->getHeaderBar().setMutatorOpen(false);
    mainLayout->getCanvas().repaintCanvas();
    saveFloaterState();
    return;
  }

  if (!mutatorWindow) {
    mutatorWindow = std::make_unique<MutatorWindow>();
    mutatorWindow->onGlobalKey =
        [this](const juce::KeyPress& k) { return handleFloaterShortcut(k); };
    mutatorWindow->onClosed = [this]() {
      PatchCanvas::setMutatorMode(false);
      mainLayout->getHeaderBar().setMutatorOpen(false);
      mainLayout->getCanvas().repaintCanvas();
      saveFloaterState();
    };

    auto& panel = mutatorWindow->getPanel();

    panel.onCaptureCurrent = [this]() -> ParamSnapshot {
      if (!currentPatch()) return {};
      return Mutator::captureCurrent(*currentPatch());
    };

    panel.onGenerate = [this](MutatorPanel::GenOp op,
                              const ParamSnapshot& mother,
                              const ParamSnapshot& father,
                              const MutatorPanel::GenParams& gp) -> ParamSnapshot {
      if (!currentPatch()) return {};
      Patch& patch = *currentPatch();

      bool anySolo = false;
      for (int i = 0; i < kNumMutCategories; ++i)
        anySolo = anySolo || gp.solo[i];

      // Locked = Parameter::locked, module excluded, or filtered by Quick Locks
      Mutator::LockPredicate isLocked =
          [&patch, gp, anySolo](int section, int moduleId, int paramId) {
            auto* mod = patch.getContainer(section).getModuleByIndex(moduleId);
            if (!mod) return true;
            if (mod->isExcludedFromMutation()) return true;
            auto* param = mod->getParameter(paramId);
            if (!param || param->isLocked()) return true;

            const auto* md = mod->getDescriptor();
            const auto* pd = param->getDescriptor();
            if (!md || !pd) return true;

            // Output modules (1/2/4 Output) set overall volume and routing,
            // not timbre — never mutate them, even when Solo is active.
            if (md->category == "In/Out" && md->name.endsWithIgnoreCase("Output"))
              return true;

            if (anySolo) {
              for (int i = 0; i < kNumMutCategories; ++i)
                if (gp.solo[i] && mutCategoryMatches(static_cast<MutCategory>(i), *md, *pd))
                  return false;
              return true;  // solo active: everything else is locked
            }
            for (int i = 0; i < kNumMutCategories; ++i)
              if (gp.lock[i] && mutCategoryMatches(static_cast<MutCategory>(i), *md, *pd))
                return true;
            return false;
          };

      auto& rng = juce::Random::getSystemRandom();
      switch (op) {
        case MutatorPanel::GenOp::Mutate:
          return Mutator::mutate(mother, patch, gp.mutateProb, gp.mutateRange, rng, isLocked);
        case MutatorPanel::GenOp::Randomize:
          return Mutator::randomize(mother, patch, rng, isLocked);
        case MutatorPanel::GenOp::Interpolate:
          return Mutator::interpolate(mother, father, gp.interpT);
        case MutatorPanel::GenOp::Cross:
          return Mutator::cross(mother, father, gp.crossProb, rng, gp.independentCross);
      }
      return {};
    };

    panel.onAudition = [this](const ParamSnapshot& snap, float seconds) {
      // Debounce: rapid clicks would needlessly churn the coalescing param queue
      static juce::uint32 lastAuditionMs = 0;
      const auto now = juce::Time::getMillisecondCounter();
      if (now - lastAuditionMs < 150) return;
      lastAuditionMs = now;
      if (seconds > 0.01f)
        startInterpolationTo(snap, seconds, -1);
      else
        applySnapshot(snap, "Mutator Audition");
    };

    panel.onStoreToVariation = [this](int index, const ParamSnapshot& snap) {
      if (!currentPatch() || index < 0 || index >= PatchVariations::kNumSlots || !snap.filled)
        return;
      variations[activeSlot].slots[index] = snap;
      refreshSnapshotUi();
      mainLayout->getStatusBar().showMessage(
          "Mutator sound stored to Variation " + juce::String(index + 1), 2000);
    };
  }

  mutatorWindow->applyTheme();
  mutatorWindow->getPanel().setVariations(variations[activeSlot]);
  PatchCanvas::setMutatorMode(true);
  mainLayout->getHeaderBar().setMutatorOpen(true);
  mainLayout->getCanvas().repaintCanvas();
  showFloaterWindow(*mutatorWindow, "mutatorFloater");
}

void MainComponent::toggleSysexMonitor() {
  if (sysexMonitorWindow && sysexMonitorWindow->isVisible()) {
    sysexMonitorWindow->setVisible(false);  // stops capture+polling (zero overhead)
    saveFloaterState();
    return;
  }

  if (!sysexMonitorWindow) {
    sysexMonitorWindow = std::make_unique<SysexMonitorWindow>();
    sysexMonitorWindow->onClosed = [this]() { saveFloaterState(); };
    sysexMonitorWindow->onGlobalKey =
        [this](const juce::KeyPress& k) { return handleFloaterShortcut(k); };
  }

  sysexMonitorWindow->applyTheme();
  showFloaterWindow(*sysexMonitorWindow, "sysexMonitor");
}

void MainComponent::toggleSlotWindow(int slot) {
  if (slot < 0 || slot >= numSlots) return;

  if (slotWindows[slot] && slotWindows[slot]->isVisible()) {
    slotWindows[slot]->setVisible(false);
    saveFloaterState();
    return;
  }

  if (!slotWindows[slot]) {
    slotWindows[slot] = std::make_unique<SlotWindow>(slot);
    slotWindows[slot]->onClosed = [this]() { saveFloaterState(); };
    slotWindows[slot]->onGlobalKey =
        [this, slot](const juce::KeyPress& k) {
          // Ctrl+I toggles this window's own inspector - handled here rather
          // than in handleFloaterShortcut() since that's shared by every
          // floater and has no notion of which slot's window is asking.
          if (k.getModifiers().isCommandDown() && !k.getModifiers().isShiftDown()
              && k.getKeyCode() == 'i') {
            auto& content = slotWindows[slot]->getContent();
            content.setInspectorVisible(!content.isInspectorVisible());
            return true;
          }
          // Randomize / Save act on THIS window's slot (issue #22). The main
          // canvas gates these behind its fileCommandCallback; a SlotWindow
          // deliberately leaves that null (so Ctrl+1..9/floaters still route
          // through here), so its canvas passes these keys up to us unhandled.
          if (k.getModifiers().isCommandDown()) {
            const bool shift = k.getModifiers().isShiftDown();
            const int code = k.getKeyCode();
            if (code == 'r') {
              randomizeSlotParameters(slot, slotWindows[slot]->getContent().getCanvas(), shift);
              return true;
            }
            if (code == 's') {
              if (shift) saveSlotPatchAs(slot); else saveSlotPatch(slot);
              return true;
            }
          }
          return handleFloaterShortcut(k);
        };
    wireSlotWindowContent(*slotWindows[slot], slot);
  }

  // Match the main canvas's current color scheme. Editing (cables, modules,
  // parameters, undo/redo) is wired below via wireSlotWindowContent(); live
  // updates arriving FROM the synth for the focused slot now fan out to this
  // window too (issue #22), via mirrorLiveUpdateToSlotWindow() and the
  // light/meter callback.
  const auto& theme = ThemeRegistry::get(editorOptions.uiThemeIndex);
  auto canvasScheme = theme.makeCanvas();
  canvasScheme.wireframe = editorOptions.wireframe;
  auto& content = slotWindows[slot]->getContent();
  content.setTheme(canvasScheme);
  content.getCanvas().setPatch(slotPatches[slot].get(), &moduleDescs, &themeData);
  content.getHeaderBar().setPatch(slotPatches[slot].get());
  content.getInspector().setPatch(slotPatches[slot].get());
  updateSlotWindowDspLoad(slot);
  updateSlotWindowFocusIndicators();

  showFloaterWindow(*slotWindows[slot],
                    "slotWindow" + juce::String::charToString(static_cast<char>('A' + slot)));
}

// Slot-scoped equivalent of updateDspLoadDisplay(), which only ever touches
// the main window's header bar — a SlotWindow has its own header bar and
// must recompute load from THAT slot's patch, not currentPatch()/activeSlot.
void MainComponent::updateSlotWindowDspLoad(int slot) {
  if (!slotWindows[slot]) return;
  auto& headerBar = slotWindows[slot]->getContent().getHeaderBar();
  auto* p = slotPatches[slot].get();
  if (!p) { headerBar.setLoadValues(-1.0f, -1.0f); return; }

  double polyCycles = 0.0;
  for (auto& mod : p->getPolyVoiceArea().getModules())
    if (mod && mod->getDescriptor()) polyCycles += mod->getDescriptor()->cycles;
  double commonCycles = 0.0;
  for (auto& mod : p->getCommonArea().getModules())
    if (mod && mod->getDescriptor()) commonCycles += mod->getDescriptor()->cycles;
  double total = polyCycles + commonCycles;
  headerBar.setLoadValues(static_cast<float>(polyCycles / 100.0), static_cast<float>(total / 100.0));
}

// A live parameter/morph value arriving from the synth is applied to the
// active slot's patch (currentPatch() == slotPatches[activeSlot]), which is the
// very same Patch object an open SlotWindow for that slot displays — so the
// model is already up to date and this only needs to repaint that window's
// surfaces (issue #22 live fan-out). No-op unless the focused slot's window is
// actually open and visible.
void MainComponent::mirrorLiveUpdateToSlotWindow() {
  const int focused = activeSlot;
  if (focused < 0 || focused >= numSlots) return;
  auto* sw = slotWindows[focused].get();
  if (!sw || !sw->isVisible()) return;
  auto& content = sw->getContent();
  content.getCanvas().repaintCanvas();
  content.getHeaderBar().repaint();
}

// With N windows potentially open at once, it's not obvious at a glance
// which slot currently has the synth's front-panel focus (the one that gets
// live parameter/light/meter updates, per the multi-window plan) — mirrors
// the original Nomad editor highlighting the focused patch's title bar.
// Marks the title of whichever open SlotWindow matches
// connectionManager.getCurrentSlot() and brings it to front (per Javier:
// the original editor's equivalent left the window wherever it was, but in
// practice that meant the just-focused slot's window could stay hidden
// behind others — bringing it forward matches the hardware's own behavior
// more usefully, at the cost of possibly interrupting whatever window the
// user was in; worth a second look if that turns out to be annoying).
void MainComponent::updateSlotWindowFocusIndicators() {
  const int focused = connectionManager.getCurrentSlot();
  for (int i = 0; i < numSlots; ++i) {
    if (!slotWindows[i]) continue;
    // juce::String has no dedicated char constructor — juce::String(char)
    // silently resolves to the int overload and prints the ASCII value
    // instead of the letter (bit us before in the #15 alert; bit us again
    // here). Always use charToString for a single character.
    juce::String base = "Slot " + juce::String::charToString(static_cast<char>('A' + i));
    slotWindows[i]->setName(i == focused ? base + " - Focused" : base);
    if (i == focused && slotWindows[i]->isVisible())
      slotWindows[i]->toFront(true);
  }
}

// Editing wiring for one SlotWindow, called once when it's first created.
// Deliberately NOT shared with the ~250-line block that wires the main
// canvas (MainComponent constructor, above): the main wiring's lambdas
// close over the mutable `activeSlot` and read it at call time (correct
// there, since it changes on every tab switch), while a SlotWindow's slot
// is fixed for its whole lifetime — unifying the two would need threading a
// slot-getter through every lambda for one-time code reuse, more risk than
// it's worth against the proven main-window wiring. Every action here reads
// slotPatches[slot]/slotUndoManagers[slot]/slotUndoContexts[slot] directly
// instead of the currentPatch()/undoManager()/undoContext() convenience
// accessors, which are activeSlot-only.
//
// Scope: the core editing chain (selection, parameters, cables, modules,
// morph/knob/CC assignment, rename, undo/redo). Randomize (Ctrl+R/Ctrl+Shift+R)
// and Save (Ctrl+S/Ctrl+Shift+S) are handled per-slot in this window's
// onGlobalKey (issue #22), NOT here — they hang off the window's key routing
// rather than the canvas's fileCommandCallback (still left null so Ctrl+1..9/T
// keep routing through onGlobalKey). Deliberately still NOT wired: the rest of
// the file commands (Ctrl+N/O open/new have no unambiguous meaning for a window
// bound to one already-loaded slot); the morph-keyboard/snapshot/mutator
// buttons (tied to the single shared keyboard/mutator floaters and the top
// settings bar, which — matching the original Nomad editor — live only on the
// main window); snippet save/drop.
void MainComponent::wireSlotWindowContent(SlotWindow& window, int slot) {
  auto& content = window.getContent();
  auto& canvas = content.getCanvas();
  auto& inspector = content.getInspector();
  auto& headerBar = content.getHeaderBar();

  auto patch = [this, slot]() -> Patch* { return slotPatches[slot].get(); };
  auto ctx    = [this, slot]() -> UndoContext* { return slotUndoContexts[slot].get(); };
  auto undoMgr = [this, slot]() -> juce::UndoManager& { return slotUndoManagers[slot]; };

  auto updateLoad = [this, slot]() { updateSlotWindowDspLoad(slot); };

  // Selection -> inspector
  canvas.setModuleSelectedCallback([&inspector](Module* module, int section) {
    if (module) inspector.setModule(module, section);
    else inspector.clearModule();
  });

  // Inspector-originated edits
  inspector.onNameChanged = [patch, ctx, undoMgr](int section, Module* module,
                                const juce::String& oldName, const juce::String& newName) {
    if (!patch() || !ctx() || !module) return;
    undoMgr().beginNewTransaction("Rename Module");
    undoMgr().perform(new RenameModuleAction(
        *ctx(), section, module->getContainerIndex(), oldName, newName));
  };
  inspector.onMorphGroupChanged = [this, slot, patch, ctx, undoMgr]
      (int section, Module* module, int paramIndex, int morphGroup) {
    if (!patch() || !module || !ctx()) return;
    int moduleId = module->getContainerIndex();
    int oldGroup = -1, oldRange = 0;
    for (auto& ma : patch()->morphAssignments)
      if (ma.section == section && ma.module == moduleId && ma.param == paramIndex)
      { oldGroup = ma.morph; oldRange = ma.range; break; }
    undoMgr().beginNewTransaction("Morph Assign");
    undoMgr().perform(new MorphAssignAction(*ctx(), section, moduleId, paramIndex, morphGroup, oldGroup, oldRange));
  };
  inspector.onMorphRangeChanged = [patch, ctx, undoMgr]
      (int section, Module* module, int paramIndex, int span, int dir) {
    if (!module || !patch() || !ctx()) return;
    int moduleId = module->getContainerIndex();
    int newRange = (dir == 0) ? span : -span;
    int oldRange = 0;
    for (auto& ma : patch()->morphAssignments)
      if (ma.section == section && ma.module == moduleId && ma.param == paramIndex)
      { oldRange = ma.range; break; }
    undoMgr().beginNewTransaction("Morph Range");
    undoMgr().perform(new MorphRangeChangeAction(*ctx(), section, moduleId, paramIndex, oldRange, newRange));
  };
  inspector.onKnobRemoved = [patch, ctx, undoMgr](int section, int moduleId, int paramId, int) {
    if (!patch() || !ctx()) return;
    int prevKnob = -1;
    for (int k = 0; k < 23; ++k) {
      auto& ka = patch()->knobAssignments[static_cast<size_t>(k)];
      if (ka.assigned && ka.section == section && ka.module == moduleId && ka.param == paramId)
      { prevKnob = k; break; }
    }
    if (prevKnob < 0) return;
    undoMgr().beginNewTransaction("Knob Deassign");
    undoMgr().perform(new KnobAssignAction(*ctx(), section, moduleId, paramId, -1, prevKnob));
  };
  inspector.onMidiCtrlRemoved = [patch, ctx, undoMgr](int section, int moduleId, int paramId, int) {
    if (!patch() || !ctx()) return;
    int prevCtrl = -1;
    for (auto& ca : patch()->ctrlAssignments)
      if (ca.section == section && ca.module == moduleId && ca.param == paramId)
      { prevCtrl = ca.control; break; }
    if (prevCtrl < 0) return;
    undoMgr().beginNewTransaction("MIDI CC Deassign");
    undoMgr().perform(new MidiCtrlAssignAction(*ctx(), section, moduleId, paramId, -1, prevCtrl));
  };

  // Canvas -> synth (live parameter changes) and canvas -> undo (structural edits)
  canvas.setParameterChangeCallback([this, slot](int section, int moduleId, int parameterId, int value) {
    connectionManager.sendParameter(slot, section, moduleId, parameterId, value);
  });
  canvas.setParameterDragCompleteCallback([ctx, undoMgr]
      (int section, int moduleId, int parameterId, int oldValue, int newValue) {
    if (!ctx()) return;
    undoMgr().beginNewTransaction("Parameter Change");
    undoMgr().perform(new ParameterChangeAction(*ctx(), section, moduleId, parameterId, oldValue, newValue));
  });
  canvas.setModuleDropCallback([patch, ctx, undoMgr, updateLoad]
      (int typeId, int section, int gridX, int gridY, const juce::String& name) {
    if (!patch() || !ctx()) return;
    if (!undoMgr().perform(new AddModuleAction(*ctx(), section, typeId, gridX, gridY, name)))
      std::cout << "[SLOTWIN] Failed to add module - check synth memory/limits" << std::endl;
    updateLoad();
  });
  canvas.setDeleteModuleCallback([patch, ctx, undoMgr, updateLoad](int section, Module* module) {
    if (!patch() || !ctx() || !module) return;
    undoMgr().perform(new DeleteModuleAction(*ctx(), section, module));
    updateLoad();
  });
  canvas.setModuleMoveCallback([patch, ctx, undoMgr]
      (int section, int moduleIndex, juce::Point<int> oldPos, juce::Point<int> newPos) {
    if (!patch() || !ctx()) return;
    undoMgr().perform(new MoveModuleAction(*ctx(), section, moduleIndex, oldPos, newPos));
  });
  canvas.setRenameModuleCallback([patch, ctx, undoMgr](int section, Module* module,
                                     const juce::String& oldName, const juce::String& newName) {
    if (!patch() || !ctx() || !module) return;
    undoMgr().beginNewTransaction("Rename Module");
    undoMgr().perform(new RenameModuleAction(
        *ctx(), section, module->getContainerIndex(), oldName, newName));
  });
  canvas.setMorphAssignCallback([patch, ctx, undoMgr]
      (int section, int moduleId, int paramId, int morphGroup) {
    if (!patch() || !ctx()) return;
    int oldGroup = -1, oldRange = 0;
    for (auto& ma : patch()->morphAssignments)
      if (ma.section == section && ma.module == moduleId && ma.param == paramId)
      { oldGroup = ma.morph; oldRange = ma.range; break; }
    undoMgr().beginNewTransaction("Morph Assign");
    undoMgr().perform(new MorphAssignAction(*ctx(), section, moduleId, paramId, morphGroup, oldGroup, oldRange));
  });
  canvas.setMorphRangeChangeCallback([patch, ctx, undoMgr]
      (int section, int moduleId, int paramId, int span, int direction) {
    if (!patch() || !ctx()) return;
    int newSignedRange = (direction == 0) ? span : -span;
    int oldSignedRange = 0;
    for (auto& ma : patch()->morphAssignments)
      if (ma.section == section && ma.module == moduleId && ma.param == paramId)
      { oldSignedRange = ma.range; break; }
    undoMgr().beginNewTransaction("Morph Range");
    undoMgr().perform(new MorphRangeChangeAction(*ctx(), section, moduleId, paramId, oldSignedRange, newSignedRange));
  });
  auto wireKnobAssign = [patch, ctx, undoMgr](int section, int moduleId, int paramId, int knobIndex) {
    if (!patch() || !ctx()) return;
    int prevKnob = -1;
    for (int k = 0; k < 23; ++k) {
      auto& ka = patch()->knobAssignments[static_cast<size_t>(k)];
      if (ka.assigned && ka.section == section && ka.module == moduleId && ka.param == paramId)
      { prevKnob = k; break; }
    }
    if (knobIndex == prevKnob) return;
    undoMgr().beginNewTransaction("Knob Assign");
    undoMgr().perform(new KnobAssignAction(*ctx(), section, moduleId, paramId, knobIndex, prevKnob));
  };
  canvas.setKnobAssignCallback(wireKnobAssign);
  headerBar.setKnobAssignCallback(wireKnobAssign);
  auto wireMidiCtrlAssign = [patch, ctx, undoMgr](int section, int moduleId, int paramId, int midiCC) {
    if (!patch() || !ctx()) return;
    int prevCtrl = -1;
    for (auto& ca : patch()->ctrlAssignments)
      if (ca.section == section && ca.module == moduleId && ca.param == paramId)
      { prevCtrl = ca.control; break; }
    if (midiCC == prevCtrl) return;
    undoMgr().beginNewTransaction("MIDI CC Assign");
    undoMgr().perform(new MidiCtrlAssignAction(*ctx(), section, moduleId, paramId, midiCC, prevCtrl));
  };
  canvas.setMidiCtrlAssignCallback(wireMidiCtrlAssign);
  headerBar.setMidiCtrlAssignCallback(wireMidiCtrlAssign);

  canvas.setCableCreatedCallback([patch, ctx, undoMgr]
      (int section, int outModIdx, int outConnIdx, bool outIsOut,
       int inModIdx, int inConnIdx, bool inIsOut) {
    if (!patch() || !ctx()) return;
    undoMgr().perform(new AddCableAction(*ctx(), section,
        outModIdx, outConnIdx, outIsOut, inModIdx, inConnIdx, inIsOut, true));
  });
  canvas.setCableDeletedCallback([patch, ctx, undoMgr]
      (int section, int outModIdx, int outConnIdx, bool outIsOut,
       int inModIdx, int inConnIdx, bool inIsOut) {
    if (!patch() || !ctx()) return;
    undoMgr().perform(new DeleteCableAction(*ctx(), section,
        outModIdx, outConnIdx, outIsOut, inModIdx, inConnIdx, inIsOut, true));
  });

  // Header bar: morph knob live change, patch rename (also updates the main
  // window's SlotBar label, since there's only one of those), cable
  // visibility / shake cosmetic toggles (self-contained, no slot needed)
  headerBar.setMorphChangeCallback([this, slot](int morphIndex, int value) {
    connectionManager.sendParameter(slot, 2, 1, morphIndex, value);
  });
  headerBar.setNameChangeCallback([this, slot, patch, ctx](const juce::String& newName) {
    if (!patch() || !ctx()) return;
    juce::String oldName = patch()->getName();
    if (oldName == newName) return;
    slotUndoManagers[slot].beginNewTransaction("Rename Patch");
    slotUndoManagers[slot].perform(new RenamePatchAction(*ctx(), oldName, newName));
    mainLayout->getSlotBar().setSlotName(slot, newName);
  });
  headerBar.setVoiceChangeCallback([this, slot, patch](int voices) {
    if (!patch()) return;
    mainLayout->getStatusBar().showMessage("Voices: " + juce::String(voices), 2000);
    if (connectionManager.isConnected())
      connectionManager.uploadPatch(slot, *patch());
  });
  headerBar.setCableVisibilityCallback([&canvas]() { canvas.repaintCanvas(); });
  headerBar.setShakeCablesCallback([&canvas]() { canvas.shakeCables(); });

  // Undo/redo keyboard shortcuts (Ctrl+Z/Shift+Ctrl+Z), scoped to this slot
  canvas.setUndoCallback([this, slot, updateLoad]() { slotUndoManagers[slot].undo(); updateLoad(); });
  canvas.setRedoCallback([this, slot, updateLoad]() { slotUndoManagers[slot].redo(); updateLoad(); });
  canvas.setUndoManager(&slotUndoManagers[slot]);

  canvas.setInitModuleCallback([patch, ctx, undoMgr, updateLoad, &canvas]
      (int section, Module* module) {
    if (!module || !patch() || !ctx()) return;
    std::vector<RandomizeAction::ParamChange> changes;
    for (auto& param : module->getParameters()) {
      if (param.isLocked()) continue;
      auto* pd = param.getDescriptor();
      if (!pd || pd->paramClass != "parameter") continue;
      int oldVal = param.getValue();
      int newVal = pd->defaultValue;
      if (oldVal != newVal)
        changes.push_back({section, module->getContainerIndex(), pd->index, oldVal, newVal});
    }
    if (changes.empty()) return;
    if (!undoMgr().isPerformingUndoRedo())
      undoMgr().beginNewTransaction("Initialize Module");
    undoMgr().perform(new RandomizeAction(*ctx(), std::move(changes)));
    canvas.repaintCanvas();
    updateLoad();
  });

  updateLoad();  // Show the slot's actual DSP load as soon as the window opens
}

void MainComponent::handleConnectionRequest(const juce::String &inputId,
                                            const juce::String &outputId) {
  lastInputId = inputId;
  lastOutputId = outputId;
  connectionManager.connect(inputId, outputId);
}

void MainComponent::handleDisconnectionRequest() {
  // Release any held virtual-keyboard notes while the port is still open
  if (keyboardFloaterWindow)
    keyboardFloaterWindow->allNotesOff();
  connectionManager.disconnect();
}

void MainComponent::onConnectionStatusChanged(
    const ConnectionManager::Status &status) {
  bool connected = (status.state == ConnectionManager::State::Connected);
  mainLayout->getStatusBar().setConnectionStatus(status.message, connected);
  menuItemsChanged(); // rebuild native macOS menu bar to update enabled states

  if (connected) {
    // Save settings on successful connection
    saveMidiSettings(lastInputId, lastOutputId);

    // Patch loading is triggered by SlotActivated (sc=0x09) from synth,
    // with a fallback timer in ConnectionManager if no slot message arrives.

    // Enable synchronizer if we have a patch loaded
    if (currentPatch() && !currentSynchronizer()) {
      currentSynchronizer() = std::make_unique<PatchSynchronizer>(
          *currentPatch(), connectionManager, activeSlot);
      std::cout << "[SYNC] Patch synchronizer enabled on connection" << std::endl;
    }

    // Show synth name in header bar (will be replaced by real name from SynthSettings later)
    mainLayout->getHeaderBar().setSynthName("Nord Modular");

    connectionManager.requestPatchList();
    connectionManager.requestSynthSettings();
  } else {
    // Disable all synchronizers on disconnect
    for (int s = 0; s < numSlots; ++s)
      slotSynchronizers[s].reset();
    // Visual reset of the virtual keyboard (sends are no-ops while disconnected)
    if (keyboardFloaterWindow)
      keyboardFloaterWindow->allNotesOff();
    mainLayout->getHeaderBar().setSynthName({});
    mainLayout->getHeaderBar().setSynthDspLoad(-1, -1, -1, -1);
    std::cout << "[SYNC] All slot synchronizers disabled on disconnect" << std::endl;
  }
}

void MainComponent::attemptAutoConnect() {
  auto *settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  auto savedInputId = settings->getValue("midiInputDevice", "");
  auto savedOutputId = settings->getValue("midiOutputDevice", "");
  auto savedInputName = settings->getValue("midiInputName", "");
  auto savedOutputName = settings->getValue("midiOutputName", "");

  if (savedInputId.isEmpty() && savedInputName.isEmpty())
    return;

  auto inputs = ConnectionManager::getAvailableInputDevices();
  auto outputs = ConnectionManager::getAvailableOutputDevices();

  DBG("Available MIDI inputs (" + juce::String(inputs.size()) + "):");
  for (auto &dev : inputs)
    DBG("  id=\"" + dev.identifier + "\" name=\"" + dev.name + "\"");
  DBG("Available MIDI outputs (" + juce::String(outputs.size()) + "):");
  for (auto &dev : outputs)
    DBG("  id=\"" + dev.identifier + "\" name=\"" + dev.name + "\"");

  // Find input: try identifier first, then fall back to name match
  juce::String resolvedInputId;
  for (auto &dev : inputs) {
    if (dev.identifier == savedInputId) {
      resolvedInputId = dev.identifier;
      break;
    }
  }
  if (resolvedInputId.isEmpty() && savedInputName.isNotEmpty()) {
    for (auto &dev : inputs)
      if (dev.name == savedInputName) {
        resolvedInputId = dev.identifier;
        break;
      }
  }

  // Find output: try identifier first, then fall back to name match
  juce::String resolvedOutputId;
  for (auto &dev : outputs) {
    if (dev.identifier == savedOutputId) {
      resolvedOutputId = dev.identifier;
      break;
    }
  }
  if (resolvedOutputId.isEmpty() && savedOutputName.isNotEmpty()) {
    for (auto &dev : outputs)
      if (dev.name == savedOutputName) {
        resolvedOutputId = dev.identifier;
        break;
      }
  }

  if (resolvedInputId.isNotEmpty() && resolvedOutputId.isNotEmpty()) {
    DBG("Auto-connecting: input=" + resolvedInputId +
        " output=" + resolvedOutputId);
    lastInputId = resolvedInputId;
    lastOutputId = resolvedOutputId;
    connectionManager.connect(resolvedInputId, resolvedOutputId);
  } else {
    // ALSA may not have enumerated devices yet — retry a few times
    if (autoConnectRetries > 0 && (inputs.isEmpty() || outputs.isEmpty())) {
      autoConnectRetries--;
      DBG("No MIDI devices found yet, retrying in 500ms (" +
          juce::String(autoConnectRetries) + " left)");
      juce::Component::SafePointer<MainComponent> safeThis(this);
      juce::Timer::callAfterDelay(500, [safeThis]() { if (safeThis) safeThis->attemptAutoConnect(); });
    } else {
      DBG("Saved MIDI ports not found (id=" + savedInputId + "/" +
          savedOutputId + " name=" + savedInputName + "/" + savedOutputName +
          ")");
    }
  }
}

void MainComponent::saveMidiSettings(const juce::String &inputId,
                                     const juce::String &outputId) {
  auto *settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  settings->setValue("midiInputDevice", inputId);
  settings->setValue("midiOutputDevice", outputId);

  // Also save device names for robust matching (ALSA identifiers can change
  // between reboots)
  for (auto &dev : ConnectionManager::getAvailableInputDevices())
    if (dev.identifier == inputId) {
      settings->setValue("midiInputName", dev.name);
      break;
    }
  for (auto &dev : ConnectionManager::getAvailableOutputDevices())
    if (dev.identifier == outputId) {
      settings->setValue("midiOutputName", dev.name);
      break;
    }

  settings->saveIfNeeded();
  DBG("Saved MIDI settings: input=" + inputId + " output=" + outputId);
}

// Self-owning beta warning popup using the same style as ModuleHelpPopup
class BetaWarningPopup : public juce::Component
{
public:
    BetaWarningPopup(juce::Component* relativeTo, juce::ApplicationProperties& props)
        : appProperties(props)
    {
        setOpaque(true);

        titleLabel.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
        titleLabel.setColour(juce::Label::textColourId, AppTheme::palette().accentActive);
        titleLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        titleLabel.setText("Animatek NME - Beta", juce::dontSendNotification);
        addAndMakeVisible(titleLabel);

        closeButton.setButtonText("x");
        closeButton.setColour(juce::TextButton::buttonColourId,   AppTheme::palette().backgroundPanel);
        closeButton.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().inputBackground);
        closeButton.setColour(juce::TextButton::textColourOffId,  AppTheme::palette().accentActive);
        closeButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        closeButton.onClick = [this]() { removeFromDesktop(); delete this; };
        addAndMakeVisible(closeButton);

        bodyText.setFont(juce::Font(juce::FontOptions(13.0f)));
        bodyText.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));
        bodyText.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        bodyText.setText(
            "Welcome to Animatek NME (Nord Modular Editor G1) Beta!\n\n"
            "This software is under active development and may contain bugs "
            "that could corrupt patches on your Nord Modular.\n\n"
            "PLEASE:\n"
            "  - Use experimental patches only\n"
            "  - Back up any important patches before using this editor\n"
            "  - Do NOT use this with patches you rely on for live performance\n\n"
            "Found a bug? Click 'Report a bug' on the toolbar\n"
            "or visit: github.com/animatek/Animatek-NME/issues\n\n"
            "Nord Modular is a trademark of Clavia DMI AB.\n"
            "This project is not affiliated with or endorsed by Clavia.",
            juce::dontSendNotification);
        bodyText.setJustificationType(juce::Justification::topLeft);
        bodyText.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(bodyText);

        suppressToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaaaacc));
        addAndMakeVisible(suppressToggle);

        okButton.setButtonText("I understand, let me in!");
        okButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().buttonActive);
        okButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        okButton.onClick = [this]() {
            if (suppressToggle.getToggleState()) {
                auto* s = appProperties.getUserSettings();
                if (s) { s->setValue("hideBetaWarning", true); s->saveIfNeeded(); }
            }
            removeFromDesktop();
            delete this;
        };
        addAndMakeVisible(okButton);

        setSize(440, 340);

        if (relativeTo) {
            auto* top = relativeTo->getTopLevelComponent();
            auto screen = top->localAreaToGlobal(top->getLocalBounds());
            setTopLeftPosition(screen.getX() + (screen.getWidth() - 440) / 2,
                               screen.getY() + (screen.getHeight() - 340) / 2);
        }

        addToDesktop(juce::ComponentPeer::windowHasDropShadow);
        setVisible(true);
        toFront(true);
        grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(AppTheme::palette().backgroundPanel);
        g.setColour(AppTheme::palette().buttonActive);
        g.fillRect(0, 31, getWidth(), 1);
    }

    void resized() override {
        titleLabel.setBounds(8, 0, getWidth() - 40, 32);
        closeButton.setBounds(getWidth() - 32, 2, 28, 28);
        bodyText.setBounds(12, 38, getWidth() - 24, 210);
        suppressToggle.setBounds(12, getHeight() - 68, getWidth() - 24, 24);
        okButton.setBounds((getWidth() - 200) / 2, getHeight() - 38, 200, 30);
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey) {
            okButton.triggerClick();
            return true;
        }
        return false;
    }

    void mouseDown(const juce::MouseEvent& e) override { dragger.startDraggingComponent(this, e); }
    void mouseDrag(const juce::MouseEvent& e) override { dragger.dragComponent(this, e, nullptr); }

private:
    juce::ApplicationProperties& appProperties;
    juce::Label titleLabel, bodyText;
    juce::TextButton closeButton, okButton;
    juce::ToggleButton suppressToggle { "Don't show this warning at startup" };
    juce::ComponentDragger dragger;
};

void MainComponent::showKeyboardShortcutsDialog() {
  // Keep in sync with manual/07-shortcuts.md
  static const char* shortcutsText =
      "FILE\n"
      "  Ctrl+N              New patch\n"
      "  Ctrl+O              Open patch\n"
      "  Ctrl+S              Save\n"
      "  Ctrl+Shift+S        Save as\n"
      "  Ctrl+B              Preset browser\n"
      "  Ctrl+P              Patch settings\n"
      "  Ctrl+G              Synth settings\n"
      "  Ctrl+,              Editor options\n"
      "  Ctrl+Q              Quit\n"
      "\n"
      "EDIT\n"
      "  Ctrl+Z              Undo\n"
      "  Ctrl+Shift+Z, Ctrl+Y  Redo\n"
      "  Ctrl+A              Select all modules (section)\n"
      "  Ctrl+X / C / V      Cut / copy / paste modules\n"
      "  Ctrl+D              Duplicate with cables\n"
      "  Delete, Backspace   Delete selection\n"
      "  Escape              Clear selection\n"
      "  Arrow keys          Nudge selected modules\n"
      "  Ctrl+R              Randomize parameters\n"
      "  Ctrl+Shift+R        Randomize (gaussian)\n"
      "\n"
      "CANVAS\n"
      "  Enter, Double-click Quick Add module at mouse\n"
      "  F1                  Module help (hovered/selected)\n"
      "  F5                  Morph values overlay\n"
      "  F7                  Morph groups overlay\n"
      "  Z                   Zoom to selection / reset\n"
      "  Shift+Z             Reset zoom to 100%\n"
      "  Ctrl++ / Ctrl+-     Zoom in / out\n"
      "  Ctrl+T              Cycle color theme\n"
      "  Ctrl+W              Toggle wireframe modules\n"
      "  Ctrl+I              Toggle inspector panel (left)\n"
      "  Ctrl+Shift+I        Toggle patch browser (right)\n"
      "  S                   Shake cables\n"
      "  Middle-drag         Pan canvas\n"
      "\n"
      "SLOTS\n"
      "  Ctrl+1..4           Switch to slot A..D\n"
      "  Right-click slot    Open that slot in its own window\n"
      "  Ctrl+click slot     Enable/disable without selecting\n"
      "\n"
      "FLOATERS\n"
      "  Ctrl+5              Knob Floater\n"
      "  Ctrl+6              Keyboard Floater\n"
      "  Ctrl+7              Patch Notes\n"
      "  Ctrl+8              Patch Mutator\n"
      "  Ctrl+9              SysEx Monitor\n"
      "\n"
      "SLOT WINDOW (window focused)\n"
      "  Ctrl+I              Toggle inspector panel\n"
      "  Ctrl+R / Ctrl+Shift+R  Randomize (uniform / gaussian)\n"
      "  Ctrl+S / Ctrl+Shift+S  Save / Save as\n"
      "\n"
      "PATCH MUTATOR (window focused)\n"
      "  1-8                 Focus Mother / Children / Father\n"
      "  O / T               Copy focused sound to Mother / Father\n"
      "  E / U               Mutate from focused / from Mother\n"
      "  N                   Randomize\n"
      "  I / X               Interpolate / Cross (Mother+Father)\n"
      "  S                   Save focused to Temporary Storage\n"
      "  Shift+drag          Interpolate two sounds\n"
      "  Ctrl+drag           Cross two sounds\n";

  auto* editor = new juce::TextEditor();
  editor->setMultiLine(true, false);
  editor->setReadOnly(true);
  editor->setScrollbarsShown(true);
  editor->setCaretVisible(false);
  editor->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f,
                             juce::Font::plain));
  editor->setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().backgroundMain);
  editor->setColour(juce::TextEditor::textColourId, AppTheme::palette().textPrimary);
  editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
  editor->setText(juce::String::fromUTF8(shortcutsText));
  editor->setSize(440, 560);

  juce::DialogWindow::LaunchOptions opts;
  opts.content.setOwned(editor);
  opts.dialogTitle = "Keyboard Shortcuts";
  opts.dialogBackgroundColour = AppTheme::palette().backgroundMain;
  opts.escapeKeyTriggersCloseButton = true;
  opts.useNativeTitleBar = false;
  opts.resizable = false;
  opts.launchAsync();
}

void MainComponent::showBetaWarning(bool forceShow)
{
    auto* settings = appProperties.getUserSettings();
    if (!forceShow && settings && settings->getBoolValue("hideBetaWarning", false))
        return;

    new BetaWarningPopup(this, appProperties);
}

void MainComponent::rebuildUndoContext(int slot)
{
    if (!slotPatches[slot]) { slotUndoContexts[slot].reset(); return; }
    slotUndoContexts[slot] = std::make_unique<UndoContext>(UndoContext{
        *slotPatches[slot], connectionManager, slotSynchronizers[slot],
        moduleDescs,
        // This UndoContext (and its repaint callback) is shared by BOTH the
        // main window (when this slot happens to be the active tab) and this
        // slot's own pop-out SlotWindow if one is open — an edit made in
        // either place must repaint whichever surface(s) are actually
        // displaying it, not unconditionally the main window. Without the
        // `slot == activeSlot` guard, editing a slot only open in its own
        // window repainted the main window's (unrelated) canvas instead,
        // and the SlotWindow's own canvas was never told to redraw — the
        // model updated correctly, but nothing visibly changed until some
        // unrelated event (e.g. clicking the window) forced a repaint.
        // The load figures are recomputed here rather than at each call site:
        // repainting the header bar only redraws whatever load it was last told,
        // so a path that adds or deletes modules without its own explicit
        // update — the MCP bridge, the Mutator — left the meter reading the
        // previous patch's cost (issue #31).
        [this, slot]() {
            if (slot == activeSlot) {
                mainLayout->getCanvas().repaintCanvas();
                mainLayout->getInspector().refreshMorphList();
                updateDspLoadDisplay();
                mainLayout->getHeaderBar().repaint();
                if (knobFloaterWindow)
                    knobFloaterWindow->refresh();
            }
            if (slotWindows[slot] && slotWindows[slot]->isVisible()) {
                auto& content = slotWindows[slot]->getContent();
                content.getCanvas().repaintCanvas();
                content.getInspector().refreshMorphList();
                updateSlotWindowDspLoad(slot);
                content.getHeaderBar().repaint();
            }
        },
        [this, slot, syncGen = std::make_shared<int>(0)]() {
            if (!connectionManager.isConnected() || !slotPatches[slot]) return;
            int gen = ++(*syncGen);
            auto capturedGen = syncGen;
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(80, [safeThis, capturedGen, gen, slot]() {
                if (!safeThis || *capturedGen != gen) return;
                if (!safeThis->connectionManager.isConnected() || !safeThis->slotPatches[slot]) return;
                if (safeThis->slotSynchronizers[slot]) safeThis->slotSynchronizers[slot]->setSuppressed(true);
                safeThis->connectionManager.setUploadCompleteCallback([safeThis, slot]() {
                    if (!safeThis) return;
                    safeThis->connectionManager.setUploadCompleteCallback(nullptr);
                    if (safeThis->slotSynchronizers[slot])
                        safeThis->slotSynchronizers[slot]->setSuppressed(false);
                    safeThis->mainLayout->getStatusBar().showMessage("Patch synced to synth", 2000);
                });
                safeThis->connectionManager.uploadPatch(slot, *safeThis->slotPatches[slot]);
            });
        },
        [this, slot](int section, int moduleId, int paramId, int value) {
            variations[slot].updateValue(section, moduleId, paramId, value);
        },
        slot
    });
}

// --- Parameter Snapshots ---

void MainComponent::clearSnapshots(int slot) {
    if (slot < 0 || slot >= numSlots) return;

    // Stop interpolation if running on this slot
    if (interpolation.active && slot == activeSlot)
        stopInterpolation("patch snapshot reset");

    variations[slot].clear();
    if (slot == activeSlot) {
        resetMorphAB();  // A/B captures referenced the previous patch's modules
        refreshSnapshotUi();
    }
    // Mutator snapshots reference module indices of the previous patch
    if (mutatorWindow)
        mutatorWindow->getPanel().clearAll();
}

void MainComponent::refreshSnapshotUi() {
    auto& vars = variations[activeSlot];
    for (int i = 0; i < PatchVariations::kNumSlots; ++i)
        mainLayout->getHeaderBar().setSnapshotFilled(i, vars.slots[i].filled);
    mainLayout->getHeaderBar().setActiveSnapshot(vars.activeIndex);
    if (mutatorWindow)
        mutatorWindow->getPanel().setVariations(vars);
}

void MainComponent::handleSnapshotClick(int index, bool isShiftClick) {
    if (!currentPatch()) return;
    if (isShiftClick)
        saveSnapshot(index);
    else if (variations[activeSlot].slots[index].filled)
        recallSnapshot(index);
    else
        saveSnapshot(index);  // click on empty = save
}

void MainComponent::saveSnapshot(int index) {
    if (!currentPatch() || index < 0 || index >= PatchVariations::kNumSlots) return;

    variations[activeSlot].captureFrom(*currentPatch(), index);
    refreshSnapshotUi();
    mainLayout->getStatusBar().showMessage(
        "Variation " + juce::String(index + 1) + " saved (" +
        juce::String(static_cast<int>(variations[activeSlot].slots[index].entries.size())) +
        " params)", 2000);
}

void MainComponent::applySnapshot(const ParamSnapshot& snap, const juce::String& undoName) {
    if (!currentPatch() || !undoContext() || !snap.filled) return;

    // Build changes list (skips locked params and deleted modules)
    std::vector<RandomizeAction::ParamChange> changes;
    for (auto& e : snap.entries) {
        auto& container = currentPatch()->getContainer(e.section);
        auto* mod = container.getModuleByIndex(e.moduleId);
        if (!mod) continue;
        auto* param = mod->getParameter(e.paramId);
        if (!param || param->isLocked()) continue;
        int oldVal = param->getValue();
        if (oldVal != e.value)
            changes.push_back({e.section, e.moduleId, e.paramId, oldVal, e.value});
    }

    if (!changes.empty()) {
        undoManager().beginNewTransaction(undoName);
        undoManager().perform(new RandomizeAction(*undoContext(), std::move(changes)));
        mainLayout->getCanvas().repaintCanvas();
    }

    // Morph knob values (protocol: section=2, module=1, param=morphIndex)
    if (currentPatch()->morphValues != snap.morphValues) {
        currentPatch()->morphValues = snap.morphValues;
        if (connectionManager.isConnected())
            for (int m = 0; m < 4; ++m)
                connectionManager.queueParameter(activeSlot, 2, 1, m, snap.morphValues[static_cast<size_t>(m)]);
        mainLayout->getHeaderBar().repaint();
    }
}

// ---- Morph A/B fader ------------------------------------------------------

void MainComponent::setMorphEndpoint(bool isB, int snapIndex) {
    if (!currentPatch()) return;

    juce::String sourceName;
    if (snapIndex < 0) {
        (isB ? morphB : morphA) = Mutator::captureCurrent(*currentPatch());
        sourceName = "current sound";
    } else {
        if (snapIndex >= PatchVariations::kNumSlots) return;
        auto& s = variations[activeSlot].slots[snapIndex];
        if (!s.filled) return;
        (isB ? morphB : morphA) = s;
        sourceName = "Snapshot " + juce::String(snapIndex + 1);
    }
    rebuildMorphPairs();
    refreshMorphUi();
    mainLayout->getStatusBar().showMessage(
        juce::String("Morph ") + (isB ? "B" : "A") + " set from " + sourceName, 2000);
}

void MainComponent::assignMorphKnob(int knobIndex) {
    if (!currentPatch() || !undoContext()) return;

    // Carrier = a spare morph group (prefer the highest index so we don't steal
    // a low group the user is more likely to use). The synth reports morph-knob
    // positions to the editor, so an empty group is a free, inert control source.
    int group = -1;
    for (int g = 3; g >= 0; --g) {
        bool used = false;
        for (auto& ma : currentPatch()->morphAssignments)
            if (ma.morph == g) { used = true; break; }
        if (!used) { group = g; break; }
    }
    if (group < 0) {
        mainLayout->getStatusBar().showMessage(
            "No free morph group to use as carrier — all 4 are in use", 4000);
        return;
    }

    // Assign the physical knob to that morph group on the synth (native protocol).
    int prevKnob = -1;
    for (int k = 0; k < 23; ++k) {
        auto& ka = currentPatch()->knobAssignments[static_cast<size_t>(k)];
        if (ka.assigned && ka.section == 2 && ka.module == 1 && ka.param == group)
        { prevKnob = k; break; }
    }
    undoManager().beginNewTransaction("Assign Morph Fader Knob");
    undoManager().perform(new KnobAssignAction(*undoContext(), 2, 1, group, knobIndex, prevKnob));

    // Register that morph group as the fader's drive source.
    morphKnobSection = 2;
    morphKnobModule = 1;
    morphKnobParam = group;
    morphKnobIndex = knobIndex;
    morphKnobMin = 0;
    morphKnobMax = 127;
    refreshMorphUi();
    mainLayout->getStatusBar().showMessage(
        "Morph fader assigned to Knob " + juce::String(knobIndex + 1)
        + " (carrier = morph group " + juce::String(group + 1) + ")", 5000);
}

void MainComponent::rebuildMorphPairs() {
    morphPairs.clear();
    if (!morphA.filled || !morphB.filled) return;
    for (auto& be : morphB.entries) {
        int aVal = be.value;
        for (auto& ae : morphA.entries)
            if (ae.section == be.section && ae.moduleId == be.moduleId
                && ae.paramId == be.paramId) { aVal = ae.value; break; }
        if (aVal != be.value)  // only params that actually differ between A and B
            morphPairs.push_back({be.section, be.moduleId, be.paramId, aVal, be.value});
    }
}

void MainComponent::applyMorphPosition(float t) {
    if (!currentPatch() || (!morphA.filled || !morphB.filled)) return;
    t = juce::jlimit(0.0f, 1.0f, t);

    // Live scrub: lerp each differing param and push to model + synth. The
    // connection's coalescing queue collapses rapid ticks. No undo — this is a
    // performance gesture, like a morph knob.
    for (auto& p : morphPairs) {
        int v = juce::roundToInt(p.aVal + (p.bVal - p.aVal) * t);
        auto& container = currentPatch()->getContainer(p.section);
        auto* mod = container.getModuleByIndex(p.moduleId);
        if (!mod) continue;
        auto* param = mod->getParameter(p.paramId);
        if (!param || param->isLocked()) continue;
        if (param->getValue() == v) continue;
        param->setValue(v);
        connectionManager.queueParameter(activeSlot, p.section, p.moduleId, p.paramId, v);
    }

    // Morph knob values (protocol: section=2, module=1, param=morphIndex)
    for (int m = 0; m < 4; ++m) {
        // Skip the group used as the fader's knob carrier — the physical knob
        // owns that value; overwriting it here would fight the knob.
        if (morphKnobSection == 2 && morphKnobModule == 1 && morphKnobParam == m)
            continue;
        int av = morphA.morphValues[static_cast<size_t>(m)];
        int bv = morphB.morphValues[static_cast<size_t>(m)];
        int v = juce::roundToInt(av + (bv - av) * t);
        if (currentPatch()->morphValues[static_cast<size_t>(m)] != v) {
            currentPatch()->morphValues[static_cast<size_t>(m)] = v;
            if (connectionManager.isConnected())
                connectionManager.queueParameter(activeSlot, 2, 1, m, v);
        }
    }

    mainLayout->getCanvas().repaintCanvas();
    mainLayout->getHeaderBar().repaint();
}

void MainComponent::armMorphKnobLearn() {
    morphLearnArmed = true;
    refreshMorphUi();
    mainLayout->getStatusBar().showMessage(
        "Turn a front-panel knob to assign it to the Morph fader...", 8000);
}

void MainComponent::clearMorphKnobAssignment() {
    // If we assigned a carrier knob (via the fader's Knob menu), deassign it on
    // the synth too. Learn-based mappings leave morphKnobIndex == -1 and reuse a
    // real param's own knob, so we must not touch those.
    if (morphKnobIndex >= 0 && morphKnobSection == 2 && morphKnobModule == 1
        && morphKnobParam >= 0 && currentPatch() && undoContext()) {
        undoManager().beginNewTransaction("Remove Morph Fader Knob");
        undoManager().perform(new KnobAssignAction(*undoContext(), 2, 1, morphKnobParam, -1, morphKnobIndex));
    }
    morphKnobSection = morphKnobModule = morphKnobParam = -1;
    morphKnobIndex = -1;
    morphLearnArmed = false;
    refreshMorphUi();
    mainLayout->getStatusBar().showMessage("Morph fader knob assignment cleared", 2000);
}

void MainComponent::resetMorphAB() {
    morphA = {};
    morphB = {};
    morphPairs.clear();
    morphLearnArmed = false;
    morphKnobSection = morphKnobModule = morphKnobParam = -1;
    morphKnobIndex = -1;
    morphKnobMin = 0;
    morphKnobMax = 127;
    if (mainLayout) {
        mainLayout->getHeaderBar().setMorphFaderPos(0.0f);
        refreshMorphUi();
    }
}

void MainComponent::refreshMorphUi() {
    if (!mainLayout) return;
    auto& hb = mainLayout->getHeaderBar();
    hb.setMorphEndpoints(morphA.filled, morphB.filled);
    hb.setMorphKnobAssigned(morphKnobParam >= 0);
    hb.setMorphLearnArmed(morphLearnArmed);
    // Surface the carrier knob in the inspector (only carrier assignments carry a
    // physical knob index; Learn-based ones leave morphKnobIndex == -1).
    mainLayout->getInspector().setMorphFaderKnob(morphKnobIndex, morphKnobParam);
}

void MainComponent::recallSnapshot(int index) {
    if (!currentPatch() || !undoContext() || index < 0 || index >= PatchVariations::kNumSlots)
        return;
    auto& vars = variations[activeSlot];
    auto& snap = vars.slots[index];
    if (!snap.filled) return;

    // Stop any running interpolation before applying an instant recall.
    if (interpolation.active)
        stopInterpolation("instant variation recall");

    // Mark active BEFORE applying so write-through into the recalled slot is a no-op
    vars.activeIndex = index;
    applySnapshot(snap, "Recall Variation " + juce::String(index + 1));

    refreshSnapshotUi();
    mainLayout->getStatusBar().showMessage(
        "Variation " + juce::String(index + 1) + " recalled", 2000);
}

void MainComponent::copySnapshot(int from, int to) {
    if (!currentPatch()) return;
    auto& vars = variations[activeSlot];
    if (from < 0 || from >= PatchVariations::kNumSlots || !vars.slots[from].filled) return;
    vars.copySlot(from, to);
    refreshSnapshotUi();
    mainLayout->getStatusBar().showMessage(
        "Variation " + juce::String(from + 1) + " copied to " + juce::String(to + 1), 2000);
}

void MainComponent::initSnapshot(int index) {
    if (!currentPatch() || index < 0 || index >= PatchVariations::kNumSlots) return;
    variations[activeSlot].initFromDefaults(*currentPatch(), index);
    refreshSnapshotUi();
    mainLayout->getStatusBar().showMessage(
        "Variation " + juce::String(index + 1) + " set to default values", 2000);
}

void MainComponent::interpolateSnapshots(int /*fromIndex*/, int toIndex, float seconds) {
    if (!currentPatch() || toIndex < 0 || toIndex >= PatchVariations::kNumSlots) return;
    auto& toSnap = variations[activeSlot].slots[toIndex];
    if (!toSnap.filled) return;
    startInterpolationTo(toSnap, seconds, toIndex);
}

void MainComponent::startInterpolationTo(const ParamSnapshot& toSnap, float seconds,
                                         int targetVariation) {
    if (!currentPatch() || !toSnap.filled) return;

    // Stop any running interpolation
    if (interpolation.active)
        stopInterpolation("new interpolation");

    // Capture current state as "from"
    interpolation.from.clear();
    interpolation.to.clear();

    // Build matched pairs: current value → target value
    for (auto& e : toSnap.entries) {
        auto& container = currentPatch()->getContainer(e.section);
        auto* mod = container.getModuleByIndex(e.moduleId);
        if (!mod) continue;
        auto* param = mod->getParameter(e.paramId);
        if (!param || param->isLocked()) continue;
        if (param->getDescriptor()->paramClass != "parameter") continue;

        interpolation.from.push_back({e.section, e.moduleId, e.paramId, param->getValue()});
        interpolation.to.push_back({e.section, e.moduleId, e.paramId, e.value});
    }

    interpolation.durationMs = seconds * 1000.0f;
    interpolation.elapsedMs = 0.0f;
    interpolation.targetSnapshot = targetVariation;
    interpolation.targetMorphs = toSnap.morphValues;
    interpolation.active = true;

    const juce::String targetName = targetVariation >= 0
        ? "variation " + juce::String(targetVariation + 1)
        : juce::String("mutator sound");
    std::cout << "[SNAP] Interpolation START → " << targetName
              << ", " << interpolation.from.size() << " params, "
              << seconds << "s (" << interpolation.durationMs << "ms)" << std::endl;

    mainLayout->getHeaderBar().setInterpolationProgress(0.0f);
    mainLayout->getStatusBar().showMessage(
        "Interpolating to " + targetName +
        " over " + juce::String(seconds, 1) + "s", static_cast<int>(seconds * 1000));

    // Timer: ~30ms ticks for smooth interpolation
    struct InterpTimer : public juce::Timer {
        MainComponent& mc;
        InterpTimer(MainComponent& m) : mc(m) {}
        void timerCallback() override { mc.onInterpolationTick(); }
    };

    interpolationTimer = std::make_unique<InterpTimer>(*this);
    interpolationTimer->startTimer(30);
}

void MainComponent::stopInterpolation(const char* reason) {
    if (!interpolation.active && !interpolationTimer)
        return;

    std::cout << "[SNAP] Interpolation cancelled: " << reason << std::endl;
    interpolation.active = false;
    if (interpolationTimer)
        interpolationTimer->stopTimer();
    interpolationTimer.reset();
    interpolation = {};
    if (mainLayout)
        mainLayout->getHeaderBar().setInterpolationProgress(-1.0f);
}

void MainComponent::onInterpolationTick() {
    if (!interpolation.active || !currentPatch()) {
        std::cout << "[SNAP] Tick ABORTED: active=" << interpolation.active
                  << " patch=" << (currentPatch() != nullptr) << std::endl;
        interpolation.active = false;
        if (interpolationTimer) interpolationTimer->stopTimer();
        interpolationTimer.reset();
        mainLayout->getHeaderBar().setInterpolationProgress(-1.0f);
        return;
    }

    interpolation.elapsedMs += 30.0f;
    float t = juce::jlimit(0.0f, 1.0f, interpolation.elapsedMs / interpolation.durationMs);

    if (interpolation.elapsedMs <= 60.0f || t >= 0.99f)
        std::cout << "[SNAP] Tick: elapsed=" << interpolation.elapsedMs
                  << "ms t=" << t << " duration=" << interpolation.durationMs << "ms" << std::endl;

    // Apply interpolated values and queue changed params for the synth. The
    // connection's coalescing queue collapses repeated ticks on the same
    // parameter to the latest value, so long interpolations stay smooth.
    for (size_t i = 0; i < interpolation.from.size(); ++i) {
        auto& f = interpolation.from[i];
        auto& toE = interpolation.to[i];
        int interpolatedVal = juce::roundToInt(
            f.value + (toE.value - f.value) * t);

        auto& container = currentPatch()->getContainer(f.section);
        auto* mod = container.getModuleByIndex(f.moduleId);
        if (!mod) continue;
        auto* param = mod->getParameter(f.paramId);
        if (!param) continue;

        int oldVal = param->getValue();
        param->setValue(interpolatedVal);
        if (oldVal != interpolatedVal)
            connectionManager.queueParameter(activeSlot, f.section, f.moduleId, f.paramId, interpolatedVal);
    }

    mainLayout->getCanvas().repaintCanvas();
    mainLayout->getHeaderBar().setInterpolationProgress(t);

    // Done?
    if (t >= 1.0f) {
        std::cout << "[SNAP] Interpolation COMPLETE (target "
                  << interpolation.targetSnapshot << ")" << std::endl;
        interpolation.active = false;
        if (interpolationTimer) interpolationTimer->stopTimer();
        interpolationTimer.reset();
        mainLayout->getHeaderBar().setInterpolationProgress(-1.0f);
        if (interpolation.targetSnapshot >= 0) {
            // Variation recall: mark it active (mutator auditions don't)
            variations[activeSlot].activeIndex = interpolation.targetSnapshot;
            mainLayout->getHeaderBar().setActiveSnapshot(interpolation.targetSnapshot);
        }

        // Morph knob values land at the end (protocol: section=2, module=1)
        if (currentPatch() && currentPatch()->morphValues != interpolation.targetMorphs) {
            currentPatch()->morphValues = interpolation.targetMorphs;
            if (connectionManager.isConnected())
                for (int m = 0; m < 4; ++m)
                    connectionManager.queueParameter(activeSlot, 2, 1, m,
                        interpolation.targetMorphs[static_cast<size_t>(m)]);
            mainLayout->getHeaderBar().repaint();
        }

        // Queue all final values so the synth lands exactly on the target sound.
        for (size_t i = 0; i < interpolation.to.size(); ++i) {
            auto& e = interpolation.to[i];
            connectionManager.queueParameter(activeSlot, e.section, e.moduleId, e.paramId, e.value);
        }

        mainLayout->getStatusBar().showMessage(
            interpolation.targetSnapshot >= 0
                ? "Interpolation complete — Variation " +
                      juce::String(interpolation.targetSnapshot + 1)
                : juce::String("Interpolation complete"), 2000);
    }
}

void MainComponent::initializeModule(int section, Module* module) {
  if (!module || !currentPatch() || !undoContext()) return;

  std::vector<RandomizeAction::ParamChange> changes;
  for (auto& param : module->getParameters()) {
      if (param.isLocked()) continue;
      auto* pd = param.getDescriptor();
      if (!pd || pd->paramClass != "parameter") continue;
      int oldVal = param.getValue();
      int newVal = pd->defaultValue;
      if (oldVal != newVal)
          changes.push_back({section, module->getContainerIndex(),
                             pd->index, oldVal, newVal});
  }
  if (changes.empty()) return;

  // Use the current transaction if one is already open (multi-select groups calls)
  if (!undoManager().isPerformingUndoRedo())
      undoManager().beginNewTransaction("Initialize Module");
  undoManager().perform(new RandomizeAction(*undoContext(), std::move(changes)));
  mainLayout->getCanvas().repaintCanvas();
  mainLayout->getStatusBar().showMessage(
      "Initialized " + module->getTitle(), 2000);
}

void MainComponent::randomizeParameters(bool gaussian) {
  // Main-window entry point: the active slot, its canvas selection and canvas.
  randomizeSlotParameters(activeSlot, mainLayout->getCanvas(), gaussian);
}

// Slot-scoped core (issue #22): a SlotWindow routes its Ctrl+R here with its
// own slot and canvas, so Randomize acts on the window's patch and honours the
// window's own module selection, instead of being a silent no-op that only
// ever touched the active slot.
void MainComponent::randomizeSlotParameters(int slot, PatchCanvasComponent& canvas, bool gaussian) {
  if (slot < 0 || slot >= numSlots) return;
  Patch* p = slotPatches[slot].get();
  UndoContext* uc = slotUndoContexts[slot].get();
  if (!p || !uc) return;

  // Auto-exclude list: parameter names that should not be randomized
  static const juce::StringArray excludeNames = {
      "Mute", "mute", "Level", "level", "Vol", "vol",
      "Active", "active", "Bypass", "bypass"
  };

  auto shouldExclude = [&](const Parameter& param) -> bool {
      if (param.isLocked()) return true;
      auto* pd = param.getDescriptor();
      if (!pd) return true;
      if (pd->paramClass != "parameter") return true;  // skip morph/custom
      if (pd->minValue == pd->maxValue) return true;    // no range
      if (pd->maxValue - pd->minValue <= 1) return true; // skip binary switches
      if (pd->role.containsIgnoreCase("level")) return true;
      for (auto& ex : excludeNames)
          if (pd->name.containsIgnoreCase(ex)) return true;
      return false;
  };

  juce::Random rng;

  std::vector<RandomizeAction::ParamChange> changes;

  // If modules are selected, only randomize those; otherwise randomize all
  auto selected = canvas.getSelectedModules();
  std::set<Module*> selectedSet;
  for (auto& [mod, sec] : selected)
      selectedSet.insert(mod);
  bool hasSelection = !selectedSet.empty();

  auto processContainer = [&](ModuleContainer& container, int section) {
      for (auto& modPtr : container.getModules()) {
          if (!modPtr) continue;
          if (hasSelection && selectedSet.count(modPtr.get()) == 0) continue;
          for (auto& param : modPtr->getParameters()) {
              if (shouldExclude(param)) continue;
              auto* pd = param.getDescriptor();
              int oldVal = param.getValue();
              int newVal;
              if (gaussian) {
                  // Gaussian centered on midpoint, sigma = range/6
                  float mid = (pd->minValue + pd->maxValue) * 0.5f;
                  float sigma = (pd->maxValue - pd->minValue) / 6.0f;
                  float g1 = rng.nextFloat();
                  float g2 = rng.nextFloat();
                  // Box-Muller
                  float z = std::sqrt(-2.0f * std::log(juce::jmax(g1, 1e-10f)))
                            * std::cos(2.0f * juce::MathConstants<float>::pi * g2);
                  newVal = juce::jlimit(pd->minValue, pd->maxValue,
                                        juce::roundToInt(mid + z * sigma));
              } else {
                  newVal = pd->minValue + rng.nextInt(pd->maxValue - pd->minValue + 1);
              }
              if (newVal != oldVal) {
                  changes.push_back({section, modPtr->getContainerIndex(),
                                     pd->index, oldVal, newVal});
              }
          }
      }
  };

  processContainer(p->getPolyVoiceArea(), 1);
  processContainer(p->getCommonArea(), 0);

  if (changes.empty()) return;

  // Single undo transaction with batched synth upload. The RandomizeAction runs
  // against this slot's UndoContext, whose repaint callback already redraws
  // whichever surface(s) show the slot (main canvas and/or its SlotWindow), so
  // we don't repaint here explicitly.
  int numChanges = static_cast<int>(changes.size());
  slotUndoManagers[slot].beginNewTransaction("Randomize Parameters");
  slotUndoManagers[slot].perform(new RandomizeAction(*uc, std::move(changes)));

  juce::String scope = hasSelection ? " (selection)" : " (all)";
  mainLayout->getStatusBar().showMessage(
      "Randomized " + juce::String(numChanges) + " parameters" + scope
      + (gaussian ? " — Gaussian" : " — Simple"), 3000);
}

void MainComponent::updateDspLoadDisplay() {
  if (currentPatch() == nullptr) {
    mainLayout->getHeaderBar().setLoadValues(-1.0f, -1.0f);
    return;
  }

  // Sum cycles from all modules in each voice area
  double polyCycles = 0.0;
  for (auto& mod : currentPatch()->getPolyVoiceArea().getModules())
    if (mod && mod->getDescriptor())
      polyCycles += mod->getDescriptor()->cycles;

  double commonCycles = 0.0;
  for (auto& mod : currentPatch()->getCommonArea().getModules())
    if (mod && mod->getDescriptor())
      commonCycles += mod->getDescriptor()->cycles;

  double total = polyCycles + commonCycles;

  // cycles values in modules.xml are already percentages (0-100 scale)
  mainLayout->getHeaderBar().setLoadValues(
      static_cast<float>(polyCycles / 100.0),
      static_cast<float>(total / 100.0));
}

void MainComponent::saveSnippet(SnipData snip)
{
  auto startFolder = editorOptions.getSnippetsFolder();
  auto chooser = std::make_shared<juce::FileChooser>(
      "Save Snippet", startFolder.exists() ? startFolder : juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
      [this, chooser, snip = std::move(snip)](const juce::FileChooser& fc) mutable {
        auto result = fc.getResult();
        if (result == juce::File()) return;

        auto file = result.hasFileExtension(".pch") ? result : result.withFileExtension("pch");
        snip.name = file.getFileNameWithoutExtension();

        auto tempPatch = snipDataToPatch(snip, moduleDescs);
        PchFileIO io(moduleDescs);
        if (io.writeFile(*tempPatch, file))
          mainLayout->getStatusBar().showMessage("Snippet saved: " + file.getFileName(), 3000);
        else
          mainLayout->getStatusBar().showMessage("ERROR: Failed to save snippet", 5000);
      });
}

void MainComponent::importSnippet()
{
  if (!currentPatch() || !undoContext()) return;

  auto startFolder = editorOptions.getSnippetsFolder();
  auto chooser = std::make_shared<juce::FileChooser>(
      "Import Snippet", startFolder.exists() ? startFolder : juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        importSnippetFromFile(result);
      });
}

void MainComponent::importSnippetFromFile(const juce::File& file)
{
  importSnippetFromFile(file, 3, 3);
}

void MainComponent::importSnippetFromFile(const juce::File& file, int targetGridX, int targetGridY)
{
  if (!currentPatch() || !undoContext()) return;
  if (!file.existsAsFile()) return;

  PchFileIO io(moduleDescs);
  auto tempPatch = io.readFile(file);
  if (!tempPatch)
  {
    mainLayout->getStatusBar().showMessage("ERROR: Could not read .pch file", 5000);
    return;
  }

  auto snip = patchToSnipData(*tempPatch);
  if (snip.entries.empty())
  {
    mainLayout->getStatusBar().showMessage("ERROR: No modules found in file", 5000);
    return;
  }

  // Offset so snippet lands at the requested grid position.
  int minX = snip.entries[0].gridPos.x;
  int minY = snip.entries[0].gridPos.y;
  for (auto& e : snip.entries)
  {
    minX = std::min(minX, e.gridPos.x);
    minY = std::min(minY, e.gridPos.y);
  }

  undoManager().beginNewTransaction("Import Snippet");
  undoManager().perform(new InsertSnippetAction(
      *undoContext(), std::move(snip), targetGridX - minX, targetGridY - minY));

  mainLayout->getStatusBar().showMessage(
      "Snippet imported from " + file.getFileName(), 3000);
}
