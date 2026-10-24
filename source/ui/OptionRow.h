#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AppTheme.h"

// A row of the options pages: the setting's name and what it does on the left, the switch (or the
// round radio, for a choice of one among several) on the right. The whole row is the button.
class OptionRow : public juce::ToggleButton
{
public:
    OptionRow (const juce::String& title, const juce::String& description = {}, bool radioStyle = false)
        : juce::ToggleButton (title), description_ (description), radio_ (radioStyle)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
{
        const auto& pal = AppTheme::palette();
        const auto r = getLocalBounds();
        const bool on = getToggleState();
        const float fade = isEnabled() ? 1.0f : 0.45f;
    
        if (showRule_)
        {
            g.setColour (pal.borderColor.withAlpha (0.5f * fade));
            g.fillRect (0, r.getBottom() - 1, r.getWidth(), 1);
        }
    
        // The control at the right: a switch, or a round radio
        const float cy = static_cast<float> (r.getCentreY());
        if (radio_)
        {
            const auto ring = juce::Rectangle<float> (static_cast<float> (r.getRight()) - 28.0f, cy - 10.0f, 20.0f, 20.0f);
            g.setColour ((on ? pal.accentActive : highlighted ? pal.textSecondary : pal.borderColor).withAlpha (fade));
            g.drawEllipse (ring.reduced (1.0f), 2.0f);
            if (on)
            {
                g.setColour (pal.accentActive.withAlpha (fade));
                g.fillEllipse (ring.reduced (6.0f));
            }
        }
        else
        {
            const auto track = juce::Rectangle<float> (static_cast<float> (r.getRight()) - 52.0f, cy - 11.0f, 44.0f, 22.0f);
            g.setColour ((on ? pal.accentActive : pal.inputBackground).withAlpha (fade));
            g.fillRoundedRectangle (track, 11.0f);
            if (! on)
            {
                g.setColour (pal.borderColor.withAlpha (fade));
                g.drawRoundedRectangle (track.reduced (0.5f), 11.0f, 1.0f);
            }
            const float thumbX = on ? track.getRight() - 20.0f : track.getX() + 2.0f;
            g.setColour ((on ? pal.backgroundMain : pal.textSecondary).withAlpha (fade));
            g.fillEllipse (thumbX, cy - 9.0f, 18.0f, 18.0f);
        }
    
        // The words
        const int textW = r.getWidth() - 80;
        g.setColour (pal.textPrimary.withAlpha (fade));
        g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
        if (description_.isEmpty())
        {
            g.drawText (getButtonText(), 0, 0, textW, r.getHeight() - 1, juce::Justification::centredLeft, true);
            return;
        }
        g.drawText (getButtonText(), 0, 12, textW, 22, juce::Justification::centredLeft, true);
        g.setColour (pal.textMuted.withAlpha (fade));
        g.setFont (juce::Font (AppTheme::uiFont (12.5f)));
        g.drawFittedText (description_, 0, 35, textW, r.getHeight() - 42, juce::Justification::topLeft, 2, 1.0f);
    }


    // The rule under the row; the last row of a list has the footer's below it instead
    void setShowRule (bool show) { showRule_ = show; repaint(); }

private:
    bool showRule_ = true;
    juce::String description_;
    bool radio_;
};
