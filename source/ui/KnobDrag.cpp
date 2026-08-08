#include "KnobDrag.h"

#include <cmath>

namespace KnobDrag
{
namespace
{
    Mode currentMode = Mode::Horizontal;

    // One drag at a time, so one set of state is enough.
    bool captured = false;
    juce::Point<float> grabScreenPos;   // where the control was picked up
    juce::Point<float> lastScreenPos;   // where the pointer was last seen
    juce::Point<float> travelled;       // total movement since the grab

    // Circular mode: where the pointer sits relative to the knob's centre, and
    // whether the drag has moved far enough to start answering.
    juce::Point<float> grabOffsetFromCentre;
    bool circularArmed = false;

    // Warp back to the middle of the display once the pointer is this close to
    // the edge, so there is always room for the next movement.
    constexpr float edgeMargin = 8.0f;

    // A single mouse movement is never this large. Anything bigger is the
    // pointer arriving from a warp we have already accounted for, or an event
    // that overtook one, so it is dropped rather than counted as a sweep.
    constexpr float warpJump = 300.0f;

    // Closer to the centre than this the angle says nothing, so it is ignored.
    constexpr float minRadius = 8.0f;

    // Circular sets the knob to wherever the pointer is, so a plain click would
    // move it. Wait for this much movement before answering: a click does not
    // move, and neither does the first half of a double-click, which is what
    // made a double-click visibly jump before settling on the default value.
    constexpr float armDistance = 4.0f;

    // Knobs are drawn sweeping from -135 to +135 degrees, so read them back over
    // the same arc. Straight down is the dead zone either side of it.
    constexpr float halfSweep = juce::MathConstants<float>::pi * 0.75f;

    // 0 straight up, growing clockwise, so it reads like a knob.
    float angleOf (juce::Point<float> v)
    {
        return std::atan2 (v.x, -v.y);
    }

    juce::Rectangle<float> displayAreaFor (juce::Point<float> screenPos)
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        if (auto* d = displays.getDisplayForPoint (screenPos.roundToInt()))
            return d->userArea.toFloat();
        return displays.getPrimaryDisplay() != nullptr
            ? displays.getPrimaryDisplay()->userArea.toFloat()
            : juce::Rectangle<float>();
    }
}

void setMode (int index)
{
    currentMode = static_cast<Mode> (juce::jlimit (0, 2, index));
}

Mode getMode()
{
    return currentMode;
}

void begin (const juce::MouseEvent& e, juce::Component& owner, juce::Point<int> grabOffset)
{
    if (captured)
        return;

    grabScreenPos = e.source.getScreenPosition();
    lastScreenPos = grabScreenPos;
    travelled = {};

    // Grabbing the very middle of a knob leaves no direction to start from, so
    // push the starting point out to where the angle means something.
    grabOffsetFromCentre = grabOffset.toFloat();
    const float r = grabOffsetFromCentre.getDistanceFromOrigin();
    if (r < minRadius)
        grabOffsetFromCentre = r > 0.001f
            ? grabOffsetFromCentre * (minRadius / r)
            : juce::Point<float> (0.0f, -minRadius);

    captured = true;
    circularArmed = false;

    // Hiding the pointer and letting it run off the screen is what the linear
    // modes want, and it is what issue #46 asks for. Circular is the opposite
    // case: where the pointer sits around the knob IS the control, so hiding it
    // leaves you circling your own hand, and warping it away from the edge puts
    // the real pointer somewhere the reading no longer matches. Leave it alone
    // there; a knob you are circling never needs to leave the screen anyway.
    if (currentMode != Mode::Circular)
        owner.setMouseCursor (juce::MouseCursor::NoCursor);
}

void end (const juce::MouseEvent& e, juce::Component& owner)
{
    if (! captured)
        return;

    captured = false;

    // Circular never took the pointer away, so there is nothing to give back:
    // putting it back on the knob would undo a move the user made on purpose.
    if (currentMode == Mode::Circular)
        return;

    owner.setMouseCursor (juce::MouseCursor::NormalCursor);
    // MouseEvent::source is a const member and setScreenPosition is not const;
    // the wrapper is a lightweight handle, so copy it, as juce::Slider does.
    auto source = e.source;
    source.setScreenPosition (grabScreenPos);
}

juce::Point<int> travel (const juce::MouseEvent& e)
{
    if (! captured)
        return {};

    const auto now = e.source.getScreenPosition();
    const auto step = now - lastScreenPos;

    if (std::abs (step.x) < warpJump && std::abs (step.y) < warpJump)
        travelled += step;

    // See begin(): circular reads the pointer's real position, so it must not
    // be moved out from under the user.
    const auto area = currentMode == Mode::Circular ? juce::Rectangle<float>()
                                                    : displayAreaFor (now);
    if (! area.isEmpty()
        && (now.x <= area.getX() + edgeMargin || now.x >= area.getRight()  - edgeMargin
         || now.y <= area.getY() + edgeMargin || now.y >= area.getBottom() - edgeMargin))
    {
        const auto centre = area.getCentre();
        auto source = e.source;
        source.setScreenPosition (centre);
        lastScreenPos = centre;
    }
    else
    {
        lastScreenPos = now;
    }

    return travelled.roundToInt();
}

int valueFor (juce::Point<int> travel, int startValue, int minValue, int maxValue)
{
    const int range = maxValue - minValue;
    if (range <= 0)
        return startValue;

    if (currentMode == Mode::Circular)
    {
        // Absolute, not relative: the knob goes to the point it was grabbed by
        // and follows the pointer from there. Grabbing a knob at the spot that
        // reads 100 sets it to 100, rather than turning it up by 100, which is
        // how the original editor's dials answer the mouse.
        if (! circularArmed)
        {
            if (travel.toFloat().getDistanceFromOrigin() < armDistance)
                return startValue;
            circularArmed = true;
        }

        const auto here = grabOffsetFromCentre + travel.toFloat();
        if (here.getDistanceFromOrigin() < minRadius)
            return startValue;   // no direction to read this close to the centre

        const float a = angleOf (here);
        if (a >  halfSweep) return maxValue;   // past the dead zone, either side
        if (a < -halfSweep) return minValue;   // of straight down

        const float norm = (a + halfSweep) / (halfSweep * 2.0f);
        return juce::jlimit (minValue, maxValue,
                             minValue + juce::roundToInt (norm * static_cast<float> (range)));
    }

    // Half a unit per pixel, the feel the canvas knobs have always had.
    const int rawDelta = currentMode == Mode::Vertical ? -travel.y : travel.x;
    return juce::jlimit (minValue, maxValue,
                         startValue + static_cast<int> (rawDelta * 0.5f));
}
}
