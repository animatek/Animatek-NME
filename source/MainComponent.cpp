#include "MainComponent.h"
#include "ui/KnobDrag.h"
#include "model/PatchParser.h"
#include "model/PchFileIO.h"
#include "ui/BankTransferDialog.h"
#include "ui/MidiSettingsDialog.h"
#include "ui/PatchLocationDialog.h"
#include "ui/AboutDialog.h"
#include "ui/SelfOwnedDialog.h"
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

MainComponent::MainComponent(juce::ApplicationProperties &props)
    : appProperties(props) {
  editorOptions = EditorOptions::load(appProperties.getUserSettings());
  AppTheme::setPalette(ThemeRegistry::get(editorOptions.uiThemeIndex).app);
  editorOptions.ensureLibraryFolders();
  PatchCanvas::setCableStyle   (static_cast<int>(editorOptions.cableStyle));
  KnobDrag::setMode           (static_cast<int>(editorOptions.knobControl));
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
  InspectorPanel::setSharedSettings(appProperties.getUserSettings());

  // Load module descriptions — prefer embedded BinaryData, fall back to disk
  if (BinaryData::modules_xmlSize > 0)
    moduleDescs.loadFromXmlString(juce::String::createStringFromData(
        BinaryData::modules_xml, BinaryData::modules_xmlSize));
  else {
    auto xmlPath = findDataFile("nmedit/libs/nordmodular/data/module-descriptions/modules.xml");
    if (xmlPath.existsAsFile()) moduleDescs.loadFromFile(xmlPath);
    else DBG("WARNING: modules.xml not found!");
  }

  DBG("Loaded " + juce::String(moduleDescs.getModuleCount()) + " module descriptions");

  // Load classic theme — prefer embedded BinaryData, fall back to disk
  if (BinaryData::classictheme_xmlSize > 0)
    themeData.loadFromXmlString(juce::String::createStringFromData(
        BinaryData::classictheme_xml, BinaryData::classictheme_xmlSize));
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

  mainLayout->getPatchArea().setAnimated(editorOptions.animateTiling);
  mainLayout->setModuleIconBarVisible(editorOptions.moduleIconBar);
  mainLayout->getModuleIconBar().setSelectedCategory(editorOptions.moduleIconBarCategory);
  mainLayout->getModuleIconBar().onCategoryChanged = [this](const juce::String& category) {
    editorOptions.moduleIconBarCategory = category;
    editorOptions.save(appProperties.getUserSettings());
  };

  initModulePresetLibrary();

  // Every slot's canvas is wired the same way, once, against its own slot. The
  // shared surfaces below (inspector, header bar, browsers, status bar) stay
  // bound to activeSlot and follow focus.
  for (int s = 0; s < numSlots; ++s)
    wireSlotView(s);

  // Clicking a sub-window makes its slot the active one. switchToSlot focuses
  // the window in turn, so this comes straight back round; inSlotFocusChange
  // breaks the loop.
  mainLayout->getPatchArea().onSlotFocused = [this](int slot) {
    // Restoring opens slots one by one and each one takes focus as it appears,
    // which would run a full slot switch per slot; restoreMdiLayout does one at
    // the end for the slot that should actually end up focused.
    if (inSlotFocusChange || restoringMdiLayout) return;
    switchToSlot(slot);
  };
  // Maximise button: same thing as F11, on the slot whose button was pressed.
  mainLayout->getPatchArea().onSlotMaximiseRequested = [this](int slot) {
    auto& area = mainLayout->getPatchArea();
    // Focus mode always blows up whichever slot has focus, so maximising a
    // window that does not have it has to take it first.
    if (!area.isFocusMode() && area.getFocusedSlot() != slot)
      area.focusSlot(slot);
    toggleFocusMode();
  };
  // A slot whose window the user closed keeps its patch; if it was the active
  // one, hand the shared surfaces to whatever is still on screen.
  mainLayout->getPatchArea().onSlotClosed = [this](int slot) {
    if (syncingSlotWindows) return;
    if (slot != activeSlot) return;
    const int next = mainLayout->getPatchArea().getFocusedSlot();
    if (next >= 0 && next != activeSlot)
      switchToSlot(next);
  };
  // Open/closed, tile mode and focus mode all show as tick marks in the View
  // menu, and the native macOS menu bar keeps a stale tick unless it is rebuilt.
  // Which slots are open is also persisted from here; restoreMdiLayout() below
  // is what actually opens them, so nothing opens a slot before then.
  mainLayout->getPatchArea().onLayoutChanged = [this]() {
    menuItemsChanged();
    saveMdiLayout();
    // The ABCD button greys out when the layout is already the canonical one,
    // and this is the only place that knows the layout moved.
    mainLayout->getHeaderBar().setRetileEnabled(
        mainLayout->getPatchArea().canResetTileOrder());
  };


  // The main window's inspector follows whichever slot is active, so it binds to
  // "the active slot" rather than to a fixed one the way a slot window does.
  wirePresetCallbacks(mainLayout->getInspector(), -1);

  // Wire inspector name changes (undoable)
  mainLayout->getInspector().onNameChanged =
      [this](int section, Module* module, const juce::String& oldName, const juce::String& newName) {
    if (!module || !currentPatch() || !undoContext()) return;
    undoManager().beginNewTransaction("Rename Module");
    undoManager().perform(new RenameModuleAction(
        *undoContext(), section, module->getContainerIndex(), oldName, newName));
  };

  // Parameters edited in the inspector's own list. Same two-step shape as the
  // canvas and the knob floater: every move goes on the wire, and the gesture
  // is recorded once so it undoes in one step. The list is repainted, never
  // rebuilt, or a live drag would lose the row it is holding.
  mainLayout->getInspector().onParameterChanged =
      [this](int section, Module* module, int paramIndex, int value) {
    if (!module) return;
    connectionManager.sendParameter(activeSlot, section, module->getContainerIndex(),
                                    paramIndex, value);
    canvasFor(activeSlot).repaintCanvas();
    if (knobFloaterWindow && knobFloaterWindow->isVisible())
      knobFloaterWindow->refresh();
  };
  mainLayout->getInspector().onParameterEditComplete =
      [this](int section, Module* module, int paramIndex, int oldValue, int newValue) {
    if (!module || !undoContext()) return;
    undoManager().beginNewTransaction("Parameter Change");
    undoManager().perform(new ParameterChangeAction(
        *undoContext(), section, module->getContainerIndex(), paramIndex, oldValue, newValue));
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
  // The synth answers a bank load with the location it loaded, whether the load
  // came from here or from the front panel. Arrives on the MIDI thread.
  connectionManager.setBankLocationCallback([this](int slot) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, slot]() {
      if (!safeThis) return;
      if (slot == safeThis->activeSlot)
        safeThis->updateStoreLocationDisplay();
    });
  });

  connectionManager.setPatchListCallback([this](const std::vector<std::string>& names) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, names]() {
      if (!safeThis) return;
      safeThis->mainLayout->getPatchBrowser().setPatchList(names);
      safeThis->mainLayout->getPatchBrowser().setLoadingState(false);
      // The list is usually the last thing to arrive, so this is where slots
      // filled at connection time finally learn where their patch lives.
      for (int s = 0; s < numSlots; ++s)
        safeThis->inferSlotBankLocation(s);
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
    loadBankPatchIntoSlot(section, position, activeSlot);
  };
  mainLayout->getPatchBrowser().onPatchLoadToSlot = [this](int section, int position, int slot) {
    loadBankPatchIntoSlot(section, position, slot);
  };

  // Dragging a patch out of the Synth browser onto a slot loads it there
  // (issue #50). Two targets, and both end in the same call the right-click
  // "Load to Slot A..D" already used: the slot's sub-window, which is the
  // obvious gesture, and the slot bar's rows, which is the one that still works
  // when that slot's window is closed.
  // The Disk browser's patches drop the same way, and land in the same places.
  // No slot chooser on this path, unlike File > Open: the drop already named the
  // slot, and asking again would be asking twice.
  mainLayout->getSlotBar().onPatchDroppedOnSlot =
      [this](int section, int position, int slot) {
    loadBankPatchIntoSlot(section, position, slot);
  };
  mainLayout->getSlotBar().onPatchFileDroppedOnSlot =
      [this](const juce::File& file, int slot) {
    loadPatchFromFile(file, slot, /*localOnly=*/false);
  };
  for (int slot = 0; slot < numSlots; ++slot) {
    auto& view = mainLayout->getPatchArea().getView(slot);
    view.onPatchDropped = [this](int section, int position, int targetSlot) {
      loadBankPatchIntoSlot(section, position, targetSlot);
    };
    view.onPatchFileDropped = [this](const juce::File& file, int targetSlot) {
      loadPatchFromFile(file, targetSlot, /*localOnly=*/false);
    };
  }

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
            " sections, parsing...");

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
          // Whatever the outgoing patch had that was not written yet goes to the
          // library before it is destroyed.
          safeThis->flushExtras(targetSlot);

          // Comments and patch notes live in the editor and the .pch, never on
          // the wire, so a patch coming back from the synth arrives without
          // them. Carry them over when it is plainly the same patch being
          // re-read (same name): otherwise the synth loaded something else and
          // the old notes would belong to a patch that is no longer here. The
          // library below is what covers every other case, but this one is more
          // current than anything on disk, so it goes first.
          if (safeThis->slotPatches[targetSlot] != nullptr && p != nullptr
              && safeThis->slotPatches[targetSlot]->getName() == p->getName()) {
            p->extrasId = safeThis->slotPatches[targetSlot]->extrasId;
            p->adoptComments(safeThis->slotPatches[targetSlot]->getComments());
            if (p->patchNotes.isEmpty())
              p->patchNotes = safeThis->slotPatches[targetSlot]->patchNotes;
          }

          safeThis->slotPatches[targetSlot] = std::move(p);
          if (safeThis->slotPatches[targetSlot]) {
            if (safeThis->connectionManager.isConnected()) {
              safeThis->slotSynchronizers[targetSlot] = std::make_unique<PatchSynchronizer>(
                  *safeThis->slotPatches[targetSlot], safeThis->connectionManager, targetSlot);
            }

            safeThis->slotUndoManagers[targetSlot].clearUndoHistory();
            safeThis->rebuildUndoContext(targetSlot);
            safeThis->clearSnapshots(targetSlot);

            // The slot's own canvas always adopts the new patch object, on
            // screen or not: leaving a background canvas pointing at the Patch
            // that is about to be destroyed is the dangling-Module* crash from
            // 0.12.0 waiting to happen.
            safeThis->canvasFor(targetSlot).setPatch(
                safeThis->slotPatches[targetSlot].get(), &safeThis->moduleDescs, &safeThis->themeData);
            safeThis->mainLayout->getPatchArea().getView(targetSlot).setPatchTitle(
                safeThis->slotPatches[targetSlot]->getName());

            // If this is the currently viewed slot, update the shared surfaces
            if (targetSlot == safeThis->activeSlot) {
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
              safeThis->updateStoreLocationDisplay();
            }

            // Update slot bar with patch name. This patch came from the synth,
            // so the slot is in sync — clear any stale LOCAL badge (issue #21).
            safeThis->mainLayout->getSlotBar().setSlotName(targetSlot, safeThis->slotPatches[targetSlot]->getName());
            safeThis->setSlotLocal(targetSlot, false);

            // Then work out where it lives. This has to come after the LOCAL
            // badge is cleared: a slot still marked local is not looked up.
            safeThis->slotPatchFromSynth[targetSlot] = true;
            safeThis->inferSlotBankLocation(targetSlot);

            // And give it back its comments, notes, variations and Mutator
            // exclusions, which could not travel over the wire.
            safeThis->attachExtrasFromLibrary(targetSlot);

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

  // Wire morph knob changes from header bar to synth
  // Morphs use section=2 (morph section), module=1 (morph module),
  // parameter=0-3
  mainLayout->getHeaderBar().setMorphChangeCallback(
      [this](int morphIndex, int value) {
        connectionManager.sendParameter(activeSlot, 2, 1, morphIndex, value);
        // A knob assigned to this morph group has a cell in the floater showing
        // the value it drives, so dragging the dial has to move it there too
        // (issue #64). The header bar has already written the patch.
        refreshKnobFloater();
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
        mainLayout->getPatchArea().getView(activeSlot).setPatchTitle(newName);
      });

  // Wire the header's store button (one click, no dialog)
  mainLayout->getHeaderBar().setQuickSaveCallback([this]() { quickStoreToBank(); });

  // Wire cable visibility toggles to repaint the canvas
  mainLayout->getHeaderBar().setCableVisibilityCallback(
      [this]() { activeCanvas().repaintCanvas(); });

  // Wire real-time light/meter data from synth. Delivered straight through:
  // incoming SysEx is already bounced to the message thread before the
  // protocol ever sees it (MidiDeviceManager::handleIncomingMidiMessage), so
  // the extra callAsync only bought a copy of two 128-int arrays and a frame
  // of latency, on the one callback the synth sends many times a second.
  connectionManager.setLightMeterCallback(
      [this](const int lights[128], const int meters[128]) {
          JUCE_ASSERT_MESSAGE_THREAD
          // The synth streams lights/meters for the slot it has focused,
          // which the editor keeps in sync with activeSlot, so they belong
          // to that slot's canvas, whichever one the user is looking at.
          canvasFor(activeSlot).setLightMeterData(lights, meters);
      });

  // Wire shake cables button
  mainLayout->getHeaderBar().setShakeCablesCallback(
      [this]() { activeCanvas().shakeCables(); });

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
  mainLayout->getHeaderBar().setRetileButtonCallback(
      [this]() { mainLayout->getPatchArea().resetTileOrder(); });

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
        return;
      }

      // Skip if the user is currently dragging this exact parameter (avoid
      // fighting the user)
      if (safeThis->canvasFor(safeThis->activeSlot).isDragging(section, moduleId, parameterId))
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
        safeThis->canvasFor(safeThis->activeSlot).repaintCanvas();
        if (safeThis->knobFloaterWindow && safeThis->knobFloaterWindow->isVisible())
          safeThis->knobFloaterWindow->refresh();
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
        + " (" + description + "): check console for details", 8000);
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
    importSnippetFromFile(activeSlot, file);
  };
  // Wire bug report button on header bar
  mainLayout->getHeaderBar().setReportBugCallback([this]() {
    openURL("https://github.com/animatek/Animatek-NME/issues");
  });

  // Wire slot tab changes (user clicks tab)
  mainLayout->onSlotChanged = [this](int slot) {
    switchToSlot(slot);
  };

  // Right-click a slot row: show or hide that slot's sub-window in the work area
  mainLayout->onSlotViewToggled = [this](int slot) {
    toggleSlotOpen(slot);
  };

  // Wire slot enable state (fixed LEDs on hardware; several can be on at once)
  connectionManager.setSlotsEnabledCallback([this](const std::array<bool, 4>& enabled) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, enabled]() {
      if (!safeThis) return;
      safeThis->mainLayout->getSlotBar().setSlotsEnabled(enabled);
      safeThis->lastEnabledSlots = enabled;
      safeThis->slotEnableStateKnown = true;
      // Only the first mask of a connection touches the sub-windows.
      safeThis->scheduleSlotWindowReconcile();
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
      safeThis->switchToSlot(slot, /*notifySynth=*/false, /*bringOnScreen=*/false);
    });
  });

  setSize(1280, 800);

  // After setSize, so the work area has real bounds to lay sub-windows out in.
  restoreMdiLayout();

  // The extras library: comments, notes, variations and Mutator exclusions for
  // every patch this editor has seen, so a patch coming back from the synth can
  // be recognised and given them back.
  patchExtras.setFolder(appConfigFolder().getChildFile("extras"));
  struct ExtrasTimer : public juce::Timer {
    MainComponent& mc;
    explicit ExtrasTimer(MainComponent& m) : mc(m) {}
    void timerCallback() override { mc.flushAllExtras(); }
  };
  extrasFlushTimer = std::make_unique<ExtrasTimer>(*this);
  extrasFlushTimer->startTimer(3000);

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

  // Last chance to write out comments and variations: the timer that normally
  // does it is about to stop, and the patches are about to be destroyed.
  if (extrasFlushTimer)
    extrasFlushTimer->stopTimer();
  flushAllExtras();
  extrasFlushTimer.reset();

  // Stop interpolation timer before anything else
  if (interpolationTimer)
    interpolationTimer->stopTimer();
  interpolationTimer.reset();

  // Quitting with a dialog on screen must not leave its caption sitting on the
  // synth's display. Before the disconnect, obviously, and before the dialogs
  // are taken down further below: by then the port is shut and the name would
  // have nowhere to go.
  clearSynthCaption();

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
  connectionManager.setBankLocationCallback(nullptr);
  connectionManager.setPatchLoadProgressCallback(nullptr);
  connectionManager.setPatchLoadIncompleteCallback(nullptr);

  // Tear down UI before members are destroyed
#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
  // The slot chooser and the store-location dialog live on the desktop and are
  // owned by nobody, so quitting with one still open leaked it and printed an
  // assertion for it and for every child it held.
  SelfOwnedDialog::closeAllOpen();
  menuBar.reset();
  saveFloaterState();
  // Dragging a sub-window around inside Free mode does not fire onLayoutChanged
  // (only the first drag does, when it leaves Auto), so catch the final
  // arrangement here rather than saving on every mouse move.
  saveMdiLayout();
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

// Which slot Ctrl+Shift+<digit> means.
//
// X11 reports a shifted digit by its symbol rather than by the digit: when Ctrl
// swallows the character, JUCE falls back to the *shifted* keysym, so
// Ctrl+Shift+1 arrives as '!' and never as '1'. It is the same mechanism behind
// the "Ctrl+8 arrives as DEL" workaround in the canvas. Letters survive it
// because KeyPress::operator== case-folds 'S' onto 's'; digits have no such
// luck. Cover the layouts we know and let the View menu carry the rest.
// macOS keeps Cmd+Shift+3 and Cmd+Shift+4 for its own screen capture and never
// passes them on, so two of the four slots could not be toggled there at all
// (issue #49). Option is used by nothing in this editor, so the Mac gets
// Cmd+Alt+<digit> instead. Windows and Linux keep Ctrl+Shift+<digit>, which
// works and which people already know.
#if JUCE_MAC
 #define NME_SLOT_TOGGLE_CHORD "Cmd+Alt+"
#else
 #define NME_SLOT_TOGGLE_CHORD "Ctrl+Shift+"
#endif

static int slotDigitFromShiftedKey(const juce::KeyPress& key)
{
  const int code = key.getKeyCode();
  if (code >= '1' && code <= '4')
    return code - '1';

  switch (code)
  {
    case '!':               return 0;
    case '@': case '"':     return 1;   // US, Spanish
    case '#': case 0xB7: case 0xA3: return 2;   // US, Spanish middle dot, UK pound
    case '$':               return 3;
    default:                return -1;
  }
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
  // Modules waiting on the pointer answer Enter and Escape from here as well as
  // from the canvas itself. Quick Add is a window of its own, so the keyboard
  // focus does not always come back to the canvas when it closes, and Enter is
  // the whole point of adding a module from the keyboard.
  if (key == juce::KeyPress::escapeKey && PatchCanvas::isDropPending()) {
    PatchCanvas::cancelPendingDrop();
    return true;
  }
  if (key == juce::KeyPress::returnKey && PatchCanvas::dropPendingAtPointer())
    return true;

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
  // On macOS the naked Cmd+W belongs to "close window" and fired both ways
  // (issue #55), so wireframe takes Shift there; elsewhere Ctrl+W is free.
 #if JUCE_MAC
  if (key == juce::KeyPress('w', juce::ModifierKeys::commandModifier
                                     | juce::ModifierKeys::shiftModifier, 0))
 #else
  if (key == juce::KeyPress('w', juce::ModifierKeys::commandModifier, 0))
 #endif
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
  // F4 blows the focused slot up to fill the work area and back, the way a
  // tiling window manager's monocle does. It lived on F11 first, but macOS
  // owns F11 for Show Desktop (issue #55); F11 stays as a quiet alias for
  // fingers that learned it.
  if (key == juce::KeyPress::F4Key || key == juce::KeyPress::F11Key)
  {
    toggleFocusMode();
    return true;
  }
  // Show or hide a slot's sub-window. Ctrl+1..4 (no second modifier) still means
  // "make this slot the active one", which also opens it.
  if (key.getModifiers().isCommandDown())
  {
   #if JUCE_MAC
    // Option is ignored by charactersIgnoringModifiers, so the digit arrives as
    // itself and needs none of the X11 unshifting below.
    const int code = key.getKeyCode();
    const int slot = (key.getModifiers().isAltDown() && ! key.getModifiers().isShiftDown()
                      && code >= '1' && code <= '4') ? code - '1' : -1;
   #else
    const int slot = key.getModifiers().isShiftDown() ? slotDigitFromShiftedKey(key) : -1;
   #endif
    if (slot >= 0)
    {
      toggleSlotOpen(slot);
      return true;
    }
  }
  // Moving a slot between tiles stays on Ctrl+Shift+arrows everywhere: the
  // arrows clash with nothing, and their keysyms are stable under Shift.
  if (key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown())
  {
    // Which slot sits in which tile, the way Hyprland's swapwindow and rollnext
    // reorder a layout without changing what is in it.
    // All four arrows move the focused slot to the neighbouring tile in that
    // direction. Rotating every slot at once is a separate command, on the View
    // menu only, so the arrows mean one thing and mean it literally.
    using Dir = SlotMdiArea::Direction;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey  || code == juce::KeyPress::rightKey
        || code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
      mainLayout->getPatchArea().moveFocusedTile(
          code == juce::KeyPress::leftKey  ? Dir::Left
        : code == juce::KeyPress::rightKey ? Dir::Right
        : code == juce::KeyPress::upKey    ? Dir::Up
                                           : Dir::Down);
      return true;
    }
  }
  if (handleFloaterShortcut(key))
    return true;
  // The overlay readouts (F5, F7-F10) set an editor-wide mode and repaint every
  // live canvas themselves (phase 0), so this must stay a single call — one per
  // canvas would toggle the mode four times.
  return mainLayout != nullptr
      && PatchCanvas::handleOverlayKey(key, activeCanvas());
}

juce::StringArray MainComponent::getMenuBarNames() {
  return {"File", "Edit", "View", "Device", "Help", "About"};
}

#if JUCE_MAC
// The Mac menu bar is a real NSMenu, and JUCE only ever fills in an item's key
// equivalent from an ApplicationCommandManager: shortcutKeyDescription is a
// LookAndFeel affair that the native menu drops on the floor. So moving the
// hints into that field for issue #56 left the Mac menus with no shortcuts at
// all (issue #74). Write them into the item's own text there instead, in the
// symbols a Mac user reads, since our "Ctrl" is Cmd on that platform anyway.
static juce::String macShortcutSymbols(const juce::String& shortcut)
{
  auto sym = [](const char* utf8) { return juce::String::fromUTF8(utf8); };

  juce::String rest = shortcut;
  bool cmd = false, shift = false, alt = false;
  for (;;)
  {
    // Note the modifier names are stripped one at a time rather than split on
    // '+', which "Ctrl++" (zoom in) would tear into empty pieces.
    if (rest.startsWith("Ctrl+") || rest.startsWith("Cmd+"))       cmd   = true;
    else if (rest.startsWith("Shift+"))                            shift = true;
    else if (rest.startsWith("Alt+") || rest.startsWith("Option+")) alt  = true;
    else break;

    rest = rest.fromFirstOccurrenceOf("+", false, false);
  }

  juce::String key = rest;
  if      (key == "Left")  key = sym("\xe2\x86\x90");
  else if (key == "Right") key = sym("\xe2\x86\x92");
  else if (key == "Up")    key = sym("\xe2\x86\x91");
  else if (key == "Down")  key = sym("\xe2\x86\x93");

  // Control, Option, Shift, Command: the order the Mac itself prints them in.
  juce::String out;
  if (alt)   out << sym("\xe2\x8c\xa5");
  if (shift) out << sym("\xe2\x87\xa7");
  if (cmd)   out << sym("\xe2\x8c\x98");
  return out + key;
}
#endif

// Shortcut hints go in the Item's own shortcut field, which the popup
// LookAndFeel right-aligns in its lighter style. They used to ride inside the
// label after a "\t", which the in-window menus rendered as one ragged line
// and the macOS native menu bar printed literally, tab and all (issue #56).
static void addShortcutItem(juce::PopupMenu& menu, int id, const juce::String& text,
                            const juce::String& shortcut,
                            bool enabled = true, bool ticked = false)
{
 #if JUCE_MAC
  juce::PopupMenu::Item item(text + "   " + macShortcutSymbols(shortcut));
 #else
  juce::PopupMenu::Item item(text);
 #endif
  item.itemID = id;
  item.shortcutKeyDescription = shortcut;
  item.isEnabled = enabled;
  item.isTicked = ticked;
  menu.addItem(std::move(item));
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex,
                                               const juce::String &) {
  juce::PopupMenu menu;

  if (menuIndex == 0) // File
  {
    addShortcutItem(menu, 1, "New Patch", "Ctrl+N");
    addShortcutItem(menu, 2, "Open...", "Ctrl+O");
    menu.addSeparator();
    addShortcutItem(menu, 3, "Save", "Ctrl+S");
    addShortcutItem(menu, 4, "Save As...", "Ctrl+Shift+S");
    menu.addSeparator();
    menu.addItem(5, "Import Snippet...", currentPatch() != nullptr);
    addShortcutItem(menu, 6, "Preset Browser...", "Ctrl+B");
    menu.addSeparator();
    addShortcutItem(menu, 8, "Patch Settings...", "Ctrl+P", currentPatch() != nullptr);
    addShortcutItem(menu, 9, "Synth Settings...", "Ctrl+G");
    addShortcutItem(menu, 11, "Editor Options...", "Ctrl+,");
    menu.addSeparator();
    addShortcutItem(menu, 10, "Quit", "Ctrl+Q");
  } else if (menuIndex == 1) // Edit
  {
    addShortcutItem(menu, 20, "Undo " + undoManager().getUndoDescription(), "Ctrl+Z",
                    undoManager().canUndo());
    addShortcutItem(menu, 21, "Redo " + undoManager().getRedoDescription(), "Ctrl+Shift+Z",
                    undoManager().canRedo());
    menu.addSeparator();
    // These have always been on the keyboard and on the module's own
    // right-click menu, but never here, which is the first place anyone looks
    // (issue #42).
    auto& canvas = activeCanvas();
    const bool anySelected = canvas.hasSelection();
    addShortcutItem(menu, 24, "Cut", "Ctrl+X", anySelected);
    addShortcutItem(menu, 25, "Copy", "Ctrl+C", anySelected);
    addShortcutItem(menu, 26, "Paste", "Ctrl+V", canvas.canPaste());
    addShortcutItem(menu, 27, "Duplicate", "Ctrl+D", anySelected);
    menu.addSeparator();
    bool hasPatch = (currentPatch() != nullptr);
    addShortcutItem(menu, 22, "Randomize (Simple)", "Ctrl+R", hasPatch);
    addShortcutItem(menu, 23, "Randomize (Gaussian)", "Ctrl+Shift+R", hasPatch);
  } else if (menuIndex == 2) // View
  {
    float zoom = activeCanvas().getZoomLevel();
    addShortcutItem(menu, 60, "Zoom In", "Ctrl++");
    addShortcutItem(menu, 61, "Zoom Out", "Ctrl+-");
    addShortcutItem(menu, 62, "Reset Zoom (100%)", "Shift+Z");
    addShortcutItem(menu, 63, "Zoom to Selection", "Z", !activeCanvas().isDragging(0, 0, 0));  // always enabled
    menu.addSeparator();
    juce::String zoomLabel = "Zoom: " + juce::String(juce::roundToInt(zoom * 100)) + "%";
    menu.addItem(-1, zoomLabel, false);
    menu.addSeparator();
    addShortcutItem(menu, 64, "Shake Cables", "S");
    menu.addSeparator();
    juce::PopupMenu themeMenu;
    for (int i = 0; i < ThemeRegistry::count(); ++i)  // ids 200+ reserved for themes
      themeMenu.addItem(200 + i, ThemeRegistry::get(i).name, true,
                        i == editorOptions.uiThemeIndex);
    {
      // A submenu with a shortcut hint needs the Item form: addSubMenu has no
      // shortcut field, and Ctrl+T cycling the theme is worth advertising.
     #if JUCE_MAC
      juce::PopupMenu::Item themeItem("Theme   " + macShortcutSymbols("Ctrl+T"));
     #else
      juce::PopupMenu::Item themeItem("Theme");
     #endif
      themeItem.subMenu = std::make_unique<juce::PopupMenu>(themeMenu);
      themeItem.shortcutKeyDescription = "Ctrl+T";
      menu.addItem(std::move(themeItem));
    }
    // Cmd+W is "close window" on macOS and the system wins the argument, so
    // wireframe rides with Shift there (issue #55).
   #if JUCE_MAC
    addShortcutItem(menu, 66, "Wireframe Modules", "Cmd+Shift+W", true, editorOptions.wireframe);
   #else
    addShortcutItem(menu, 66, "Wireframe Modules", "Ctrl+W", true, editorOptions.wireframe);
   #endif
    menu.addSeparator();

    auto& patchArea = mainLayout->getPatchArea();
    juce::PopupMenu slotMenu;
    for (int i = 0; i < numSlots; ++i)
    {
        auto letter = juce::String::charToString(static_cast<char>('A' + i));
        auto name = slotPatches[i] ? slotPatches[i]->getName() : juce::String("empty");
        addShortcutItem(slotMenu, 90 + i, "Slot " + letter + " - " + name,
                        NME_SLOT_TOGGLE_CHORD + juce::String(i + 1),
                        true, patchArea.isSlotOpen(i));
    }
    slotMenu.addSeparator();
    const bool severalOpen = patchArea.getNumOpenSlots() > 1;
    const bool grid = patchArea.getNumOpenSlots() == 4;  // up/down only exist in the 2x2
    addShortcutItem(slotMenu, 96, "Move Slot Left", "Ctrl+Shift+Left", severalOpen);
    addShortcutItem(slotMenu, 97, "Move Slot Right", "Ctrl+Shift+Right", severalOpen);
    addShortcutItem(slotMenu, 99, "Move Slot Up", "Ctrl+Shift+Up", grid);
    addShortcutItem(slotMenu, 100, "Move Slot Down", "Ctrl+Shift+Down", grid);
    slotMenu.addItem(98, "Rotate Slots", severalOpen);
    slotMenu.addSeparator();
    slotMenu.addItem(94, "Tile Slots", severalOpen,
                     patchArea.getTileMode() == SlotMdiArea::TileMode::Auto);
    // Same thing the ABCD button in the header bar does. "Tile Slots" above
    // only puts the windows back into the tiling; this also puts the slots back
    // into A, B, C, D order within it.
    slotMenu.addItem(101, "Reset Slot Order (ABCD)", patchArea.canResetTileOrder());
    addShortcutItem(slotMenu, 95, "Focus Mode", "F4", patchArea.getNumOpenSlots() > 1,
                    patchArea.isFocusMode());
    menu.addSubMenu("Slots", slotMenu);

    // The readouts had lived on the function keys alone, and people asking for
    // the module cost readout without finding F10 is how issue #44 was raised.
    // One is on at a time, so they tick like radio buttons.
    using Overlay = PatchCanvas::OverlayMode;
    const auto overlay = PatchCanvas::getOverlayMode();
    juce::PopupMenu overlayMenu;
    addShortcutItem(overlayMenu, 110, "Parameter Values", "F5", true, overlay == Overlay::Values);
    addShortcutItem(overlayMenu, 111, "Morph Groups", "F7", true, overlay == Overlay::MorphGroups);
    addShortcutItem(overlayMenu, 112, "Knob Assignments", "F8", true, overlay == Overlay::Knobs);
    addShortcutItem(overlayMenu, 113, "MIDI CC Assignments", "F9", true, overlay == Overlay::MidiCtrls);
    addShortcutItem(overlayMenu, 114, "Module DSP Cost", "F3", true, overlay == Overlay::ModuleCosts);
    overlayMenu.addSeparator();
    overlayMenu.addItem(115, "None", overlay != Overlay::Off);
    menu.addSubMenu("Overlays", overlayMenu);

    menu.addSeparator();
    menu.addItem(69, "Module Icon Bar", true, mainLayout->isModuleIconBarVisible());
    addShortcutItem(menu, 67, "Inspector Panel", "Ctrl+I", true, mainLayout->isLeftPanelVisible());
    addShortcutItem(menu, 68, "Patch Browser", "Ctrl+Shift+I", true, mainLayout->isRightPanelVisible());
    menu.addSeparator();
    addShortcutItem(menu, 80, "Knob Floater", "Ctrl+5", true,
                    knobFloaterWindow != nullptr && knobFloaterWindow->isVisible());
    addShortcutItem(menu, 81, "Keyboard Floater", "Ctrl+6", true,
                    keyboardFloaterWindow != nullptr && keyboardFloaterWindow->isVisible());
    addShortcutItem(menu, 82, "Patch Notes", "Ctrl+7", true,
                    patchNotesFloaterWindow != nullptr && patchNotesFloaterWindow->isVisible());
    addShortcutItem(menu, 83, "Patch Mutator", "Ctrl+8", true,
                    mutatorWindow != nullptr && mutatorWindow->isVisible());
    addShortcutItem(menu, 84, "SysEx Monitor", "Ctrl+9", true,
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
    saveSlotPatch(activeSlot);
    break;
  case 4:
    saveSlotPatchAs(activeSlot);
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
    runUndoRestoringSelection(activeSlot, /*redo=*/false);
    updateDspLoadDisplay();
    break;
  case 21:
    runUndoRestoringSelection(activeSlot, /*redo=*/true);
    updateDspLoadDisplay();
    break;
  case 24:
    activeCanvas().cutSelection();
    updateDspLoadDisplay();
    break;
  case 25:
    activeCanvas().copySelection();
    break;
  case 26:
    activeCanvas().pasteClipboard();
    updateDspLoadDisplay();
    break;
  case 27:
    activeCanvas().duplicateSelected();
    updateDspLoadDisplay();
    break;
  case 22:
    randomizeSlotParameters(activeSlot, activeCanvas(), false);
    break;
  case 23:
    randomizeSlotParameters(activeSlot, activeCanvas(), true);
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
    announceDialogOnSynth(
        AboutDialog::show(this, [this](const juce::String& url) { openURL(url); }));
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
    activeCanvas().setZoomLevel(activeCanvas().getZoomLevel() + 0.1f);
    break;
  case 61:  // Zoom Out
    activeCanvas().setZoomLevel(activeCanvas().getZoomLevel() - 0.1f);
    break;
  case 62:  // Reset Zoom
    activeCanvas().resetZoom();
    break;
  case 63:  // Zoom to Selection
    activeCanvas().zoomToSelection();
    break;
  case 64:  // Shake Cables
    activeCanvas().shakeCables();
    break;
  case 66:  // Wireframe Modules
    toggleWireframe();
    break;
  // Overlays: same toggles as F5 and F7-F10, so picking the mode that is
  // already showing turns it off again.
  case 110:
    PatchCanvas::toggleOverlayMode(PatchCanvas::OverlayMode::Values);
    break;
  case 111:
    PatchCanvas::toggleOverlayMode(PatchCanvas::OverlayMode::MorphGroups);
    break;
  case 112:
    PatchCanvas::toggleOverlayMode(PatchCanvas::OverlayMode::Knobs);
    break;
  case 113:
    PatchCanvas::toggleOverlayMode(PatchCanvas::OverlayMode::MidiCtrls);
    break;
  case 114:
    PatchCanvas::toggleOverlayMode(PatchCanvas::OverlayMode::ModuleCosts);
    break;
  case 115:
    PatchCanvas::setOverlayMode(PatchCanvas::OverlayMode::Off);
    break;
  case 67:  // Inspector Panel
    toggleLeftPanel();
    break;
  case 68:  // Patch Browser
    toggleRightPanel();
    break;
  case 69:  // Module Icon Bar
    toggleModuleIconBar();
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
  case 90: case 91: case 92: case 93:  // show/hide slot A..D
    toggleSlotOpen(menuItemID - 90);
    break;
  case 94:  // Tile Slots
    mainLayout->getPatchArea().retile();
    mainLayout->getStatusBar().showMessage("Slots tiled", 2000);
    break;
  case 95:  // Focus Mode
    toggleFocusMode();
    break;
  case 96:  // Move the focused slot one tile left
    mainLayout->getPatchArea().moveFocusedTile(SlotMdiArea::Direction::Left);
    break;
  case 97:  // ...and right
    mainLayout->getPatchArea().moveFocusedTile(SlotMdiArea::Direction::Right);
    break;
  case 98:  // Rotate every slot round one tile
    mainLayout->getPatchArea().rotateTiles(1);
    break;
  case 99:  // Move up (2x2 only)
    mainLayout->getPatchArea().moveFocusedTile(SlotMdiArea::Direction::Up);
    break;
  case 100:  // ...and down
    mainLayout->getPatchArea().moveFocusedTile(SlotMdiArea::Direction::Down);
    break;
  case 101:  // Reset slot order: A|B / C|D, same as the header bar's ABCD
    mainLayout->getPatchArea().resetTileOrder();
    mainLayout->getStatusBar().showMessage("Slots back in ABCD order", 2000);
    break;

  default:
    if (menuItemID >= 200 && menuItemID < 200 + ThemeRegistry::count())
      applyUiTheme(menuItemID - 200, true);
    break;
  }
}

void MainComponent::switchToSlot(int slot, bool notifySynth, bool bringOnScreen) {
  if (slot < 0 || slot >= numSlots)
    return;

  // Bringing the slot on screen focuses its sub-window, which reports back
  // through onSlotFocused and lands here again. Hold the guard across the whole
  // body, not just the focus call, so the re-entry is dropped outright.
  const juce::ScopedValueSetter<bool> focusGuard(inSlotFocusChange, true);

  auto& area = mainLayout->getPatchArea();

  // A slot button on the front panel is the user asking for that slot, just
  // from the other end of the cable, so the editor follows it into the slot
  // exactly as it follows its own slot bar: the window opens if it was closed.
  // It used to follow only into a window that already happened to be open,
  // which meant pressing A..D on the synth appeared to do nothing at all.
  //
  // The one thing the synth-initiated path will not do is interrupt a gesture
  // in progress: a slot press arriving mid-drag would cut a cable in half.
  const bool follow =
      bringOnScreen || juce::Desktop::getInstance().getNumDraggingMouseSources() == 0;

  if (follow) {
    if (!area.isSlotOpen(slot))
      area.openSlot(slot);
    area.focusSlot(slot);
  }

  if (slot == activeSlot) {
    mainLayout->getSlotBar().setCurrentTab(slot);
    if (notifySynth && connectionManager.isConnected()
        && connectionManager.getCurrentSlot() != slot) {
      stopInterpolation("synth slot realignment");
      notifySynthOfSlot(slot);
    }
    return;
  }

  // The synth streams lights and meters for one slot at a time. Zero the canvas
  // being left, or its LEDs stay frozen lit next to the live one.
  clearLightMeterData(activeSlot);

  // Interpolation entries reference module indices in the previously active patch.
  // Stop before activeSlot changes, otherwise the next timer tick could apply those
  // entries to the new slot and queue them for the wrong synth patch.
  stopInterpolation("slot switch");

  activeSlot = slot;
  mainLayout->getSlotBar().setCurrentTab(slot);

  // Tell synth to switch active slot (skip when the synth itself initiated the change)
  if (notifySynth)
    notifySynthOfSlot(slot);

  // The inspector points into the old slot's patch, so it has to let go before
  // activeSlot moves. It picks the new slot's own selection back up below.
  mainLayout->getInspector().clearModule();
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(nullptr);
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(nullptr);

  if (currentPatch()) {
    mainLayout->getHeaderBar().setPatch(currentPatch().get());
    mainLayout->getInspector().setPatch(currentPatch().get());
    if (knobFloaterWindow)
      knobFloaterWindow->setPatch(currentPatch().get());
    if (patchNotesFloaterWindow)
      patchNotesFloaterWindow->setPatch(currentPatch().get());
    // Adopt whatever this slot's canvas already had selected, rather than
    // leaving the inspector blank: each canvas keeps its selection while it is
    // in the background, so coming back to a slot should look like you left it.
    if (auto sel = canvasFor(slot).getPrimarySelection(); sel.module != nullptr)
      mainLayout->getInspector().setModule(sel.module, sel.section);
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

  // The store button belongs to the patch on screen, so it follows the slot.
  updateStoreLocationDisplay();

  std::cout << "[SLOT] Switched to slot " << slot << std::endl;
}

// Walking focus across four sub-windows must not spray SlotActivated messages
// down the wire, so the send is coalesced: only the last slot asked for within
// the window is actually told to the synth, and only if the synth is not
// already there.
void MainComponent::notifySynthOfSlot(int slot) {
  if (!connectionManager.isConnected())
    return;

  pendingSynthSlot = slot;
  const int generation = ++synthSlotGeneration;
  juce::Component::SafePointer<MainComponent> safeThis(this);
  juce::Timer::callAfterDelay(250, [safeThis, generation]() {
    if (!safeThis || safeThis->synthSlotGeneration != generation)
      return;  // superseded by a later focus change
    const int target = safeThis->pendingSynthSlot;
    if (target < 0 || !safeThis->connectionManager.isConnected())
      return;
    if (safeThis->connectionManager.getCurrentSlot() == target)
      return;  // the synth is already there
    safeThis->connectionManager.selectSlot(target);
  });
}

void MainComponent::clearLightMeterData(int slot) {
  if (slot < 0 || slot >= numSlots)
    return;
  static const int zeros[128] = {};
  canvasFor(slot).setLightMeterData(zeros, zeros);
}

void MainComponent::toggleFocusMode() {
  auto& area = mainLayout->getPatchArea();
  if (area.getNumOpenSlots() < 2) {
    mainLayout->getStatusBar().showMessage(
        "Focus mode needs a second slot open (" NME_SLOT_TOGGLE_CHORD "1..4)", 2500);
    return;
  }
  const bool on = !area.isFocusMode();
  area.setFocusMode(on);
  mainLayout->getStatusBar().showMessage(
      on ? "Focus mode on - F4 to go back to the tiling" : "Focus mode off", 2500);
}

void MainComponent::toggleSlotOpen(int slot) {
  if (slot < 0 || slot >= numSlots)
    return;

  auto& area = mainLayout->getPatchArea();
  if (area.isSlotOpen(slot)) {
    // Never close the last one: an empty work area looks like a broken editor,
    // and there would be nothing left for the shared surfaces to follow.
    if (area.getNumDocuments() > 1)
      area.closeSlot(slot);
    return;
  }

  switchToSlot(slot);
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

  // The patch about to be replaced may hold comments or variations that the
  // flush timer has not written yet.
  flushExtras(slot);

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
  slotPatches[slot] = std::move(patch);
  slotPatchFiles[slot] = sourceFile;
  // Whatever bank location this slot held belongs to the patch just replaced.
  connectionManager.clearSlotBankLocation(slot);
  slotBankCandidates[slot].clear();
  slotPatchFromSynth[slot] = false;

  // Re-point this slot's canvas immediately, before anything else can lay it
  // out or paint it: the Patch it was showing has just been destroyed by the
  // line above, so until this runs its modules are dangling pointers. In
  // particular switchToSlot() below brings the sub-window on screen and resizes
  // it, which reads the patch.
  canvasFor(slot).setPatch(slotPatches[slot].get(), &moduleDescs, &themeData);
  mainLayout->getPatchArea().getView(slot).setPatchTitle(slotPatches[slot]->getName());

  clearSnapshots(slot);

  if (loadVariations && sourceFile.existsAsFile()) {
    if (loadVarFile(variations[slot], sourceFile.withFileExtension("var"))) {
      for (auto& [section, moduleIndex] : variations[slot].mutationExcluded)
        if (auto* module = slotPatches[slot]->getContainer(section).getModuleByIndex(moduleIndex))
          module->setExcludedFromMutation(true);
    }
  }

  if (sourceFile.existsAsFile()) {
    // A file opened from disk is the authority on its own extras: it carries its
    // comments and notes, and its .var sidecar its variations. Binding tells the
    // library that this is the same patch, so the extras are found again when
    // the synth hands it back with none of them. A file with nothing of its own
    // falls back to the library, which is how a patch saved before any of this
    // existed picks its extras up.
    if (slotPatches[slot]->extrasId.isEmpty() && slotPatches[slot]->getComments().empty()
        && !variations[slot].anyFilled())
      attachExtrasFromLibrary(slot);
    else
      bindExtrasFromPatch(slot);
  } else {
    // A patch created here and now gets an entry of its own and is never
    // matched against anything: every brand new patch looks exactly like every
    // other brand new patch, and inheriting a stranger's comments because of
    // that would be the one way this whole mechanism could embarrass itself.
    slotPatches[slot]->extrasId = PatchExtrasStore::newId();
    bindExtrasFromPatch(slot);
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
    mainLayout->getHeaderBar().setPatch(slotPatches[slot].get());
    mainLayout->getInspector().setPatch(slotPatches[slot].get());
    updateStoreLocationDisplay();
    if (knobFloaterWindow)
      knobFloaterWindow->setPatch(slotPatches[slot].get());
    if (patchNotesFloaterWindow)
      patchNotesFloaterWindow->setPatch(slotPatches[slot].get());
    refreshSnapshotUi();
    updateDspLoadDisplay();
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
  if (slot >= 0 && slot < numSlots)
    canvasFor(slot).clearModuleSelection();
  if (slot == activeSlot)
    mainLayout->getInspector().clearModule();
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

  // Anything the outgoing patch had goes to the library first; the new one gets
  // an entry of its own rather than being matched against other empty patches.
  flushExtras(activeSlot);

  currentPatch() = std::make_unique<Patch>();
  currentPatch()->extrasId = PatchExtrasStore::newId();
  currentPatchFile() = juce::File();
  clearSnapshots(activeSlot);
  bindExtrasFromPatch(activeSlot);
  canvasFor(activeSlot).setPatch(currentPatch().get(), &moduleDescs, &themeData);
  mainLayout->getPatchArea().getView(activeSlot).setPatchTitle(currentPatch()->getName());
  mainLayout->getHeaderBar().setPatch(currentPatch().get());
  mainLayout->getInspector().setPatch(currentPatch().get());
  if (knobFloaterWindow)
    knobFloaterWindow->setPatch(currentPatch().get());
  if (patchNotesFloaterWindow)
    patchNotesFloaterWindow->setPatch(currentPatch().get());
  connectionManager.clearSlotBankLocation(activeSlot);
  updateStoreLocationDisplay();
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

// Ctrl+S saves the slot the keystroke came from, never "the" slot: with more
// than one canvas on screen there is no such thing (issue #22). The File menu
// passes activeSlot, which is what it means by "the current patch".
void MainComponent::saveSlotPatch(int slot) {
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr) return;
  if (slotPatchFiles[slot].existsAsFile()) {
    bool ok = saveSlotPatchToFile(slot, slotPatchFiles[slot]);
    mainLayout->getStatusBar().showMessage(
        ok ? "Saved: " + slotPatchFiles[slot].getFileName()
           : "ERROR: Failed to save: " + slotPatchFiles[slot].getFileName(),
        ok ? 3000 : 5000);
  } else {
    saveSlotPatchAs(slot);
  }
}

// What the Save dialog should open with typed in. A patch that has a file of
// its own suggests that file; every other one suggests its own patch name, so
// a patch pulled off the synth arrives at the dialog already called what the
// synth calls it instead of leaving the name field blank.
juce::File MainComponent::suggestedSaveFileForSlot(int slot, const juce::File& folder) const {
  if (slotPatchFiles[slot].existsAsFile()) return slotPatchFiles[slot];

  auto dir = folder.isDirectory()
                 ? folder
                 : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

  auto name = juce::File::createLegalFileName(slotPatches[slot]->getName().trim());
  if (name.isEmpty()) return dir;

  // Appended, not withFileExtension(): a patch name is free to hold a dot and
  // "Bass.2" must not be saved as "Bass.pch".
  if (!name.endsWithIgnoreCase(".pch")) name << ".pch";
  return dir.getChildFile(name);
}

void MainComponent::saveSlotPatchAs(int slot) {
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr) return;

  auto startFolder = editorOptions.getPatchesFolder();
  auto chooser = std::make_shared<juce::FileChooser>(
      "Save Patch As", suggestedSaveFileForSlot(slot, startFolder), "*.pch");

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
               : "ERROR: Failed to save: " + file.getFileName(),
            ok ? 3000 : 5000);
      });
}

void MainComponent::loadPatchFromFile(const juce::File &file, int targetSlot, bool localOnly) {
  PchFileIO io(moduleDescs);
  auto patch = io.readFile(file);

  if (patch == nullptr) {
    mainLayout->getStatusBar().showMessage("ERROR: Failed to load: " + file.getFileName(), 5000);
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
  canvasFor(activeSlot).setPatch(currentPatch().get(), &moduleDescs, &themeData);
  mainLayout->getPatchArea().getView(activeSlot).setPatchTitle(currentPatch()->getName());
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
  // Turned off in the editor options: every open goes to the slot on screen,
  // uploaded like any other, with no question asked (issue #59).
  if (!editorOptions.askSlotOnOpen) {
    loadPatchFromFile(file, activeSlot, /*localOnly=*/false);
    return;
  }

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
  mainLayout->getPatchArea().getView(slot).setLocal(local);
}

// Fetch a patch out of the synth's banks into one slot. Double-clicking in the
// browser aims at the active slot; the right-click menu names one, which is what
// makes four sub-windows worth having.
void MainComponent::loadBankPatchIntoSlot(int section, int position, int slot) {
  if (slot < 0 || slot >= numSlots)
    return;

  pendingBrowserLoadSlot = slot;

  // Show the destination, so the patch does not arrive somewhere off screen.
  // notifySynth is false deliberately: loadPatchFromBank carries the slot in its
  // SysEx envelope, so moving hardware focus first is unnecessary and only adds
  // competing ACKs to a fetch that is already in flight.
  switchToSlot(slot, /*notifySynth=*/false);

  std::cout << "[MAIN] Loading patch from browser: section=" << section
            << " pos=" << position
            << " targetSlot=" << juce::String::charToString(static_cast<char>('A' + slot))
            << std::endl;

  connectionManager.loadPatchFromBank(section, position, slot);
  updateStoreLocationDisplay();
  mainLayout->getPatchBrowser().setLoadedPatch(section, position);
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

  int slot = activeSlot;
  juce::Component::SafePointer<MainComponent> safeThis(this);

  // Open on the patch's own location, so storing it back where it came from is
  // just OK instead of hunting through nine banks of 99 every time.
  int initialSection = connectionManager.getSlotBankSection(slot);
  int initialPosition = connectionManager.getSlotBankPosition(slot);
  // No certain location, but positions with this patch's name: open on the
  // first of them, which beats bank 1 position 1 by a mile.
  if (initialSection < 0 && !slotBankCandidates[slot].empty()) {
    initialSection = slotBankCandidates[slot].front().first;
    initialPosition = slotBankCandidates[slot].front().second;
  }

  PatchLocationDialog::show(this, "Store Patch to Bank",
    connectionManager.getPatchList(), true, slot,
    [safeThis](const PatchLocationDialog::Result& result) {
      if (safeThis == nullptr || !result.confirmed) return;
      safeThis->sendStoreToBank(result.slot, result.section, result.position);
    },
    initialSection, initialPosition);
}

void MainComponent::quickStoreToBank() {
  if (!connectionManager.isConnected() || currentPatch() == nullptr)
    return;

  const int section = connectionManager.getSlotBankSection(activeSlot);
  const int position = connectionManager.getSlotBankPosition(activeSlot);

  // Nothing certain to overwrite: a new patch, one opened from a file, or one
  // the synth loaded from its front panel without saying from where. Ask.
  if (section < 0 || position < 0) {
    const auto& candidates = slotBankCandidates[activeSlot];
    if (!candidates.empty()) {
      juce::StringArray places;
      for (const auto& c : candidates)
        places.add(juce::String((c.first + 1) * 100 + c.second + 1));
      mainLayout->getStatusBar().showMessage(
          "\"" + currentPatch()->getName() + "\" is in " + juce::String(places.size())
          + " bank positions (" + places.joinIntoString(", ") + ") - pick one", 6000);
    }
    storePatchToBank();
    return;
  }

  sendStoreToBank(activeSlot, section, position);
}

void MainComponent::sendStoreToBank(int slot, int section, int position) {
  if (slot < 0 || slot >= numSlots || section < 0 || position < 0)
    return;

  const int location = (section + 1) * 100 + position + 1;
  StorePatchMessage msg(slot, section, position);
  connectionManager.sendRawSysEx(msg.toSysEx(slot));

  // The patch now lives here, which is what the next store offers by default,
  // and the shortlist of same-named positions stops mattering.
  connectionManager.setSlotBankLocation(slot, section, position);
  slotBankCandidates[slot].clear();
  if (slot == activeSlot)
    updateStoreLocationDisplay();
  mainLayout->getPatchBrowser().setLoadedPatch(section, position);

  const char* slotNames[] = {"A", "B", "C", "D"};
  mainLayout->getStatusBar().showMessage(
      "Stored slot " + juce::String(slotNames[slot])
      + " to bank location " + juce::String(location), 3000);
  std::cout << "[STORE] Slot " << slotNames[slot] << " -> bank location "
            << location << std::endl;
}

void MainComponent::inferSlotBankLocation(int slot) {
  if (slot < 0 || slot >= numSlots || slotPatches[slot] == nullptr)
    return;
  if (!slotPatchFromSynth[slot] || slotIsLocal[slot])
    return;
  if (connectionManager.getSlotBankSection(slot) >= 0)
    return;  // the synth already told us, and it is the better answer

  const auto name = slotPatches[slot]->getName();
  const char* slotLetters[] = {"A", "B", "C", "D"};
  auto matches = connectionManager.findPatchLocations(name);
  slotBankCandidates[slot] = matches;

  if (matches.size() != 1) {
    std::cout << "[STORE] Slot " << slotLetters[slot] << " patch \""
              << name.toStdString() << "\": "
              << (connectionManager.isPatchListLoaded()
                      ? (matches.empty()
                             ? std::string("no bank position carries that name")
                             : "the name is in " + std::to_string(matches.size())
                                   + " bank positions, asking rather than guessing")
                      : std::string("bank list not loaded yet"))
              << std::endl;
    if (slot == activeSlot)
      updateStoreLocationDisplay();
    return;
  }

  connectionManager.setSlotBankLocation(slot, matches[0].first, matches[0].second);
  slotBankCandidates[slot].clear();
  std::cout << "[STORE] Slot " << slotLetters[slot] << " patch \""
            << name.toStdString() << "\" matched bank location "
            << ((matches[0].first + 1) * 100 + matches[0].second + 1) << std::endl;
  if (slot == activeSlot)
    updateStoreLocationDisplay();
}

void MainComponent::updateStoreLocationDisplay() {
  auto& headerBar = mainLayout->getHeaderBar();
  headerBar.setStoreEnabled(connectionManager.isConnected());
  headerBar.setCurrentLocation(connectionManager.getSlotBankSection(activeSlot),
                               connectionManager.getSlotBankPosition(activeSlot));
  headerBar.setStoreUncertain(!slotBankCandidates[activeSlot].empty());
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
        safeThis->sendStoreToBank(slot, bankSection, position);
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

void MainComponent::showPatchSettingsDialog() {
  if (currentPatch() == nullptr)
    return;

  announceDialogOnSynth(
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
      }));
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
  announceDialogOnSynth(synthSettingsDialog);
}

void MainComponent::showMidiSettingsDialog() {
  announceDialogOnSynth(
      MidiSettingsDialog::show(
          this, lastInputId, lastOutputId, connectionManager.getStatus(),
          [this](const juce::String &inputId, const juce::String &outputId) {
            handleConnectionRequest(inputId, outputId);
          },
          [this]() { handleDisconnectionRequest(); }));
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
  auto* dialog =
      EditorOptionsDialog::show(this, editorOptions, mcpStatus, mcpStatusText, mcpCommand,
                                [this](const EditorOptions& opts) {
        applyEditorOptions(opts);
      });
  // Cable opacity is a look, not a number: the canvas follows the slider live
  // and the dialog itself puts the old value back unless OK is what closed it.
  dialog->onCableOpacityPreview = [](float v) {
    PatchCanvas::setCableOpacity(v);
    if (auto* topComp = juce::Desktop::getInstance().getComponent(0))
      topComp->repaint();
  };
  announceDialogOnSynth(dialog);
}

void MainComponent::applyEditorOptions(const EditorOptions& opts) {
  editorOptions = opts;
  auto libraryOk = editorOptions.ensureLibraryFolders();
  editorOptions.save(appProperties.getUserSettings());
  // Pointing the library somewhere else moves where presets are read and written.
  modulePresets.setFolder(presetsFolder());
  refreshInspectorPresets();
  PatchCanvas::setCableStyle   (static_cast<int>(opts.cableStyle));
  KnobDrag::setMode           (static_cast<int>(opts.knobControl));
  PatchCanvas::setAutoUpload   (opts.autoUpload);
  PatchCanvas::setCableOpacity (opts.cableOpacity);
  mainLayout->getPatchArea().setAnimated(opts.animateTiling);
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

// The icon bar is how long-time users of the original reach for a module, but
// it is screen furniture to anyone working from Quick Add, so it comes off the
// View menu and the choice is remembered (issue #17).
void MainComponent::toggleModuleIconBar() {
  if (!mainLayout) return;
  const bool show = !mainLayout->isModuleIconBarVisible();
  mainLayout->setModuleIconBarVisible(show);
  editorOptions.moduleIconBar = show;
  editorOptions.save(appProperties.getUserSettings());
  menuItemsChanged();
  mainLayout->getStatusBar().showMessage(
      show ? "Module icon bar shown" : "Module icon bar hidden (View menu to show)",
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
      importSnippetFromFile(activeSlot, file);
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

// Free-mode geometry is stored as fractions of the work area rather than in
// pixels, so a layout still means the same thing after a panel is collapsed, the
// window is resized or the editor moves to a different monitor. In Auto mode the
// tiling recomputes everything from the open-slot mask, so only the mask matters.
void MainComponent::saveMdiLayout() {
  if (restoringMdiLayout || syncingSlotWindows)
    return;
  auto* settings = appProperties.getUserSettings();
  if (settings == nullptr || mainLayout == nullptr)
    return;

  auto& area = mainLayout->getPatchArea();
  const bool freeMode = area.getTileMode() == SlotMdiArea::TileMode::Free;

  int openMask = 0;
  for (int i = 0; i < numSlots; ++i)
    if (area.isSlotOpen(i))
      openMask |= (1 << i);

  settings->setValue("mdiOpenSlots", openMask);
  settings->setValue("mdiFocusedSlot", activeSlot);
  settings->setValue("mdiFreeLayout", freeMode);
  settings->setValue("mdiFocusMode", area.isFocusMode());
  settings->setValue("mdiTileOrder", area.getTileOrderString());

  // Only in Free mode, and only for slots that are actually on screen: writing
  // zeroes for a closed slot would restore as an empty rectangle.
  if (freeMode)
    for (int i = 0; i < numSlots; ++i) {
      const auto key = "mdiSlot" + juce::String::charToString(static_cast<char>('A' + i));
      const auto bounds = area.getNormalisedSlotBounds(i);
      settings->setValue(key + "X", bounds.getX());
      settings->setValue(key + "Y", bounds.getY());
      settings->setValue(key + "W", bounds.getWidth());
      settings->setValue(key + "H", bounds.getHeight());
    }

  settings->saveIfNeeded();
}

void MainComponent::restoreMdiLayout() {
  // Whatever happens, stop suppressing saves on the way out — the flag starts
  // true so the constructor's own openSlot cannot overwrite the stored layout.
  struct Unsuppress {
    bool& flag;
    ~Unsuppress() { flag = false; }
  } unsuppress { restoringMdiLayout };

  auto* settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  auto& area = mainLayout->getPatchArea();

  // Which windows are open is the editor's own state and comes back as it was
  // left. Connecting reconciles it once against the synth's enabled slots
  // (reconcileSlotWindowsWithSynth), so a session started offline still opens
  // where the user left it rather than on an empty work area.
  area.setTileOrderString(settings->getValue("mdiTileOrder", "0123"));

  // Default to the slot that is already open, so a first run and a corrupt or
  // empty mask both land somewhere sensible rather than on an empty work area.
  int openMask = settings->getIntValue("mdiOpenSlots", 1 << activeSlot);
  if ((openMask & 0x0f) == 0)
    openMask = 1 << activeSlot;

  for (int i = 0; i < numSlots; ++i)
    if (openMask & (1 << i))
      area.openSlot(i);

  if (settings->getBoolValue("mdiFreeLayout", false)) {
    area.setTileMode(SlotMdiArea::TileMode::Free);
    for (int i = 0; i < numSlots; ++i) {
      const auto key = "mdiSlot" + juce::String::charToString(static_cast<char>('A' + i));
      area.setNormalisedSlotBounds(i,
          { (float) settings->getDoubleValue(key + "X"),
            (float) settings->getDoubleValue(key + "Y"),
            (float) settings->getDoubleValue(key + "W"),
            (float) settings->getDoubleValue(key + "H") });
    }
  }

  std::cout << "[MDI] Restored layout: openMask=" << openMask
            << " free=" << settings->getBoolValue("mdiFreeLayout", false)
            << " open=" << mainLayout->getPatchArea().getNumOpenSlots()
            << " tileOrder=" << mainLayout->getPatchArea().getTileOrderString() << std::endl;

  const int focused = settings->getIntValue("mdiFocusedSlot", activeSlot);
  if (focused >= 0 && focused < numSlots && (openMask & (1 << focused)))
    switchToSlot(focused, /*notifySynth=*/false);

  // Last, so it maximises the slot that ended up focused.
  if (settings->getBoolValue("mdiFocusMode", false))
    area.setFocusMode(true);
}

// The synth's enable mask is "pinned + selected", and with nothing pinned that
// is a single slot which changes on every slot press. Following it live would
// close every window but one the moment the synth echoed a slot change, so it
// is consulted exactly once per connection: the work area adopts the slots the
// Nord currently has enabled, and from then on opening and closing sub-windows
// is the user's business alone.
void MainComponent::scheduleSlotWindowReconcile() {
  if (slotWindowsReconciled || slotWindowsReconcileScheduled)
    return;

  slotWindowsReconcileScheduled = true;

  // SlotsSelected (the mask) and SlotActivated (the focused slot) are separate
  // messages arriving in either order during the connect handshake. Let both
  // land before reconciling, or a mask processed first pairs with a stale
  // focused slot and leaves a spurious window open for the rest of the session.
  juce::Component::SafePointer<MainComponent> safeThis(this);
  juce::Timer::callAfterDelay(400, [safeThis]() {
    if (safeThis == nullptr || !safeThis->slotWindowsReconcileScheduled)
      return;
    safeThis->slotWindowsReconcileScheduled = false;
    if (safeThis->slotEnableStateKnown)
      safeThis->reconcileSlotWindowsWithSynth(safeThis->lastEnabledSlots);
  });
}

void MainComponent::reconcileSlotWindowsWithSynth(const std::array<bool, 4>& enabled) {
  if (mainLayout == nullptr)
    return;

  slotWindowsReconciled = true;

  auto& area = mainLayout->getPatchArea();
  const int focused = juce::jlimit(0, numSlots - 1, connectionManager.getCurrentSlot());

  int desiredMask = 1 << focused;  // focused slot must always remain reachable
  for (int slot = 0; slot < numSlots; ++slot)
    if (enabled[static_cast<size_t>(slot)])
      desiredMask |= 1 << slot;

  int currentMask = 0;
  for (int slot = 0; slot < numSlots; ++slot)
    if (area.isSlotOpen(slot))
      currentMask |= 1 << slot;

  if (currentMask == desiredMask) {
    std::cout << "[MDI] Slot windows already match the synth: mask=" << desiredMask
              << std::endl;
    return;
  }

  {
    const juce::ScopedValueSetter<bool> syncGuard(syncingSlotWindows, true);
    // Reconciliation can close a window immediately after opening another one.
    // Animating that transient state leaves JUCE proxy snapshots alive while
    // their document windows are being removed.
    area.setAnimated(false);

    // Open first, so the work area is never momentarily empty while the stored
    // layout is being replaced by the synth's.
    {
      const juce::ScopedValueSetter<bool> focusGuard(inSlotFocusChange, true);
      for (int slot = 0; slot < numSlots; ++slot)
        if ((desiredMask & (1 << slot)) != 0 && !area.isSlotOpen(slot))
          area.openSlot(slot);
    }

    // Adopt hardware focus before closing anything; onSlotClosed is suppressed
    // by syncGuard, so this reconciliation never sends a slot command back.
    switchToSlot(focused, /*notifySynth=*/false, /*bringOnScreen=*/false);

    for (int slot = 0; slot < numSlots; ++slot)
      if ((desiredMask & (1 << slot)) == 0 && area.isSlotOpen(slot))
        area.closeSlot(slot);

    // A restored F11 state would leave the newly opened windows underneath the
    // maximised one, where they look lost. Reconciling changed the layout, so
    // show it.
    if (area.isFocusMode() && area.getNumOpenSlots() > 1)
      area.setFocusMode(false);

    // Slots opened here still need their saved Free geometry; Auto mode has
    // already tiled itself from desiredMask.
    if (area.getTileMode() == SlotMdiArea::TileMode::Free)
      if (auto* settings = appProperties.getUserSettings())
        for (int slot = 0; slot < numSlots; ++slot) {
          if (!area.isSlotOpen(slot)) continue;
          const auto key = "mdiSlot" + juce::String::charToString(static_cast<char>('A' + slot));
          area.setNormalisedSlotBounds(slot,
              { (float) settings->getDoubleValue(key + "X"),
                (float) settings->getDoubleValue(key + "Y"),
                (float) settings->getDoubleValue(key + "W"),
                (float) settings->getDoubleValue(key + "H") });
        }

    area.setAnimated(editorOptions.animateTiling);
  }

  saveMdiLayout();
  std::cout << "[MDI] Reconciled slot windows with the synth: mask=" << desiredMask
            << " focused=" << static_cast<char>('A' + focused)
            << " open=" << area.getNumOpenSlots() << std::endl;
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
  // The slotWindowA..D keys are deliberately not written any more: the slots
  // are sub-windows now, and their layout is persisted by phase 4 of the MDI
  // plan against the work area rather than against screen coordinates.
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
          canvasFor(activeSlot).repaintCanvas();
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
    repaintAllCanvases();
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
      repaintAllCanvases();
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
  repaintAllCanvases();
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

PatchCanvasComponent& MainComponent::canvasFor(int slot) {
  jassert(slot >= 0 && slot < numSlots);
  return mainLayout->getPatchArea().getCanvas(juce::jlimit(0, numSlots - 1, slot));
}

PatchCanvasComponent& MainComponent::activeCanvas() {
  return canvasFor(activeSlot);
}

void MainComponent::repaintAllCanvases() {
  mainLayout->getPatchArea().forEachCanvas(
      [](int, PatchCanvasComponent& c) { c.repaintCanvas(); });
}

// Editing wiring for one slot's canvas, called once per slot at startup for all
// four, on screen or not. Every action reads slotPatches[slot] /
// slotUndoManagers[slot] / slotUndoContexts[slot] directly rather than the
// currentPatch() / undoManager() / undoContext() accessors, which resolve
// through activeSlot.
//
// That is the point of the MDI change (docs/MDI_PLAN.md): once several canvases
// are visible there is no 1:1 relation between the canvas that fired a callback
// and the slot the user is looking at, so resolving through activeSlot stops
// being correct by construction. With the slot captured here instead, a
// sendParameter or an undo lands on the right slot wherever focus happens to be,
// which is also why the focus timing does not have to be won narrowly.
//
// The shared surfaces (inspector, header bar, browsers, status bar) stay wired
// once in the constructor against activeSlot; this function reaches them only
// through the `slot == activeSlot` guards below.
void MainComponent::wireSlotView(int slot) {
  auto& canvas = canvasFor(slot);

  auto patch   = [this, slot]() -> Patch* { return slotPatches[slot].get(); };
  auto ctx     = [this, slot]() -> UndoContext* { return slotUndoContexts[slot].get(); };
  auto undoMgr = [this, slot]() -> juce::UndoManager& { return slotUndoManagers[slot]; };

  // The cost meter lives on the shared header bar, so it only moves when this
  // slot is the one the header bar is showing.
  auto updateLoad = [this, slot]() {
    if (slot == activeSlot) updateDspLoadDisplay();
  };

  // Selection -> the shared inspector. Clicking a sub-window both focuses it and
  // selects in it, so the inspector follows; a programmatic selection change in
  // a slot the inspector is not bound to must leave it alone.
  canvas.setModuleSelectedCallback([this, slot](Module* module, int section) {
    if (slot != activeSlot) return;
    if (module) mainLayout->getInspector().setModule(module, section);
    else        mainLayout->getInspector().clearModule();
  });

  canvas.setPresetLibrary(&modulePresets);

  // Canvas -> synth (live parameter changes)
  canvas.setParameterChangeCallback([this, slot](int section, int moduleId, int parameterId, int value) {
    connectionManager.sendParameter(slot, section, moduleId, parameterId, value);
    if (slot == activeSlot) {
      // The inspector lists the selected module's values, so a knob turned on
      // the canvas has to read true there as it moves.
      mainLayout->getInspector().repaintValues();
      if (knobFloaterWindow && knobFloaterWindow->isVisible())
        knobFloaterWindow->refresh();
    }
  });

  // Canvas -> undo (structural edits). Parameter drags fire once on mouseUp
  // with old+new, so the whole drag is one undo step.
  canvas.setParameterDragCompleteCallback([ctx, undoMgr]
      (int section, int moduleId, int parameterId, int oldValue, int newValue) {
    if (!ctx()) return;
    undoMgr().beginNewTransaction("Parameter Change");
    undoMgr().perform(new ParameterChangeAction(*ctx(), section, moduleId, parameterId, oldValue, newValue));
  });
  // Display-only parameters (frequency units, sequencer zoom): stored in the
  // patch, never put on the wire.
  canvas.setCustomParameterChangeCallback([ctx, undoMgr]
      (int section, int moduleId, int parameterId, int oldValue, int newValue) {
    if (!ctx()) return;
    undoMgr().beginNewTransaction("Display Change");
    undoMgr().perform(new CustomParameterChangeAction(*ctx(), section, moduleId, parameterId, oldValue, newValue));
  });
  canvas.setModuleDropCallback([this, patch, ctx, undoMgr, updateLoad]
      (int typeId, int section, int gridX, int gridY, const juce::String& name) {
    if (!patch() || !ctx()) return;
    // A transient message, NOT setConnectionStatus: this label is the
    // persistent status line, so an error posted there outlived its moment
    // by whole sessions (issue #65).
    if (!undoMgr().perform(new AddModuleAction(*ctx(), section, typeId, gridX, gridY, name)))
      mainLayout->getStatusBar().showMessage(
          "ERROR: Failed to add module - check synth memory/limits", 6000);
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
  // Editor text notes. They exist only here, so none of these talk to the synth;
  // what they do instead is mark the slot's extras for writing to the library,
  // which is what lets the notes come back when the patch is read off the wire.
  canvas.setCommentAddCallback([this, slot, patch, ctx, undoMgr](int section, int gridX, int gridY) {
    if (!patch() || !ctx()) return;
    undoMgr().beginNewTransaction("Add Comment");
    undoMgr().perform(new AddCommentAction(*ctx(), section, gridX, gridY,
                                           PatchCanvas::commentDefaultHeight, juce::String(),
                                           PatchCanvas::commentDefaultWidth));
    markExtrasDirty(slot);
  });
  // Moving and deleting open no transaction of their own: a note can be moved
  // or deleted as part of a selection that also holds modules, and the canvas
  // opens one transaction for the whole gesture before calling either.
  canvas.setCommentMoveCallback([this, slot, patch, ctx, undoMgr]
      (int commentId, juce::Point<int> oldPos, juce::Point<int> newPos) {
    if (!patch() || !ctx()) return;
    undoMgr().perform(new MoveCommentAction(*ctx(), commentId, oldPos, newPos));
    markExtrasDirty(slot);
  });
  canvas.setCommentTextCallback([this, slot, patch, ctx, undoMgr]
      (int commentId, const juce::String& oldText, const juce::String& newText) {
    if (!patch() || !ctx()) return;
    undoMgr().beginNewTransaction("Edit Comment");
    undoMgr().perform(new EditCommentTextAction(*ctx(), commentId, oldText, newText));
    markExtrasDirty(slot);
  });
  // Pasting and duplicating create notes inside the transaction the paste has
  // already opened, so the whole block is one Ctrl+Z. That is the only thing
  // that sets this apart from the Add callback above.
  canvas.setCommentCreateCallback([this, slot, patch, ctx, undoMgr]
      (int section, int gridX, int gridY, int width, int height, const juce::String& text) {
    if (!patch() || !ctx()) return;
    undoMgr().perform(new AddCommentAction(*ctx(), section, gridX, gridY,
                                           height, text, width));
    markExtrasDirty(slot);
  });
  canvas.setCommentDeleteCallback([this, slot, patch, ctx, undoMgr](int commentId) {
    if (!patch() || !ctx()) return;
    undoMgr().perform(new DeleteCommentAction(*ctx(), commentId));
    markExtrasDirty(slot);
  });
  canvas.setCommentResizeCallback([this, slot, patch, ctx, undoMgr]
      (int commentId, juce::Rectangle<int> oldRect, juce::Rectangle<int> newRect) {
    if (!patch() || !ctx()) return;
    undoMgr().beginNewTransaction("Resize Comment");
    undoMgr().perform(new ResizeCommentAction(*ctx(), commentId, oldRect, newRect));
    markExtrasDirty(slot);
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
  canvas.setKnobAssignCallback([patch, ctx, undoMgr]
      (int section, int moduleId, int paramId, int knobIndex) {
    if (!patch() || !ctx()) return;
    int prevKnob = -1;
    for (int k = 0; k < 23; ++k) {
      auto& ka = patch()->knobAssignments[static_cast<size_t>(k)];
      if (ka.assigned && ka.section == section && ka.module == moduleId && ka.param == paramId)
      { prevKnob = k; break; }
    }
    if (knobIndex == prevKnob) return;  // no-op
    undoMgr().beginNewTransaction("Knob Assign");
    undoMgr().perform(new KnobAssignAction(*ctx(), section, moduleId, paramId, knobIndex, prevKnob));
  });
  canvas.setMidiCtrlAssignCallback([patch, ctx, undoMgr]
      (int section, int moduleId, int paramId, int midiCC) {
    if (!patch() || !ctx()) return;
    int prevCtrl = -1;
    for (auto& ca : patch()->ctrlAssignments)
      if (ca.section == section && ca.module == moduleId && ca.param == paramId)
      { prevCtrl = ca.control; break; }
    if (midiCC == prevCtrl) return;  // no-op
    undoMgr().beginNewTransaction("MIDI CC Assign");
    undoMgr().perform(new MidiCtrlAssignAction(*ctx(), section, moduleId, paramId, midiCC, prevCtrl));
  });
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

  canvas.setInitModuleCallback([this, slot](int section, Module* module) {
    initializeModule(slot, section, module);
  });

  canvas.setSnippetSaveCallback([this](SnipData snip) { saveSnippet(std::move(snip)); });
  canvas.setSnippetDropCallback([this, slot](const juce::File& file, int, int gridX, int gridY) {
    importSnippetFromFile(slot, file, gridX, gridY);
  });

  // Paste and Duplicate insert a block of modules the same way the snippet
  // browser does, so they go through the same undoable action.
  canvas.setSnippetInsertCallback(
      [patch, ctx, undoMgr, updateLoad](SnipData snip, int section, int offsetX, int offsetY) {
        std::vector<std::pair<int, int>> created;
        if (!patch() || !ctx()) return created;

        // perform() takes the action over and deletes it when it fails, so the
        // bare pointer is only good to read once it has reported success.
        auto* action = new InsertSnippetAction(*ctx(), std::move(snip),
                                              offsetX, offsetY, section);
        if (undoMgr().perform(action))
          created = action->getCreatedIndices();
        updateLoad();
        return created;
      });

  // Undo/redo keyboard shortcuts (Ctrl+Z / Ctrl+Shift+Z), scoped to this slot
  canvas.setUndoCallback([this, slot, updateLoad]() {
    runUndoRestoringSelection(slot, /*redo=*/false);
    updateLoad();
  });
  canvas.setRedoCallback([this, slot, updateLoad]() {
    runUndoRestoringSelection(slot, /*redo=*/true);
    updateLoad();
  });
  canvas.setUndoManager(&slotUndoManagers[slot]);

  canvas.setFileCommandCallback([this, slot](const juce::String& cmd) {
    handleSlotFileCommand(slot, cmd);
  });
}

// File/edit shortcuts arriving from a slot's canvas. The ones that name a patch
// act on that canvas's slot; the rest are editor-wide.
void MainComponent::handleSlotFileCommand(int slot, const juce::String& cmd) {
  // New and Open have no unambiguous per-canvas meaning — they ask "into which
  // slot?", which Open answers with its own chooser — so both stay on the
  // active slot.
  if (cmd == "new")   newPatch();
  else if (cmd == "open")  openPatch();
  else if (cmd == "save")   saveSlotPatch(slot);
  else if (cmd == "saveAs") saveSlotPatchAs(slot);
  else if (cmd == "randomize")         randomizeSlotParameters(slot, canvasFor(slot), false);
  else if (cmd == "randomizeGaussian") randomizeSlotParameters(slot, canvasFor(slot), true);
  else if (cmd == "patchSettings") showPatchSettingsDialog();
  else if (cmd == "synthSettings") showSynthSettingsDialog();
  else if (cmd == "presetBrowser") togglePresetBrowser();
  else if (cmd.startsWith("slot")) switchToSlot(cmd.getTrailingIntValue());
  else if (cmd.startsWith("floater")) {
    switch (cmd.getTrailingIntValue()) {
      case 0: toggleKnobFloater(); break;
      case 1: toggleKeyboardFloater(); break;
      case 2: togglePatchNotesFloater(); break;
      case 3: toggleMutatorWindow(); break;
    }
  }
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
  if (!connected) {
    // Reconciling with the synth is a once-per-connection thing, so losing the
    // connection arms it again for the next one.
    slotEnableStateKnown = false;
    slotWindowsReconciled = false;
    slotWindowsReconcileScheduled = false;
  }
  mainLayout->getStatusBar().setConnectionStatus(status.message, connected);
  menuItemsChanged(); // rebuild native macOS menu bar to update enabled states
  updateStoreLocationDisplay();  // storing needs a synth to store into

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

        titleLabel.setFont(juce::Font(AppTheme::uiFont(15.0f)).boldened());
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

        bodyText.setFont(juce::Font(AppTheme::uiFont(13.0f)));
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
      "  F5                  Parameter values overlay\n"
      "  F7                  Morph groups overlay\n"
      "  F8                  Knob assignments overlay\n"
      "  F9                  MIDI CC assignments overlay\n"
      "  F3 (or F10)         Module DSP cost overlay\n"
      "  Double-click module Module DSP cost\n"
      "  + / -               Step the control under the pointer\n"
      "  Z                   Zoom to selection / reset\n"
      "  Shift+Z             Reset zoom to 100%\n"
      "  Ctrl++ / Ctrl+-     Zoom in / out\n"
     #if JUCE_MAC
      "  Cmd+wheel, pinch    Zoom around the pointer\n"
     #else
      "  Ctrl+wheel          Zoom around the pointer\n"
     #endif
      "  Ctrl+T              Cycle color theme\n"
     #if JUCE_MAC
      "  Cmd+Shift+W         Toggle wireframe modules\n"
     #else
      "  Ctrl+W              Toggle wireframe modules\n"
     #endif
      "  Ctrl+I              Toggle inspector panel (left)\n"
      "  Ctrl+Shift+I        Toggle patch browser (right)\n"
      "  S                   Shake cables\n"
      "  Middle-drag         Pan canvas\n"
     #if JUCE_MAC
      "  Cmd-drag connector  Re-route: lift the cable off that end and drop it\n"
     #else
      "  Ctrl-drag connector Re-route: lift the cable off that end and drop it\n"
     #endif
      "                      on another connector (Alt works too)\n"
      "\n"
      "SLOTS\n"
      "  Ctrl+1..4           Switch to slot A..D (opens it if closed)\n"
      "  " NME_SLOT_TOGGLE_CHORD "1..4     Show/hide slot A..D's sub-window\n"
      "  F4 (or F11)         Focus mode: blow the focused slot up, and back\n"
      "  Maximise button     Same, on that sub-window's title bar\n"
      "  Ctrl+Shift+arrows   Move the focused slot to the neighbouring tile\n"
      "                      (up/down only exist in the four-slot 2x2)\n"
      "  Right-click slot    Show/hide that slot's sub-window\n"
      "  Ctrl+click slot     Enable/disable without selecting\n"
      "\n"
      "  Open slots tile themselves: one fills the area, two split it, three\n"
      "  go in thirds, four go 2x2. Dragging or resizing a sub-window leaves\n"
      "  the windows where you put them; View > Slots > Tile Slots re-flows.\n"
      "  The ABCD button (header bar, right of MUT) goes further and puts the\n"
      "  slots back in A,B,C,D order within the tiling.\n"
      "\n"
      "FLOATERS\n"
      "  Ctrl+5              Knob Floater\n"
      "  Ctrl+6              Keyboard Floater\n"
      "  Ctrl+7              Patch Notes\n"
      "  Ctrl+8              Patch Mutator\n"
      "  Ctrl+9              SysEx Monitor\n"
      "\n"
      "SUB-WINDOW (the one with focus)\n"
      "  Ctrl+R / Ctrl+Shift+R  Randomize (uniform / gaussian)\n"
      "  Ctrl+S / Ctrl+Shift+S  Save / Save as\n"
      "  These act on that window's own slot and selection.\n"
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
  announceDialogOnSynth(opts.launchAsync());
}

juce::String MainComponent::makeSynthCaption()
{
    // "ANME 0.17v": the editor and the version it is, and nothing about which
    // window is open. Naming the dialog was the first idea and it read as noise
    // on a display whose whole job is to tell you which patch you are on; what
    // is worth saying there is only that the editor has borrowed it.
    //
    // Ten characters, against a hard limit of fifteen: at sixteen the synth
    // hangs (see SetPatchTitleMessage, which truncates as a backstop). Fixed
    // rather than assembled from a label, so the limit cannot be reached at all.
    const juce::String version(JUCE_APPLICATION_VERSION_STRING);   // e.g. "0.17.0"
    const auto major = version.upToFirstOccurrenceOf(".", false, false);
    const auto minor = version.fromFirstOccurrenceOf(".", false, false)
                              .upToFirstOccurrenceOf(".", false, false);
    return ("ANME " + major + "." + minor + "v").substring(0, 15);
}

bool MainComponent::canBorrowSynthDisplay() const
{
    if (!editorOptions.synthDisplayCaptions || !connectionManager.isConnected())
        return false;

    // Never squeeze in between the messages of an edit or a transfer already on
    // the wire. The display is cosmetic; the wire is not, and a patch title goes
    // out unqueued.
    if (!connectionManager.isAckedQueueIdle()
        || connectionManager.isFetchingPatch()
        || connectionManager.isUploadingPatch())
        return false;

    return slotPatches[activeSlot] != nullptr;
}

void MainComponent::setSynthCaption()
{
    if (!canBorrowSynthDisplay())
        return;

    if (synthCaptionSlot < 0)
        synthCaptionSlot = activeSlot;

    connectionManager.sendPatchTitle(synthCaptionSlot, makeSynthCaption());
}

void MainComponent::clearSynthCaption()
{
    if (synthCaptionSlot < 0)
        return;

    const int slot = synthCaptionSlot;
    synthCaptionSlot = -1;

    // Deliberately not gated on canBorrowSynthDisplay(): whatever the wire is
    // doing and whatever the option now says, a borrowed name has to go back.
    // The one case with nothing to do is a synth that is no longer there, whose
    // edit buffer went with it.
    if (!connectionManager.isConnected())
        return;

    if (slotPatches[slot])
        connectionManager.sendPatchTitle(slot, slotPatches[slot]->getName());
}

void MainComponent::announceDialogOnSynth(juce::Component* dialog)
{
    if (dialog == nullptr || !editorOptions.synthDisplayCaptions)
        return;

    // One caption at a time. A dialog opened on top of another takes the display
    // over, and the last one to close is what puts the patch name back.
    if (synthCaptionWatcher.watched != nullptr)
        synthCaptionWatcher.watched->removeComponentListener(&synthCaptionWatcher);

    synthCaptionWatcher.watched = dialog;
    dialog->addComponentListener(&synthCaptionWatcher);
    setSynthCaption();
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
        // An edit to this slot always redraws this slot's own canvas, whether
        // or not it is the slot the shared surfaces are bound to. Those follow
        // the `slot == activeSlot` guard instead: repainting the header bar or
        // the inspector for a background edit would show the wrong patch's
        // numbers.
        // The load figures are recomputed here rather than at each call site:
        // repainting the header bar only redraws whatever load it was last told,
        // so a path that adds or deletes modules without its own explicit
        // update — the MCP bridge, the Mutator — left the meter reading the
        // previous patch's cost (issue #31).
        [this, slot]() {
            canvasFor(slot).repaintCanvas();
            if (slot == activeSlot) {
                mainLayout->getInspector().refreshMorphList();
                updateDspLoadDisplay();
                mainLayout->getHeaderBar().repaint();
                if (knobFloaterWindow)
                    knobFloaterWindow->refresh();
            }
        },
        // Values-only redraw. A parameter edit adds and removes nothing, so the
        // DSP figures and the morph/knob assignment list still read true; those
        // two walk the whole patch, and paying for them on every button press is
        // what made buttons feel heavier than knobs (issue #37).
        [this, slot]() {
            canvasFor(slot).repaintCanvas();
            if (slot == activeSlot) {
                mainLayout->getInspector().repaint();
                if (knobFloaterWindow)
                    knobFloaterWindow->refresh();
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
        // An undo put a deleted module back. Collected here rather than
        // selected one at a time: undoing a delete of several modules restores
        // them one action at a time, and the selection is meant to come back
        // whole, so the undo that drives them reads this list afterwards.
        [this](int section, int containerIndex) {
            restoredModules.push_back({ section, containerIndex });
        },
        slot
    });
}

// Undo and redo both run through here so that an undo which puts deleted
// modules back also puts the selection back on them. Restoring is what the
// undo of a delete does; the actions report each module as they re-insert it,
// and the selection is set once at the end, whole.
void MainComponent::runUndoRestoringSelection(int slot, bool redo) {
  if (slot < 0 || slot >= numSlots)
    return;

  restoredModules.clear();

  if (redo)
    slotUndoManagers[slot].redo();
  else
    slotUndoManagers[slot].undo();

  if (!restoredModules.empty()) {
    canvasFor(slot).selectRestored(restoredModules);
    restoredModules.clear();
  }
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

// ─── The extras library ──────────────────────────────────────────────────────
//
// The G1 stores modules, cables, values and names. Comments, patch notes, the
// eight variations and the Mutator's exclusions are the editor's own, and a
// patch read back from the synth arrives without a trace of them. They are kept
// in a small library beside the settings so the editor can put them back.

void MainComponent::attachExtrasFromLibrary(int slot) {
  if (slot < 0 || slot >= numSlots || !slotPatches[slot]) return;

  auto& patch = *slotPatches[slot];
  const auto fingerprint = patchFingerprint(patch);

  // The id the patch already carries wins: it is exact, where a fingerprint is
  // only a very good guess. A patch off the wire has no id, so it takes the
  // fingerprint route.
  auto* found = patchExtras.findById(patch.extrasId);
  if (found == nullptr)
    found = patchExtras.findByFingerprint(fingerprint);

  if (found == nullptr) {
    // Nothing known about this patch. Bind it to a fresh entry anyway, so that
    // the first comment written on it has somewhere to go.
    slotExtrasId[slot] = patch.extrasId.isNotEmpty() ? patch.extrasId
                                                     : PatchExtrasStore::newId();
    patch.extrasId = slotExtrasId[slot];
    auto& entry = patchExtras.obtain(slotExtrasId[slot]);
    entry.name = patch.getName();
    entry.rememberFingerprint(fingerprint);
    return;
  }

  patch.extrasId = found->id;
  slotExtrasId[slot] = found->id;
  found->rememberFingerprint(fingerprint);
  found->name = patch.getName();
  found->lastUsed = juce::Time::currentTimeMillis();

  // Not over the top of notes the patch already has: those came from the copy
  // that was in this slot a moment ago, which is more current than the library.
  if (patch.getComments().empty())
    patch.adoptComments(found->comments);
  if (patch.patchNotes.trim().isEmpty())
    patch.patchNotes = found->notes;

  variations[slot] = found->variations;
  for (auto& [section, moduleIndex] : variations[slot].mutationExcluded)
    if (auto* module = patch.getContainer(section).getModuleByIndex(moduleIndex))
      module->setExcludedFromMutation(true);

  if (slot == activeSlot) {
    refreshSnapshotUi();
    if (patchNotesFloaterWindow)
      patchNotesFloaterWindow->setPatch(&patch);
  }
  canvasFor(slot).repaint();

  markExtrasDirty(slot);
}

void MainComponent::bindExtrasFromPatch(int slot) {
  if (slot < 0 || slot >= numSlots || !slotPatches[slot]) return;

  auto& patch = *slotPatches[slot];

  // A patch opened from a file is the authority on its own extras: the file is
  // what the user just chose to open, comments deleted in it stay deleted. All
  // this does is make sure the library agrees with it from now on.
  if (patch.extrasId.isEmpty())
    patch.extrasId = PatchExtrasStore::newId();

  slotExtrasId[slot] = patch.extrasId;
  markExtrasDirty(slot);
  flushExtras(slot);
}

juce::int64 MainComponent::extrasRevision(int slot) const {
  if (slot < 0 || slot >= numSlots || !slotPatches[slot]) return 0;

  const auto& patch = *slotPatches[slot];

  // The fingerprint is part of it: renaming a patch or adding a module changes
  // nothing about its extras but changes how it will be recognised next time, so
  // the entry has to be rewritten to remember the new fingerprint too. Without
  // this, editing a patch and re-reading it from the synth would lose the link.
  juce::String summary;
  summary << "fp:" << patchFingerprint(patch) << "\n";
  for (const auto& c : patch.getComments())
    summary << c.section << ";" << c.x << ";" << c.y << ";" << c.gridWidth() << ";"
            << c.gridHeight() << ";" << c.text << "\n";
  summary << "notes:" << patch.patchNotes << "\n";

  const auto& vars = variations[slot];
  summary << "active:" << vars.activeIndex << "\n";
  for (int i = 0; i < PatchVariations::kNumSlots; ++i) {
    const auto& s = vars.slots[i];
    summary << "v" << i << ":" << (s.filled ? 1 : 0) << ":"
            << static_cast<int>(s.entries.size());
    // Values as well, so capturing a variation or letting the live write-through
    // change one is noticed. Summed rather than listed: this runs on a timer.
    juce::int64 sum = 0;
    for (const auto& e : s.entries)
      sum += static_cast<juce::int64>(e.value) * (e.paramId + 1);
    summary << ":" << juce::String(sum) << "\n";
  }
  for (int section = 0; section <= 1; ++section)
    for (const auto& modulePtr : patch.getContainer(section).getModules())
      if (modulePtr != nullptr && modulePtr->isExcludedFromMutation())
        summary << "x" << section << ":" << modulePtr->getContainerIndex() << "\n";

  return summary.hashCode64();
}

void MainComponent::flushExtras(int slot) {
  if (slot < 0 || slot >= numSlots) return;
  extrasDirty[slot] = false;
  lastExtrasRevision[slot] = extrasRevision(slot);

  if (!slotPatches[slot] || slotExtrasId[slot].isEmpty()) return;

  auto& patch = *slotPatches[slot];

  // Mutation exclusions live on the modules while a patch is open; collect
  // them back before writing, the way saving the .var sidecar does.
  variations[slot].mutationExcluded.clear();
  for (int section = 0; section <= 1; ++section)
    for (const auto& modulePtr : patch.getContainer(section).getModules())
      if (modulePtr != nullptr && modulePtr->isExcludedFromMutation())
        variations[slot].mutationExcluded.emplace_back(section,
                                                       modulePtr->getContainerIndex());

  auto& entry = patchExtras.obtain(slotExtrasId[slot]);
  entry.name = patch.getName();
  entry.lastUsed = juce::Time::currentTimeMillis();
  entry.rememberFingerprint(patchFingerprint(patch));
  entry.comments = patch.getComments();
  entry.notes = patch.patchNotes;
  entry.variations = variations[slot];

  patchExtras.write(entry);
}

void MainComponent::flushAllExtras() {
  for (int slot = 0; slot < numSlots; ++slot)
    if (extrasDirty[slot] || extrasRevision(slot) != lastExtrasRevision[slot])
      flushExtras(slot);
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
        canvasFor(activeSlot).repaintCanvas();
    }

    // Morph knob values (protocol: section=2, module=1, param=morphIndex)
    if (currentPatch()->morphValues != snap.morphValues) {
        currentPatch()->morphValues = snap.morphValues;
        if (connectionManager.isConnected())
            for (int m = 0; m < 4; ++m)
                connectionManager.queueParameter(activeSlot, 2, 1, m, snap.morphValues[static_cast<size_t>(m)]);
        mainLayout->getHeaderBar().repaint();
        refreshKnobFloater();
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
            "No free morph group to use as carrier: all 4 are in use", 4000);
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

    canvasFor(activeSlot).repaintCanvas();
    mainLayout->getHeaderBar().repaint();
    refreshKnobFloater();
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

void MainComponent::refreshKnobFloater() {
    if (knobFloaterWindow && knobFloaterWindow->isVisible())
        knobFloaterWindow->refresh();
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
    std::cout << "[SNAP] Interpolation START -> " << targetName
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

    canvasFor(activeSlot).repaintCanvas();
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
                ? "Interpolation complete: Variation " +
                      juce::String(interpolation.targetSnapshot + 1)
                : juce::String("Interpolation complete"), 2000);
    }
}

void MainComponent::initializeModule(int slot, int section, Module* module) {
  if (slot < 0 || slot >= numSlots) return;
  Patch* p = slotPatches[slot].get();
  UndoContext* uc = slotUndoContexts[slot].get();
  if (!module || !p || !uc) return;

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

  auto& undo = slotUndoManagers[slot];
  // Use the current transaction if one is already open (multi-select groups calls)
  if (!undo.isPerformingUndoRedo())
      undo.beginNewTransaction("Initialize Module");
  undo.perform(new RandomizeAction(*uc, std::move(changes)));
  canvasFor(slot).repaintCanvas();
  mainLayout->getStatusBar().showMessage(
      "Initialized " + module->getTitle(), 2000);
}

// Always slot-scoped (issue #22): Randomize acts on the patch of the canvas the
// keystroke came from and honours that canvas's own module selection. The Edit
// menu passes activeSlot.
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
  // that slot's canvas, so we don't repaint here explicitly.
  int numChanges = static_cast<int>(changes.size());
  slotUndoManagers[slot].beginNewTransaction("Randomize Parameters");
  slotUndoManagers[slot].perform(new RandomizeAction(*uc, std::move(changes)));

  juce::String scope = hasSelection ? " (selection)" : " (all)";
  mainLayout->getStatusBar().showMessage(
      "Randomized " + juce::String(numChanges) + " parameters" + scope
      + (gaussian ? " (Gaussian)" : " (Simple)"), 3000);
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

// ── Module presets ───────────────────────────────────────────────────────────

// Parameter lookup by the component-id a preset stores its values under.
static Parameter* findParamByComponentId(Module& m, const juce::String& componentId)
{
  for (auto& p : m.getParameters())
    if (p.getDescriptor() != nullptr && p.getDescriptor()->componentId == componentId)
      return &p;
  return nullptr;
}

void MainComponent::initModulePresetLibrary()
{
  // Presets that ship with the editor. They are registered here rather than
  // inside any one component so every surface reads the same library, and they
  // are never written to disk, so a user pack can never shadow or delete them.
  // Values are p1..p15 in descriptor order, the same way the original editor's
  // own presets get transcribed.
  struct BuiltIn { const char* type; const char* name; std::array<int, 15> values; };
  // The Drum Synthesizer's factory presets, exactly as Clavia shipped them.
  // Read out of a patch holding one module per preset rather than transcribed
  // from the original editor's readouts: the readouts are rounded, several sit
  // on top of each other, and a wrong digit in a decay time is invisible.
  // Clavia lists 30; "Tom2 3" is missing from the patch these came from.
  static const BuiltIn builtIns[] = {
      { "DrumSynth", "Kick 1",
        {  42,  15,  67,  71, 120, 102,  57,  32,  39,  70,   1,  68,  82,  79, 115 } },
      { "DrumSynth", "Kick 2",
        {  43,  26,  76,  74, 105,  94,  63,  18,  78,  55,   1,  76,  65,  25, 123 } },
      { "DrumSynth", "Kick 3",
        {  31,  71,  66,  54, 127,   0,  90,  24,  71,  41,   0,  84,  63,  81, 112 } },
      { "DrumSynth", "Kick 4",
        {  32,  61,  79,  63, 113, 110,  90,   0,   0,  41,   1,  37,  76,  90, 127 } },
      { "DrumSynth", "Kick 5",
        {  36,  39,  71,  73, 104,  92, 111,   0,  40,  49,   0,  34,  88,  79,  69 } },
      { "DrumSynth", "Snare 1",
        {  79,  55,  54,  64, 127,  42, 102,  46,   0,  57,   2,   2,   0, 127, 127 } },
      { "DrumSynth", "Snare 2",
        {  68,   3,  69,  63,  84, 127,  55,  66,  63,  60,   2,  63,  58, 126, 122 } },
      { "DrumSynth", "Snare 3",
        {  64,  23,  56,  65, 120,  84,  26,   0,  19,  65,   2,  24,  65, 127,  98 } },
      { "DrumSynth", "Snare 4",
        {  68,  57,  68,  52, 105,   0,  91,  28,  42,  63,   0,  81,  65, 112, 127 } },
      { "DrumSynth", "Snare 5",
        {  85, 107,  49,  31, 127,  94,   0,   0,  63,  66,   2,  39,  76, 102,  98 } },
      { "DrumSynth", "Tom1 1",
        {  80,  38,  77,  68, 102,  58,  98,  26,  27,  71,   0,  33,  89, 105,  97 } },
      { "DrumSynth", "Tom1 2",
        {  69,  38,  78,  73, 102,  58,  96,  26,  27,  72,   0,  33,  89, 105,  89 } },
      { "DrumSynth", "Tom1 3",
        {  56,  38,  79,  73, 102,  58,  93,  26,  27,  74,   0,  33,  89, 105,  89 } },
      { "DrumSynth", "Tom2 1",
        {  86,   2,  76,  65,  99,   0,  67,   0,  56,  76,   0,  86,  86,  81, 117 } },
      { "DrumSynth", "Tom2 2",
        {  69,   2,  77,  66,  99,   0,  67,   0,  56,  76,   0,  86,  86,  81, 117 } },
      // Tom2 3 was missing from the patch the rest came from; read off the
      // original editor instead. Every value it shares with Tom2 1 and Tom2 2,
      // which differ from each other only in tuning and the two decays, is
      // confirmed by both.
      { "DrumSynth", "Tom2 3",
        {  55,   2,  79,  70,  99,   0,  67,   0,  56,  78,   0,  86,  86,  81, 117 } },
      { "DrumSynth", "Tom3 1",
        {  70,  28,  64,  72, 113,  81, 102,   4,  12,  68,   0,  44,  67,  96,  97 } },
      { "DrumSynth", "Tom3 2",
        {  58,  28,  67,  74, 113,  81, 102,   4,  12,  68,   0,  44,  78,  93,  97 } },
      { "DrumSynth", "Tom3 3",
        {  48,  28,  73,  75, 108,  81, 102,   4,  12,  68,   0,  44,  87,  96, 103 } },
      { "DrumSynth", "Cymb 1",
        { 127,  93,   0,   0,   0, 102,  91,   0,  26,  67,   2, 127,   0,   0, 127 } },
      { "DrumSynth", "Cymb 2",
        { 127,  93,   0,   0,   0, 102,  91,  50,  16,  76,   2, 127,   0,   0, 127 } },
      { "DrumSynth", "Cymb 3",
        { 127, 127,  26,   0,  28,  58, 112,  60,   0,  71,   2,   0,   0,  81, 107 } },
      { "DrumSynth", "Cymb 4",
        { 127, 127,   0,   0,   0, 127, 102, 107,   0,  74,   2, 127,   0,   0, 127 } },
      { "DrumSynth", "Cymb 5",
        {  28, 111,  63,  86,  73,  28,  83,  57,  37,  91,   0,  83,  71, 109, 123 } },
      { "DrumSynth", "Perc 1",
        {  77,  32,  48,  66, 117,  93,  32,  32,  40,  66,   2, 110, 126, 116,  30 } },
      { "DrumSynth", "Perc 2",
        { 127,  99,  73,  69,  86,  71, 110, 120,   0,  66,   2,  47,  20,  49, 114 } },
      { "DrumSynth", "Perc 3",
        {  96,  60,  56,  63, 127,  72,  32,  32,  40,  66,   2,  46,  37,  89,  46 } },
      { "DrumSynth", "Perc 4",
        {  87,  60,  56,  63, 127,  72,  32,  32,  40,  66,   2,  46,  37,  89,  46 } },
      { "DrumSynth", "Perc 5",
        { 110,  93,  79,  89,  92,  79,  92, 127,   0,  79,   0,   0,   0, 127,  89 } },
      { "DrumSynth", "Perc 6",
        {  30,  45,  83,  67, 127, 127,  55, 127,  28,  62,   2,  81,  76,  80,  97 } },
  };

  for (const auto& b : builtIns) {
    ModulePreset preset;
    preset.name = b.name;
    preset.moduleType = b.type;
    for (size_t i = 0; i < b.values.size(); ++i)
      preset.values["p" + juce::String(static_cast<int>(i) + 1)] = b.values[i];
    modulePresets.addBuiltIn(std::move(preset));
  }

  modulePresets.setFolder(presetsFolder());
  modulePresets.migrateLegacyDrumPresets(appConfigFolder().getChildFile("drum_presets.txt"));
}

juce::File MainComponent::appConfigFolder()
{
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("AnimatekNME");
}

// Presets belong in the shared library when there is one, since that is what
// gets backed up and passed around. Without a library folder configured they
// still have to work, so they fall back beside the settings, which is where the
// DrumSynth presets lived before they became a library.
juce::File MainComponent::presetsFolder() const
{
  auto configured = editorOptions.getPresetsFolder();
  return configured != juce::File() ? configured : appConfigFolder().getChildFile("Presets");
}

void MainComponent::wirePresetCallbacks(InspectorPanel& inspector, int slot)
{
  inspector.setPresetLibrary(&modulePresets);
  // The module faces, so the Parameters list draws a module's buttons as
  // buttons rather than as the bare numbers behind them.
  inspector.setThemeData(&themeData);

  inspector.onPresetRecall = [this, slot](int section, Module* module, int index) {
    recallModulePreset(slot < 0 ? activeSlot : slot, section, module, index);
  };

  inspector.onPresetSave = [this](int, Module* module) {
    if (module == nullptr || module->getDescriptor() == nullptr) return;
    if (!modulePresets.canSave()) {
      mainLayout->getStatusBar().showMessage(
          "Choose a preset library folder in Editor Options to save module presets", 5000);
      return;
    }
    const auto type = module->getDescriptor()->name;
    auto preset = ModulePresetLibrary::capture(*module, modulePresets.suggestName(type));
    if (modulePresets.add(std::move(preset)) < 0)
      mainLayout->getStatusBar().showMessage("ERROR: could not write the preset pack", 5000);
    else
      mainLayout->getStatusBar().showMessage("Saved preset for " + type, 3000);
    refreshInspectorPresets();
  };

  inspector.onPresetDelete = [this](int, Module* module, int index) {
    if (module == nullptr || module->getDescriptor() == nullptr) return;
    modulePresets.remove(module->getDescriptor()->name, index);
    refreshInspectorPresets();
  };

  inspector.onPresetRename = [this](int, Module* module, int index) {
    if (module == nullptr || module->getDescriptor() == nullptr) return;
    const auto type = module->getDescriptor()->name;
    const auto* preset = modulePresets.find(type, index);
    if (preset == nullptr) return;

    auto* dialog = new juce::AlertWindow("Rename Preset", "Preset name:",
                                         juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", preset->name, "");
    dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog, type, index](int r) {
          if (r == 1)
          {
            modulePresets.rename(type, index, dialog->getTextEditorContents("name"));
            refreshInspectorPresets();
          }
          delete dialog;
        }), true);
  };
}

void MainComponent::recallModulePreset(int slot, int section, Module* module, int presetIndex)
{
  if (module == nullptr || module->getDescriptor() == nullptr) return;
  auto* ctx = getSlotUndoContext(slot);
  if (ctx == nullptr) return;

  const auto* preset = modulePresets.find(module->getDescriptor()->name, presetIndex);
  if (preset == nullptr) return;

  // One transaction for the whole recall, so undoing it is a single Ctrl+Z
  // rather than one per parameter the preset happened to change.
  auto& undo = getSlotUndoManager(slot);
  undo.beginNewTransaction("Recall Preset: " + preset->name);

  for (const auto& [componentId, value] : preset->values) {
    auto* param = findParamByComponentId(*module, componentId);
    if (param == nullptr) continue;
    const int oldValue = param->getValue();
    if (oldValue == value) continue;
    undo.perform(new ParameterChangeAction(*ctx, section, module->getContainerIndex(),
                                           param->getDescriptor()->index, oldValue, value));
  }

  // The module may draw its own preset name (the DrumSynth does), which has to
  // follow a recall made from the Inspector rather than from the module itself.
  canvasFor(slot).notePresetRecalled(section, module->getContainerIndex(), presetIndex);
}

void MainComponent::refreshInspectorPresets()
{
  mainLayout->getInspector().refreshMorphList();
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
        importSnippetFromFile(activeSlot, result);
      });
}

void MainComponent::importSnippetFromFile(int slot, const juce::File& file)
{
  importSnippetFromFile(slot, file, 3, 3);
}

void MainComponent::importSnippetFromFile(int slot, const juce::File& file,
                                          int targetGridX, int targetGridY)
{
  if (slot < 0 || slot >= numSlots) return;
  UndoContext* uc = slotUndoContexts[slot].get();
  if (!slotPatches[slot] || !uc) return;
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

  slotUndoManagers[slot].beginNewTransaction("Import Snippet");
  slotUndoManagers[slot].perform(new InsertSnippetAction(
      *uc, std::move(snip), targetGridX - minX, targetGridY - minY));

  mainLayout->getStatusBar().showMessage(
      "Snippet imported from " + file.getFileName(), 3000);
}
