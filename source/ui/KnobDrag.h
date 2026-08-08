#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// How a knob answers the mouse, in one place.
//
// The canvas and the header bar had grown separate copies of this, and the
// header bar's only ever read the vertical axis, so the morph knobs ignored the
// editor's knob-control setting entirely (issue #47).
namespace KnobDrag
{
    enum class Mode { Horizontal = 0, Circular = 1, Vertical = 2 };

    void setMode (int index);
    Mode getMode();

    // Call when a knob or slider drag starts, and again when it ends.
    //
    // grabOffset is where the mouse came down relative to the centre of the
    // control, in the caller's own coordinates. Circular mode needs it: it
    // reads the knob off the pointer's angle around its centre, and without
    // knowing where the centre is it can only measure the angle around the
    // point that was clicked, which is what made it feel like it had a curve on
    // it. Pass {} for a control with no centre to speak of, such as a slider.
    //
    // The pointer is hidden for the duration and put back on the control it was
    // picked up from, the way the original editor does. end() must run on every
    // path out of a drag, or the pointer never comes back; it is a no-op when
    // no drag is open, so calling it unconditionally at the top of mouseUp is
    // the safe way to use it.
    void begin (const juce::MouseEvent& e, juce::Component& owner,
                juce::Point<int> grabOffset);
    void end   (const juce::MouseEvent& e, juce::Component& owner);

    // How far the pointer has travelled, in screen pixels, since the control
    // was picked up. Call once per mouseDrag.
    //
    // This is what lets a sweep run past the edge of the desktop (issue #46):
    // rather than reading the pointer's absolute position, which the screen
    // edge puts a stop to, we count movement and warp the pointer back to the
    // middle of the display before it runs out of room. JUCE's own
    // enableUnboundedMouseMovement cannot be used here: it re-centres on the
    // dragged component, and the canvas is a huge component inside a viewport
    // whose centre is usually off-screen, so the warp gets clamped and its
    // bookkeeping is thrown off by a screenful at a time.
    juce::Point<int> travel (const juce::MouseEvent& e);

    // Where a control sitting at startValue lands after travelling that far.
    // Callers in a zoomed space scale the travel, and the grab offset they gave
    // begin(), to match.
    //
    // Circular is absolute: it ignores startValue once the drag has moved far
    // enough to arm, and returns whatever the pointer's angle around the knob
    // reads, so grabbing a dial at the point marked 100 sets it to 100.
    int valueFor (juce::Point<int> travel, int startValue, int minValue, int maxValue);
}
