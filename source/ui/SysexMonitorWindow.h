#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../midi/MidiMonitor.h"
#include <cstdint>

/**
 * SysEx Monitor floater: a live hex log of MIDI traffic to/from the synth.
 * Useful for diagnosing protocol issues in release builds (no console) and on
 * macOS/Windows. Polling and the monitor's capture only run while the panel is
 * visible and the Enable toggle is on, so it costs nothing when not in use.
 */
class SysexMonitorPanel : public juce::Component,
                          private juce::Timer
{
public:
    SysexMonitorPanel();
    ~SysexMonitorPanel() override;

    void applyTheme();
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Start/stop capture+polling when the floater is shown/hidden.
    void setActive(bool active);

private:
    void timerCallback() override;
    void appendEntries();
    void setMonitoring(bool on);

    juce::ToggleButton enableToggle { "Enable" };
    juce::ToggleButton showTxToggle { "TX" };
    juce::ToggleButton showRxToggle { "RX" };
    juce::ToggleButton autoScroll   { "Auto-scroll" };
    juce::TextButton   clearButton  { "Clear" };
    juce::TextButton   copyButton   { "Copy" };
    juce::Label        statsLabel;
    juce::TextEditor   logView;

    std::uint64_t cursor_ = 0;
    int           lineCount_ = 0;
    double        firstTimeMs_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SysexMonitorPanel)
};

class SysexMonitorWindow : public juce::DocumentWindow
{
public:
    SysexMonitorWindow();

    void closeButtonPressed() override;
    void applyTheme() { panel.applyTheme(); }
    void setVisible(bool shouldBeVisible) override;

    std::function<void()> onClosed;

    std::function<bool(const juce::KeyPress&)> onGlobalKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        return (onGlobalKey && onGlobalKey(key)) || juce::DocumentWindow::keyPressed(key);
    }

private:
    SysexMonitorPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SysexMonitorWindow)
};
