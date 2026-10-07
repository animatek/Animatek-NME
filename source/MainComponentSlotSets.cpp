// Slot sets: File > Save Slot Set / Open Slot Set and the Disk tab's SET
// entries. The model and its manifest are in model/SlotSet; this file is the
// part that touches the slots and the synth, kept out of MainComponent.cpp,
// which is long enough already.
#include "MainComponent.h"
#include "ui/SaveSlotSetDialog.h"

namespace
{
const char* const kSlotLetters[] = { "A", "B", "C", "D" };

// A set's patches go in one at a time. The editor refuses a new patch while a
// transfer is in flight, and uploads chasing each other are how B used to time
// out when four layers were loaded by hand. So the loader waits for each ACK,
// or for the wire to fall quiet without one, lets the synth settle, and only
// then moves on.
constexpr int kTickMs = 100;
constexpr int kSettleTicks = 3;          // after an upload, before the next step
constexpr int kQuietTicks = 6;           // upload over this long with no ACK = it failed
constexpr int kStepTimeoutTicks = 600;   // a minute for any single step

void showSetMessage (juce::Component* parent, juce::MessageBoxIconType icon,
                     const juce::String& title, const juce::String& message)
{
  juce::AlertWindow::showMessageBoxAsync(icon, title, message, "OK", parent);
}
}

void MainComponent::saveSlotSet() {
  if (editorOptions.presetLibraryRoot == juce::File()) {
    showSetMessage(this, juce::MessageBoxIconType::InfoIcon, "Save Slot Set",
                   "Slot sets are kept in the preset library. Choose a library folder first.");
    return;
  }

  // A slot set is the four slots of one synth: the one being edited.
  const int base = SynthSlot::global(synthHub.activeSynth(), 0);
  const bool maskKnown = synthHub.active().isConnected() && activeState().enableStateKnown;
  std::array<SaveSlotSetDialog::SlotInfo, 4> info;
  for (int i = 0; i < kSlotsPerSynth; ++i) {
    auto& s = info[static_cast<size_t>(i)];
    s.hasPatch = slotPatches[base + i] != nullptr;
    if (s.hasPatch)
      s.patchName = slotPatches[base + i]->getName();
    s.enabledKnown = maskKnown;
    s.enabled = activeState().lastEnabled[static_cast<size_t>(i)];
  }

  juce::Component::SafePointer<MainComponent> safeThis(this);
  SaveSlotSetDialog::show(this, {}, info, [safeThis](const SaveSlotSetDialog::Result& r) {
    if (safeThis && r.confirmed)
      safeThis->writeSlotSet(r.name, r.notes, r.modes, false);
  });
}

void MainComponent::writeSlotSet(const juce::String& name, const juce::String& notes,
                                 const std::array<SlotSet::Mode, 4>& modes, bool overwrite) {
  const auto folderName = slotSetFolderName(name);
  if (folderName.isEmpty()) {
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Save Slot Set",
                   "\"" + name + "\" cannot be used as a folder name.");
    return;
  }

  editorOptions.ensureLibraryFolders();
  const auto folder = editorOptions.getSetsFolder().getChildFile(folderName);
  const auto manifest = folder.getChildFile(folderName + kSlotSetExtension);

  if (manifest.existsAsFile() && !overwrite) {
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Save Slot Set")
            .withMessage("A slot set called \"" + folderName + "\" already exists. Replace it?")
            .withButton("Replace")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [safeThis, name, notes, modes](int result) {
          if (safeThis && result == 1)
            safeThis->writeSlotSet(name, notes, modes, true);
        });
    return;
  }

  // What the set being replaced named, so the files it no longer uses go with it.
  juce::StringArray previousFiles;
  if (manifest.existsAsFile()) {
    SlotSet previous;
    juce::String ignored;
    if (slotSetFromJson(manifest.loadFileAsString(), previous, ignored))
      for (const auto& s : previous.slots)
        if (s.mode == SlotSet::Mode::Patch)
          previousFiles.addIfNotAlreadyThere(s.file);
  }

  if (!folder.createDirectory().wasOk()) {
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Save Slot Set",
                   "Could not create " + folder.getFullPathName());
    return;
  }

  SlotSet set;
  set.name = name.trim();
  set.notes = notes;
  const int base = SynthSlot::global(synthHub.activeSynth(), 0);
  const bool maskKnown = synthHub.active().isConnected() && activeState().enableStateKnown;
  juce::StringArray failed;
  for (int i = 0; i < kSlotsPerSynth; ++i) {
    auto& s = set.slots[static_cast<size_t>(i)];
    s.mode = modes[static_cast<size_t>(i)];
    if (s.mode != SlotSet::Mode::Patch)
      continue;
    if (slotPatches[base + i] == nullptr) {   // emptied while the dialog was open
      s.mode = SlotSet::Mode::Keep;
      continue;
    }

    s.patchName = slotPatches[base + i]->getName();
    s.file = slotSetPatchFileName(i, s.patchName);
    s.enabled = maskKnown ? activeState().lastEnabled[static_cast<size_t>(i)] : true;
    s.bankSection = synthHub.getSlotBankSection(base + i);
    s.bankPosition = synthHub.getSlotBankPosition(base + i);

    const auto file = folder.getChildFile(s.file);
    // saveSlotPatchToFile only writes a .var when there is something in it, so
    // one left by an earlier save of this set would come back with the patch.
    file.withFileExtension("var").deleteFile();
    if (!saveSlotPatchToFile(base + i, file))
      failed.add(kSlotLetters[i]);
  }

  if (!failed.isEmpty()) {
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Save Slot Set",
                   "Could not write the patch for slot " + failed.joinIntoString(", ")
                       + ". The set was not saved.");
    return;
  }
  if (!set.changesAnything()) {
    showSetMessage(this, juce::MessageBoxIconType::InfoIcon, "Save Slot Set",
                   "Nothing left to save: every slot would be left as it is.");
    return;
  }

  if (set.slots[static_cast<size_t>(SynthSlot::localOf(activeSlot))].mode == SlotSet::Mode::Patch)
    set.focusSlot = SynthSlot::localOf(activeSlot);
  else
    for (int i = 0; i < kSlotsPerSynth && set.focusSlot < 0; ++i)
      if (set.slots[static_cast<size_t>(i)].mode == SlotSet::Mode::Patch)
        set.focusSlot = i;

  if (!manifest.replaceWithText(slotSetToJson(set), false, false, "\n")) {
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Save Slot Set",
                   "Could not write " + manifest.getFullPathName());
    return;
  }

  for (const auto& old : previousFiles) {
    bool stillUsed = false;
    for (const auto& s : set.slots)
      stillUsed = stillUsed || (s.mode == SlotSet::Mode::Patch && s.file == old);
    if (!stillUsed && isSafeSlotSetFileName(old)) {
      folder.getChildFile(old).deleteFile();
      folder.getChildFile(old).withFileExtension("var").deleteFile();
    }
  }

  mainLayout->getDiskPresetBrowser().refresh();
  if (presetBrowserWindow)
    presetBrowserWindow->refresh();

  juce::StringArray parts;
  for (int i = 0; i < kSlotsPerSynth; ++i) {
    const auto mode = set.slots[static_cast<size_t>(i)].mode;
    if (mode == SlotSet::Mode::Patch)
      parts.add(kSlotLetters[i]);
    else if (mode == SlotSet::Mode::Disable)
      parts.add(juce::String(kSlotLetters[i]) + " off");
  }
  mainLayout->getStatusBar().showMessage(
      "Saved slot set \"" + set.name + "\" (" + parts.joinIntoString(", ") + ")", 4000);
}

void MainComponent::openSlotSetWithChooser() {
  auto start = editorOptions.getSetsFolder();
  if (!start.isDirectory())
    start = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

  auto chooser = std::make_shared<juce::FileChooser>(
      "Open Slot Set", start, juce::String("*") + kSlotSetExtension);
  juce::Component::SafePointer<MainComponent> safeThis(this);
  chooser->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [safeThis, chooser](const juce::FileChooser& fc) {
        const auto file = fc.getResult();
        if (safeThis && file.existsAsFile())
          safeThis->loadSlotSet(file);
      });
}

// Loading replaces up to four patches at once, so unlike auditioning a single
// patch it asks first, and says what goes where and what it replaces.
void MainComponent::loadSlotSet(const juce::File& manifest) {
  if (slotSetLoad.active) {
    mainLayout->getStatusBar().showMessage("A slot set is still loading", 3000);
    return;
  }

  SlotSet set;
  juce::String error;
  if (!manifest.existsAsFile())
    error = "File not found";
  else
    slotSetFromJson(manifest.loadFileAsString(), set, error);
  if (error.isNotEmpty()) {
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Open Slot Set",
                   manifest.getFileName() + ": " + error);
    return;
  }
  if (set.name.isEmpty())
    set.name = manifest.getFileNameWithoutExtension();
  if (!set.changesAnything()) {
    showSetMessage(this, juce::MessageBoxIconType::InfoIcon, "Open Slot Set",
                   "\"" + set.name + "\" leaves every slot as it is: there is nothing to load.");
    return;
  }

  const auto folder = manifest.getParentDirectory();
  const int base = SynthSlot::global(synthHub.activeSynth(), 0);
  juce::String plan;
  juce::StringArray missing;
  for (int i = 0; i < kSlotsPerSynth; ++i) {
    const auto& s = set.slots[static_cast<size_t>(i)];
    plan << kSlotLetters[i] << "   ";
    if (s.mode == SlotSet::Mode::Keep) {
      plan << "left as is";
    } else if (s.mode == SlotSet::Mode::Disable) {
      plan << "switched off";
    } else {
      plan << (s.patchName.isNotEmpty() ? s.patchName : s.file);
      if (!s.enabled)
        plan << " (loaded, off)";
      if (slotPatches[base + i] != nullptr)
        plan << "   replaces " << slotPatches[base + i]->getName();
      if (!folder.getChildFile(s.file).existsAsFile())
        missing.add(juce::String(kSlotLetters[i]) + "  " + s.file);
    }
    plan << "\n";
  }

  // Half a set is not the sound the set was saved as.
  if (!missing.isEmpty()) {
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Open Slot Set",
                   "\"" + set.name + "\" was not loaded. Its folder is missing:\n\n"
                       + missing.joinIntoString("\n"));
    return;
  }

  if (!synthHub.active().isConnected())
    plan << "\nNot connected: the patches load into the editor only (LOCAL), and no slot is "
            "switched on or off.\n";
  if (set.notes.isNotEmpty())
    plan << "\n" << set.notes;

  juce::Component::SafePointer<MainComponent> safeThis(this);
  juce::AlertWindow::showAsync(
      juce::MessageBoxOptions()
          .withIconType(juce::MessageBoxIconType::QuestionIcon)
          .withTitle("Load slot set \"" + set.name + "\"?")
          .withMessage(plan)
          .withButton("Load")
          .withButton("Cancel")
          .withAssociatedComponent(this),
      [safeThis, set, folder](int result) {
        if (safeThis && result == 1)
          safeThis->startSlotSetLoad(set, folder);
      });
}

void MainComponent::startSlotSetLoad(const SlotSet& set, const juce::File& folder) {
  if (slotSetLoad.active)
    return;

  slotSetLoad = SlotSetLoad{};
  slotSetLoad.active = true;
  slotSetLoad.synth = synthHub.activeSynth();   // the whole load stays on this synth
  slotSetLoad.set = set;
  slotSetLoad.folder = folder;
  for (int i = 0; i < kSlotsPerSynth; ++i)
    if (set.slots[static_cast<size_t>(i)].mode == SlotSet::Mode::Patch)
      slotSetLoad.pending.push_back(i);

  if (!slotSetLoadTimer) {
    struct SlotSetTimer : public juce::Timer {
      MainComponent& mc;
      explicit SlotSetTimer(MainComponent& m) : mc(m) {}
      void timerCallback() override { mc.slotSetLoadTick(); }
    };
    slotSetLoadTimer = std::make_unique<SlotSetTimer>(*this);
  }
  slotSetLoadTimer->startTimer(kTickMs);
  mainLayout->getStatusBar().showMessage("Loading slot set \"" + set.name + "\"...", 0);
}

void MainComponent::slotSetLoadTick() {
  auto& load = slotSetLoad;
  if (!load.active) {
    if (slotSetLoadTimer)
      slotSetLoadTimer->stopTimer();
    return;
  }
  auto& synthConn = synthHub.synth(load.synth);
  const int base = SynthSlot::global(load.synth, 0);
  const bool connected = synthConn.isConnected();

  switch (load.stage) {
  case SlotSetLoad::Stage::Patches: {
    // A load in flight: wait for its ACK, or for the upload to end without one.
    if (load.current >= 0) {
      const int slot = load.current;
      const juce::String letter = kSlotLetters[slot];
      if (!load.uploading || load.uploadAcked) {
        load.loaded.add(letter);
        load.current = -1;
      } else if (!connected) {
        load.failed.add(letter + ": the connection dropped during the upload");
        load.current = -1;
      } else if (!synthConn.isUploadingPatch()) {
        // The ACK callback is posted after the upload flag drops, so give it a
        // moment before calling the upload lost.
        if (++load.quietTicks >= kQuietTicks) {
          load.failed.add(letter + ": the synth did not acknowledge the upload");
          load.current = -1;
        }
      } else if (++load.stepTicks >= kStepTimeoutTicks) {
        load.failed.add(letter + ": the upload timed out");
        load.current = -1;
      }

      if (load.current < 0 && load.uploading) {
        synthConn.setUploadCompleteCallback(nullptr);
        load.settleTicks = kSettleTicks;
        if (!load.uploadAcked)
          setSlotLocal(base + slot, true);   // the editor has it, the synth does not
      }
      return;
    }

    if (load.settleTicks > 0) {
      --load.settleTicks;
      return;
    }
    if (load.pending.empty()) {
      load.stage = SlotSetLoad::Stage::Focus;
      return;
    }
    if (connected && (synthConn.isUploadingPatch() || synthConn.isFetchingPatch() || !synthConn.isAckedQueueIdle())) {
      if (++load.stepTicks >= kStepTimeoutTicks) {
        for (int slot : load.pending)
          load.failed.add(juce::String(kSlotLetters[slot]) + ": the synth stayed busy");
        load.pending.clear();
      }
      return;
    }

    const int slot = load.pending.front();
    load.pending.erase(load.pending.begin());
    const auto& entry = load.set.slots[static_cast<size_t>(slot)];

    load.current = slot;
    load.uploading = connected;
    load.uploadAcked = false;
    load.quietTicks = 0;
    load.stepTicks = 0;

    if (connected) {
      juce::Component::SafePointer<MainComponent> safeThis(this);
      synthConn.setUploadCompleteCallback([safeThis, slot]() {
        if (safeThis && safeThis->slotSetLoad.active && safeThis->slotSetLoad.current == slot)
          safeThis->slotSetLoad.uploadAcked = true;
      });
    }

    mainLayout->getStatusBar().showMessage(
        "Slot set \"" + load.set.name + "\": loading " + kSlotLetters[slot] + "  "
            + entry.patchName + "...", 0);

    juce::String error;
    if (!loadPatchFileIntoSlot(base + slot, load.folder.getChildFile(entry.file), false, error)) {
      load.failed.add(juce::String(kSlotLetters[slot]) + ": " + error);
      load.current = -1;
      if (connected)
        synthConn.setUploadCompleteCallback(nullptr);
    }
    return;
  }

  case SlotSetLoad::Stage::Focus: {
    // Worked out before focus moves: moving it changes what the synth reports.
    load.mask = slotSetEnableMask(load.set, synthState[static_cast<size_t>(load.synth)].lastEnabled);
    const int currentFocus = connected ? synthConn.getCurrentSlot() : SynthSlot::localOf(activeSlot);
    const int focus = slotSetFocusAfterLoad(load.set, load.mask, currentFocus);
    if (focus >= 0 && focus < kSlotsPerSynth) {
      if (base + focus != activeSlot)
        switchToSlot(base + focus, /*notifySynth=*/false);
      if (connected && synthConn.getCurrentSlot() != focus)
        synthHub.selectSlot(base + focus);
    }
    load.stage = SlotSetLoad::Stage::Enable;
    load.settleTicks = kSettleTicks;
    return;
  }

  case SlotSetLoad::Stage::Enable: {
    if (load.settleTicks > 0) {
      --load.settleTicks;
      return;
    }
    if (connected && !synthConn.setSlotPins(load.mask))
      load.failed.add("No slot was switched on or off: the synth has not reported which "
                      "slots are enabled yet");
    finishSlotSetLoad();
    return;
  }
  }
}

void MainComponent::finishSlotSetLoad() {
  if (slotSetLoadTimer)
    slotSetLoadTimer->stopTimer();
  const auto load = std::move(slotSetLoad);
  slotSetLoad = SlotSetLoad{};

  juce::String summary = "Slot set \"" + load.set.name + "\": ";
  summary << (load.loaded.isEmpty() ? juce::String("no patch loaded")
                                    : "loaded " + load.loaded.joinIntoString(", "));
  if (!load.failed.isEmpty())
    summary << ", with problems";
  mainLayout->getStatusBar().showMessage(summary, 5000);
  updateDspLoadDisplay();

  if (!load.failed.isEmpty())
    showSetMessage(this, juce::MessageBoxIconType::WarningIcon, "Slot set \"" + load.set.name + "\"",
                   summary + "\n\n" + load.failed.joinIntoString("\n"));
}
