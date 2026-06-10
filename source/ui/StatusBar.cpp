#include "StatusBar.h"
#include "AppTheme.h"

StatusBar::StatusBar()
{
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(label);
    };

    setupLabel(connectionLabel, "Disconnected");
    setupLabel(voiceLabel, "");
    voiceLabel.setVisible(false);
    setupLabel(dspLabel, "");
    dspLabel.setVisible(false);

    // Message label (centered, initially hidden)
    messageLabel.setJustificationType(juce::Justification::centred);
    messageLabel.setColour(juce::Label::textColourId, AppTheme::palette().accentWarning); // Orange
    messageLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    addAndMakeVisible(messageLabel);
    messageLabel.setVisible(false);
}

void StatusBar::setConnectionStatus(const juce::String& status, bool connected)
{
    isConnected = connected;
    connectionLabel.setText(status, juce::dontSendNotification);

    // Set color based on connection state
    if (connected)
        connectionLabel.setColour(juce::Label::textColourId, AppTheme::palette().accentSuccess); // Green
    else
        connectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa)); // Gray

    repaint();
}

void StatusBar::setVoiceCount(int count)
{
    voiceLabel.setText("Voices: " + juce::String(count), juce::dontSendNotification);
}

void StatusBar::setDspLoad(float percent)
{
    dspLabel.setText("DSP: " + juce::String(percent, 1) + "%", juce::dontSendNotification);
}

void StatusBar::showMessage(const juce::String& message, int durationMs)
{
    messageLabel.setText(message, juce::dontSendNotification);
    messageLabel.setVisible(true);

    // Auto-hide after duration
    if (durationMs > 0)
        messageTimer.startTimer(durationMs);
}

void StatusBar::clearMessage()
{
    messageTimer.stopTimer();
    messageLabel.setVisible(false);
    messageLabel.setText("", juce::dontSendNotification);
}

void StatusBar::setProgress(double fraction, const juce::String& label)
{
    progressVisible = true;
    progressFraction = juce::jlimit(0.0, 1.0, fraction);
    progressText = label;

    // The progress bar replaces any transient message in the same area
    messageLabel.setVisible(false);

    // Hide on its own if updates stop coming (stalled/aborted transfer)
    progressTimer.startTimer(4000);
    repaint(progressBounds);
}

void StatusBar::clearProgress()
{
    progressTimer.stopTimer();
    if (!progressVisible)
        return;
    progressVisible = false;
    repaint(progressBounds);
}

void StatusBar::paint(juce::Graphics& g)
{
    g.fillAll(AppTheme::palette().backgroundPanel);
    g.setColour(AppTheme::palette().buttonActive);
    g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);

    // Draw LED indicator
    if (!ledBounds.isEmpty())
    {
        // Outer glow for connected state
        if (isConnected)
        {
            g.setColour(AppTheme::palette().accentSuccess.withAlpha(0.3f));
            g.fillEllipse(ledBounds.expanded(2.0f));
        }

        // LED circle
        g.setColour(isConnected ? AppTheme::palette().accentSuccess : juce::Colour(0xff555555));
        g.fillEllipse(ledBounds);

        // Highlight for 3D effect
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        auto highlightBounds = ledBounds.reduced(1.0f).translated(-0.5f, -0.5f);
        g.fillEllipse(highlightBounds.removeFromTop(ledBounds.getHeight() * 0.4f));
    }

    if (progressVisible && !progressBounds.isEmpty())
    {
        const auto track = progressBounds.toFloat().withSizeKeepingCentre(
            juce::jmin(260.0f, static_cast<float>(progressBounds.getWidth())), 8.0f);

        g.setColour(AppTheme::palette().inputBackground);
        g.fillRoundedRectangle(track, 4.0f);

        auto fill = track.reduced(1.0f);
        fill.setWidth(fill.getWidth() * static_cast<float>(progressFraction));
        g.setColour(AppTheme::palette().accentActive);
        g.fillRoundedRectangle(fill, 3.0f);

        g.setColour(AppTheme::palette().borderColor);
        g.drawRoundedRectangle(track, 4.0f, 1.0f);

        if (progressText.isNotEmpty())
        {
            g.setColour(AppTheme::palette().textSecondary);
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(progressText,
                       progressBounds.withTrimmedLeft(static_cast<int>(track.getRight() - progressBounds.getX()) + 8),
                       juce::Justification::centredLeft, true);
        }
    }
}

void StatusBar::resized()
{
    auto area = getLocalBounds().reduced(8, 0);

    // LED indicator (small circle on the left)
    auto ledSize = 10.0f;
    auto ledY = (area.getHeight() - ledSize) * 0.5f;
    ledBounds = juce::Rectangle<float>(area.getX() + 4.0f, ledY, ledSize, ledSize);

    // Connection label (more space, starts after LED)
    auto connectionArea = area.removeFromLeft(280);
    connectionArea.removeFromLeft(20);  // Space for LED
    connectionLabel.setBounds(connectionArea);

    // Right-aligned labels
    dspLabel.setBounds(area.removeFromRight(100));
    voiceLabel.setBounds(area.removeFromRight(100));

    // Message label in the center (takes remaining space), shared with the
    // progress bar
    messageLabel.setBounds(area);
    progressBounds = area;
}
