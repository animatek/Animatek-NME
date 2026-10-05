// Hardware look drawing code: Grant Atkinson (teezdalien), 2026. Written with an AI
// assistant from general 2D-drawing techniques; no third-party code or assets.
// Contributed to Animatek NME under the project licence (GPL).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

// Skeuomorphic drawing helpers for the patch canvas. Header-only on purpose, so
// no CMakeLists change is needed. Every function draws in canvas (logical)
// coordinates, exactly where the flat version draws, so hit-testing, cable
// anchors and the connector-to-knob lines are untouched.
//
// Light comes from the top-left throughout: lit edges are on the top and left,
// shaded edges on the bottom and right, and shadows fall down and to the right.
namespace skeuo
{
    // ------------------------------------------------------------------ panels

    // A fine horizontal streak texture, tiled over faceplates for a brushed-metal
    // grain. Built once; each pixel is a translucent white or black speck.
    inline const juce::Image& brushedTexture()
    {
        static const juce::Image img = []
        {
            juce::Image im(juce::Image::ARGB, 64, 64, true);
            juce::Random rnd(0x4e4d45);
            juce::Image::BitmapData bd(im, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < 64; ++y)
            {
                const float rowBias = rnd.nextFloat() - 0.5f;          // streak per row
                for (int x = 0; x < 64; ++x)
                {
                    const float n = rowBias * 0.5f + (rnd.nextFloat() - 0.5f) * 0.12f;
                    const auto a = static_cast<juce::uint8>(
                        juce::jlimit(0, 255, static_cast<int>(std::abs(n) * 60.0f)));
                    bd.setPixelColour(x, y, n > 0.0f ? juce::Colour::fromRGBA(255, 255, 255, a)
                                                     : juce::Colour::fromRGBA(0, 0, 0, a));
                }
            }
            return im;
        }();
        return img;
    }

    // Soft shadow under a module. Called for every visible module BEFORE any
    // module body is drawn, so a neighbour's shadow can never land on top of a
    // body that happens to be painted earlier.
    inline void drawModuleShadow(juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (int i = 0; i < 4; ++i)
        {
            g.setColour(juce::Colours::black.withAlpha(0.085f));
            g.fillRoundedRectangle(r.translated(1.5f, 2.5f).expanded(static_cast<float>(i) * 1.1f),
                                   4.0f + static_cast<float>(i));
        }
    }

    // Module faceplate: a vertical gradient over the module colour, a brushed
    // grain, and a bevelled edge (lit top and left, shaded bottom and right).
    inline void drawFaceplate(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base)
    {
        const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

        juce::ColourGradient body(base.brighter(0.10f), x, y,
                                  base.darker(0.12f),   x, y + h, false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(r, 3.0f);

        g.setTiledImageFill(brushedTexture(), 0, 0, 1.0f);
        g.fillRoundedRectangle(r, 3.0f);

        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.drawLine(x + 3.0f, y + 1.0f, x + w - 3.0f, y + 1.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.24f));
        g.drawLine(x + 1.0f, y + 3.0f, x + 1.0f, y + h - 3.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.drawLine(x + 3.0f, y + h - 1.0f, x + w - 3.0f, y + h - 1.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.drawLine(x + w - 1.0f, y + 3.0f, x + w - 1.0f, y + h - 3.0f, 1.0f);
    }

    // A group box as a shallow inset panel: shaded top and left, lit bottom and right.
    inline void drawInsetPanel(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base)
    {
        const float x = r.getX(), y = r.getY(), x2 = r.getRight(), y2 = r.getBottom();

        g.setColour(base.darker(0.10f));
        g.fillRoundedRectangle(r, 3.0f);

        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.drawLine(x + 3.0f, y + 1.0f, x2 - 3.0f, y + 1.0f, 1.0f);
        g.drawLine(x + 1.0f, y + 3.0f, x + 1.0f, y2 - 3.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.drawLine(x + 3.0f, y2 - 1.0f, x2 - 3.0f, y2 - 1.0f, 1.0f);
        g.drawLine(x2 - 1.0f, y + 3.0f, x2 - 1.0f, y2 - 3.0f, 1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
    }

    // A value display as a recessed glass window: a dark bezel with a lit lower
    // lip, a faintly graded screen, and a soft sheen across the top half.
    inline void drawDisplayGlass(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour screen)
    {
        const auto bezel = r.expanded(1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(bezel, 2.5f);

        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawLine(bezel.getX() + 2.0f, bezel.getBottom() - 0.5f,
                   bezel.getRight() - 2.0f, bezel.getBottom() - 0.5f, 1.0f);
        g.drawLine(bezel.getRight() - 0.5f, bezel.getY() + 2.0f,
                   bezel.getRight() - 0.5f, bezel.getBottom() - 2.0f, 1.0f);

        juce::ColourGradient face(screen.brighter(0.08f), r.getX(), r.getY(),
                                  screen.darker(0.15f),   r.getX(), r.getBottom(), false);
        g.setGradientFill(face);
        g.fillRoundedRectangle(r, 1.5f);

        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.16f), r.getX(), r.getY(),
                                   juce::Colours::white.withAlpha(0.0f),  r.getX(), r.getY() + r.getHeight() * 0.55f, false);
        g.setGradientFill(sheen);
        g.fillRoundedRectangle(r.withHeight(r.getHeight() * 0.55f), 1.5f);
    }


    // ------------------------------------------------------- small controls

    // A raised pad (increment arrows and similar): vertical gradient, lit top and
    // left edges, shaded bottom and right.
    inline void drawRaisedPad(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base)
    {
        const float x = r.getX(), y = r.getY(), x2 = r.getRight(), y2 = r.getBottom();

        juce::ColourGradient face(base.brighter(0.18f), x, y, base.darker(0.18f), x, y2, false);
        g.setGradientFill(face);
        g.fillRect(r);

        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.drawLine(x, y + 0.5f, x2, y + 0.5f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawLine(x + 0.5f, y, x + 0.5f, y2, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawLine(x, y2 - 0.5f, x2, y2 - 0.5f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.drawLine(x2 - 0.5f, y, x2 - 0.5f, y2, 1.0f);
    }

    // A slider track as a routed slot: a recessed bed with a groove down the middle.
    inline void drawSliderTrack(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, bool vertical)
    {
        const float x = r.getX(), y = r.getY(), x2 = r.getRight(), y2 = r.getBottom();

        g.setColour(base.darker(0.25f));
        g.fillRect(r);

        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawLine(x, y + 0.5f, x2, y + 0.5f, 1.0f);
        g.drawLine(x + 0.5f, y, x + 0.5f, y2, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawLine(x, y2 - 0.5f, x2, y2 - 0.5f, 1.0f);
        g.drawLine(x2 - 0.5f, y, x2 - 0.5f, y2, 1.0f);

        if (vertical)
        {
            const float cx = r.getCentreX();
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawLine(cx, y + 3.0f, cx, y2 - 3.0f, 1.2f);
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawLine(cx + 1.0f, y + 3.0f, cx + 1.0f, y2 - 3.0f, 1.0f);
        }
        else
        {
            const float cy = r.getCentreY();
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawLine(x + 3.0f, cy, x2 - 3.0f, cy, 1.2f);
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawLine(x + 3.0f, cy + 1.0f, x2 - 3.0f, cy + 1.0f, 1.0f);
        }
    }

    // A fader cap: a small raised block with a drop shadow, lit from above, and
    // a centre line across it where it is big enough to hold one. `cap` is the
    // theme's grip colour, or the morph group's colour when one is assigned.
    inline void drawSliderGrip(juce::Graphics& g, juce::Rectangle<float> r, bool vertical, juce::Colour cap)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(r.translated(0.6f, 0.9f));

        juce::ColourGradient body(cap.brighter(0.35f), r.getX(), r.getY(),
                                  cap.darker(0.35f),   r.getX(), r.getBottom(), false);
        g.setGradientFill(body);
        g.fillRect(r);

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRect(r, 0.8f);

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        if (vertical && r.getHeight() >= 5.0f)
            g.drawLine(r.getX() + 1.5f, r.getCentreY(), r.getRight() - 1.5f, r.getCentreY(), 0.8f);
        else if (!vertical && r.getWidth() >= 5.0f)
            g.drawLine(r.getCentreX(), r.getY() + 1.5f, r.getCentreX(), r.getBottom() - 1.5f, 0.8f);
    }

    // An LED as a lens in a dark bezel with a specular spot. Lit LEDs get an
    // inner glow. Everything stays inside `r`, so a repaint clipped to the LED's
    // own rectangle can never leave a stale halo behind.
    inline void drawLed(juce::Graphics& g, juce::Rectangle<float> r, bool on,
                        juce::Colour onColour, juce::Colour offColour)
    {
        const float d  = juce::jmax(1.0f, juce::jmin(r.getWidth(), r.getHeight()));
        const float cx = r.getCentreX(), cy = r.getCentreY();

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(r);

        if (on)
        {
            juce::ColourGradient glow(onColour.withAlpha(0.55f), cx, cy,
                                      onColour.withAlpha(0.0f),  cx + d * 0.5f, cy, true);
            g.setGradientFill(glow);
            g.fillEllipse(r);
        }

        const auto lens = r.reduced(juce::jmax(0.6f, d * 0.14f));
        juce::ColourGradient body(on ? onColour.brighter(0.6f) : offColour.brighter(0.25f),
                                  lens.getX() + lens.getWidth() * 0.35f, lens.getY() + lens.getHeight() * 0.30f,
                                  on ? onColour.darker(0.3f) : offColour.darker(0.35f),
                                  lens.getRight(), lens.getBottom(), true);
        g.setGradientFill(body);
        g.fillEllipse(lens);

        g.setColour(juce::Colours::white.withAlpha(on ? 0.55f : 0.28f));
        g.fillEllipse(lens.getX() + lens.getWidth() * 0.18f, lens.getY() + lens.getHeight() * 0.14f,
                      lens.getWidth() * 0.34f, lens.getHeight() * 0.28f);
    }


    // A level meter as an LED ladder: a row of small segments in a recessed bed,
    // coloured by position along the scale (low, mid, high) and lit up to `fill`
    // (0..1). Unlit segments stay faintly visible, as on a real ladder. Draws only
    // inside `r`, so a partial repaint can never leave a stale edge.
    inline void drawMeterLadder(juce::Graphics& g, juce::Rectangle<float> r, float fill,
                                juce::Colour bed, juce::Colour low, juce::Colour mid, juce::Colour high)
    {
        const float x = r.getX(), y = r.getY(), x2 = r.getRight(), y2 = r.getBottom();

        g.setColour(bed);
        g.fillRect(r);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawLine(x, y + 0.5f, x2, y + 0.5f, 1.0f);
        g.drawLine(x + 0.5f, y, x + 0.5f, y2, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawLine(x, y2 - 0.5f, x2, y2 - 0.5f, 1.0f);
        g.drawLine(x2 - 0.5f, y, x2 - 0.5f, y2, 1.0f);

        const auto inner = r.reduced(1.5f);
        if (inner.getWidth() < 8.0f || inner.getHeight() < 2.0f)
            return;

        const int   n     = juce::jlimit(8, 32, juce::roundToInt(inner.getWidth() / 4.0f));
        const float pitch = inner.getWidth() / static_cast<float>(n);
        const float gap   = juce::jmax(0.8f, pitch * 0.22f);

        for (int i = 0; i < n; ++i)
        {
            const float t   = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
            const auto  col = t < 0.6f ? low : (t < 0.85f ? mid : high);
            const bool  lit = (static_cast<float>(i) / static_cast<float>(n)) < fill;

            const juce::Rectangle<float> seg(inner.getX() + static_cast<float>(i) * pitch, inner.getY(),
                                             pitch - gap, inner.getHeight());

            if (lit)
            {
                juce::ColourGradient body(col.brighter(0.35f), seg.getX(), seg.getY(),
                                          col.darker(0.25f),   seg.getX(), seg.getBottom(), false);
                g.setGradientFill(body);
                g.fillRect(seg);
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawLine(seg.getX(), seg.getY() + 0.5f, seg.getRight(), seg.getY() + 0.5f, 0.8f);
            }
            else
            {
                g.setColour(col.withAlpha(0.16f));
                g.fillRect(seg);
            }
        }
    }

    // ------------------------------------------------------------------- knobs

    // Knob styles (ColorScheme::knobStyle):
    //   0  domed cap: a recessed well and a shaded dome in the theme's knob colour
    //   1  black knurled: satin black skirt and top, knurled edge, white pointer
    //   2  aluminium knurled: brushed silver, knurled edge, dark pointer
    //
    //   radius       the radius the flat knob uses (rSz * 0.5)
    //   pixelRadius  radius * zoom: knurling is only drawn when it can be seen
    //   capColour    style 0 only: the theme's knob colour
    //   collar       the morph-group colour, drawn as a ring round the cap
    inline void drawKnobBody(juce::Graphics& g, float cx, float cy, float radius,
                             int style, float pixelRadius,
                             juce::Colour capColour, bool hasCollar, juce::Colour collar)
    {
        if (style <= 0)
        {
            // Well: dark at the top-left, light at the bottom-right, so it reads
            // as a hole the cap sits in.
            {
                const float r = radius * 1.04f;
                juce::ColourGradient well(juce::Colours::black.withAlpha(0.60f), cx - r * 0.7f, cy - r * 0.7f,
                                          juce::Colours::white.withAlpha(0.28f), cx + r * 0.7f, cy + r * 0.7f, false);
                g.setGradientFill(well);
                g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            }
            {
                const float r = radius * 0.93f;
                const float w = juce::jmax(1.2f, radius * 0.13f);
                g.setColour(hasCollar ? collar : capColour.brighter(0.25f));
                g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, w);
            }
            {
                const float rc = radius * 0.84f;
                const float hx = cx - rc * 0.35f;
                const float hy = cy - rc * 0.40f;
                juce::ColourGradient cap(capColour.brighter(0.55f), hx, hy,
                                         capColour.darker(0.55f),   hx + rc * 1.35f, hy, true);
                cap.addColour(0.45, capColour);
                g.setGradientFill(cap);
                g.fillEllipse(cx - rc, cy - rc, rc * 2.0f, rc * 2.0f);

                g.setColour(capColour.darker(0.7f).withAlpha(0.7f));
                g.drawEllipse(cx - rc, cy - rc, rc * 2.0f, rc * 2.0f, 0.8f);
            }
            return;
        }

        const bool alu = (style >= 2);
        const float pi = juce::MathConstants<float>::pi;

        // The knob stands proud of the panel, so it casts a small shadow.
        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillEllipse(cx - radius * 1.02f + 0.6f, cy - radius * 1.02f + 1.2f,
                      radius * 2.04f, radius * 2.04f);

        // Skirt: the wide base the top sits on.
        {
            const float r = radius;
            juce::ColourGradient skirt(alu ? juce::Colour(0xfff4f4f4) : juce::Colour(0xff4a4a4a), cx - r * 0.7f, cy - r * 0.8f,
                                       alu ? juce::Colour(0xff7c7c7c) : juce::Colour(0xff0a0a0a), cx + r * 0.7f, cy + r * 0.8f, false);
            g.setGradientFill(skirt);
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

            // Knurling: ridges round the skirt, one path stroked once. Only when
            // the knob is big enough on screen for them to be more than noise.
            if (pixelRadius >= 9.0f)
            {
                const int n = (pixelRadius >= 18.0f) ? 56 : 32;
                juce::Path ridges;
                for (int i = 0; i < n; ++i)
                {
                    const float a = static_cast<float>(i) * 2.0f * pi / static_cast<float>(n);
                    ridges.startNewSubPath(cx + std::sin(a) * r * 0.88f, cy - std::cos(a) * r * 0.88f);
                    ridges.lineTo         (cx + std::sin(a) * r * 0.99f, cy - std::cos(a) * r * 0.99f);
                }
                g.setColour(alu ? juce::Colours::black.withAlpha(0.28f)
                                : juce::Colours::white.withAlpha(0.22f));
                g.strokePath(ridges, juce::PathStrokeType(0.7f));
            }

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.9f);
        }

        // Ring between skirt and top: the morph colour, or a dark groove.
        {
            const float r = radius * 0.80f;
            const float w = juce::jmax(1.2f, radius * 0.11f);
            g.setColour(hasCollar ? collar : juce::Colours::black.withAlpha(0.45f));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, w);
        }

        // Top: flat disc; aluminium gets faint concentric machining lines.
        {
            const float r = radius * 0.68f;
            juce::ColourGradient top(alu ? juce::Colour(0xfffafafa) : juce::Colour(0xff3c3c3c), cx - r * 0.6f, cy - r * 0.7f,
                                     alu ? juce::Colour(0xff9a9a9a) : juce::Colour(0xff111111), cx + r * 0.7f, cy + r * 0.8f, false);
            g.setGradientFill(top);
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

            if (alu && pixelRadius >= 9.0f)
            {
                g.setColour(juce::Colours::black.withAlpha(0.07f));
                for (int k = 1; k <= 3; ++k)
                {
                    const float rr = r * (0.30f + 0.22f * static_cast<float>(k));
                    g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.6f);
                }
            }

            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);
        }
    }

    // Flat knob, as on the Animatek Rack modules: a disc with a thin rim, a
    // slightly different face and a pointer, with the value painted over the rim
    // in the theme's secondary colour (bipolar ranges fill from the centre). A
    // morph group colours the rim instead, so assignments stay readable.
    struct FlatKnobColours { juce::Colour rimFill, rim, face, pointer, arc; };

    // Colours from the theme: a dark disc on dark themes, a light one on light
    // themes (told by the module text, which is dark on a light panel).
    inline FlatKnobColours flatKnobColours(juce::Colour knobBase, juce::Colour knobBorder,
                                           juce::Colour knobGrip, juce::Colour moduleText,
                                           juce::Colour gridBackground, juce::Colour arc)
    {
        FlatKnobColours c;
        c.arc = arc;
        if (moduleText.getBrightness() < 0.5f)       // light theme
        {
            c.rimFill = knobBase.brighter(0.25f);
            c.rim     = knobBorder;
            c.face    = knobBase.brighter(0.55f);
            c.pointer = knobGrip;
        }
        else                                         // dark theme
        {
            c.rimFill = gridBackground.darker(0.4f);
            c.rim     = knobBase.interpolatedWith(c.rimFill, 0.55f);
            c.face    = c.rimFill.interpolatedWith(knobBase, 0.16f);
            c.pointer = juce::Colours::white.interpolatedWith(knobBase, 0.2f);
        }
        return c;
    }

    //   normalized  0 at the left stop, 1 at the right
    //   origin      where the arc starts: 0, or the centre for a bipolar range
    inline void drawFlatKnob(juce::Graphics& g, float cx, float cy, float radius,
                             float normalized, float origin,
                             bool hasMorph, juce::Colour morph, const FlatKnobColours& col)
    {
        const float pi = juce::MathConstants<float>::pi;
        const float rimW = juce::jmax(1.2f, radius * 2.0f * 0.045f);
        const float rimR = radius - rimW * 0.5f;

        g.setColour(col.rimFill);
        g.fillEllipse(cx - rimR, cy - rimR, rimR * 2.0f, rimR * 2.0f);
        g.setColour(hasMorph ? morph : col.rim);
        g.drawEllipse(cx - rimR, cy - rimR, rimR * 2.0f, rimR * 2.0f, rimW);
        g.setColour(col.face);
        g.fillEllipse(cx - radius * 0.8f, cy - radius * 0.8f, radius * 1.6f, radius * 1.6f);

        // Value arc, clockwise from 12 o'clock: the stops sit at -135 and +135 degrees.
        const float a0 = -0.75f * pi, sweep = 1.5f * pi;
        const float t  = juce::jlimit(0.0f, 1.0f, normalized);
        const float o  = juce::jlimit(0.0f, 1.0f, origin);
        const float t0 = juce::jmin(o, t), t1 = juce::jmax(o, t);
        if (t1 > t0 + 1.0e-4f)
        {
            juce::Path arc;
            arc.addCentredArc(cx, cy, rimR, rimR, 0.0f, a0 + t0 * sweep, a0 + t1 * sweep, true);
            g.setColour(col.arc);
            g.strokePath(arc, juce::PathStrokeType(rimW + 0.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));
        }

        // Pointer, wholly inside the face, round end included.
        const float a  = a0 + t * sweep;
        const float pw = juce::jmax(1.2f, radius * 2.0f * 0.055f);
        const float pOut = radius * 0.8f - pw * 0.5f - juce::jmax(0.8f, radius * 2.0f * 0.04f);
        const float pIn  = radius * 0.15f;
        g.setColour(col.pointer);
        g.drawLine(cx + std::sin(a) * pIn,  cy - std::cos(a) * pIn,
                   cx + std::sin(a) * pOut, cy - std::cos(a) * pOut, pw);
    }

    // Knurled-knob pointer: a bold line from near the centre out across the skirt.
    // White on the black knob, dark on the aluminium one.
    inline void drawKnurledPointer(juce::Graphics& g, float cx, float cy, float radius,
                                float angleRad, int style)
    {
        const float s = std::sin(angleRad), c = std::cos(angleRad);
        const float inner = radius * 0.12f, outer = radius * 0.97f;
        const float w = juce::jmax(1.4f, radius * 0.15f);

        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.drawLine(cx + s * inner + 0.5f, cy - c * inner + 0.7f,
                   cx + s * outer + 0.5f, cy - c * outer + 0.7f, w);

        g.setColour(style == 1 ? juce::Colour(0xfff2f2f2) : juce::Colour(0xff1b1b1b));
        g.drawLine(cx + s * inner, cy - c * inner, cx + s * outer, cy - c * outer, w);
    }

    // ------------------------------------------------------------------- jacks

    // A hardware jack. Inputs stay round and outputs stay square, as in the flat
    // version, so the shapes people already know don't change.
    //   (x, y, sz)  the connector's top-left and size, as in paintConnectors
    //   signal      the signal-type colour: a ring round the metal nut
    //   hole        activeScheme_.connHole
    //   capped      a cable of this type is hidden by the cable filters
    inline void drawJack(juce::Graphics& g, float x, float y, float sz, bool isOutput,
                         juce::Colour signal, juce::Colour hole, bool capped)
    {
        auto fillShape = [&](float ix, float iy, float isz)
        {
            if (isOutput) g.fillRoundedRectangle(ix, iy, isz, isz, isz * 0.25f);
            else          g.fillEllipse(ix, iy, isz, isz);
        };

        // Contact shadow, offset down and right.
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        fillShape(x + sz * 0.06f, y + sz * 0.10f, sz);

        // Signal ring: the whole footprint in the signal colour.
        g.setColour(signal);
        fillShape(x, y, sz);

        // Metal nut inside the ring, brushed-steel gradient.
        {
            const float inset = sz * 0.14f;
            const float nsz   = sz - inset * 2.0f;
            juce::ColourGradient nut(juce::Colour(0xffe6e6e6), x, y + inset,
                                     juce::Colour(0xff7a7a7a), x, y + inset + nsz, false);
            g.setGradientFill(nut);
            fillShape(x + inset, y + inset, nsz);
        }

        // Socket: dark, with a faint lit lip on the lower edge.
        {
            const float ssz = sz * 0.42f * (isOutput ? 1.1f : 1.0f);
            const float sx  = x + (sz - ssz) * 0.5f;
            const float sy  = y + (sz - ssz) * 0.5f;

            if (capped)
            {
                // Dust cap: the hole filled with a darkened signal colour.
                g.setColour(signal.darker(0.4f));
                fillShape(sx, sy, ssz);
            }
            else
            {
                // Lip: the socket shape drawn once, shifted down and pale, then
                // the dark hole over it. What shows is a thin lit crescent.
                g.setColour(juce::Colours::white.withAlpha(0.30f));
                fillShape(sx, sy + sz * 0.07f, ssz);

                g.setColour(hole);
                fillShape(sx, sy, ssz);
            }
        }
    }

    // ------------------------------------------------------------------ cables

    // A rounded patch cord: a shadow on the panel, a dark edge, the coloured
    // body, and a highlight along the top. `opacity` is the editor-wide cable
    // opacity setting.
    inline void drawCord(juce::Graphics& g, const juce::Path& path, juce::Colour colour,
                         float width, float opacity)
    {
        const juce::PathStrokeType::JointStyle  joint = juce::PathStrokeType::curved;
        const juce::PathStrokeType::EndCapStyle cap   = juce::PathStrokeType::rounded;

        juce::Path shadow(path);
        shadow.applyTransform(juce::AffineTransform::translation(1.2f, 3.2f));
        g.setColour(juce::Colours::black.withAlpha(0.28f * opacity));
        g.strokePath(shadow, juce::PathStrokeType(width + 1.2f, joint, cap));

        g.setColour(colour.darker(0.65f).withAlpha(0.95f * opacity));
        g.strokePath(path, juce::PathStrokeType(width + 1.6f, joint, cap));

        g.setColour(colour.withAlpha(0.95f * opacity));
        g.strokePath(path, juce::PathStrokeType(width, joint, cap));

        juce::Path highlight(path);
        highlight.applyTransform(juce::AffineTransform::translation(-0.3f, -width * 0.22f));
        g.setColour(colour.brighter(0.5f).withAlpha(0.40f * opacity));
        g.strokePath(highlight, juce::PathStrokeType(width * 0.32f, joint, cap));
    }

    // A plug head where a cable meets a jack: a dark barrel with a metal tip.
    inline void drawPlug(juce::Graphics& g, float x, float y, float opacity)
    {
        const float r = 3.6f;

        g.setColour(juce::Colours::black.withAlpha(0.30f * opacity));
        g.fillEllipse(x - r + 0.6f, y - r + 1.2f, r * 2.0f, r * 2.0f);

        g.setColour(juce::Colour(0xff1c1c1c).withAlpha(opacity));
        g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);

        juce::ColourGradient metal(juce::Colour(0xffeeeeee).withAlpha(opacity), x - r * 0.4f, y - r * 0.5f,
                                   juce::Colour(0xff6a6a6a).withAlpha(opacity), x + r * 0.5f, y + r * 0.6f, false);
        g.setGradientFill(metal);
        g.fillEllipse(x - r * 0.72f, y - r * 0.72f, r * 1.44f, r * 1.44f);
    }
}
