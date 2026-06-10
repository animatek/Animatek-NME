#include "BankTransferDialog.h"
#include "AppTheme.h"
#include "../midi/ConnectionManager.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kGold   (AppTheme::palette().accentActive)
#define kAmber  (AppTheme::palette().accentWarning)
#define kText   (AppTheme::palette().textSecondary)
#define kCtrlBg (AppTheme::palette().inputBackground)
#define kCtrlBd (AppTheme::palette().borderColor)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)
static const juce::Colour kOkBg { 0xff1e3a1e };
static const juce::Colour kOkOn { 0xff2a5a2a };

static void styleLabel(juce::Label& l)
{
    l.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    l.setColour(juce::Label::textColourId,       kGold);
    l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

static void styleCombo(juce::ComboBox& c)
{
    c.setColour(juce::ComboBox::backgroundColourId,     kCtrlBg);
    c.setColour(juce::ComboBox::outlineColourId,        kCtrlBd);
    c.setColour(juce::ComboBox::textColourId,           kText);
    c.setColour(juce::ComboBox::arrowColourId,          kAmber);
    c.setColour(juce::ComboBox::focusedOutlineColourId, kGold);
}

static void styleBtn(juce::TextButton& b, bool isOk = false)
{
    b.setColour(juce::TextButton::buttonColourId,   isOk ? kOkBg : kBtnBg);
    b.setColour(juce::TextButton::buttonOnColourId, isOk ? kOkOn : kBtnOn);
    b.setColour(juce::TextButton::textColourOffId,  isOk ? juce::Colour(0xffaaffaa) : kText);
}

// ─────────────────────────────────────────────────────────────────────────────
BankTransferDialog::BankTransferDialog(Mode mode, BankTransferManager& mgr,
                                       ConnectionManager& conn, const juce::File& fixedFolder)
    : mode_(mode), manager(mgr), connection(conn), fixedFolder_(fixedFolder)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    closeButton.onClick = [this]() { close(); };
    addAndMakeVisible(closeButton);

    styleLabel(bankLabel);
    bankLabel.setText(mode_ == Mode::SendToSynth ? "DESTINATION BANK" : "SOURCE BANK",
                      juce::dontSendNotification);
    if (mode_ == Mode::BackupAllBanks)
        bankCombo.addItem("All banks (1-9)", 1);
    else
        for (int b = 1; b <= 9; ++b)
            bankCombo.addItem("Bank " + juce::String(b), b);
    bankCombo.setSelectedItemIndex(0, juce::dontSendNotification);
    bankCombo.setEnabled(mode_ != Mode::BackupAllBanks);
    bankCombo.onChange = [this]() { updateInfo(); };
    styleCombo(bankCombo);
    addAndMakeVisible(bankLabel);
    addAndMakeVisible(bankCombo);

    styleLabel(slotLabel);
    slotCombo.addItem("A  (Slot 1)", 1);
    slotCombo.addItem("B  (Slot 2)", 2);
    slotCombo.addItem("C  (Slot 3)", 3);
    slotCombo.addItem("D  (Slot 4)", 4);
    slotCombo.setSelectedItemIndex(juce::jlimit(0, 3, connection.getCurrentSlot()),
                                   juce::dontSendNotification);
    styleCombo(slotCombo);
    addAndMakeVisible(slotLabel);
    addAndMakeVisible(slotCombo);

    styleLabel(folderLabel);
    folderLabel.setText(mode_ == Mode::SendToSynth ? "SOURCE FOLDER" : "DESTINATION FOLDER",
                        juce::dontSendNotification);
    folderField.setFont(juce::Font(juce::FontOptions(12.0f)));
    folderField.setColour(juce::Label::textColourId, kText);
    folderField.setColour(juce::Label::backgroundColourId, kCtrlBg);
    folderField.setColour(juce::Label::outlineColourId, kCtrlBd);
    if (mode_ == Mode::BackupAllBanks)
    {
        chosenFolder = fixedFolder_;
        folderField.setText(fixedFolder_.getFullPathName(), juce::dontSendNotification);
        browseButton.setEnabled(false);
    }
    else
        folderField.setText("(no folder selected)", juce::dontSendNotification);
    styleBtn(browseButton);
    browseButton.onClick = [this]() { browseForFolder(); };
    addAndMakeVisible(folderLabel);
    addAndMakeVisible(folderField);
    addAndMakeVisible(browseButton);

    infoLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    infoLabel.setColour(juce::Label::textColourId,
                        mode_ == Mode::SaveToDisk ? kText : kAmber);
    infoLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(infoLabel);

    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    statusLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(statusLabel);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, kGold);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, kCtrlBg);
    addAndMakeVisible(progressBar);

    styleBtn(startButton, true);
    styleBtn(cancelButton);
    startButton.onClick  = [this]() { startTransfer(); };
    cancelButton.onClick = [this]()
    {
        if (running)
        {
            manager.cancel();
            statusLabel.setText("Cancelling after current patch...", juce::dontSendNotification);
        }
        else
            close();
    };
    addAndMakeVisible(startButton);
    addAndMakeVisible(cancelButton);

    updateInfo();
    setSize(440, 332);
}

// ─────────────────────────────────────────────────────────────────────────────
void BankTransferDialog::browseForFolder()
{
    const auto title = mode_ == Mode::SaveToDisk
        ? "Choose a folder to save the bank into"
        : "Choose a folder with .pch files to send";

    chooser = std::make_unique<juce::FileChooser>(
        title, juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));

    juce::Component::SafePointer<BankTransferDialog> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [safeThis](const juce::FileChooser& fc) {
            if (safeThis == nullptr || fc.getResult() == juce::File())
                return;

            safeThis->chosenFolder = fc.getResult();
            safeThis->folderField.setText(safeThis->chosenFolder.getFullPathName(),
                                          juce::dontSendNotification);

            if (safeThis->mode_ == Mode::SendToSynth)
            {
                safeThis->scannedFiles = safeThis->chosenFolder.findChildFiles(
                    juce::File::findFiles, /*recursive=*/false, "*.pch");
                safeThis->scannedFiles.sort();
            }

            safeThis->updateInfo();
        });
}

void BankTransferDialog::updateInfo()
{
    const int bank = bankCombo.getSelectedItemIndex() + 1;

    if (mode_ == Mode::BackupAllBanks)
    {
        int count = 0;
        for (const auto& name : connection.getPatchList())
            if (!name.empty())
                ++count;

        juce::String info;
        if (!connection.isPatchListLoaded())
            info = "Patch list not loaded yet - connect and let the Synth browser load it first.";
        else
            info = juce::String(count) + " patches across all 9 banks will be saved into "
                 + "Bank1-Bank9 folders.\nWARNING: existing .pch files inside those folders"
                 + " are deleted first (mirror backup).";
        infoLabel.setText(info, juce::dontSendNotification);
    }
    else if (mode_ == Mode::SaveToDisk)
    {
        int count = 0;
        const auto& list = connection.getPatchList();
        for (int pos = 0; pos < 99; ++pos)
        {
            const size_t idx = static_cast<size_t>((bank - 1) * 99 + pos);
            if (idx < list.size() && !list[idx].empty())
                ++count;
        }

        juce::String info;
        if (!connection.isPatchListLoaded())
            info = "Patch list not loaded yet - connect and let the Synth browser load it first.";
        else
            info = juce::String(count) + " patches in bank " + juce::String(bank)
                 + " will be saved as .pch files.\nThe temp slot's content on the synth"
                 + " will be replaced during the transfer.";
        infoLabel.setText(info, juce::dontSendNotification);
    }
    else
    {
        juce::String info;
        if (scannedFiles.isEmpty())
            info = "Select a folder containing .pch files.";
        else
            info = juce::String(scannedFiles.size()) + " .pch files found (sorted by name).\n"
                 + "WARNING: they will OVERWRITE bank " + juce::String(bank)
                 + " positions 1-" + juce::String(juce::jmin(scannedFiles.size(), 99)) + ".";
        infoLabel.setText(info, juce::dontSendNotification);
    }
}

void BankTransferDialog::startTransfer()
{
    if (running || manager.isBusy())
        return;

    if (!connection.isConnected())
    {
        statusLabel.setText("Not connected to the synth.", juce::dontSendNotification);
        return;
    }

    if (chosenFolder == juce::File())
    {
        statusLabel.setText("Choose a folder first.", juce::dontSendNotification);
        return;
    }

    const int section = bankCombo.getSelectedItemIndex();
    const int slot    = slotCombo.getSelectedItemIndex();

    if (mode_ != Mode::SendToSynth && !connection.isPatchListLoaded())
    {
        statusLabel.setText("Patch list not loaded yet - try again shortly.",
                            juce::dontSendNotification);
        connection.requestPatchList();
        return;
    }

    if (mode_ == Mode::SendToSynth && scannedFiles.isEmpty())
    {
        statusLabel.setText("No .pch files in the selected folder.", juce::dontSendNotification);
        return;
    }

    running = true;
    progressValue = 0.0;
    startButton.setEnabled(false);
    bankCombo.setEnabled(false);
    slotCombo.setEnabled(false);
    browseButton.setEnabled(false);
    cancelButton.setButtonText("Cancel");

    juce::Component::SafePointer<BankTransferDialog> safeThis(this);
    auto progressCb = [safeThis](const BankTransferManager::Progress& p) {
        if (safeThis != nullptr)
            safeThis->onProgress(p);
    };

    if (mode_ == Mode::SaveToDisk)
        manager.saveBankToDisk(section, chosenFolder, slot, progressCb);
    else if (mode_ == Mode::BackupAllBanks)
        manager.saveAllBanksToDisk(chosenFolder, slot, progressCb);
    else
        manager.sendBankToSynth(scannedFiles, section, slot, progressCb);
}

void BankTransferDialog::onProgress(const BankTransferManager::Progress& p)
{
    progressValue = p.total > 0 ? static_cast<double>(p.current) / p.total : 0.0;

    if (!p.finished)
    {
        statusLabel.setText(juce::String(p.current + 1) + "/" + juce::String(p.total)
                                + ":  " + p.itemName,
                            juce::dontSendNotification);
        return;
    }

    running = false;
    startButton.setEnabled(true);
    bankCombo.setEnabled(true);
    slotCombo.setEnabled(true);
    browseButton.setEnabled(true);
    cancelButton.setButtonText("Close");

    juce::String summary = p.cancelled ? "Cancelled. " : "Done. ";
    summary << p.current << "/" << p.total << " transferred";
    if (!p.failures.isEmpty())
        summary << ", " << p.failures.size() << " failed:\n"
                << p.failures.joinIntoString(", ").substring(0, 200);
    statusLabel.setText(summary, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
void BankTransferDialog::close()
{
    if (running)
        manager.cancel();
    removeFromDesktop();
    delete this;
}

bool BankTransferDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void BankTransferDialog::mouseDown(const juce::MouseEvent& e)
    { if (e.getPosition().getY() < 32) dragger.startDraggingComponent(this, e); }
void BankTransferDialog::mouseDrag(const juce::MouseEvent& e)
    { dragger.dragComponent(this, e, nullptr); }

void BankTransferDialog::paint(juce::Graphics& g)
{
    g.fillAll(kBg);

    g.setColour(kGold);
    g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
    const auto title = mode_ == Mode::SaveToDisk    ? "Save Bank to Disk"
                     : mode_ == Mode::BackupAllBanks ? "Backup All Banks"
                                                     : "Send Bank to Synth";
    g.drawText(title, 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour(kSep);
    g.fillRect(0, 31, getWidth(), 1);

    const int sepY = getHeight() - 50;
    g.drawHorizontalLine(sepY, 14.0f, static_cast<float>(getWidth() - 14));
}

void BankTransferDialog::resized()
{
    constexpr int titleH = 32, pad = 14, secH = 14, rowH = 28, gap = 6;

    closeButton.setBounds(getWidth() - 32, 2, 28, 28);

    int y = titleH + gap;
    const int w = getWidth() - pad * 2;

    bankLabel.setBounds(pad, y, w / 2 - 4, secH);
    slotLabel.setBounds(pad + w / 2 + 4, y, w / 2 - 4, secH);
    y += secH + 2;
    bankCombo.setBounds(pad, y, w / 2 - 4, rowH);
    slotCombo.setBounds(pad + w / 2 + 4, y, w / 2 - 4, rowH);
    y += rowH + gap + 4;

    folderLabel.setBounds(pad, y, w, secH);
    y += secH + 2;
    folderField.setBounds(pad, y, w - 90, rowH);
    browseButton.setBounds(pad + w - 84, y, 84, rowH);
    y += rowH + gap + 4;

    infoLabel.setBounds(pad, y, w, 44);
    y += 44 + gap;

    progressBar.setBounds(pad, y, w, 18);
    y += 18 + gap;

    statusLabel.setBounds(pad, y, w, 40);

    const int btnW = 80, btnH = 26;
    const int btnY = getHeight() - pad - btnH;
    cancelButton.setBounds(getWidth() - pad - btnW,         btnY, btnW, btnH);
    startButton .setBounds(getWidth() - pad - btnW * 2 - 8, btnY, btnW, btnH);
}

// ─────────────────────────────────────────────────────────────────────────────
void BankTransferDialog::show(juce::Component* parent, Mode mode,
                              BankTransferManager& manager, ConnectionManager& connection,
                              const juce::File& fixedFolder)
{
    auto* dlg = new BankTransferDialog(mode, manager, connection, fixedFolder);

    if (parent != nullptr)
    {
        auto* top    = parent->getTopLevelComponent();
        auto  screen = top->localAreaToGlobal(top->getLocalBounds());
        dlg->setTopLeftPosition(screen.getX() + (screen.getWidth()  - dlg->getWidth())  / 2,
                                screen.getY() + (screen.getHeight() - dlg->getHeight()) / 2);
    }

    dlg->addToDesktop(juce::ComponentPeer::windowHasDropShadow);
    dlg->setVisible(true);
    dlg->toFront(true);
    dlg->grabKeyboardFocus();
}
