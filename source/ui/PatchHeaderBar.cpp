#include "PatchHeaderBar.h"
#include "AppTheme.h"
#include "KnobDrag.h"
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

    // Layout constants for section widths
    constexpr int padL = 8;
    constexpr int sepGap = 14;
    constexpr int patchLblW = 48;
    constexpr int patchNameW = 150;
    // Store-to-bank button: diskette + the bank location the patch lives at
    constexpr int storeBtnW = 60;
    constexpr int patchSecW = patchLblW + patchNameW + storeBtnW + 12;
    constexpr int voicesLblW = 54;
    constexpr int voicesValW = 32;
    constexpr int arrowBtnW = 16;
    constexpr int voicesSecW = voicesLblW + voicesValW + arrowBtnW;
    constexpr int loadLblW = 40;
    constexpr int loadBarTotalW = 74;
    constexpr int loadSecW = loadLblW + loadBarTotalW;
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
    patchNameEditor->setColour(juce::Label::backgroundColourId, AppTheme::palette().inputBackground);
    patchNameEditor->setColour(juce::Label::textColourId, juce::Colours::white);
    patchNameEditor->setColour(juce::Label::outlineColourId, AppTheme::palette().borderColor);
    patchNameEditor->setFont(AppTheme::uiFont(12.0f));
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
        repaint();  // Redraw after editing
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
    int x = padL;
    patchSecX_ = x;     x += patchSecW + sepGap;
    voicesSecX_ = x;    x += voicesSecW + sepGap;
    loadSecX_ = x;      x += loadSecW + sepGap;
    morphSecX_ = x;     x += 4 * (morphKnobSize + morphKnobSpacing) + sepGap;
    cableSecX_ = x;

    // Position patch name editor
    if (patchNameEditor)
        patchNameEditor->setBounds(getPatchNameBounds());
}

juce::Rectangle<int> PatchHeaderBar::getPatchNameBounds() const
{
    int x = patchSecX_ + patchLblW;
    return juce::Rectangle<int>(x, 6, patchNameW, getHeight() - 12);
}

juce::Rectangle<float> PatchHeaderBar::getStoreButtonBounds() const
{
    auto name = getPatchNameBounds();
    return juce::Rectangle<float>(static_cast<float>(name.getRight() + 6),
                                  static_cast<float>(name.getY()),
                                  static_cast<float>(storeBtnW),
                                  static_cast<float>(name.getHeight()));
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
    float kx = static_cast<float>(morphSecX_ + i * (morphKnobSize + morphKnobSpacing));
    float ky = (static_cast<float>(getHeight()) - morphKnobSize) / 2.0f - 5.0f;
    return { kx, ky, static_cast<float>(morphKnobSize), static_cast<float>(morphKnobSize) };
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
    float cx = static_cast<float>(cableSecX_ + i * (cableToggleSize + cableToggleSpacing));
    float cy = (static_cast<float>(getHeight()) - cableToggleSize) / 2.0f;
    return { cx, cy, static_cast<float>(cableToggleSize), static_cast<float>(cableToggleSize) };
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
    // Place "S" button right after the last cable toggle, with some spacing
    float lastToggleRight = static_cast<float>(cableSecX_ + numCableTypes * (cableToggleSize + cableToggleSpacing));
    float cy = (static_cast<float>(getHeight()) - cableToggleSize) / 2.0f;
    return { lastToggleRight + 2.0f, cy, static_cast<float>(cableToggleSize), static_cast<float>(cableToggleSize) };
}

bool PatchHeaderBar::isShakeButtonAt(juce::Point<int> pos) const
{
    return getShakeButtonBounds().expanded(2.0f).contains(pos.toFloat());
}

juce::Rectangle<float> PatchHeaderBar::getBugButtonBounds() const
{
    auto shakeBounds = getShakeButtonBounds();
    return { shakeBounds.getRight() + 6.0f, shakeBounds.getY(),
             80.0f, static_cast<float>(cableToggleSize) };
}

bool PatchHeaderBar::isBugButtonAt(juce::Point<int> pos) const
{
    return getBugButtonBounds().expanded(2.0f).contains(pos.toFloat());
}

juce::Rectangle<float> PatchHeaderBar::getSnapshotButtonBounds(int index) const
{
    auto bugBounds = getBugButtonBounds();
    float startX = bugBounds.getRight() + 12.0f;
    float btnSize = static_cast<float>(cableToggleSize);
    float spacing = 3.0f;
    float cy = (static_cast<float>(getHeight()) - btnSize) / 2.0f;
    return { startX + index * (btnSize + spacing), cy, btnSize, btnSize };
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
    // Between the snapshot row (past the interpolation-time label) and MUT.
    auto last = getSnapshotButtonBounds(7);
    return { last.getRight() + 28.0f, last.getY(), 76.0f, last.getHeight() };
}

bool PatchHeaderBar::isMorphFaderAt(juce::Point<int> pos) const
{
    return getMorphFaderBounds().expanded(2.0f).contains(pos.toFloat());
}

float PatchHeaderBar::morphFaderPosFromX(float x) const
{
    auto fb = getMorphFaderBounds();
    constexpr float inset = 11.0f;  // room for the A/B end labels
    float l = fb.getX() + inset, r = fb.getRight() - inset;
    return juce::jlimit(0.0f, 1.0f, (x - l) / juce::jmax(1.0f, r - l));
}

juce::Rectangle<float> PatchHeaderBar::getMutatorButtonBounds() const
{
    // Right of the morph fader
    auto fader = getMorphFaderBounds();
    return { fader.getRight() + 10.0f, fader.getY(), 36.0f, fader.getHeight() };
}

bool PatchHeaderBar::isMutatorButtonAt(juce::Point<int> pos) const
{
    return getMutatorButtonBounds().expanded(2.0f).contains(pos.toFloat());
}

juce::Rectangle<float> PatchHeaderBar::getRetileButtonBounds() const
{
    // Right of MUT, where the issue asked for it, but two rows tall instead of
    // one: the face is a picture of the layout it produces (A|B over C|D), and
    // two rows of letters do not fit in MUT's single-line height. The bar is
    // 48px, so the extra height costs nothing.
    auto mut = getMutatorButtonBounds();
    constexpr float w = 34.0f, h = 22.0f;
    return { mut.getRight() + 6.0f, mut.getCentreY() - h * 0.5f, w, h };
}

bool PatchHeaderBar::isRetileButtonAt(juce::Point<int> pos) const
{
    return retileEnabled && getRetileButtonBounds().expanded(2.0f).contains(pos.toFloat());
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
    int ax = voicesSecX_ + voicesLblW + voicesValW;
    auto area = juce::Rectangle<int>(ax, 0, arrowBtnW, getHeight());
    if (!area.contains(pos))
        return ArrowHit::None;
    return pos.y < getHeight() / 2 ? ArrowHit::Up : ArrowHit::Down;
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
    if (!patch) return;
    auto& hdr = patch->getHeader();
    switch (index)
    {
        case 0: hdr.cableVisRed    = !hdr.cableVisRed; break;
        case 1: hdr.cableVisBlue   = !hdr.cableVisBlue; break;
        case 2: hdr.cableVisYellow = !hdr.cableVisYellow; break;
        case 3: hdr.cableVisGray   = !hdr.cableVisGray; break;
        case 4: hdr.cableVisGreen  = !hdr.cableVisGreen; break;
        case 5: hdr.cableVisPurple = !hdr.cableVisPurple; break;
        case 6: hdr.cableVisWhite  = !hdr.cableVisWhite; break;
    }
    repaint();
    if (cableVisCallback) cableVisCallback();
}

// --- Drawing ---

void PatchHeaderBar::drawMorphKnob(juce::Graphics& g, float cx, float cy, float size,
                                    float normalized, const juce::String& label,
                                    juce::Colour colour)
{
    // Body always filled with the morph colour so the macro reads at a glance,
    // regardless of the theme background; a black outline + pointer define the dial.
    g.setColour(colour);
    g.fillEllipse(cx, cy, size, size);

    // Contrast against the dial itself; the four morph fills span both light
    // and dark colours independently of the selected UI theme.
    const auto ink = contrastingInk(colour);
    g.setColour(ink);
    g.drawEllipse(cx + 1.0f, cy + 1.0f, size - 2.0f, size - 2.0f, 1.5f);

    // Grip line
    float angle = (-135.0f + normalized * 270.0f) * (juce::MathConstants<float>::pi / 180.0f);
    float centerX = cx + size * 0.5f;
    float centerY = cy + size * 0.5f;
    float sinA = std::sin(angle);
    float cosA = std::cos(angle);

    g.setColour(ink);
    g.drawLine(centerX + sinA * size * 0.15f, centerY - cosA * size * 0.15f,
               centerX + sinA * size * 0.4f, centerY - cosA * size * 0.4f, 2.0f);

    // Caption below in the theme's ink, not the macro colour: the dial itself
    // already carries the colour, and green M2 was unreadable against the light
    // Nord Classic chrome. All four read as one row this way.
    g.setColour(AppTheme::macroLabelInk());
    g.setFont(AppTheme::uiFont(9.0f));
    // 12 rather than 10: the caption box has to hold the scaled font without
    // clipping, and the bar is 48 tall so there is room below the dial.
    g.drawText(label, static_cast<int>(cx - 2), static_cast<int>(cy + size + 1),
               static_cast<int>(size + 4), 12, juce::Justification::centred, false);
}

void PatchHeaderBar::drawLoadBar(juce::Graphics& g, int x, int y, int w, int h,
                                  float percent, const juce::String& label)
{
    // Inner label (PVA: / E:)
    int lblW = 30;
    g.setColour(AppTheme::palette().textSecondary);
    g.setFont(AppTheme::uiFont(9.0f));
    g.drawText(label, x, y, lblW, h, juce::Justification::centredLeft);

    int barX = x + lblW;
    int barW = w - lblW;

    // Bar background
    g.setColour(AppTheme::palette().inputBackground);
    g.fillRect(barX, y, barW, h);
    g.setColour(AppTheme::palette().borderColor);
    g.drawRect(barX, y, barW, h, 1);

    if (percent >= 0.0f)
    {
        int fillW = juce::jlimit(0, barW - 2, static_cast<int>(percent * (barW - 2)));
        juce::Colour barCol = percent < 0.6f  ? juce::Colour(0xff5a9a5a)
                            : percent < 0.85f ? juce::Colour(0xffb0a030)
                                              : juce::Colour(0xffcb4f4f);
        g.setColour(barCol);
        g.fillRect(barX + 1, y + 1, fillW, h - 2);

        g.setColour(juce::Colours::white);
        g.setFont(AppTheme::uiFont(8.0f));
        // One decimal, matching the original Clavia editor (e.g. "47.6%"); the
        // load is a client-side estimate so a truncated integer read a hair high
        // (a 99.5%-cycle patch showing "100%").
        g.drawText(juce::String(percent * 100.0f, 1) + "%",
                   barX, y, barW, h, juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff888888));
        g.setFont(AppTheme::uiFont(8.0f));
        g.drawText("--%", barX, y, barW, h, juce::Justification::centred);
    }
}

void PatchHeaderBar::paint(juce::Graphics& g)
{
    int h = getHeight();

    // Background
    g.fillAll(AppTheme::palette().backgroundPanel);

    // Bottom border
    g.setColour(AppTheme::palette().buttonActive);
    g.drawLine(0.0f, static_cast<float>(h) - 0.5f,
               static_cast<float>(getWidth()), static_cast<float>(h) - 0.5f, 1.0f);

    // Separators between sections
    auto drawSep = [&](int sectionX)
    {
        float sx = static_cast<float>(sectionX) - sepGap * 0.5f;
        g.setColour(AppTheme::palette().buttonActive);
        g.drawLine(sx, 4.0f, sx, static_cast<float>(h - 4), 1.0f);
    };
    drawSep(voicesSecX_);
    drawSep(loadSecX_);
    drawSep(morphSecX_);
    drawSep(cableSecX_);

    // --- Patch Name ---
    int x = patchSecX_;
    g.setColour(AppTheme::palette().textSecondary);
    g.setFont(AppTheme::uiFont(11.0f));
    g.drawText("Patch:", x, 0, patchLblW, h, juce::Justification::centredLeft);
    x += patchLblW;

    // Only draw name if editor is not visible
    if (!patchNameEditor || !patchNameEditor->isVisible())
    {
        auto nameRect = juce::Rectangle<int>(x, 6, patchNameW, h - 12);
        g.setColour(AppTheme::palette().inputBackground);
        g.fillRoundedRectangle(nameRect.toFloat(), 3.0f);
        g.setColour(AppTheme::palette().borderColor);
        g.drawRoundedRectangle(nameRect.toFloat(), 3.0f, 1.0f);

        juce::String patchName = patch ? patch->getName() : "No Patch";
        g.setColour(juce::Colours::white);
        g.setFont(AppTheme::uiFont(12.0f));
        g.drawText(patchName, nameRect.reduced(6, 0), juce::Justification::centredLeft, true);
    }

    // --- Store to bank button (diskette + the location the patch lives at) ---
    {
        auto sb = getStoreButtonBounds();
        const bool known = currentSection >= 0 && currentPosition >= 0;
        const bool live = storeEnabled && patch != nullptr;

        g.setColour(live && storeHover ? AppTheme::palette().buttonActive
                                       : AppTheme::palette().inputBackground);
        g.fillRoundedRectangle(sb, 3.0f);
        g.setColour(live ? AppTheme::palette().borderColor
                         : AppTheme::palette().borderColor.withAlpha(0.4f));
        g.drawRoundedRectangle(sb.reduced(0.5f), 3.0f, 1.0f);

        // Diskette: body, shutter, label
        const float ds = 11.0f;
        const float dx = sb.getX() + 6.0f;
        const float dy = sb.getCentreY() - ds * 0.5f;
        auto ink = live ? AppTheme::palette().textPrimary : AppTheme::palette().textMuted;
        g.setColour(ink);
        g.drawRoundedRectangle(dx, dy, ds, ds, 1.5f, 1.0f);
        g.fillRect(dx + 3.0f, dy + 1.0f, ds - 6.0f, ds * 0.35f);          // shutter
        g.fillRect(dx + 2.0f, dy + ds * 0.58f, ds - 4.0f, ds * 0.32f);    // label

        g.setColour(live ? (known ? AppTheme::palette().textPrimary
                                  : AppTheme::palette().textMuted)
                         : AppTheme::palette().textMuted);
        g.setFont(AppTheme::uiFont(10.0f).withStyle("Bold"));
        juce::String loc = known
            ? juce::String(currentSection + 1) + ":" + juce::String(currentPosition + 1).paddedLeft('0', 2)
            : juce::String(storeUncertain ? "?" : "--");
        g.drawText(loc, sb.withTrimmedLeft(ds + 9.0f).withTrimmedRight(3.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);
    }

    // --- Voices ---
    x = voicesSecX_;
    g.setColour(AppTheme::palette().textSecondary);
    g.setFont(AppTheme::uiFont(11.0f));
    g.drawText("Voices:", x, 0, voicesLblW, h, juce::Justification::centredLeft);
    x += voicesLblW;

    int voices = patch ? patch->getHeader().voices : 0;
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(voices) + "/-", x, 0, voicesValW, h, juce::Justification::centredLeft);
    x += voicesValW;

    // Up/Down arrows
    {
        float arrowMidX = static_cast<float>(x) + arrowBtnW * 0.5f;
        int midY = h / 2;

        g.setColour(AppTheme::palette().textSecondary);
        juce::Path up;
        up.addTriangle(arrowMidX, static_cast<float>(midY - 7),
                       arrowMidX - 4.0f, static_cast<float>(midY - 1),
                       arrowMidX + 4.0f, static_cast<float>(midY - 1));
        g.fillPath(up);

        juce::Path down;
        down.addTriangle(arrowMidX - 4.0f, static_cast<float>(midY + 1),
                         arrowMidX + 4.0f, static_cast<float>(midY + 1),
                         arrowMidX, static_cast<float>(midY + 7));
        g.fillPath(down);
    }

    // --- Load ---
    x = loadSecX_;
    g.setColour(AppTheme::palette().textSecondary);
    g.setFont(AppTheme::uiFont(11.0f));
    g.drawText("Load:", x, 0, loadLblW, h, juce::Justification::centredLeft);

    int barX = loadSecX_ + loadLblW;
    int barH = 10;
    int topBarY = h / 2 - barH - 1;
    int botBarY = h / 2 + 1;
    drawLoadBar(g, barX, topBarY, loadBarTotalW, barH, loadPva, "PVA:");
    drawLoadBar(g, barX, botBarY, loadBarTotalW, barH, loadE, "E:");

    // --- Morph Knobs ---
    for (int i = 0; i < 4; i++)
    {
        auto kb = getMorphKnobBounds(i);
        float norm = 0.0f;
        if (patch)
            norm = static_cast<float>(patch->morphValues[static_cast<size_t>(i)]) / 127.0f;

        drawMorphKnob(g, kb.getX(), kb.getY(), kb.getWidth(), norm,
                      "M" + juce::String(i + 1), morphColours[i]);
    }

    // --- Cable Visibility Toggles ---
    for (int i = 0; i < numCableTypes; i++)
    {
        auto tb = getCableToggleBounds(i);
        bool vis = getCableVisibility(i);

        if (vis)
        {
            g.setColour(cableColours[i]);
            g.fillEllipse(tb);
            g.setColour(cableColours[i].darker(0.3f));
            g.drawEllipse(tb.reduced(0.5f), 1.0f);
        }
        else
        {
            g.setColour(cableColours[i].withAlpha(0.3f));
            g.drawEllipse(tb.reduced(0.5f), 1.5f);
        }
    }

    // --- Shake Cables Button ("S") ---
    {
        auto sb = getShakeButtonBounds();
        g.setColour(AppTheme::palette().buttonBackground);
        g.fillRoundedRectangle(sb, 3.0f);
        g.setColour(AppTheme::palette().borderColor);
        g.drawRoundedRectangle(sb.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(AppTheme::palette().textSecondary);
        g.setFont(AppTheme::uiFont(10.0f).withStyle("Bold"));
        g.drawText("S", sb.toNearestInt(), juce::Justification::centred, false);
    }

    // --- Bug Report Button ---
    {
        auto bb = getBugButtonBounds();
        g.setColour(AppTheme::palette().buttonBackground);
        g.fillRoundedRectangle(bb, 3.0f);
        g.setColour(AppTheme::palette().borderColor);
        g.drawRoundedRectangle(bb.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(AppTheme::palette().textSecondary);
        g.setFont(AppTheme::uiFont(10.0f).withStyle("Bold"));
        g.drawText("Report a bug", bb.toNearestInt(), juce::Justification::centred, false);
    }

    // --- Snapshot Buttons (1-8) ---
    {
        g.setFont(AppTheme::uiFont(9.0f).withStyle("Bold"));
        for (int i = 0; i < 8; ++i)
        {
            auto sb = getSnapshotButtonBounds(i);
            bool filled = snapshotFilled[i];
            bool active = (i == activeSnapshot);

            // Background
            const auto fill = active ? AppTheme::palette().buttonActive
                                     : filled ? AppTheme::palette().borderColor
                                              : AppTheme::palette().buttonBackground;
            g.setColour(fill);
            g.fillRoundedRectangle(sb, 2.0f);

            // Border
            g.setColour(active ? AppTheme::palette().textMuted : AppTheme::palette().borderColor);
            g.drawRoundedRectangle(sb.reduced(0.5f), 2.0f, 1.0f);

            // Number label
            g.setColour(active ? contrastingInk(fill)
                               : filled ? AppTheme::palette().textPrimary : AppTheme::palette().textMuted);
            g.drawText(juce::String(i + 1), sb.toNearestInt(), juce::Justification::centred, false);
        }

        // Interpolation time label (after last button)
        {
            auto last = getSnapshotButtonBounds(7);
            float labelX = last.getRight() + 4.0f;
            float labelY = last.getY();
            g.setFont(AppTheme::uiFont(9.0f));
            if (snapshotInterpSeconds > 0.0f)
            {
                g.setColour(AppTheme::palette().textSecondary);
                juce::String timeLabel = (snapshotInterpSeconds < 10.0f)
                    ? juce::String(snapshotInterpSeconds, 0) + "s"
                    : juce::String(juce::roundToInt(snapshotInterpSeconds)) + "s";
                g.drawText(timeLabel, juce::Rectangle<float>(labelX, labelY, 24.0f, last.getHeight()),
                           juce::Justification::centredLeft, false);
            }
        }

        // Morph A/B fader (editor-side software morph between two captures)
        {
            auto fb = getMorphFaderBounds();
            const bool ready = morphHasA && morphHasB;

            // Track
            g.setColour(AppTheme::palette().inputBackground);
            g.fillRoundedRectangle(fb, 3.0f);
            g.setColour(morphLearnArmed ? AppTheme::palette().buttonActive
                                        : AppTheme::palette().borderColor);
            g.drawRoundedRectangle(fb.reduced(0.5f), 3.0f, 1.0f);

            // A / B end labels (dimmed until that endpoint is captured)
            g.setFont(AppTheme::uiFont(8.0f).withStyle("Bold"));
            g.setColour(morphHasA ? AppTheme::palette().textSecondary
                                  : AppTheme::palette().textMuted);
            g.drawText("A", juce::Rectangle<float>(fb.getX() + 2.0f, fb.getY(),
                                                   9.0f, fb.getHeight()),
                       juce::Justification::centred, false);
            g.setColour(morphHasB ? AppTheme::palette().textSecondary
                                  : AppTheme::palette().textMuted);
            g.drawText("B", juce::Rectangle<float>(fb.getRight() - 11.0f, fb.getY(),
                                                   9.0f, fb.getHeight()),
                       juce::Justification::centred, false);

            // Thumb
            constexpr float inset = 11.0f;
            float l = fb.getX() + inset, r = fb.getRight() - inset;
            float thumbX = l + (r - l) * morphFaderPos;
            auto thumbCol = !ready ? AppTheme::palette().textMuted
                          : morphKnobAssigned ? AppTheme::palette().buttonActive
                                              : AppTheme::palette().textSecondary;
            g.setColour(thumbCol);
            g.fillRoundedRectangle(thumbX - 2.0f, fb.getY() + 1.5f, 4.0f,
                                   fb.getHeight() - 3.0f, 1.5f);
        }

        // Patch Mutator quick-access button
        {
            auto mb = getMutatorButtonBounds();
            g.setColour(mutatorOpen ? AppTheme::palette().buttonActive
                                    : AppTheme::palette().buttonBackground);
            g.fillRoundedRectangle(mb, 2.0f);
            g.setColour(mutatorOpen ? AppTheme::palette().textMuted
                                    : AppTheme::palette().borderColor);
            g.drawRoundedRectangle(mb.reduced(0.5f), 2.0f, 1.0f);
            g.setColour(mutatorOpen ? contrastingInk(AppTheme::palette().buttonActive)
                                    : AppTheme::palette().textSecondary);
            g.setFont(AppTheme::uiFont(9.0f).withStyle("Bold"));
            g.drawText("MUT", mb.toNearestInt(), juce::Justification::centred, false);
        }

        // ABCD: put the slot sub-windows back in order (#51)
        {
            auto rb = getRetileButtonBounds();
            g.setColour(AppTheme::palette().buttonBackground);
            g.fillRoundedRectangle(rb, 2.0f);
            g.setColour(AppTheme::palette().borderColor);
            g.drawRoundedRectangle(rb.reduced(0.5f), 2.0f, 1.0f);
            // Nothing to reorder is said by the ink, not by hiding the button:
            // it keeps its place in the bar so the row does not shuffle about.
            const auto ink = retileEnabled ? AppTheme::palette().textSecondary
                                           : AppTheme::palette().textMuted;

            // The four letters sit in the quadrants they actually land in, with
            // the tile divisions drawn between them, so the button reads as the
            // arrangement it produces rather than as the word "ABCD".
            auto cells = rb.reduced(2.0f);
            g.setColour(ink.withAlpha(0.35f));
            g.drawLine(cells.getCentreX(), cells.getY(),
                       cells.getCentreX(), cells.getBottom(), 1.0f);
            g.drawLine(cells.getX(), cells.getCentreY(),
                       cells.getRight(), cells.getCentreY(), 1.0f);

            g.setColour(ink);
            g.setFont(AppTheme::uiFont(8.0f).withStyle("Bold"));
            static const char* const tileLetters[] = { "A", "B", "C", "D" };
            for (int i = 0; i < 4; ++i)
            {
                juce::Rectangle<float> cell(
                    cells.getX() + static_cast<float>(i % 2) * cells.getWidth()  * 0.5f,
                    cells.getY() + static_cast<float>(i / 2) * cells.getHeight() * 0.5f,
                    cells.getWidth() * 0.5f, cells.getHeight() * 0.5f);
                g.drawText(tileLetters[i], cell.toNearestInt(),
                           juce::Justification::centred, false);
            }
        }

        // Interpolation progress bar (below snapshot buttons)
        if (interpolationProgress >= 0.0f)
        {
            auto first = getSnapshotButtonBounds(0);
            auto last = getSnapshotButtonBounds(7);
            float barY = first.getBottom() + 1.0f;
            float barW = last.getRight() - first.getX();
            float barH = 2.0f;
            g.setColour(AppTheme::palette().inputBackground);
            g.fillRect(first.getX(), barY, barW, barH);
            g.setColour(AppTheme::palette().textSecondary);
            g.fillRect(first.getX(), barY, barW * interpolationProgress, barH);
        }
    }

    // --- Synth Connection Indicator (right-aligned) ---
    if (!synthName.isEmpty())
    {
        constexpr int synthNameW = 110;
        constexpr int synthPad = 10;
        int sx = getWidth() - synthNameW - synthPad;

        // Separator
        {
            float sepX = static_cast<float>(sx) - sepGap * 0.5f;
            g.setColour(AppTheme::palette().buttonActive);
            g.drawLine(sepX, 4.0f, sepX, static_cast<float>(h - 4), 1.0f);
        }

        // Synth name box
        auto nameRect = juce::Rectangle<int>(sx, 6, synthNameW, h - 12);
        g.setColour(AppTheme::palette().inputBackground);
        g.fillRoundedRectangle(nameRect.toFloat(), 3.0f);
        g.setColour(AppTheme::palette().borderColor);
        g.drawRoundedRectangle(nameRect.toFloat(), 3.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(AppTheme::uiFont(11.0f));
        g.drawText(synthName, nameRect.reduced(4, 0), juce::Justification::centredLeft, true);
    }

    // Last, so the arrows sit on top of the dial they belong to.
    morphSpinner.paint(g, { AppTheme::palette().inputBackground,
                            AppTheme::palette().borderColor,
                            AppTheme::macroLabelInk() });
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
    int cableIdx = getCableToggleAt(pos);
    if (cableIdx >= 0)
    {
        toggleCableVisibility(cableIdx);
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

    // ABCD re-tile button
    if (isRetileButtonAt(pos))
    {
        if (retileButtonCallback)
            retileButtonCallback();
        return;
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
        auto addKnob = [&](int k, const juce::String& name)
        {
            juce::String label = name;
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned)
                label += " (used)";
            knobSub.addItem(100 + k, label);
        };
        for (int k = 0; k < 6; ++k)  addKnob(k, "Knob " + juce::String(k + 1));
        knobSub.addSeparator();
        for (int k = 6; k < 12; ++k) addKnob(k, "Knob " + juce::String(k + 1));
        knobSub.addSeparator();
        for (int k = 12; k < 15; ++k) addKnob(k, "Knob " + juce::String(k + 1));
        knobSub.addSeparator();
        for (int k = 15; k < 18; ++k) addKnob(k, "Knob " + juce::String(k + 1));
        knobSub.addSeparator();
        for (int k : { 19, 20, 22 }) addKnob(k, KnobAssignmentMessage::getKnobName(k));
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
        for (int k = 0; k < 6; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        for (int k = 6; k < 12; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        for (int k = 12; k < 15; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        for (int k = 15; k < 18; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Pedal=19, After touch=20, On/Off switch=22 (18 and 21 unused in the wire format)
        for (int k : { 19, 20, 22 })
        {
            juce::String label = KnobAssignmentMessage::getKnobName(k);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
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
