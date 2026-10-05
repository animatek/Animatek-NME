#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Where the two partial-ratio arrow buttons (< and >) of a text display sit.
// The drawing and the hit test both ask here, so they can never disagree.
// Rectangles are relative to the module, like the theme's own coordinates.
//
// By default the arrows are a two-halves bar directly under the display, as the
// original editor has them. The OscSineBank is the exception: under each ratio
// window it prints a "Tune" label, which the bar hid, so there the two arrows
// become separate buttons either side of the tune knob below the window.
struct PartialArrowRects
{
    juce::Rectangle<float> left, right;
    bool besideKnob = false;   // true: two separate buttons, false: one two-halves bar
};

template <class Theme, class TextDisplay>
inline PartialArrowRects partialArrowRects(const Theme& theme, const TextDisplay& td)
{
    const float dh      = static_cast<float>(td.height);
    const float renderH = juce::jmin(dh, 13.0f);
    const float renderY = static_cast<float>(td.y) + (dh - renderH) * 0.5f;
    const float arrowY  = renderY + renderH + 1.0f;
    const float arrowH  = 8.0f;
    const float x       = static_cast<float>(td.x);
    const float w       = static_cast<float>(td.width);

    PartialArrowRects r;
    r.left  = juce::Rectangle<float>(x,            arrowY, w * 0.5f, arrowH);
    r.right = juce::Rectangle<float>(x + w * 0.5f, arrowY, w * 0.5f, arrowH);

    if (theme.componentId == "m106")   // OscSineBank
    {
        // The knob this column's arrows belong to: the first knob below the
        // display whose centre lies within the display's width.
        int best = -1;
        for (size_t i = 0; i < theme.knobs.size(); ++i)
        {
            const auto& tk = theme.knobs[i];
            const float kcx = static_cast<float>(tk.x) + static_cast<float>(tk.size) * 0.5f;
            if (kcx < x || kcx > x + w)
                continue;
            if (static_cast<float>(tk.y) < static_cast<float>(td.y + td.height) - 2.0f)
                continue;
            if (best < 0 || tk.y < theme.knobs[static_cast<size_t>(best)].y)
                best = static_cast<int>(i);
        }

        if (best >= 0)
        {
            const auto& tk = theme.knobs[static_cast<size_t>(best)];
            const float kx = static_cast<float>(tk.x);
            const float ky = static_cast<float>(tk.y);
            const float ks = static_cast<float>(tk.size);
            const float bw = 9.0f, bh = 12.0f, gap = 1.0f, edge = 2.0f;
            const float by = ky + (ks - bh) * 0.5f;

            // Beside the knob, but never closer than `edge` to either side of the
            // module: the first column's knob is so near the left edge that the
            // button would otherwise hang out over it.
            float lx = kx - gap - bw;
            float rx = kx + ks + gap;
            const float modW = static_cast<float>(theme.width);
            if (lx < edge)             lx = edge;
            if (rx + bw > modW - edge) rx = modW - edge - bw;

            r.left  = juce::Rectangle<float>(lx, by, bw, bh);
            r.right = juce::Rectangle<float>(rx, by, bw, bh);
            r.besideKnob = true;
        }
    }
    return r;
}
