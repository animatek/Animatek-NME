#include "SysexMonitorWindow.h"
#include "AppTheme.h"

namespace
{
const juce::Colour& bg()      { return AppTheme::palette().backgroundMain; }
const juce::Colour& panelBg() { return AppTheme::palette().backgroundPanel; }

juce::String formatEntry(const MidiMonitor::Entry& e, double firstMs)
{
    juce::String line;
    line << "[" << juce::String(e.timeMs - firstMs, 1).paddedLeft(' ', 9) << " ms]  "
         << (e.dir == MidiMonitor::Direction::Tx ? "TX" : "RX")
         << "  " << juce::String(static_cast<int>(e.bytes.size())).paddedLeft(' ', 4) << " B  ";

    juce::String hex;
    for (auto b : e.bytes)
        hex << juce::String::toHexString(b).paddedLeft('0', 2) << " ";
    return line + hex.trimEnd();
}
}

// ============================================================================
// SysexMonitorPanel
// ============================================================================
SysexMonitorPanel::SysexMonitorPanel()
{
    enableToggle.setToggleState(true, juce::dontSendNotification);
    enableToggle.onClick = [this] { setMonitoring(enableToggle.getToggleState()); };
    enableToggle.setTooltip("Capture MIDI SysEx traffic. Off = zero overhead.");

    showTxToggle.setToggleState(true, juce::dontSendNotification);
    showRxToggle.setToggleState(true, juce::dontSendNotification);
    autoScroll.setToggleState(true, juce::dontSendNotification);
    for (auto* t : { &showTxToggle, &showRxToggle, &autoScroll })
        t->onClick = [] {};  // filters consulted live in appendEntries

    clearButton.onClick = [this] {
        MidiMonitor::instance().clear();
        logView.clear();
        lineCount_ = 0;
        firstTimeMs_ = 0.0;
        cursor_ = 0;  // MidiMonitor keeps seq monotonic; re-fetch from current tail
        statsLabel.setText("0 messages", juce::dontSendNotification);
    };
    copyButton.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard(logView.getText());
    };

    statsLabel.setJustificationType(juce::Justification::centredRight);
    statsLabel.setFont(juce::FontOptions(11.0f));
    statsLabel.setText("0 messages", juce::dontSendNotification);

    logView.setMultiLine(true);
    logView.setReadOnly(true);
    logView.setScrollbarsShown(true);
    logView.setCaretVisible(false);
    logView.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    for (auto* c : { (juce::Component*)&enableToggle, (juce::Component*)&showTxToggle,
                     (juce::Component*)&showRxToggle, (juce::Component*)&autoScroll,
                     (juce::Component*)&clearButton, (juce::Component*)&copyButton,
                     (juce::Component*)&statsLabel, (juce::Component*)&logView })
        addAndMakeVisible(*c);

    applyTheme();
    setSize(640, 420);
}

SysexMonitorPanel::~SysexMonitorPanel()
{
    setActive(false);
}

void SysexMonitorPanel::applyTheme()
{
    auto& pal = AppTheme::palette();
    auto styleToggle = [&pal](juce::ToggleButton& t) {
        t.setColour(juce::ToggleButton::textColourId, pal.textSecondary);
        t.setColour(juce::ToggleButton::tickColourId, pal.accentActive);
    };
    for (auto* t : { &enableToggle, &showTxToggle, &showRxToggle, &autoScroll })
        styleToggle(*t);

    auto styleBtn = [&pal](juce::TextButton& b) {
        b.setColour(juce::TextButton::buttonColourId, pal.buttonBackground);
        b.setColour(juce::TextButton::textColourOffId, pal.textSecondary);
    };
    styleBtn(clearButton);
    styleBtn(copyButton);

    statsLabel.setColour(juce::Label::textColourId, pal.textMuted);

    logView.setColour(juce::TextEditor::backgroundColourId, panelBg());
    logView.setColour(juce::TextEditor::textColourId, pal.textPrimary);
    logView.setColour(juce::TextEditor::outlineColourId, pal.borderColor);
    repaint();
}

void SysexMonitorPanel::paint(juce::Graphics& g)
{
    g.fillAll(bg());
}

void SysexMonitorPanel::resized()
{
    auto r = getLocalBounds().reduced(8);
    auto top = r.removeFromTop(26);
    enableToggle.setBounds(top.removeFromLeft(74));
    top.removeFromLeft(8);
    showTxToggle.setBounds(top.removeFromLeft(50));
    showRxToggle.setBounds(top.removeFromLeft(50));
    autoScroll.setBounds(top.removeFromLeft(100));
    copyButton.setBounds(top.removeFromRight(64));
    top.removeFromRight(6);
    clearButton.setBounds(top.removeFromRight(64));
    top.removeFromRight(10);
    statsLabel.setBounds(top);

    r.removeFromTop(6);
    logView.setBounds(r);
}

void SysexMonitorPanel::setActive(bool active)
{
    if (active)
    {
        setMonitoring(enableToggle.getToggleState());
        if (!isTimerRunning())
            startTimerHz(12);
    }
    else
    {
        stopTimer();
        setMonitoring(false);  // stop capturing while hidden -> zero overhead
    }
}

void SysexMonitorPanel::setMonitoring(bool on)
{
    MidiMonitor::instance().setEnabled(on);
    if (on && !isTimerRunning() && isShowing())
        startTimerHz(12);
}

void SysexMonitorPanel::timerCallback()
{
    appendEntries();
}

void SysexMonitorPanel::appendEntries()
{
    std::vector<MidiMonitor::Entry> fresh;
    MidiMonitor::instance().fetchSince(cursor_, fresh);
    if (fresh.empty())
        return;

    const bool showTx = showTxToggle.getToggleState();
    const bool showRx = showRxToggle.getToggleState();

    juce::String chunk;
    for (const auto& e : fresh)
    {
        if (e.dir == MidiMonitor::Direction::Tx && !showTx) continue;
        if (e.dir == MidiMonitor::Direction::Rx && !showRx) continue;
        if (lineCount_ == 0 && firstTimeMs_ == 0.0)
            firstTimeMs_ = e.timeMs;
        chunk << formatEntry(e, firstTimeMs_) << juce::newLine;
        ++lineCount_;
    }

    if (chunk.isNotEmpty())
    {
        logView.moveCaretToEnd();
        logView.insertTextAtCaret(chunk);
        if (autoScroll.getToggleState())
            logView.moveCaretToEnd();
    }
    statsLabel.setText(juce::String(lineCount_) + " messages", juce::dontSendNotification);
}

// ============================================================================
// SysexMonitorWindow
// ============================================================================
SysexMonitorWindow::SysexMonitorWindow()
    : juce::DocumentWindow("SysEx Monitor", AppTheme::palette().backgroundMain,
                           juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setResizable(true, false);
    setContentNonOwned(&panel, true);
    setResizeLimits(420, 240, 1600, 1200);
}

void SysexMonitorWindow::closeButtonPressed()
{
    setVisible(false);
    if (onClosed) onClosed();
}

void SysexMonitorWindow::setVisible(bool shouldBeVisible)
{
    juce::DocumentWindow::setVisible(shouldBeVisible);
    panel.setActive(shouldBeVisible);
}
