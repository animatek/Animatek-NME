#include "OsUpdateDialog.h"
#include "AppTheme.h"
#include "../midi/ConnectionManager.h"
#include "../midi/SynthHub.h"
#include "../midi/OsUpdate.h"

namespace
{
    const auto& p() { return AppTheme::palette(); }

    void styleBtn(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId,   p().buttonBackground);
        b.setColour(juce::TextButton::buttonOnColourId, p().buttonActive);
        b.setColour(juce::TextButton::textColourOffId,  p().textSecondary);
        b.setColour(juce::TextButton::textColourOnId,   p().textPrimary);
    }

    // The synth takes the messages as they come and answers none of them, so
    // the pace is the sender's. Safe is about what Clavia's updater takes with
    // its Safe (Slower) box ticked; Fast has only been tried on G1-Emu.
    struct Speed { const char* name; int intervalMs; };
    const Speed speeds[] = {
        { "Safe: about 12 minutes", 200 },
        { "Fast: about 3 1/2 minutes (G1-Emu)", 60 },
    };

    // A page of text: a heading and what it says, one after the other
    struct Para { const char* heading; const char* text; };

    void drawParagraphs(juce::Graphics& g, juce::Rectangle<int> area, std::initializer_list<Para> paras)
    {
        juce::AttributedString s;
        s.setWordWrap(juce::AttributedString::byWord);
        s.setLineSpacing(3.0f);
        for (const auto& para : paras)
        {
            s.append(juce::String(para.heading) + "\n", juce::Font(AppTheme::uiFont(14.0f)).boldened(), p().textPrimary);
            s.append(juce::String(para.text) + "\n\n", juce::Font(AppTheme::uiFont(13.0f)), p().textSecondary);
        }
        juce::TextLayout layout;
        layout.createLayout(s, float(area.getWidth()));
        layout.draw(g, area.toFloat());
    }
}

OsUpdateDialog::OsUpdateDialog(SynthHub& synthHub) : hub(synthHub)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    closeButton.onClick = [this] { close(); };
    addAndMakeVisible(closeButton);

    for (auto* b : { &browseButton, &checkButton, &closeFooterButton, &installButton })
    {
        styleBtn(*b);
        addAndMakeVisible(*b);
    }
    installButton.setColour(juce::TextButton::buttonColourId, p().accentActive);   // Install does the thing
    installButton.setColour(juce::TextButton::textColourOffId, p().backgroundMain);
    browseButton.onClick = [this] { chooseImage(); };
    checkButton.onClick = [this]
    {
        for (int i = 0; i < kMaxSynths; ++i)
            hub.synth(i).greetSynth();
    };
    synthCombo.onChange = [this]
    {
        targetChosen = true;
        targetSynth = synthCombo.getSelectedId() - 1;
        refresh();
    };
    synthCombo.setColour(juce::ComboBox::backgroundColourId, p().inputBackground);
    synthCombo.setColour(juce::ComboBox::outlineColourId, p().borderColor);
    synthCombo.setColour(juce::ComboBox::textColourId, p().textSecondary);
    synthCombo.setColour(juce::ComboBox::arrowColourId, p().textSecondary);
    addAndMakeVisible(synthCombo);
    installButton.onClick = [this]
    {
        if (phase == Phase::Sending)
            stop("Stopped. The synth is still in update mode: install again, or restart it to keep its old OS.");
        else
            start();
    };
    closeFooterButton.onClick = [this] { close(); };

    for (int i = 0; i < int(std::size(speeds)); ++i)
        speedCombo.addItem(speeds[i].name, i + 1);
    speedCombo.setSelectedId(1, juce::dontSendNotification);
    speedCombo.onChange = [this] { speedChosen = true; refresh(); };
    speedCombo.setColour(juce::ComboBox::backgroundColourId, p().inputBackground);
    speedCombo.setColour(juce::ComboBox::outlineColourId, p().borderColor);
    speedCombo.setColour(juce::ComboBox::textColourId, p().textSecondary);
    speedCombo.setColour(juce::ComboBox::arrowColourId, p().textSecondary);
    addAndMakeVisible(speedCombo);

    riskToggle.setColour(juce::ToggleButton::textColourId, p().textPrimary);
    riskToggle.setColour(juce::ToggleButton::tickColourId, p().accentActive);
    riskToggle.setColour(juce::ToggleButton::tickDisabledColourId, p().textSecondary);
    riskToggle.onClick = [this] { refresh(); };
    addAndMakeVisible(riskToggle);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, p().accentActive);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, p().inputBackground);
    addAndMakeVisible(progressBar);

    statusLabel.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    statusLabel.setColour(juce::Label::textColourId, p().textSecondary);
    statusLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(statusLabel);

    // The update's only answer, to its checksum, is no editor message. Every synth
    // is greeted: one in update mode says so, and NME learns it from here.
    juce::Component::SafePointer<OsUpdateDialog> safeThis(this);
    for (int i = 0; i < kMaxSynths; ++i)
    {
        hub.synth(i).onRawSysEx = [safeThis, i](const uint8_t* data, size_t length)
        {
            if (safeThis != nullptr && safeThis->targetSynth == i && safeThis->phase == Phase::WaitingForAnswer
                && OsUpdate::isChecksumAccepted(data, length))
            {
                safeThis->phase = Phase::Accepted;
                safeThis->refresh();
            }
        };
        hub.synth(i).greetSynth();
    }

    setSize(dialogWidth, dialogHeight);
    showPage(0);
    startTimer(20);
}

OsUpdateDialog::~OsUpdateDialog()
{
    stopTimer();
    for (int i = 0; i < kMaxSynths; ++i)
        hub.synth(i).onRawSysEx = nullptr;
}

void OsUpdateDialog::show(juce::Component* parent, SynthHub& hub)
{
    auto* dlg = new OsUpdateDialog(hub);
    if (parent != nullptr)
    {
        auto* top = parent->getTopLevelComponent();
        auto screen = top->localAreaToGlobal(top->getLocalBounds());
        dlg->setTopLeftPosition(screen.getX() + (screen.getWidth() - dlg->getWidth()) / 2,
                                screen.getY() + (screen.getHeight() - dlg->getHeight()) / 2);
    }
    dlg->addToDesktop(juce::ComponentPeer::windowHasDropShadow);
    dlg->setVisible(true);
    dlg->toFront(true);
    dlg->grabKeyboardFocus();
}

// ─── the image ───────────────────────────────────────────────────────────────

// Where G1-Emu keeps the OS it received with Clavia's updater (its own data folder)
juce::File OsUpdateDialog::emulatorOsFolder()
{
#if JUCE_WINDOWS
    const auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#else
    const auto base = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/share");
#endif
    return base.getChildFile("Animatek/G1-Emu/os");
}

void OsUpdateDialog::chooseImage()
{
    static juce::File lastFolder;
    auto start = lastFolder.isDirectory() ? lastFolder : emulatorOsFolder();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser>("G1 OS image", start, "*.bin");
    juce::Component::SafePointer<OsUpdateDialog> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (safeThis == nullptr || file == juce::File())
                return;
            lastFolder = file.getParentDirectory();
            safeThis->loadImage(file);
        });
}

void OsUpdateDialog::loadImage(const juce::File& file)
{
    imageFile = file;
    image.clear();
    stoppedNote.clear();
    juce::MemoryBlock block;
    if (file.getSize() > juce::int64(OsUpdate::kMaxImageBytes) * 2 || ! file.loadFileAsData(block))
        imageProblem = "It cannot be read, or it is far too big to be a G1 OS.";
    else
    {
        image.assign(static_cast<const uint8_t*>(block.getData()),
                     static_cast<const uint8_t*>(block.getData()) + block.getSize());
        imageProblem = OsUpdate::checkImage(image);
    }
    if (phase != Phase::Sending && phase != Phase::WaitingForAnswer)
    {
        phase = Phase::Idle;
        progressValue = 0.0;
    }
    refresh();
}

// ─── the transfer ────────────────────────────────────────────────────────────

int OsUpdateDialog::intervalMs() const
{
    return speeds[juce::jlimit(0, int(std::size(speeds)) - 1, speedCombo.getSelectedId() - 1)].intervalMs;
}

void OsUpdateDialog::start()
{
    auto* c = target();
    if (image.empty() || imageProblem.isNotEmpty() || c == nullptr || ! c->isInUpdateMode())
        return;

    const auto question = "Install " + imageFile.getFileName() + " into the synth on "
        + c->getConnectedPortName() + "?\n\n"
        "It replaces the synth's OS once you press Store on it. The patch banks are kept, "
        "but back them up first if they matter (Device > Backup All Banks, before update mode).\n\n"
        "If the new OS does not start, the Recovery page says how to come back.";
    juce::Component::SafePointer<OsUpdateDialog> safeThis(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Install an OS", question,
        "Install", "Cancel", nullptr,
        juce::ModalCallbackFunction::create([safeThis](int result)
        {
            if (safeThis == nullptr || result == 0 || safeThis->target() == nullptr || ! safeThis->target()->isInUpdateMode())
                return;
            safeThis->messages = OsUpdate::buildMessages(safeThis->image);
            safeThis->nextMessage = 0;
            safeThis->stoppedNote.clear();
            safeThis->phase = Phase::Sending;
            safeThis->phaseStartMs = juce::Time::getMillisecondCounter();
            safeThis->lastSendMs = 0;
            safeThis->refresh();
        }));
}

void OsUpdateDialog::stop(const juce::String& why)
{
    phase = Phase::Idle;
    messages.clear();
    stoppedNote = why;
    refresh();
}

void OsUpdateDialog::sendNext()
{
    auto* c = target();
    if (c == nullptr || ! c->hasOpenPorts())
    {
        stop("The synth's port closed. Put the synth in update mode again and install again.");
        return;
    }
    c->sendOsUpdateMessage(messages[nextMessage++]);
    lastSendMs = juce::Time::getMillisecondCounter();
    progressValue = double(nextMessage) / double(messages.size());
    if (nextMessage == messages.size())
    {
        messages.clear();
        phase = Phase::WaitingForAnswer;
        phaseStartMs = lastSendMs;
    }
}

void OsUpdateDialog::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (phase == Phase::Sending && now - lastSendMs >= juce::uint32(intervalMs()))
        sendNext();
    if (phase == Phase::WaitingForAnswer && now - phaseStartMs > 10000)
        phase = Phase::NoAnswer;

    if (++ticks % 10 == 0)
        refresh();
}

// ─── what the page says ──────────────────────────────────────────────────────

ConnectionManager* OsUpdateDialog::target()
{
    return targetSynth >= 0 && targetSynth < kMaxSynths && hub.synth(targetSynth).hasOpenPorts()
        ? &hub.synth(targetSynth) : nullptr;
}

const ConnectionManager* OsUpdateDialog::target() const
{
    return const_cast<OsUpdateDialog*>(this)->target();
}

juce::String OsUpdateDialog::synthState(const ConnectionManager& c) const
{
    if (c.isInUpdateMode())
        return "in update mode";
    if (c.isConnected())
        return "running its OS";
    return "no answer";
}

// The synths NME has a port open to, by their number in the hub. The one in update mode is
// picked until the user picks one: with a G1 and G1-Emu both there, the OS goes only where it
// is awaited.
void OsUpdateDialog::fillSynths()
{
    if (! targetChosen)
    {
        targetSynth = -1;
        for (int i = 0; i < kMaxSynths && targetSynth < 0; ++i)
            if (hub.synth(i).hasOpenPorts() && hub.synth(i).isInUpdateMode())
                targetSynth = i;
        if (targetSynth < 0 && hub.active().hasOpenPorts())
            targetSynth = hub.activeSynth();
    }
    // Refilled only when what it shows changes: clearing it every tick would shut its menu
    juce::StringArray items;
    for (int i = 0; i < kMaxSynths; ++i)
    {
        const auto& c = hub.synth(i);
        if (c.hasOpenPorts())
            items.add(juce::String(i + 1) + "|Port " + juce::String(i + 1) + ": " + c.getConnectedPortName()
                      + " (" + synthState(c) + ")");
    }
    if (items != synthItems)
    {
        synthItems = items;
        synthCombo.clear(juce::dontSendNotification);
        for (const auto& item : items)
            synthCombo.addItem(item.fromFirstOccurrenceOf("|", false, false),
                               item.upToFirstOccurrenceOf("|", false, false).getIntValue());
    }
    const int wanted = target() != nullptr ? targetSynth + 1 : 0;
    if (synthCombo.getSelectedId() != wanted)
        synthCombo.setSelectedId(wanted, juce::dontSendNotification);
    synthCombo.setTextWhenNothingSelected("No synth port open");
}

juce::String OsUpdateDialog::synthLine() const
{
    const auto* c = target();
    if (c == nullptr)
        return "No synth port is open: choose it in Device > MIDI Settings.";
    if (c->isInUpdateMode())
        return "In update mode, ready.";
    if (c->isConnected())
        return "Running its OS. Put it in update mode first (see Update mode).";
    return "No answer yet. Put it in update mode (see Update mode), then Check again.";
}

void OsUpdateDialog::refresh()
{
    // Until the user picks a speed, the port says which: G1-Emu takes it fast
    const bool busy0 = phase == Phase::Sending || phase == Phase::WaitingForAnswer;
    if (! busy0)
        fillSynths();
    const auto* c = target();
    const bool ready = c != nullptr && c->isInUpdateMode();
    if (! speedChosen && c != nullptr)
    {
        const bool emulator = c->isDirectLink() || c->getConnectedPortName().containsIgnoreCase("G1-Emu");
        speedCombo.setSelectedId(emulator ? 2 : 1, juce::dontSendNotification);
    }

    const bool busy = phase == Phase::Sending || phase == Phase::WaitingForAnswer;
    browseButton.setEnabled(! busy);
    speedCombo.setEnabled(! busy);
    checkButton.setEnabled(! busy);
    synthCombo.setEnabled(! busy);
    installButton.setButtonText(phase == Phase::Sending ? "Stop" : "Install");
    installButton.setEnabled(phase == Phase::Sending
        || (! busy && riskToggle.getToggleState() && ! image.empty() && imageProblem.isEmpty() && ready));

    juce::String status;
    switch (phase)
    {
        case Phase::Idle:
            if (stoppedNote.isNotEmpty())
                status = stoppedNote;
            else if (! riskToggle.getToggleState())
                status = "Read the Warning page first: Install waits for it.";
            else if (image.empty() && imageFile == juce::File())
                status = "Choose the OS image to install.";
            else if (imageProblem.isNotEmpty())
                status = "This file cannot go: " + imageProblem;
            else if (! ready)
                status = "The image is ready. The synth is not in update mode yet.";
            else
                status = "Ready to install.";
            break;
        case Phase::Sending:
        {
            const auto left = (messages.size() - nextMessage) * size_t(intervalMs()) / 1000;
            status = "Sending " + juce::String(int(nextMessage)) + " of " + juce::String(int(messages.size()))
                   + " (about " + juce::String(int(left / 60)) + " min " + juce::String(int(left % 60)) + " s left). "
                   "Leave the synth and this window alone until it ends.";
            break;
        }
        case Phase::WaitingForAnswer:
            status = "All sent. Waiting for the synth to check it...";
            break;
        case Phase::Accepted:
            status = "The synth took the whole OS and its checksum is right.\n"
                     "On the synth, at Update flash?, press Store. At Update completed, restart it "
                     "(G1-Emu: the power button at the bottom right) and it runs the new OS.";
            break;
        case Phase::NoAnswer:
            status = "No answer from the synth after the last message. Look at its display: "
                     "Checksum ERROR! means start again from update mode, with the Safe speed.";
            break;
    }
    statusLabel.setText(status, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
        phase == Phase::Accepted ? p().accentActive
        : (phase == Phase::NoAnswer || imageProblem.isNotEmpty()) ? p().accentWarning : p().textSecondary);
    repaint();
}

// ─── paint and layout ────────────────────────────────────────────────────────

const char* OsUpdateDialog::pageName(int page)
{
    static const char* const names[numPages] = { "Warning", "Install OS", "Update mode", "Recovery" };
    return names[juce::jlimit(0, numPages - 1, page)];
}

juce::Rectangle<int> OsUpdateDialog::pageButtonBounds(int index) const
{
    return { 8, titleBarH + 14 + index * 42, sidebarW - 16, 38 };
}

void OsUpdateDialog::showPage(int newPage)
{
    page = juce::jlimit(0, numPages - 1, newPage);
    for (auto* c : std::initializer_list<juce::Component*> { &browseButton, &checkButton, &synthCombo, &speedCombo,
                                                             &progressBar, &statusLabel, &installButton })
        c->setVisible(page == 1);
    riskToggle.setVisible(page == 0);
    refresh();
}

void OsUpdateDialog::paint(juce::Graphics& g)
{
    const auto& pal = p();
    g.fillAll(pal.backgroundMain);

    g.setColour(pal.textPrimary);
    g.setFont(juce::Font(AppTheme::uiFont(15.0f)).boldened());
    g.drawText("Update Synth OS", 18, 0, getWidth() - 60, titleBarH, juce::Justification::centredLeft);
    g.setColour(pal.borderColor);
    g.fillRect(0, titleBarH - 1, getWidth(), 1);

    g.setColour(pal.backgroundPanel);
    g.fillRect(0, titleBarH, sidebarW, getHeight() - titleBarH);
    g.setColour(pal.borderColor);
    g.fillRect(sidebarW - 1, titleBarH, 1, getHeight() - titleBarH);
    for (int i = 0; i < numPages; ++i)
    {
        const auto r = pageButtonBounds(i).toFloat();
        const bool on = (i == page);
        if (on)
        {
            g.setColour(pal.buttonBackground);
            g.fillRoundedRectangle(r, 7.0f);
            g.setColour(pal.accentActive);
            g.fillRoundedRectangle(r.getX(), r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);
        }
        g.setColour(on ? pal.textPrimary : pal.textSecondary);
        g.setFont(juce::Font(AppTheme::uiFont(14.0f).withStyle(on ? "Bold" : "Regular")));
        g.drawText(pageName(i), r.toNearestInt().withTrimmedLeft(18), juce::Justification::centredLeft, false);
    }

    const int x0 = sidebarW + 32, w = getWidth() - x0 - 32;
    g.setColour(pal.textPrimary);
    g.setFont(juce::Font(AppTheme::uiFont(20.0f)).boldened());
    g.drawText(pageName(page), x0, titleBarH + 16, 400, 36, juce::Justification::centredLeft);

    const auto text = juce::Rectangle<int>(x0, titleBarH + 64, w, getHeight() - titleBarH - 64 - footerH - 8);
    if (page == 0)
    {
        drawParagraphs(g, text.withTrimmedBottom(44), {
            { "There is a real risk",
              "Installing an OS rewrites the synth's flash. NME does it the way Clavia's own updater does, but it is "
              "not Clavia's software: Animatek NME is an independent, unofficial project, not affiliated with or "
              "supported by Clavia. A wrong or modified OS, an update cut halfway, may leave the synth not starting, "
              "may lose its patches, and recovery is not guaranteed." },
            { "Try it on an emulator first",
              "Every OS, above all a modified one, goes first to G1-Emu (Settings > OS... > Update mode), where "
              "nothing can break. Only when it works there, and you know the way back (the Recovery page), think "
              "about a real synth. Back up its banks before." },
            { "At your own risk",
              "NME comes with no warranty. Its authors take no responsibility for any harm to your instrument, "
              "your patches or anything else. You do this entirely at your own risk; you alone are responsible "
              "for your synth." },
        });
    }
    else if (page == 1)
    {
        // A name and what it says on the left, its control on the right, as Editor Options' rows
        auto row = [&](int y, int h, const juce::String& title, const juce::String& desc, int controlW, juce::Colour descColour)
        {
            const int textW = w - controlW - 28;
            g.setColour(pal.borderColor.withAlpha(0.5f));
            g.fillRect(x0, y + h - 1, w, 1);
            g.setColour(pal.textPrimary);
            g.setFont(juce::Font(AppTheme::uiFont(14.0f)).boldened());
            g.drawText(title, x0, y + 12, textW, 22, juce::Justification::centredLeft, true);
            g.setColour(descColour);
            g.setFont(juce::Font(AppTheme::uiFont(12.5f)));
            g.drawFittedText(desc, x0, y + 35, textW, h - 42, juce::Justification::topLeft, 3, 1.0f);
        };
        juce::String imageDesc = "The OS as it runs from RAM (a .bin): G1-Emu keeps the one it received in its os folder. "
                                 "Nothing of Clavia's comes with NME.";
        juce::Colour imageColour = pal.textMuted;
        if (imageFile != juce::File())
        {
            imageDesc = imageFile.getFileName() + ", " + juce::String(int(image.size())) + " bytes: "
                      + (imageProblem.isEmpty() ? juce::String("a G1 OS image.") : imageProblem);
            imageColour = imageProblem.isEmpty() ? pal.textSecondary : pal.accentWarning;
        }
        row(titleBarH + 64, 84, "OS image", imageDesc, 130, imageColour);
        const auto* c = target();
        row(titleBarH + 148, 100, "Synth", synthLine(), 320,
            c != nullptr && c->isInUpdateMode() ? pal.textSecondary : pal.accentWarning);
        row(titleBarH + 248, 84, "Speed", "How fast the messages go. Nothing answers them one by one: too fast "
                                           "for the synth ends in a checksum error, and you start again.", 260, pal.textMuted);
    }
    else if (page == 2)
    {
        drawParagraphs(g, text, {
            { "A real G1",
              "Switch it off. Hold Shift and Store down and switch it on: the display says Update utility. "
              "Connect NME to it as ever (Device > MIDI Settings, the PC port)." },
            { "G1-Emu",
              "Settings > OS... > Update mode. It restarts as a G1 would with Shift and Store held." },
            { "Then",
              "Come back to Install OS: the Synth row says ready when NME hears it (Check again asks). "
              "A synth in update mode is not connected for editing: the status bar says so." },
        });
    }
    else
    {
        drawParagraphs(g, text, {
            { "The OS does not start",
              "Nothing is lost: the update utility is in the G1's ROM, which no update writes. Hold Shift and "
              "Store at power-on again and install an OS that works, Clavia's 3.03b if nothing else." },
            { "Just to play",
              "Shift and Patch/Load held at power-on boot the OS that came in the ROM, for that one time; "
              "the next plain power-on runs the flash's again." },
            { "The banks",
              "An update keeps them. Still, back them up before (Device > Backup All Banks): a patch made "
              "for one OS may not load in another." },
        });
    }

    g.setColour(pal.borderColor);
    g.fillRect(sidebarW, getHeight() - footerH, getWidth() - sidebarW, 1);
}

void OsUpdateDialog::resized()
{
    const int x0 = sidebarW + 32, w = getWidth() - x0 - 32, right = x0 + w;
    closeButton.setBounds(getWidth() - 36, 6, 28, 28);
    browseButton.setBounds(right - 130, titleBarH + 64 + 26, 130, 30);
    synthCombo.setBounds(right - 320, titleBarH + 148 + 16, 320, 30);
    checkButton.setBounds(right - 130, titleBarH + 148 + 54, 130, 30);
    speedCombo.setBounds(right - 260, titleBarH + 248 + 26, 260, 30);
    progressBar.setBounds(x0, titleBarH + 350, w, 18);
    statusLabel.setBounds(x0 - 4, titleBarH + 376, w + 8, getHeight() - footerH - titleBarH - 384);
    const int by = getHeight() - footerH + (footerH - 34) / 2;
    closeFooterButton.setBounds(right - 110, by, 110, 34);
    riskToggle.setBounds(x0 - 4, getHeight() - footerH - 48, w, 30);
    installButton.setBounds(right - 230, by, 110, 34);
}

// ─── interaction ─────────────────────────────────────────────────────────────

bool OsUpdateDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void OsUpdateDialog::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < numPages; ++i)
        if (pageButtonBounds(i).contains(e.getPosition()))
        {
            showPage(i);
            return;
        }
    draggingWindow = e.getPosition().getY() < titleBarH;
    if (draggingWindow)
        dragger.startDraggingComponent(this, e);
}

void OsUpdateDialog::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingWindow)
        dragger.dragComponent(this, e, nullptr);
}

void OsUpdateDialog::mouseUp(const juce::MouseEvent&)
{
    draggingWindow = false;
}

// Closing in the middle would leave the synth half fed: it asks first
void OsUpdateDialog::close()
{
    if (phase != Phase::Sending)
    {
        closeSelf();
        return;
    }
    juce::Component::SafePointer<OsUpdateDialog> safeThis(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Stop the update?",
        "The OS is still going to the synth. Stopping leaves it in update mode, with its old OS in the flash "
        "until another update is stored.", "Stop and close", "Keep sending", nullptr,
        juce::ModalCallbackFunction::create([safeThis](int result)
        {
            if (safeThis != nullptr && result != 0)
            {
                safeThis->phase = Phase::Idle;
                safeThis->closeSelf();
            }
        }));
}
