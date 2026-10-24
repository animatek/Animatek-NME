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
static void styleLabel(juce::Label& l)
{
    l.setFont(juce::Font(AppTheme::uiFont(12.5f).withStyle("Bold")));
    l.setColour(juce::Label::textColourId,       AppTheme::palette().textMuted);
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

static void styleBtn(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId,   kBtnBg);
    b.setColour(juce::TextButton::buttonOnColourId, kBtnOn);
    b.setColour(juce::TextButton::textColourOffId,  kText);
    b.setColour(juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
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

    styleLabel(folderLabel);
    folderLabel.setText(mode_ == Mode::SendToSynth ? "SOURCE (.PCHLIST OR FOLDER)" : "DESTINATION FOLDER",
                        juce::dontSendNotification);
    folderField.setFont(juce::Font(AppTheme::uiFont(14.0f)));
    folderField.setBorderSize(juce::BorderSize<int>(0, 10, 0, 10));
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

    infoLabel.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    infoLabel.setColour(juce::Label::textColourId,
                        mode_ == Mode::SaveToDisk ? kText : kAmber);
    infoLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(infoLabel);

    statusLabel.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    statusLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(statusLabel);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, kGold);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, kCtrlBg);
    addAndMakeVisible(progressBar);

    styleBtn(startButton);
    styleBtn(cancelButton);
    startButton.setColour(juce::TextButton::buttonColourId,  kGold);   // Start does the thing
    startButton.setColour(juce::TextButton::textColourOffId, AppTheme::palette().backgroundMain);
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
    setSize(520, 440);
}

// ─────────────────────────────────────────────────────────────────────────────
void BankTransferDialog::browseForFolder()
{
    const auto title = mode_ == Mode::SaveToDisk
        ? "Choose a folder to save the bank into"
        : "Choose a .pchList, or a folder with .pch files, to send";

    chooser = std::make_unique<juce::FileChooser>(
        title, juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        mode_ == Mode::SendToSynth ? "*.pchList;*.pchlist" : juce::String());

    auto chooserFlags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectDirectories;
    if (mode_ == Mode::SendToSynth)
        chooserFlags |= juce::FileBrowserComponent::canSelectFiles;

    juce::Component::SafePointer<BankTransferDialog> safeThis(this);
    chooser->launchAsync(chooserFlags,
        [safeThis](const juce::FileChooser& fc) {
            if (safeThis == nullptr || fc.getResult() == juce::File())
                return;

            safeThis->chosenFolder = fc.getResult();
            safeThis->folderField.setText(safeThis->chosenFolder.getFullPathName(),
                                          juce::dontSendNotification);

            if (safeThis->mode_ == Mode::SendToSynth)
            {
                safeThis->sendProblems.clear();
                safeThis->sendItems = BankTransferManager::itemsFrom(
                    safeThis->chosenFolder, safeThis->sendProblems, safeThis->sendList);
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
                 + "Bank1-Bank9 folders.\nThe folders are replaced only once the whole "
                 + "backup has been fetched, so a backup that fails or is cancelled "
                 + "leaves the previous one intact.";
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
            info = "All 99 positions of bank " + juce::String(bank)
                 + " will be read, and the patches saved as .pch files with a Bank"
                 + juce::String(bank) + ".pchList.";
        else
            info = juce::String(count) + " patches in bank " + juce::String(bank)
                 + " will be saved as .pch files, with a Bank" + juce::String(bank)
                 + ".pchList that keeps their positions.\nNo slot on the synth changes.";
        infoLabel.setText(info, juce::dontSendNotification);
    }
    else
    {
        juce::String info;
        const int n = static_cast<int>(sendItems.size());
        if (chosenFolder == juce::File())
            info = "Select a .pchList, or a folder with .pch files.";
        else if (n == 0)
            info = sendProblems.isEmpty() ? "No .pch files in this folder."
                                          : sendProblems.joinIntoString(", ").substring(0, 160);
        else if (sendList != juce::File())
            info = juce::String(n) + " patches from " + sendList.getFileName()
                 + ", each to its own position.\n"
                 + "WARNING: they will OVERWRITE those positions of bank " + juce::String(bank) + ".";
        else
            info = juce::String(n) + " .pch files, sorted by name.\n"
                 + "WARNING: they will OVERWRITE bank " + juce::String(bank)
                 + " positions 1-" + juce::String(n) + ".";
        if (n > 0 && !sendProblems.isEmpty())
            info << " " << sendProblems.size() << " left out.";
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

    if (mode_ == Mode::BackupAllBanks && !connection.isPatchListLoaded())
    {
        statusLabel.setText("Patch list not loaded yet - try again shortly.",
                            juce::dontSendNotification);
        connection.requestPatchList();
        return;
    }

    if (mode_ == Mode::SendToSynth && sendItems.empty())
    {
        statusLabel.setText("No .pch files in the selected folder.", juce::dontSendNotification);
        return;
    }

    running = true;
    progressValue = 0.0;
    startButton.setEnabled(false);
    bankCombo.setEnabled(false);
    browseButton.setEnabled(false);
    cancelButton.setButtonText("Cancel");

    juce::Component::SafePointer<BankTransferDialog> safeThis(this);
    auto progressCb = [safeThis](const BankTransferManager::Progress& p) {
        if (safeThis != nullptr)
            safeThis->onProgress(p);
    };

    if (mode_ == Mode::SaveToDisk)
        manager.saveBankToDisk(section, chosenFolder, progressCb);
    else if (mode_ == Mode::BackupAllBanks)
        manager.saveAllBanksToDisk(chosenFolder, progressCb);
    else
        manager.sendBankToSynth(sendItems, section, progressCb);
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
    bankCombo.setEnabled(mode_ != Mode::BackupAllBanks);
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
    { if (e.getPosition().getY() < 40) dragger.startDraggingComponent(this, e); }
void BankTransferDialog::mouseDrag(const juce::MouseEvent& e)
    { if (e.getMouseDownPosition().getY() < 40) dragger.dragComponent(this, e, nullptr); }  // only a drag that began on the title bar moves the window (the press position says where it began)

void BankTransferDialog::paint(juce::Graphics& g)
{
    const auto title = mode_ == Mode::SaveToDisk    ? "Save Bank to Disk"
                     : mode_ == Mode::BackupAllBanks ? "Backup All Banks"
                                                     : "Send Bank to Synth";
    AppTheme::drawDialogChrome(g, getLocalBounds(), title, 40, 68);
}

void BankTransferDialog::resized()
{
    constexpr int titleH = 40, footerH = 68, pad = 24, secH = 18, rowH = 36, gap = 10;

    closeButton.setBounds(getWidth() - 36, 6, 28, 28);

    int y = titleH + 16;
    const int w = getWidth() - pad * 2;

    bankLabel.setBounds(pad, y, w / 2 - 6, secH);
    y += secH + 4;
    bankCombo.setBounds(pad, y, w / 2 - 6, rowH);
    y += rowH + 18;

    folderLabel.setBounds(pad, y, w, secH);
    y += secH + 4;
    folderField.setBounds(pad, y, w - 104, rowH);
    browseButton.setBounds(pad + w - 96, y, 96, rowH);
    y += rowH + 18;

    infoLabel.setBounds(pad, y, w, 48);
    y += 48 + gap;

    progressBar.setBounds(pad, y, w, 26);
    y += 26 + gap;

    statusLabel.setBounds(pad, y, w, 40);

    const int btnW = 100, btnH = 36;
    const int btnY = getHeight() - footerH + (footerH - btnH) / 2;
    cancelButton.setBounds(getWidth() - pad - btnW,          btnY, btnW, btnH);
    startButton .setBounds(getWidth() - pad - btnW * 2 - 12, btnY, btnW, btnH);
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
