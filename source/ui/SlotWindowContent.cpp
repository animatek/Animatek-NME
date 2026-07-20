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
}

void SlotWindowContent::resized()
{
    auto area = getLocalBounds();
    headerBar.setBounds(area.removeFromTop(headerBarHeight));
    inspector.setBounds(area.removeFromLeft(inspectorWidth));
    canvas.setBounds(area);
}
