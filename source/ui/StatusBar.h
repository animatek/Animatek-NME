#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class StatusBar : public juce::Component
{
public:
    StatusBar();

    void setConnectionStatus(const juce::String& status, bool connected = false);
    void setVoiceCount(int count);
    void setDspLoad(float percent);
    void showMessage(const juce::String& message, int durationMs = 3000);
    void clearMessage();

    // Slim progress bar in the message area (e.g. patch fetch sections).
    // Auto-hides if no update arrives for a few seconds (stalled transfer).
    void setProgress(double fraction, const juce::String& label);
    void clearProgress();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label connectionLabel;
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
