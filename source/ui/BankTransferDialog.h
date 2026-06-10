#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatCloseButton.h"
#include "../sync/BankTransferManager.h"

class ConnectionManager;

// Dialog driving BankTransferManager in either direction:
//   SaveToDisk  — synth bank → folder of .pch files
//   SendToSynth — folder of .pch files → synth bank (overwrites!)
class BankTransferDialog : public juce::Component
{
public:
    // BackupAllBanks mirrors all 9 banks into a fixed folder (the preset
    // library's Banks/ root) — bank/folder selection is locked in that mode.
    enum class Mode { SaveToDisk, SendToSynth, BackupAllBanks };

    BankTransferDialog(Mode mode, BankTransferManager& manager, ConnectionManager& connection,
                       const juce::File& fixedFolder = {});

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    static void show(juce::Component* parent, Mode mode,
                     BankTransferManager& manager, ConnectionManager& connection,
                     const juce::File& fixedFolder = {});

private:
    void browseForFolder();
    void updateInfo();
    void startTransfer();
    void onProgress(const BankTransferManager::Progress& p);
    void close();

    Mode mode_;
    BankTransferManager& manager;
    ConnectionManager& connection;

    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    juce::Label    bankLabel   { {}, "BANK" };
    juce::ComboBox bankCombo;
    juce::Label    slotLabel   { {}, "TEMP SLOT" };
    juce::ComboBox slotCombo;
    juce::Label    folderLabel { {}, "FOLDER" };
    juce::Label    folderField;
    juce::TextButton browseButton { "Browse..." };
    juce::Label    infoLabel;
    juce::Label    statusLabel;

    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };

    juce::TextButton startButton  { "Start" };
    juce::TextButton cancelButton { "Close" };

    juce::File chosenFolder;
    juce::File fixedFolder_;                // BackupAllBanks: locked destination
    juce::Array<juce::File> scannedFiles;   // SendToSynth: .pch files found
    bool running = false;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankTransferDialog)
};
