#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Borderless close button: gold 'x', no JUCE LookAndFeel chrome
class FlatCloseButton : public juce::Component
{
public:
    std::function<void()> onClick;

    FlatCloseButton()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        if (isMouseOver())
        {
            g.setColour (juce::Colour (0xff25282E));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
        }
        g.setColour (isMouseOver() ? juce::Colours::white : juce::Colour (0xffffcc44));
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText ("x", getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};
