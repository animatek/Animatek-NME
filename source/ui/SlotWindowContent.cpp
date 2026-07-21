#include "SlotWindowContent.h"
#include "AppTheme.h"

SlotWindowContent::SlotWindowContent()
{
    addAndMakeVisible(headerBar);
    addAndMakeVisible(inspector);
    addAndMakeVisible(canvas);
}

void SlotWindowContent::paint(juce::Graphics& g)
{
    g.fillAll(AppTheme::palette().backgroundPanel);

    const auto& pal = AppTheme::palette();
    auto strip = getToggleStripBounds();
    g.setColour(pal.backgroundSecondary);
    g.fillRect(strip);
    g.setColour(pal.borderColor);
    g.drawVerticalLine(strip.getRight() - 1, (float) strip.getY(), (float) strip.getBottom());

    // Chevron points toward the inspector when it's open (click collapses it)
    // and away from it when collapsed (click reopens it).
    const float cx = (float) strip.getCentreX();
    const float cy = (float) strip.getCentreY();
    const float s = 4.0f;
    juce::Path chevron;
    if (inspectorVisible)
    {
        chevron.startNewSubPath(cx + s, cy - s * 1.4f);
        chevron.lineTo(cx - s, cy);
        chevron.lineTo(cx + s, cy + s * 1.4f);
    }
    else
    {
        chevron.startNewSubPath(cx - s, cy - s * 1.4f);
        chevron.lineTo(cx + s, cy);
        chevron.lineTo(cx - s, cy + s * 1.4f);
    }
    g.setColour(pal.textSecondary);
    g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SlotWindowContent::resized()
{
    auto area = getLocalBounds();
    headerBar.setBounds(area.removeFromTop(headerBarHeight));

    toggleStripBounds_ = area.removeFromLeft(toggleStripWidth);

    inspector.setVisible(inspectorVisible);
    if (inspectorVisible)
        inspector.setBounds(area.removeFromLeft(inspectorWidth));

    canvas.setBounds(area);
}

void SlotWindowContent::mouseDown(const juce::MouseEvent& e)
{
    if (getToggleStripBounds().contains(e.getPosition()))
        setInspectorVisible(!inspectorVisible);
}

void SlotWindowContent::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(getToggleStripBounds().contains(e.getPosition())
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void SlotWindowContent::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void SlotWindowContent::setInspectorVisible(bool visible)
{
    if (inspectorVisible == visible)
        return;
    inspectorVisible = visible;
    resized();
    repaint();
}
