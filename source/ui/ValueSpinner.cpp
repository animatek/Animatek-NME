#include "ValueSpinner.h"

void ValueSpinner::repaintButtons (juce::Rectangle<float> oldDown, juce::Rectangle<float> oldUp)
{
    auto area = oldDown.getUnion (oldUp).getUnion (down_).getUnion (up_);
    if (area.isEmpty())
        return;

    if (repaintArea)
        repaintArea (area.expanded (2.0f));
    else
        owner_.repaint();
}

void ValueSpinner::showFor (const juce::String& key, juce::Rectangle<float> control,
                            Placement placement)
{
    if (held_ >= 0)
        return;

    const bool sameControl = key.isNotEmpty() && key == key_;

    key_     = key;
    control_ = control;

    if (key.isEmpty())
    {
        if (hot_ != -1 || !down_.isEmpty())
        {
            const auto oldDown = down_, oldUp = up_;
            hot_  = -1;
            down_ = up_ = {};
            repaintButtons (oldDown, oldUp);
        }
        return;
    }

    const bool inside = placement == Placement::Inside;
    const float w = inside ? juce::jlimit (8.0f, 14.0f, control.getWidth()  * 0.40f)
                           : juce::jlimit (9.0f, 15.0f, control.getWidth()  * 0.44f);
    const float h = inside ? juce::jlimit (6.0f, 10.0f, control.getHeight() * 0.32f)
                           : juce::jlimit (7.0f, 12.0f, control.getHeight() * 0.34f);
    const float gap = 2.0f;
    const float cx  = control.getCentreX();

    // Below the edge is where the original puts them: clear of the knob's
    // pointer, and still inside the module rather than down in whatever occupies
    // the next row. Inside is for a dial with a caption right underneath it,
    // and lands in the dead zone at the bottom of the sweep, which no pointer
    // ever reaches.
    const float y = inside ? control.getBottom() - h - juce::jmax (1.0f, control.getHeight() * 0.06f)
                           : control.getBottom() - h * 0.55f;

    const auto oldDown = down_, oldUp = up_;
    down_ = { cx - gap * 0.5f - w, y, w, h };
    up_   = { cx + gap * 0.5f,     y, w, h };

    const int oldHot = hot_;
    if (!sameControl)
        hot_ = -1;

    // Resting on a control calls this on every pointer move. Repainting each
    // time was a redraw of the whole canvas per mouse event, for a pair of
    // buttons that had not moved: this is what the class always claimed to do
    // ("hovering the same control again is not a change worth repainting for")
    // and now actually does.
    if (down_ == oldDown && up_ == oldUp && hot_ == oldHot)
        return;

    repaintButtons (oldDown, oldUp);
}

bool ValueSpinner::contains (juce::Point<float> p) const
{
    return isShowing() && (down_.contains (p) || up_.contains (p));
}

void ValueSpinner::updateHover (juce::Point<float> p)
{
    if (held_ >= 0)
        return;

    const int was = hot_;
    hot_ = !isShowing()      ? -1
         : down_.contains (p) ? 0
         : up_.contains (p)   ? 1 : -1;

    if (hot_ != was)
        repaintButtons (down_, up_);
}

void ValueSpinner::drawButton (juce::Graphics& g, juce::Rectangle<float> r, bool pointsUp,
                               bool hot, bool held, Colours c) const
{
    g.setColour (held ? c.background.darker (0.35f)
                      : hot ? c.background.brighter (0.30f)
                            : c.background);
    g.fillRect (r);
    g.setColour (c.border);
    g.drawRect (r, 1.0f);

    const auto in = r.reduced (r.getWidth() * 0.30f, r.getHeight() * 0.32f);
    juce::Path tri;
    if (pointsUp)
        tri.addTriangle (in.getX(), in.getBottom(), in.getRight(), in.getBottom(),
                         in.getCentreX(), in.getY());
    else
        tri.addTriangle (in.getX(), in.getY(), in.getRight(), in.getY(),
                         in.getCentreX(), in.getBottom());

    g.setColour (c.arrow);
    g.fillPath (tri);
}

void ValueSpinner::paint (juce::Graphics& g, Colours c) const
{
    if (!isShowing())
        return;

    drawButton (g, down_, false, hot_ == 0, held_ == 0, c);
    drawButton (g, up_,   true,  hot_ == 1, held_ == 1, c);
}

bool ValueSpinner::mouseDown (juce::Point<float> p, std::function<void(int)> step)
{
    if (!isShowing() || !step)
        return false;

    const int which = down_.contains (p) ? 0 : up_.contains (p) ? 1 : -1;
    if (which < 0)
        return false;

    held_ = which;
    hot_  = which;

    const int delta = which == 1 ? 1 : -1;
    step (delta);

    // Hold to repeat, after the pause a click needs in order to stay a single
    // step. The first tick swaps the timer over to the faster rate.
    repeat_.onTick = [this, delta, step]
    {
        repeat_.startTimer (repeatMs);
        step (delta);
    };
    repeat_.startTimer (repeatDelayMs);
    return true;
}

bool ValueSpinner::mouseUp()
{
    if (held_ < 0)
        return false;

    held_ = -1;
    repeat_.stopTimer();
    repeat_.onTick = nullptr;
    owner_.repaint();
    return true;
}
