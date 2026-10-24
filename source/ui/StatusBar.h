#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// A panel's switch in the status bar, the way Bitwig keeps its own at the foot of the window: an "i"
// for the inspector, a magnifying glass for the browser. Lit in the accent while the panel is open.
class PanelIconButton : public juce::Button
{
public:
    enum class Icon { Inspector, Browser };
    explicit PanelIconButton(Icon i);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
private:
    Icon icon;
};

class StatusBar : public juce::Component
{
public:
    StatusBar();

    // The inspector and browser switches; setPanelStates lights them to match the panels
    std::function<void()> onInspectorClicked, onBrowserClicked;
    void setPanelStates(bool inspectorOpen, bool browserOpen);

    void setConnectionStatus(const juce::String& status, bool connected = false);
    // Where the editor is connected: "Real G1 (port)", "G1-Emu (port)", "Not connected".
    void setSynthLink(const juce::String& text);
    void setVoiceCount(int count);
    void setDspLoad(float percent);
    void showMessage(const juce::String& message, int durationMs = 3000);
    void clearMessage();

    // Slim progress bar in the message area (e.g. patch fetch sections).
    // Auto-hides if no update arrives for a few seconds (stalled transfer).
    void setProgress(double fraction, const juce::String& label);
    void clearProgress();

    // Re-reads the palette. The label colours are set on juce::Labels rather
    // than painted, so a theme change has to hand them the new ink; without
    // this the bar kept the colours of whichever theme was up at startup.
    void applyTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;
    // A click on a transient message dismisses it (issue #65).
    void mouseDown(const juce::MouseEvent& e) override;

private:
    PanelIconButton inspectorButton { PanelIconButton::Icon::Inspector };
    PanelIconButton browserButton   { PanelIconButton::Icon::Browser };
    juce::Label connectionLabel;
    juce::Label linkLabel;
    juce::Label voiceLabel;
    juce::Label dspLabel;
    juce::Label messageLabel;  // Temporary status messages

    bool isConnected = false;
    juce::Rectangle<float> ledBounds;

    bool progressVisible = false;
    double progressFraction = 0.0;
    juce::String progressText;
    juce::Rectangle<int> progressBounds;

    // Auto-hide timer for messages
    class MessageTimer : public juce::Timer
    {
    public:
        explicit MessageTimer(StatusBar& sb) : statusBar(sb) {}
        void timerCallback() override { statusBar.clearMessage(); }
    private:
        StatusBar& statusBar;
    } messageTimer { *this };

    class ProgressTimer : public juce::Timer
    {
    public:
        explicit ProgressTimer(StatusBar& sb) : statusBar(sb) {}
        void timerCallback() override { statusBar.clearProgress(); }
    private:
        StatusBar& statusBar;
    } progressTimer { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
