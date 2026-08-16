#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The pair of nudge buttons the original editor pops under the control the
// pointer is resting on: the left one takes the value down a step, the right
// one up, and holding either repeats. A knob packs its whole range into a few
// pixels, so landing on an exact frequency or MIDI note by dragging is
// guesswork; this is how you walk onto the number you actually want.
//
// It lives here rather than in the canvas because the morph dials in the header
// bar want the same thing, and the two had already grown separate copies of the
// knob drag once (issue #47). The host keeps whatever identifies the control it
// is pointing at and does the actual stepping in a callback; everything below
// is the part that would otherwise be written twice.
class ValueSpinner
{
public:
    // Where the pair sits relative to the control it belongs to.
    enum class Placement
    {
        BelowEdge,  // straddling its bottom edge, the way the original draws them
        Inside      // in its lower half, for a dial with nothing but a caption below
    };

    explicit ValueSpinner (juce::Component& owner) : owner_ (owner) {}

    ~ValueSpinner()
    {
        // The repeat holds a lambda over the host; a tick after it is gone would
        // step a parameter through a dangling pointer.
        repeat_.stopTimer();
        repeat_.onTick = nullptr;
    }

    // Puts the pair on a control. The key names it, so hovering the same control
    // again is not a change worth repainting for; an empty key takes the pair
    // away. Ignored while a button is held: a press outlives the hover that
    // started it, however far the pointer wanders mid-nudge.
    void showFor (const juce::String& key, juce::Rectangle<float> control, Placement placement);
    void hide() { showFor ({}, {}, Placement::BelowEdge); }

    bool isShowing() const                   { return key_.isNotEmpty(); }
    const juce::String& key() const          { return key_; }
    juce::Rectangle<float> controlBounds() const { return control_; }

    // True when the point is on one of the two buttons. The pair hangs below the
    // control it belongs to, so being on a button is not the same as being on
    // the control, and a host that forgets to ask this makes the pair vanish the
    // moment you reach for it.
    bool contains (juce::Point<float> p) const;

    // Which button the pointer is over, so it can light up. Call from mouseMove.
    void updateHover (juce::Point<float> p);

    struct Colours { juce::Colour background, border, arrow; };
    void paint (juce::Graphics& g, Colours c) const;

    /** How the host repaints one rectangle of the space the buttons are placed
        in. The canvas works in zoomed canvas coordinates and the header bar in
        its own, so the transform is the host's business. Left unset, the whole
        owner is repainted, which is what these two little buttons used to cost
        on every pointer move. */
    std::function<void (juce::Rectangle<float>)> repaintArea;

    // Takes the press if it landed on a button: steps once straight away, then
    // repeats while held. step is called with -1 or +1. Returns false when the
    // press missed, in which case the host carries on as if nothing happened.
    bool mouseDown (juce::Point<float> p, std::function<void(int)> step);

    // Closes a press. True when one was open, which is the host's cue to record
    // the whole thing as a single undo step.
    bool mouseUp();

    bool isHeld() const { return held_ >= 0; }

    // For a step that had nowhere to go: at the end of a parameter's range
    // there is no point holding the repeat open.
    void stopRepeat() { repeat_.stopTimer(); }

private:
    static constexpr int repeatDelayMs = 400;  // pause before a hold repeats
    static constexpr int repeatMs      = 70;   // and the rate once it does

    struct RepeatTimer : juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override { if (onTick) onTick(); }
    };

    void drawButton (juce::Graphics& g, juce::Rectangle<float> r, bool pointsUp,
                     bool hot, bool held, Colours c) const;

    /** Repaints wherever the pair is now and wherever it just was. Either may
        be empty, for the pair appearing or going away. */
    void repaintButtons (juce::Rectangle<float> oldDown, juce::Rectangle<float> oldUp);

    juce::Component& owner_;
    juce::String key_;
    juce::Rectangle<float> control_, down_, up_;
    int hot_  = -1;   // button under the pointer: 0 down, 1 up, -1 neither
    int held_ = -1;   // button being held down
    RepeatTimer repeat_;
};
