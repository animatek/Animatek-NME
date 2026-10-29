#include "PatchHeaderBar.h"
#include "AppTheme.h"
#include "KnobDrag.h"
#include "KnobMenu.h"
#include "VersionCheck.h"
#include "../protocol/KnobAssignmentMessage.h"

namespace
{
    // Morph colors from original Java JTNM1Context
    const juce::Colour morphColours[4] = {
        juce::Colour(0xffCB4F4F),  // Morph 1 - Red
        juce::Colour(0xff9AC889),  // Morph 2 - Green
        juce::Colour(0xff5A5FB3),  // Morph 3 - Blue
        juce::Colour(0xffE5DE45)   // Morph 4 - Yellow
    };

    // Cable colors matching SignalType order
    const juce::Colour cableColours[7] = {
        juce::Colour(0xffCB4F4F),  // Audio (Red)
        juce::Colour(0xff5A5FB3),  // Control (Blue)
        juce::Colour(0xffE5DE45),  // Logic (Yellow)
        juce::Colour(0xffA8A8A8),  // MasterSlave (Gray)
        juce::Colour(0xff9AC899),  // User1 (Green)
        juce::Colour(0xffBB00D7),  // User2 (Purple)
        juce::Colour(0xffEEEEEE)   // None (White)
    };

    juce::Colour contrastingInk(juce::Colour background)
    {
        return background.getPerceivedBrightness() > 0.5f
            ? juce::Colours::black : juce::Colours::white;
    }

    // Layout constants (see resized())
    constexpr int nameW = 150;
    constexpr float loadW = 104.0f;
    constexpr float snapSize = 24.0f;
    constexpr float snapGap = 4.0f;
    constexpr float xfadeW = 150.0f;       // the crossfader's wanted width; it narrows to 96 when the bar is tight
    constexpr float mutatorW = 76.0f;
    constexpr float compactCablesW = 76.0f;
}

PatchHeaderBar::PatchHeaderBar()
{
    // The morph dials' arrows are placed straight in this component's
    // coordinates, so a rectangle needs no transform to become a repaint.
    morphSpinner.repaintArea = [this](juce::Rectangle<float> area)
    {
        repaint(area.getSmallestIntegerContainer());
    };

    // Create patch name editor (initially hidden)
    patchNameEditor = std::make_unique<juce::Label>("PatchName", "");
    patchNameEditor->setEditable(true);
    patchNameEditor->setColour(juce::Label::backgroundColourId, AppTheme::palette().inputBackground.darker(0.15f));
    patchNameEditor->setColour(juce::Label::textColourId, AppTheme::palette().textPrimary);
    patchNameEditor->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    // The same type as the title it replaces while it is being typed over
    patchNameEditor->setFont(AppTheme::uiFont(15.0f).withStyle("Bold"));
    patchNameEditor->setBorderSize(juce::BorderSize<int>(0, 11, 0, 8));
    patchNameEditor->setJustificationType(juce::Justification::centredLeft);

    // CRITICAL: Limit input to 15 characters (16+ hangs the synth!)
    patchNameEditor->onEditorShow = [this]()
    {
        if (auto* editor = patchNameEditor->getCurrentTextEditor())
            editor->setInputRestrictions(15);  // Hard limit to 15 characters
    };

    patchNameEditor->onTextChange = []()
    {
        // Don't fire callback on every keystroke — wait until editing is done
    };
    patchNameEditor->onEditorHide = [this]()
    {
        // Fire callback once when editing finishes (Enter / click away)
        if (patch && nameChangeCallback)
        {
            juce::String newName = patchNameEditor->getText().substring(0, 15);  // CRITICAL: Max 15 chars (16+ hangs synth!)
            nameChangeCallback(newName);
        }
        // Editing is over: the label goes, and the bar draws the name as a title again (it used to
        // stay on screen as a boxed field, with whatever was typed in it)
        patchNameEditor->setVisible(false);
        repaint();
    };
    addAndMakeVisible(patchNameEditor.get());
    patchNameEditor->setVisible(false);  // Hidden by default

    // The store-to-bank button next to the name is painted in paint(), not a
    // child component: the whole header bar is custom-drawn.
}

void PatchHeaderBar::setPatch(Patch* p)
{
    patch = p;
    if (patchNameEditor->isVisible())
        patchNameEditor->setVisible(false);
    repaint();
}

juce::String PatchHeaderBar::getTooltip()
{
    const auto p = getMouseXYRelative();
    const auto pf = p.toFloat();
    auto over = [&pf](juce::Rectangle<float> r) { return r.expanded(2.0f).contains(pf); };

    if (L.name.contains(p))
        return "Patch name: click to rename";
    if (over(L.voicesField) || over(L.voicesUp) || over(L.voicesDown))
        return "Voices: how many notes the patch can play at once";
    if (over(L.loadPva) || over(L.loadE))
        return "DSP load: PVA is the polyphonic area, E the common one";
    if (over(L.store))
        return "Store the patch back to its bank location";
    for (int i = 0; i < 4; ++i)
        if (over(L.morph[static_cast<size_t>(i)]))
            return "Morph " + juce::String(i + 1) + ": drag to change, right-click to assign";
    for (int i = 0; i < 8; ++i)
        if (over(L.snap[static_cast<size_t>(i)]))
            return "Snapshot " + juce::String(i + 1) + ": click to recall, Shift+click to save, right-click for more";
    if (over(L.xfade))
        return "Crossfade between snapshots A and B; right-click to choose them";
    if (L.cablesCompact && over(L.cablesBtn))
        return "Show or hide cables by colour, or shake them";
    static const char* const colourNames[numCableTypes] = { "red", "blue", "yellow", "grey", "green", "purple", "white" };
    for (int i = 0; i < numCableTypes; ++i)
        if (over(L.cable[static_cast<size_t>(i)]))
            return "Click to " + juce::String(getCableVisibility(i) ? "hide" : "show") + " the "
                 + colourNames[i] + " cables";
    if (over(L.shake))
        return "Shake the cables so overlapping runs spread out";
    if (over(L.mutator))
        return "Patch Mutator: breed new sounds from this patch";
    if (over(L.donate))
        return "Donate with PayPal";
    if (over(L.patreon))
        return "Support Animatek NME on Patreon";
    return {};
}

void PatchHeaderBar::mouseMove(const juce::MouseEvent& e)
{
    const auto p = e.getPosition().toFloat();

    // Being on a button is not the same as being on the dial it belongs to, so
    // the buttons get asked first or they vanish as you reach for them.
    if (!morphSpinner.contains(p))
    {
        const int idx = patch != nullptr ? getMorphKnobAt(e.getPosition()) : -1;
        morphSpinnerIndex = idx;
        morphSpinner.showFor(idx < 0 ? juce::String() : "morph" + juce::String(idx),
                             idx < 0 ? juce::Rectangle<float>() : getMorphKnobBounds(idx),
                             ValueSpinner::Placement::Inside);
    }
    morphSpinner.updateHover(p);

    const bool over = isStoreButtonAt(e.getPosition());
    if (over == storeHover)
        return;
    storeHover = over;
    repaint(getStoreButtonBounds().expanded(2.0f).toNearestInt());
}

void PatchHeaderBar::mouseExit(const juce::MouseEvent&)
{
    if (!morphSpinner.isHeld())
    {
        morphSpinner.hide();
        morphSpinnerIndex = -1;
    }

    if (!storeHover)
        return;
    storeHover = false;
    repaint(getStoreButtonBounds().expanded(2.0f).toNearestInt());
}

// One step of a morph macro. There is no undo behind the morph values, and
// nothing else to close afterwards, so a step is the whole story.
void PatchHeaderBar::morphSpinnerStep(int delta)
{
    if (patch == nullptr || morphSpinnerIndex < 0 || morphSpinnerIndex >= 4)
        return;

    auto& value = patch->morphValues[static_cast<size_t>(morphSpinnerIndex)];
    const int newValue = juce::jlimit(0, 127, value + delta);
    if (newValue == value)
    {
        morphSpinner.stopRepeat();   // at the end of the range there is no more
        return;
    }

    value = newValue;
    repaint();

    if (morphChangeCallback)
        morphChangeCallback(morphSpinnerIndex, newValue);
}

void PatchHeaderBar::resized()
{
    // One layout pass: every element's rectangle is worked out here and the paint code and the
    // hit tests only read them. The width of each group is known first, and the gap between
    // groups is whatever the window has left over, shared out (never under minGap, so a narrow
    // window stays tidy, and never over maxGap, so a very wide one does not fall apart). The
    // cable colours sit between the morph dials and the snapshots and fold into one button when
    // even the minimum gaps do not leave room for them.
    L = Layout{};
    const float h = static_cast<float>(getHeight());
    const float cy = h * 0.5f;
    const float width = static_cast<float>(getWidth());
    constexpr float minGap = 18.0f, maxGap = 30.0f;
    constexpr float margin = 12.0f;
    constexpr int numGaps = 9;   // eight between the nine groups, and one before the heart

    L.storeWordW = juce::Font(AppTheme::uiFont(12.0f).withStyle("Bold")).getStringWidthFloat("Store");
    const float locW = juce::Font(AppTheme::uiFont(11.0f)).getStringWidthFloat("00:00");
    const float storeW = 9.0f + 11.0f + 6.0f + L.storeWordW + 10.0f + locW + 9.0f;
    const float voicesW = 52.0f;
    const float morphW = 4.0f * morphKnobSize + 3.0f * morphKnobSpacing;
    const float snapsW = 8.0f * snapSize + 7.0f * snapGap + 22.0f;   // with room for the interpolation time
    const float groupW = numCableTypes * cableToggleSize + (numCableTypes - 1) * cableToggleSpacing + 10.0f + 24.0f;
    const float rightW = supportButtonSize * 2.0f + 4.0f;            // heart and Patreon

    // The patch name gives way last: when even the folded cables and the minimum gaps do not fit, it
    // gets narrower (down to minNameW) rather than the crossfader running under the heart
    constexpr float minNameW = 104.0f;
    float nameWe = static_cast<float>(nameW);
    float xfadeWe = xfadeW;
    auto contentW = [&](float cablesW)
    {
        return nameWe + voicesW + loadW + storeW + morphW + cablesW + snapsW + xfadeWe + mutatorW;
    };
    const float avail = width - 2.0f * margin - rightW;
    L.cablesCompact = contentW(groupW) + numGaps * minGap > avail;
    const float cablesW = L.cablesCompact ? compactCablesW : groupW;
    // Squeeze in this order: the crossfader to 120, then the name, then the crossfader down to 96
    auto overflow = [&] { return contentW(cablesW) + numGaps * minGap - avail; };
    if (overflow() > 0.0f) xfadeWe = juce::jmax(120.0f, xfadeWe - overflow());
    if (overflow() > 0.0f) nameWe  = juce::jmax(minNameW, nameWe - overflow());
    if (overflow() > 0.0f) xfadeWe = juce::jmax(96.0f, xfadeWe - overflow());
    const float gap = juce::jlimit(minGap, maxGap, (avail - contentW(cablesW)) / numGaps);

    auto sep = [this](float at) { L.seps.push_back(at); };

    // Right edge: Patreon, heart, and a hairline before them
    float rx = width - margin;
    L.patreon = { rx - supportButtonSize, cy - supportButtonSize * 0.5f, supportButtonSize, supportButtonSize };
    rx -= supportButtonSize + 4.0f;
    L.donate = { rx - supportButtonSize, L.patreon.getY(), supportButtonSize, supportButtonSize };
    sep(L.donate.getX() - minGap * 0.5f);

    float x = margin;

    // Patch name
    L.name = juce::Rectangle<int>(juce::roundToInt(x), 7, juce::roundToInt(nameWe), getHeight() - 14);
    x += nameWe + gap;

    // Voices (label above, number and arrows below)
    L.voicesLabel = { x, 5.0f, 56.0f, 12.0f };
    L.voicesField = { x, 19.0f, 34.0f, 22.0f };
    L.voicesUp    = { x + 36.0f, 19.0f, 14.0f, 11.0f };
    L.voicesDown  = { x + 36.0f, 30.0f, 14.0f, 11.0f };
    x += voicesW + gap;
    sep(x - gap * 0.5f);

    // Load: PVA and E
    L.loadPva = { x, cy - 14.0f, loadW, 13.0f };
    L.loadE   = { x, cy + 1.0f,  loadW, 13.0f };
    x += loadW + gap;
    sep(x - gap * 0.5f);

    // Store
    L.store = { x, cy - 15.0f, storeW, 30.0f };
    x += storeW + gap;
    sep(x - gap * 0.5f);

    // Morph
    for (int i = 0; i < 4; ++i)
        L.morph[static_cast<size_t>(i)] = { x + i * (morphKnobSize + morphKnobSpacing),
                                            cy - morphKnobSize * 0.5f,
                                            static_cast<float>(morphKnobSize), static_cast<float>(morphKnobSize) };
    x += morphW + gap;
    sep(x - gap * 0.5f);

    // Cable colours and the shake key, or the one button that stands for them
    if (L.cablesCompact)
    {
        const juce::Rectangle<float> away { -100.0f, -100.0f, 0.0f, 0.0f };
        L.cable.fill(away);
        L.shake = away;
        L.cablesBtn = { x, cy - 11.0f, compactCablesW, 22.0f };
    }
    else
    {
        for (int i = 0; i < numCableTypes; ++i)
            L.cable[static_cast<size_t>(i)] = { x + i * (cableToggleSize + cableToggleSpacing), cy - cableToggleSize * 0.5f,
                                                static_cast<float>(cableToggleSize), static_cast<float>(cableToggleSize) };
        L.shake = { x + numCableTypes * (cableToggleSize + cableToggleSpacing) + 4.0f, cy - 11.0f, 24.0f, 22.0f };
    }
    x += cablesW + gap;
    sep(x - gap * 0.5f);

    // Snapshots
    for (int i = 0; i < 8; ++i)
        L.snap[static_cast<size_t>(i)] = { x + i * (snapSize + snapGap), cy - snapSize * 0.5f, snapSize, snapSize };
    x += snapsW + gap;
    sep(x - gap * 0.5f);

    // The A/B crossfader, then the Mutator: both act on the sounds the snapshots hold
    L.xfade = { x, cy - 13.0f, xfadeWe, 26.0f };
    x += xfadeWe + gap;
    sep(x - gap * 0.5f);
    L.mutator = { x, cy - 14.0f, mutatorW, 28.0f };

    // Patch name editor
    if (patchNameEditor)
        patchNameEditor->setBounds(getPatchNameBounds());
}

juce::Rectangle<int> PatchHeaderBar::getPatchNameBounds() const
{
    return L.name;
}

juce::Rectangle<float> PatchHeaderBar::getStoreButtonBounds() const
{
    return L.store;
}

bool PatchHeaderBar::isStoreButtonAt(juce::Point<int> pos) const
{
    return getStoreButtonBounds().contains(pos.toFloat());
}

void PatchHeaderBar::setCurrentLocation(int section, int position)
{
    if (currentSection == section && currentPosition == position)
        return;
    currentSection = section;
    currentPosition = position;
    repaint();
}

void PatchHeaderBar::clearCurrentLocation() { setCurrentLocation(-1, -1); }

void PatchHeaderBar::setStoreEnabled(bool enabled)
{
    if (storeEnabled == enabled)
        return;
    storeEnabled = enabled;
    repaint();
}

void PatchHeaderBar::setStoreUncertain(bool uncertain)
{
    if (storeUncertain == uncertain)
        return;
    storeUncertain = uncertain;
    repaint();
}

// --- Layout helpers ---

juce::Rectangle<float> PatchHeaderBar::getMorphKnobBounds(int i) const
{
    return L.morph[static_cast<size_t>(juce::jlimit(0, 3, i))];
}

int PatchHeaderBar::getMorphKnobAt(juce::Point<int> pos) const
{
    for (int i = 0; i < 4; i++)
        if (getMorphKnobBounds(i).expanded(2.0f).contains(pos.toFloat()))
            return i;
    return -1;
}

juce::Rectangle<float> PatchHeaderBar::getCableToggleBounds(int i) const
{
    return L.cable[static_cast<size_t>(juce::jlimit(0, numCableTypes - 1, i))];
}

int PatchHeaderBar::getCableToggleAt(juce::Point<int> pos) const
{
    for (int i = 0; i < numCableTypes; i++)
        if (getCableToggleBounds(i).expanded(2.0f).contains(pos.toFloat()))
            return i;
    return -1;
}

juce::Rectangle<float> PatchHeaderBar::getShakeButtonBounds() const
{
    return L.shake;
}

bool PatchHeaderBar::isShakeButtonAt(juce::Point<int> pos) const
{
    return getShakeButtonBounds().expanded(2.0f).contains(pos.toFloat());
}

juce::Rectangle<float> PatchHeaderBar::getBugButtonBounds() const
{
    return {};
}

bool PatchHeaderBar::isBugButtonAt(juce::Point<int>) const
{
    return false;   // the button left the bar: Help > Report a Bug is where it lives now
}

juce::Rectangle<float> PatchHeaderBar::getSnapshotButtonBounds(int index) const
{
    return L.snap[static_cast<size_t>(juce::jlimit(0, 7, index))];
}

int PatchHeaderBar::getSnapshotButtonAt(juce::Point<int> pos) const
{
    for (int i = 0; i < 8; ++i)
        if (getSnapshotButtonBounds(i).expanded(1.0f).contains(pos.toFloat()))
            return i;
    return -1;
}

juce::Rectangle<float> PatchHeaderBar::getMorphFaderBounds() const
{
    return L.xfade;
}

bool PatchHeaderBar::isMorphFaderAt(juce::Point<int> pos) const
{
    return getMorphFaderBounds().expanded(2.0f).contains(pos.toFloat());
}

float PatchHeaderBar::morphFaderPosFromX(float x) const
{
    auto fb = getMorphFaderBounds();
    constexpr float inset = 28.0f;  // room for the A/B end labels, as drawn in paint
    float l = fb.getX() + inset, r = fb.getRight() - inset;
    return juce::jlimit(0.0f, 1.0f, (x - l) / juce::jmax(1.0f, r - l));
}

juce::Rectangle<float> PatchHeaderBar::getMutatorButtonBounds() const
{
    return L.mutator;
}

bool PatchHeaderBar::isMutatorButtonAt(juce::Point<int> pos) const
{
    return getMutatorButtonBounds().expanded(2.0f).contains(pos.toFloat());
}

// Heart and Patreon sit in the far right corner, clear of the ABCD button, and the
// synth name box makes room for them (supportBlockW).
juce::Rectangle<float> PatchHeaderBar::getPatreonButtonBounds() const
{
    return L.patreon;
}

juce::Rectangle<float> PatchHeaderBar::getDonateButtonBounds() const
{
    return L.donate;
}

void PatchHeaderBar::setSnapshotFilled(int index, bool filled)
{
    if (index >= 0 && index < 8) { snapshotFilled[index] = filled; repaint(); }
}

void PatchHeaderBar::setInterpolationProgress(float progress)
{
    interpolationProgress = progress;
    repaint();
}

PatchHeaderBar::ArrowHit PatchHeaderBar::getVoiceArrowAt(juce::Point<int> pos) const
{
    const auto p = pos.toFloat();
    if (L.voicesUp.expanded(1.0f, 2.0f).contains(p))
        return ArrowHit::Up;
    if (L.voicesDown.expanded(1.0f, 2.0f).contains(p))
        return ArrowHit::Down;
    return ArrowHit::None;
}

bool PatchHeaderBar::getCableVisibility(int index) const
{
    if (!patch) return true;
    const auto& hdr = patch->getHeader();
    switch (index)
    {
        case 0: return hdr.cableVisRed;
        case 1: return hdr.cableVisBlue;
        case 2: return hdr.cableVisYellow;
        case 3: return hdr.cableVisGray;
        case 4: return hdr.cableVisGreen;
        case 5: return hdr.cableVisPurple;
        case 6: return hdr.cableVisWhite;
        default: return true;
    }
}

void PatchHeaderBar::toggleCableVisibility(int index)
{
    setCableVisibility(index, !getCableVisibility(index));
}

void PatchHeaderBar::setCableVisibility(int index, bool visible)
{
    if (!patch) return;
    auto& hdr = patch->getHeader();
    switch (index)
    {
        case 0: hdr.cableVisRed    = visible; break;
        case 1: hdr.cableVisBlue   = visible; break;
        case 2: hdr.cableVisYellow = visible; break;
        case 3: hdr.cableVisGray   = visible; break;
        case 4: hdr.cableVisGreen  = visible; break;
        case 5: hdr.cableVisPurple = visible; break;
        case 6: hdr.cableVisWhite  = visible; break;
    }
    repaint();
    if (cableVisCallback) cableVisCallback();
}

void PatchHeaderBar::toggleAllCables()
{
    if (!patch) return;
    bool anyShown = false;
    for (int i = 0; i < numCableTypes; ++i)
        anyShown = anyShown || getCableVisibility(i);
    for (int i = 0; i < numCableTypes; ++i)
        setCableVisibility(i, !anyShown);
}

// --- Drawing ---

void PatchHeaderBar::drawMorphKnob(juce::Graphics& g, float cx, float cy, float size,
                                    float normalized, const juce::String& label,
                                    juce::Colour colour)
{
    juce::ignoreUnused(label);
    // The dial the Knob Floater and the modules wear, in the macro's own colour: a track, the value
    // as an arc of that colour from the left stop, a dark face and a light pointer. Strokes
    // are paths with round ends, so it stays smooth at this size.
    const auto& pal = AppTheme::palette();
    const float pi = juce::MathConstants<float>::pi;
    const float centerX = cx + size * 0.5f;
    const float centerY = cy + size * 0.5f;
    const float thick = juce::jmax(2.5f, size * 0.1f);
    const float arcR = size * 0.5f - thick * 0.5f - 0.5f;
    const float a0 = -0.75f * pi, sweep = 1.5f * pi;
    const float t = juce::jlimit(0.0f, 1.0f, normalized);

    juce::Path track;
    track.addCentredArc(centerX, centerY, arcR, arcR, 0.0f, a0, a0 + sweep, true);
    g.setColour(pal.borderColor);
    g.strokePath(track, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (t > 0.002f)
    {
        juce::Path value;
        value.addCentredArc(centerX, centerY, arcR, arcR, 0.0f, a0, a0 + t * sweep, true);
        g.setColour(colour);
        g.strokePath(value, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float faceR = arcR - thick * 0.5f - 2.0f;
    g.setColour(pal.backgroundMain);
    g.fillEllipse(centerX - faceR, centerY - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour(pal.borderColor);
    g.drawEllipse(centerX - faceR, centerY - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    const float a = a0 + t * sweep;
    g.setColour(pal.textPrimary);
    g.drawLine(centerX + std::sin(a) * faceR * 0.2f, centerY - std::cos(a) * faceR * 0.2f,
               centerX + std::sin(a) * faceR * 0.78f, centerY - std::cos(a) * faceR * 0.78f, 2.0f);
}

void PatchHeaderBar::drawLoadBar(juce::Graphics& g, juce::Rectangle<float> r,
                                  float percent, const juce::String& label)
{
    const auto& pal = AppTheme::palette();
    g.setColour(pal.textMuted);
    g.setFont(AppTheme::uiFont(10.0f).withStyle("Bold"));
    g.drawText(label, r.withWidth(26.0f), juce::Justification::centredLeft, false);

    auto bar = juce::Rectangle<float>(r.getX() + 28.0f, r.getCentreY() - 3.5f, 38.0f, 7.0f);
    g.setColour(pal.inputBackground);
    g.fillRoundedRectangle(bar, 3.5f);
    g.setColour(pal.borderColor);
    g.drawRoundedRectangle(bar.reduced(0.5f), 3.5f, 1.0f);

    if (percent >= 0.0f)
    {
        juce::Colour barCol = percent < 0.6f  ? juce::Colour(0xff5a9a5a)
                            : percent < 0.85f ? juce::Colour(0xffb0a030)
                                              : juce::Colour(0xffcb4f4f);
        g.setColour(barCol);
        g.fillRoundedRectangle(bar.withWidth(juce::jmax(2.0f, bar.getWidth() * juce::jlimit(0.0f, 1.0f, percent))), 3.5f);
    }

    // One decimal, matching the original Clavia editor (e.g. "47.6%"); the load is a
    // client-side estimate, so a truncated integer read a hair high.
    g.setColour(pal.textSecondary);
    g.setFont(AppTheme::uiFont(10.0f));
    g.drawText(percent >= 0.0f ? juce::String(percent * 100.0f, 1) + "%" : juce::String("--%"),
               juce::Rectangle<float>(bar.getRight() + 4.0f, r.getY(), r.getRight() - bar.getRight() - 4.0f, r.getHeight()),
               juce::Justification::centredLeft, false);
}

void PatchHeaderBar::paint(juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    const int h = getHeight();
    const float cy = static_cast<float>(h) * 0.5f;

    g.fillAll(pal.backgroundPanel);

    // Bottom border
    g.setColour(pal.buttonActive);
    g.drawLine(0.0f, static_cast<float>(h) - 0.5f,
               static_cast<float>(getWidth()), static_cast<float>(h) - 0.5f, 1.0f);

    // Hairlines between the groups (no boxes, no group titles: the bar is flat)
    g.setColour(pal.buttonActive);
    for (float sx : L.seps)
        g.drawLine(sx, cy - 12.0f, sx, cy + 12.0f, 1.0f);

    // A small flat button: used by Store, the shake key and the Mutator
    auto flatButton = [&](juce::Rectangle<float> r, bool hot, bool framed)
    {
        g.setColour(hot ? pal.buttonActive : (framed ? pal.buttonBackground : juce::Colours::transparentBlack));
        g.fillRoundedRectangle(r, 5.0f);
        if (framed)
        {
            g.setColour(pal.borderColor);
            g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
        }
    };

    // --- Patch name: in a field, darker than the bar, so it reads as something to type in
    // (double-click); the editor label takes its place while it is being edited ---
    if (!patchNameEditor || !patchNameEditor->isVisible())
    {
        auto nameRect = L.name;
        const bool has = patch != nullptr;
        const auto box = nameRect.toFloat();
        g.setColour(pal.inputBackground.darker(0.15f));   // just the darker ground, no outline
        g.fillRoundedRectangle(box, 6.0f);
        g.setColour(has ? pal.textPrimary : pal.textMuted);
        g.setFont(AppTheme::uiFont(15.0f).withStyle("Bold"));
        g.drawText(has ? patch->getName() : juce::String("No Patch"),
                   nameRect.reduced(11, 0), juce::Justification::centredLeft, true);
    }

    // --- Voices: label above, the number in a field with its arrows ---
    {
        g.setColour(pal.textMuted);
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        g.drawText("Voices", L.voicesLabel, juce::Justification::centredLeft, false);

        g.setColour(pal.inputBackground);
        g.fillRoundedRectangle(L.voicesField, 4.0f);
        g.setColour(pal.borderColor);
        g.drawRoundedRectangle(L.voicesField.reduced(0.5f), 4.0f, 1.0f);

        const int voices = patch ? patch->getHeader().voices : 0;
        g.setColour(pal.textPrimary);
        g.setFont(AppTheme::uiFont(13.0f));
        g.drawText(juce::String(voices), L.voicesField, juce::Justification::centred, false);

        g.setColour(pal.textSecondary);
        auto arrow = [&](juce::Rectangle<float> r, bool up)
        {
            const float mx = r.getCentreX(), my = r.getCentreY();
            juce::Path p;
            if (up) p.addTriangle(mx, my - 2.5f, mx - 3.5f, my + 2.0f, mx + 3.5f, my + 2.0f);
            else    p.addTriangle(mx - 3.5f, my - 2.0f, mx + 3.5f, my - 2.0f, mx, my + 2.5f);
            g.fillPath(p);
        };
        arrow(L.voicesUp, true);
        arrow(L.voicesDown, false);
    }

    // --- Load: PVA and E, each its own bar and number ---
    drawLoadBar(g, L.loadPva, loadPva, "PVA");
    drawLoadBar(g, L.loadE, loadE, "E");

    // --- Store to bank (diskette, the word, and the location the patch lives at) ---
    {
        auto sb = L.store;
        const bool known = currentSection >= 0 && currentPosition >= 0;
        const bool live = storeEnabled && patch != nullptr;
        flatButton(sb, live && storeHover, true);

        const float ds = 11.0f;
        const float dx = sb.getX() + 9.0f;
        const float dy = sb.getCentreY() - ds * 0.5f;
        auto ink = live ? pal.textPrimary : pal.textMuted;
        g.setColour(ink);
        g.drawRoundedRectangle(dx, dy, ds, ds, 1.5f, 1.0f);
        g.fillRect(dx + 3.0f, dy + 1.0f, ds - 6.0f, ds * 0.35f);          // shutter
        g.fillRect(dx + 2.0f, dy + ds * 0.58f, ds - 4.0f, ds * 0.32f);    // label

        g.setFont(AppTheme::uiFont(12.0f).withStyle("Bold"));
        g.drawText("Store", juce::Rectangle<float>(dx + ds + 6.0f, sb.getY(), L.storeWordW + 2.0f, sb.getHeight()),
                   juce::Justification::centredLeft, false);

        g.setColour(live ? (known ? pal.textSecondary : pal.textMuted) : pal.textMuted);
        g.setFont(AppTheme::uiFont(11.0f));
        juce::String loc = known
            ? juce::String(currentSection + 1) + ":" + juce::String(currentPosition + 1).paddedLeft('0', 2)
            : juce::String(storeUncertain ? "?" : "--");
        g.drawText(loc, sb.withTrimmedRight(8.0f).toNearestInt(), juce::Justification::centredRight, false);
    }

    // --- Morph knobs ---
    for (int i = 0; i < 4; i++)
    {
        auto kb = L.morph[static_cast<size_t>(i)];
        float norm = 0.0f;
        if (patch)
            norm = static_cast<float>(patch->morphValues[static_cast<size_t>(i)]) / 127.0f;
        drawMorphKnob(g, kb.getX(), kb.getY(), kb.getWidth(), norm,
                      "M" + juce::String(i + 1), morphColours[i]);
    }

    // --- Snapshots (1-8) ---
    {
        g.setFont(AppTheme::uiFont(12.0f).withStyle("Bold"));
        for (int i = 0; i < 8; ++i)
        {
            auto sb = L.snap[static_cast<size_t>(i)];
            const bool filled = snapshotFilled[i];
            const bool active = (i == activeSnapshot);

            const auto fill = active ? pal.buttonActive
                                     : filled ? pal.borderColor
                                              : pal.buttonBackground;
            g.setColour(fill);
            g.fillRoundedRectangle(sb, 4.0f);
            if (!active)
            {
                g.setColour(pal.borderColor);
                g.drawRoundedRectangle(sb.reduced(0.5f), 4.0f, 1.0f);
            }

            g.setColour(active ? contrastingInk(fill)
                               : filled ? pal.textPrimary : pal.textMuted);
            g.drawText(juce::String(i + 1), sb.toNearestInt(), juce::Justification::centred, false);
        }

        // Interpolation time, after the last button
        if (snapshotInterpSeconds > 0.0f)
        {
            auto last = L.snap[7];
            g.setFont(AppTheme::uiFont(10.0f));
            g.setColour(pal.textSecondary);
            g.drawText(juce::String(juce::roundToInt(snapshotInterpSeconds)) + "s",
                       juce::Rectangle<float>(last.getRight() + 3.0f, last.getY(), 24.0f, last.getHeight()),
                       juce::Justification::centredLeft, false);
        }

        // Interpolation progress bar, under the snapshot row
        if (interpolationProgress >= 0.0f)
        {
            auto first = L.snap[0];
            auto last = L.snap[7];
            const float barY = first.getBottom() + 2.0f;
            const float barW = last.getRight() - first.getX();
            g.setColour(pal.inputBackground);
            g.fillRect(first.getX(), barY, barW, 2.0f);
            g.setColour(pal.textSecondary);
            g.fillRect(first.getX(), barY, barW * interpolationProgress, 2.0f);
        }
    }

    // --- Morph A/B fader (editor-side software morph between two captures) ---
    {
        // As in the mockup: no box, an "A" and a "B" at the ends of a slim track, filled in the accent up
        // to a tall rounded thumb
        auto fb = L.xfade;
        const bool ready = morphHasA && morphHasB;

        g.setFont(AppTheme::uiFont(13.0f).withStyle("Bold"));
        g.setColour(morphHasA ? pal.textSecondary : pal.textMuted);
        g.drawText("A", juce::Rectangle<float>(fb.getX(), fb.getY(), 18.0f, fb.getHeight()),
                   juce::Justification::centred, false);
        g.setColour(morphHasB ? pal.textSecondary : pal.textMuted);
        g.drawText("B", juce::Rectangle<float>(fb.getRight() - 18.0f, fb.getY(), 18.0f, fb.getHeight()),
                   juce::Justification::centred, false);

        constexpr float inset = 28.0f;   // the same in morphFaderPosFromX: the thumb clears the A and the B
        const float l = fb.getX() + inset, r = fb.getRight() - inset;
        // With no A and B captured yet there is nothing to blend: the thumb rests in the middle
        const float pos = ready ? morphFaderPos : 0.5f;
        const float thumbX = l + (r - l) * pos;
        const float ty = fb.getCentreY();

        const auto track = juce::Rectangle<float>(l, ty - 2.5f, r - l, 5.0f);
        g.setColour(pal.borderColor);
        g.fillRoundedRectangle(track, 2.5f);
        if (ready)
        {
            g.setColour(pal.accentActive);
            g.fillRoundedRectangle(track.withWidth(juce::jmax(2.5f, thumbX - l)), 2.5f);
        }

        const auto thumb = juce::Rectangle<float>(thumbX - 5.0f, ty - 10.0f, 10.0f, 20.0f);
        g.setColour(!ready ? pal.textMuted : pal.textPrimary);
        g.fillRoundedRectangle(thumb, 3.0f);
        if (morphLearnArmed || (ready && morphKnobAssigned))
        {
            g.setColour(morphLearnArmed ? pal.accentWarning : pal.accentActive);
            g.drawRoundedRectangle(thumb.expanded(1.0f), 4.0f, 1.5f);   // armed to learn, or already on a knob
        }
    }

    // --- Cable colours: click to hide or show one kind ---
    if (L.cablesCompact)
    {
        flatButton(L.cablesBtn, false, true);
        g.setColour(pal.textSecondary);
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        g.drawText("Cables", L.cablesBtn.withTrimmedRight(14.0f).toNearestInt(), juce::Justification::centred, false);
        const float ax = L.cablesBtn.getRight() - 12.0f, ay = L.cablesBtn.getCentreY();
        juce::Path down;
        down.addTriangle(ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
        g.fillPath(down);
    }
    for (int i = 0; !L.cablesCompact && i < numCableTypes; i++)
    {
        auto tb = L.cable[static_cast<size_t>(i)];
        if (getCableVisibility(i))
        {
            g.setColour(cableColours[i]);
            g.fillEllipse(tb);
        }
        else
        {
            g.setColour(cableColours[i].withAlpha(0.45f));
            g.drawEllipse(tb.reduced(1.0f), 2.0f);
        }
    }

    // --- Shake cables ("S") ---
    if (!L.cablesCompact)
    {
        flatButton(L.shake, false, true);
        g.setColour(pal.textSecondary);
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        g.drawText("S", L.shake.toNearestInt(), juce::Justification::centred, false);
    }

    // --- Patch Mutator ---
    {
        auto mb = L.mutator;
        flatButton(mb, mutatorOpen, true);
        g.setColour(mutatorOpen ? contrastingInk(pal.buttonActive) : pal.textSecondary);
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        g.drawText("MUTATOR", mb.toNearestInt(), juce::Justification::centred, false);
    }

    // --- Donate (heart) and Patreon, like GitHub's Sponsor button ---
    {
        auto db = L.donate;
        flatButton(db, false, true);
        const auto hc = db.getCentre();
        constexpr float r = 3.4f;
        juce::Path heart;
        heart.addEllipse(hc.x - 2.0f * r + 0.4f, hc.y - r - 1.6f, 2.0f * r, 2.0f * r);
        heart.addEllipse(hc.x - 0.4f, hc.y - r - 1.6f, 2.0f * r, 2.0f * r);
        heart.addTriangle(hc.x - 2.0f * r + 0.6f, hc.y + 0.2f, hc.x + 2.0f * r - 0.6f, hc.y + 0.2f,
                          hc.x, hc.y + 2.0f * r - 0.6f);
        g.setColour(juce::Colour(0xffe0556b));
        g.fillPath(heart);

        auto pb = L.patreon;
        flatButton(pb, false, true);
        const auto pc = pb.getCentre();
        g.setColour(juce::Colour(0xfff96854));
        g.fillEllipse(pc.x - 0.5f, pc.y - 5.5f, 7.0f, 7.0f);
        g.fillRect(pc.x - 4.5f, pc.y - 5.5f, 2.2f, 11.0f);
    }

    // Last, so the arrows sit on top of the dial they belong to.
    morphSpinner.paint(g, { pal.inputBackground, pal.borderColor, AppTheme::macroLabelInk() });
}

// --- Mouse interaction ---

void PatchHeaderBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // Store to bank — one click puts the patch back where it came from
    if (isStoreButtonAt(pos) && storeEnabled && patch != nullptr)
    {
        if (quickSaveCallback)
            quickSaveCallback();
        return;
    }

    // The nudge arrows on the dial the pointer is resting on take the click
    // before the dial itself does — they are drawn on top of it.
    if (!e.mods.isPopupMenu() && patch != nullptr && morphSpinnerIndex >= 0
        && morphSpinner.mouseDown(pos.toFloat(), [this](int delta) { morphSpinnerStep(delta); }))
        return;

    // Morph knob — right-click for knob/CC assignment, left-click for drag
    int morphIdx = getMorphKnobAt(pos);
    if (morphIdx >= 0 && patch)
    {
        if (e.mods.isPopupMenu())
        {
            showMorphKnobContextMenu(morphIdx);
            return;
        }
        dragState.morphIndex = morphIdx;
        dragState.startValue = patch->morphValues[static_cast<size_t>(morphIdx)];
        dragState.startPos = pos;
        dragState.lastSentValue = -1;
        dragState.lastSendTime = 0;
        // The header bar sits at the top of the window, so a vertical sweep
        // reaches the top of the screen almost immediately (issue #46).
        KnobDrag::begin(e, *this,
                        pos - getMorphKnobBounds(morphIdx).getCentre().roundToInt());
        return;
    }

    // Voice arrows
    auto arrow = getVoiceArrowAt(pos);
    if (arrow != ArrowHit::None && patch)
    {
        auto& hdr = patch->getHeader();
        if (arrow == ArrowHit::Up)
            hdr.voices = juce::jmin(32, hdr.voices + 1);
        else
            hdr.voices = juce::jmax(1, hdr.voices - 1);
        repaint();
        if (voiceChangeCallback)
            voiceChangeCallback(hdr.voices);
        return;
    }

    // Cable toggles
    if (L.cablesCompact && L.cablesBtn.contains(pos.toFloat()))
    {
        showCablesMenu();
        return;
    }

    int cableIdx = getCableToggleAt(pos);
    if (cableIdx >= 0)
    {
        cableSwipeShow = !getCableVisibility(cableIdx);
        cableSwipe = true;
        setCableVisibility(cableIdx, cableSwipeShow);
        return;
    }

    // Shake cables button
    if (isShakeButtonAt(pos))
    {
        if (shakeCablesCallback)
            shakeCablesCallback();
        return;
    }

    // Bug report button
    if (isBugButtonAt(pos))
    {
        if (reportBugCallback)
            reportBugCallback();
        return;
    }

    // Morph A/B fader
    if (isMorphFaderAt(pos))
    {
        if (e.mods.isPopupMenu())
        {
            showMorphFaderMenu();
            return;
        }
        if (morphHasA && morphHasB)
        {
            morphDragging = true;
            morphFaderPos = morphFaderPosFromX(static_cast<float>(pos.x));
            repaint();
            if (morphFaderCallback)
                morphFaderCallback(morphFaderPos);
        }
        return;
    }

    // Patch Mutator button
    if (isMutatorButtonAt(pos))
    {
        if (mutatorButtonCallback)
            mutatorButtonCallback();
        return;
    }

    // Heart and Patreon
    if (supportCallback)
    {
        if (getDonateButtonBounds().expanded(2.0f).contains(pos.toFloat()))
        {
            supportCallback(VersionCheck::kDonateUrl);
            return;
        }
        if (getPatreonButtonBounds().expanded(2.0f).contains(pos.toFloat()))
        {
            supportCallback(VersionCheck::kPatreonUrl);
            return;
        }
    }

    // Snapshot buttons
    int snapIdx = getSnapshotButtonAt(pos);
    if (snapIdx >= 0)
    {
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addSectionHeader("Variation " + juce::String(snapIdx + 1));

            // Copy this variation to another slot (only when it holds data)
            juce::PopupMenu copyMenu;
            for (int t = 0; t < 8; ++t)
                if (t != snapIdx)
                    copyMenu.addItem(1000 + t, juce::String(t + 1)
                                     + (snapshotFilled[t] ? "  (overwrite)" : ""));
            menu.addSubMenu("Copy to", copyMenu, snapshotFilled[snapIdx]);
            menu.addItem(2000, "Init (default values)");
            menu.addSeparator();

            menu.addSectionHeader("Interpolation Time");
            menu.addItem(1, "Instant", true, snapshotInterpSeconds < 0.01f);
            for (float secs : { 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 30.0f, 60.0f })
            {
                int id = juce::roundToInt(secs * 10) + 1;  // offset by 1 to avoid clash with "Instant"=1
                juce::String label = (secs < 10.0f)
                    ? juce::String(secs, 0) + "s"
                    : juce::String(juce::roundToInt(secs)) + "s";
                menu.addItem(id, label, true, std::abs(snapshotInterpSeconds - secs) < 0.01f);
            }
            menu.showMenuAsync(juce::PopupMenu::Options{},
                [this, snapIdx](int result) {
                    if (result >= 1000 && result < 1008)
                    {
                        if (snapshotCopyCallback)
                            snapshotCopyCallback(snapIdx, result - 1000);
                    }
                    else if (result == 2000)
                    {
                        if (snapshotInitCallback)
                            snapshotInitCallback(snapIdx);
                    }
                    else if (result == 1)
                        snapshotInterpSeconds = 0.0f;
                    else if (result > 1)
                        snapshotInterpSeconds = (result - 1) / 10.0f;
                    repaint();
                });
        }
        else
        {
            bool isShift = e.mods.isShiftDown();
            if (isShift || !snapshotFilled[snapIdx])
            {
                // Shift+click or click on empty → save
                if (snapshotClickCallback)
                    snapshotClickCallback(snapIdx, true);
            }
            else if (snapshotInterpSeconds > 0.0f)
            {
                // Filled snapshot + interpolation time set → interpolate
                if (snapshotInterpolateCallback)
                    snapshotInterpolateCallback(activeSnapshot, snapIdx, snapshotInterpSeconds);
            }
            else
            {
                // Filled snapshot + instant → direct recall
                if (snapshotClickCallback)
                    snapshotClickCallback(snapIdx, false);
            }
        }
        return;
    }
}

void PatchHeaderBar::showCablesMenu()
{
    static const char* const names[numCableTypes] = { "Red (audio)", "Blue (control)", "Yellow (logic)",
                                                      "Grey (master/slave)", "Green", "Purple", "White" };
    juce::PopupMenu menu;
    menu.addSectionHeader("Show cables");
    for (int i = 0; i < numCableTypes; ++i)
        menu.addColouredItem(100 + i, names[i], cableColours[i], true, getCableVisibility(i));
    menu.addSeparator();
    menu.addItem(201, "Show / hide all cables");
    menu.addItem(200, "Shake cables");
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetScreenArea(
                           localAreaToGlobal(L.cablesBtn.toNearestInt())),
        [this](int result)
        {
            if (result >= 100 && result < 100 + numCableTypes)
                toggleCableVisibility(result - 100);
            else if (result == 201)
                toggleAllCables();
            else if (result == 200 && shakeCablesCallback)
                shakeCablesCallback();
        });
}

void PatchHeaderBar::showMorphFaderMenu()
{
    // Menu id ranges: A-from = 10 (current) / 20+i (snapshot i);
    //                 B-from = 40 (current) / 50+i (snapshot i);  Learn=3, Clear=4.
    juce::PopupMenu menu;
    menu.addSectionHeader("Morph A/B");

    juce::PopupMenu aMenu, bMenu;
    aMenu.addItem(10, "Current sound");
    bMenu.addItem(40, "Current sound");
    bool anySnap = false;
    for (int i = 0; i < 8; ++i) anySnap = anySnap || snapshotFilled[i];
    if (anySnap) { aMenu.addSeparator(); bMenu.addSeparator(); }
    for (int i = 0; i < 8; ++i)
    {
        if (!snapshotFilled[i]) continue;
        aMenu.addItem(20 + i, "Snapshot " + juce::String(i + 1));
        bMenu.addItem(50 + i, "Snapshot " + juce::String(i + 1));
    }
    menu.addSubMenu("Set A from", aMenu);
    menu.addSubMenu("Set B from", bMenu);
    menu.addSeparator();

    // Knob submenu: assign a physical panel knob from the editor (sent to the
    // synth via a spare morph group as carrier). Ids 100+k.
    {
        juce::PopupMenu knobSub;
        KnobMenu::fill(knobSub, patch, -1);
        menu.addSubMenu("Knob", knobSub);
    }

    menu.addItem(3, morphLearnArmed ? "Learning... (turn a panel knob)"
                                    : "Learn already-assigned knob", !morphLearnArmed);
    menu.addItem(4, "Clear knob assignment", morphKnobAssigned);

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this](int r)
        {
            if (r == 10)                 { if (morphSetEndpointCallback) morphSetEndpointCallback(false, -1); }
            else if (r >= 20 && r < 28)  { if (morphSetEndpointCallback) morphSetEndpointCallback(false, r - 20); }
            else if (r == 40)            { if (morphSetEndpointCallback) morphSetEndpointCallback(true, -1); }
            else if (r >= 50 && r < 58)  { if (morphSetEndpointCallback) morphSetEndpointCallback(true, r - 50); }
            else if (r >= 100 && r < 123){ if (morphAssignKnobCallback)  morphAssignKnobCallback(r - 100); }
            else if (r == 3)             { if (morphLearnCallback)       morphLearnCallback(); }
            else if (r == 4)             { if (morphClearKnobCallback)   morphClearKnobCallback(); }
        });
}

void PatchHeaderBar::showMorphKnobContextMenu(int morphIndex)
{
    // Morph knobs use section=2, module=1, param=morphIndex in the protocol
    constexpr int morphSection = 2;
    constexpr int morphModule = 1;

    const char* morphNames[] = { "Morph 1", "Morph 2", "Morph 3", "Morph 4" };

    // Find current knob assignment for this morph
    int currentKnob = -1;
    if (patch != nullptr)
    {
        for (int k = 0; k < 23; ++k)
        {
            const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == morphSection
                && ka.module == morphModule && ka.param == morphIndex)
            { currentKnob = k; break; }
        }
    }

    // Find current MIDI CC assignment for this morph
    int currentMidiCtrl = -1;
    if (patch != nullptr)
    {
        for (const auto& ca : patch->ctrlAssignments)
        {
            if (ca.section == morphSection && ca.module == morphModule && ca.param == morphIndex)
            { currentMidiCtrl = ca.control; break; }
        }
    }

    juce::PopupMenu menu;
    menu.addSectionHeader(morphNames[morphIndex]);

    // Knob assignment submenu
    {
        juce::PopupMenu knobSubMenu;
        KnobMenu::fill(knobSubMenu, patch, currentKnob);
        knobSubMenu.addSeparator();
        knobSubMenu.addItem(99, "Disable", currentKnob >= 0);
        menu.addSubMenu("Knob", knobSubMenu);
    }

    // MIDI Controller submenu
    {
        juce::PopupMenu midiSubMenu;
        for (int cc = 0; cc < 120; ++cc)
        {
            bool isCurrent = (cc == currentMidiCtrl);
            midiSubMenu.addItem(200 + cc, "CC " + juce::String(cc), true, isCurrent);
        }
        midiSubMenu.addSeparator();
        midiSubMenu.addItem(199, "Disable", currentMidiCtrl >= 0);
        menu.addSubMenu("MIDI Controller", midiSubMenu);
    }

    // Keyboard submenu (exclusive to morph knobs)
    {
        int currentKb = patch->morphKeyboard[static_cast<size_t>(morphIndex)];
        juce::PopupMenu kbSubMenu;
        kbSubMenu.addItem(401, "Velocity", true, currentKb == 1);
        kbSubMenu.addItem(402, "Note", true, currentKb == 2);
        kbSubMenu.addSeparator();
        kbSubMenu.addItem(400, "Disable", currentKb != 0);
        menu.addSubMenu("Keyboard", kbSubMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this, morphIndex, currentKnob, currentMidiCtrl](int result)
        {
            constexpr int ms = 2;  // morphSection
            constexpr int mm = 1;  // morphModule

            if (result == 99)
            {
                if (knobAssignCallback && currentKnob >= 0)
                    knobAssignCallback(ms, mm, morphIndex, -1);
            }
            else if (result >= 100 && result < 123)
            {
                if (knobAssignCallback)
                    knobAssignCallback(ms, mm, morphIndex, result - 100);
            }
            else if (result == 199)
            {
                if (midiCtrlAssignCallback && currentMidiCtrl >= 0)
                    midiCtrlAssignCallback(ms, mm, morphIndex, -1);
            }
            else if (result >= 200 && result < 320)
            {
                if (midiCtrlAssignCallback)
                    midiCtrlAssignCallback(ms, mm, morphIndex, result - 200);
            }
            // Keyboard assignment (400=disable, 401=velocity, 402=note)
            else if (result >= 400 && result <= 402)
            {
                int kb = result - 400;  // 0=disable, 1=velocity, 2=note
                if (keyboardAssignCallback)
                    keyboardAssignCallback(morphIndex, kb);
            }
        });
}

void PatchHeaderBar::mouseDrag(const juce::MouseEvent& e)
{
    if (cableSwipe)
    {
        const int idx = getCableToggleAt(e.getPosition());
        if (idx >= 0 && getCableVisibility(idx) != cableSwipeShow)
            setCableVisibility(idx, cableSwipeShow);
        return;
    }

    if (morphDragging)
    {
        morphFaderPos = morphFaderPosFromX(static_cast<float>(e.getPosition().x));
        repaint();
        if (morphFaderCallback)
            morphFaderCallback(morphFaderPos);
        return;
    }

    if (dragState.morphIndex < 0 || patch == nullptr)
        return;

    // Same reading of the mouse the canvas knobs get, so the editor's
    // knob-control setting means one thing everywhere (issue #47).
    int newVal = KnobDrag::valueFor(KnobDrag::travel(e), dragState.startValue, 0, 127);
    patch->morphValues[static_cast<size_t>(dragState.morphIndex)] = newVal;
    repaint();

    if (morphChangeCallback && newVal != dragState.lastSentValue)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - dragState.lastSendTime >= paramSendIntervalMs)
        {
            morphChangeCallback(dragState.morphIndex, newVal);
            dragState.lastSentValue = newVal;
            dragState.lastSendTime = now;
        }
    }
}

void PatchHeaderBar::mouseUp(const juce::MouseEvent& e)
{
    if (cableSwipe)
    {
        cableSwipe = false;
        return;
    }

    // Give the pointer back before anything else can return early.
    KnobDrag::end(e, *this);

    // Letting go of a nudge arrow stops the repeat. The pointer may have
    // wandered off the button while it was held, so look again now it is free.
    if (morphSpinner.mouseUp())
    {
        morphSpinner.updateHover(e.getPosition().toFloat());
        return;
    }

    if (morphDragging)
    {
        morphDragging = false;
        return;
    }

    if (dragState.morphIndex >= 0 && patch != nullptr && morphChangeCallback)
    {
        int finalVal = patch->morphValues[static_cast<size_t>(dragState.morphIndex)];
        if (finalVal != dragState.lastSentValue)
            morphChangeCallback(dragState.morphIndex, finalVal);
    }
    dragState = DragState();
}

void PatchHeaderBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!patch)
        return;

    // Check if double-click is on patch name area
    auto nameRect = getPatchNameBounds();
    if (nameRect.contains(e.getPosition()))
    {
        // Show editor and start editing
        patchNameEditor->setText(patch->getName(), juce::dontSendNotification);
        patchNameEditor->setVisible(true);
        patchNameEditor->showEditor();
    }
}
