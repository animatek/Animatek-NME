#include "PatchHeaderBar.h"
#include "AppTheme.h"
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

    // Layout constants for section widths
    constexpr int padL = 8;
    constexpr int sepGap = 14;
    constexpr int patchLblW = 44;
    constexpr int patchNameW = 150;
    constexpr int saveBtnW = 28;  // Quick save button width
    constexpr int patchSecW = patchLblW + patchNameW + saveBtnW + 8;  // +8 for spacing
    constexpr int voicesLblW = 50;
    constexpr int voicesValW = 32;
    constexpr int arrowBtnW = 16;
    constexpr int voicesSecW = voicesLblW + voicesValW + arrowBtnW;
    constexpr int loadLblW = 36;
    constexpr int loadBarTotalW = 70;
    constexpr int loadSecW = loadLblW + loadBarTotalW;
}

PatchHeaderBar::PatchHeaderBar()
{
    // Create patch name editor (initially hidden)
    patchNameEditor = std::make_unique<juce::Label>("PatchName", "");
    patchNameEditor->setEditable(true);
    patchNameEditor->setColour(juce::Label::backgroundColourId, AppTheme::palette().inputBackground);
    patchNameEditor->setColour(juce::Label::textColourId, juce::Colours::white);
    patchNameEditor->setColour(juce::Label::outlineColourId, AppTheme::palette().borderColor);
    patchNameEditor->setFont(juce::FontOptions(12.0f));
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

    // Quick save button removed — now in left column toolbar
}

void PatchHeaderBar::createDisketteIcon()
{
    // Create SVG diskette icon path
    juce::Path diskettePath;

    // Diskette outline (simplified classic floppy disk shape)
    diskettePath.addRoundedRectangle(2, 0, 20, 24, 1.0f);  // Main body
    diskettePath.addRectangle(2, 0, 20, 6);  // Top label area
    diskettePath.addRectangle(6, 8, 12, 10);  // Center metal shutter
    diskettePath.addRectangle(16, 2, 4, 2);  // Write protect notch

    // Normal state
    auto normalImage = std::make_unique<juce::DrawablePath>();
    normalImage->setPath(diskettePath);
    normalImage->setFill(juce::Colours::white.withAlpha(0.9f));
    normalImage->setStrokeFill(AppTheme::palette().borderColor);
    normalImage->setStrokeThickness(1.0f);

    // Disabled state (grayed out)
    auto disabledImage = std::make_unique<juce::DrawablePath>();
    disabledImage->setPath(diskettePath);
    disabledImage->setFill(juce::Colour(0xff555555));
    disabledImage->setStrokeFill(juce::Colour(0xff333333));
    disabledImage->setStrokeThickness(1.0f);

    quickSaveButton->setImages(normalImage.get(), nullptr, nullptr, disabledImage.get());
}

void PatchHeaderBar::setCurrentLocation(int section, int position)
{
    currentSection = section;
    currentPosition = position;

    (void)section; (void)position;
}

void PatchHeaderBar::clearCurrentLocation()
{
    currentSection = -1;
    currentPosition = -1;
}

void PatchHeaderBar::setPatch(Patch* p)
{
    patch = p;
    if (patchNameEditor->isVisible())
        patchNameEditor->setVisible(false);
    repaint();
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

    // (quick save button removed — now in left column toolbar)
}

juce::Rectangle<int> PatchHeaderBar::getPatchNameBounds() const
{
    int x = patchSecX_ + patchLblW;
    return juce::Rectangle<int>(x, 6, patchNameW, getHeight() - 12);
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
    // Dark knob body
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(cx, cy, size, size);

    // Colored ring
    g.setColour(colour);
    g.drawEllipse(cx + 1.0f, cy + 1.0f, size - 2.0f, size - 2.0f, 2.0f);

    // Grip line
    float angle = (-135.0f + normalized * 270.0f) * (juce::MathConstants<float>::pi / 180.0f);
    float centerX = cx + size * 0.5f;
    float centerY = cy + size * 0.5f;
    float sinA = std::sin(angle);
    float cosA = std::cos(angle);

    g.setColour(colour.brighter(0.4f));
    g.drawLine(centerX + sinA * size * 0.15f, centerY - cosA * size * 0.15f,
               centerX + sinA * size * 0.4f, centerY - cosA * size * 0.4f, 2.0f);

    // Label below in morph color
    g.setColour(colour.withAlpha(0.85f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(label, static_cast<int>(cx - 2), static_cast<int>(cy + size + 1),
               static_cast<int>(size + 4), 10, juce::Justification::centred, false);
}

void PatchHeaderBar::drawLoadBar(juce::Graphics& g, int x, int y, int w, int h,
                                  float percent, const juce::String& label)
{
    // Inner label (PVA: / E:)
    int lblW = 26;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(9.0f));
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
        g.setFont(juce::FontOptions(8.0f));
        g.drawText(juce::String(static_cast<int>(percent * 100)) + "%",
                   barX, y, barW, h, juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff888888));
        g.setFont(juce::FontOptions(8.0f));
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
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f));
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
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(patchName, nameRect.reduced(6, 0), juce::Justification::centredLeft, true);
    }

    // --- Voices ---
    x = voicesSecX_;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f));
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

        g.setColour(juce::Colour(0xffaaaaaa));
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
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f));
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
        g.setColour(juce::Colour(0xff555555));
        g.fillRoundedRectangle(sb, 3.0f);
        g.setColour(juce::Colour(0xff888888));
        g.drawRoundedRectangle(sb.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText("S", sb.toNearestInt(), juce::Justification::centred, false);
    }

    // --- Bug Report Button ---
    {
        auto bb = getBugButtonBounds();
        g.setColour(juce::Colour(0xff663333));
        g.fillRoundedRectangle(bb, 3.0f);
        g.setColour(juce::Colour(0xffaa6666));
        g.drawRoundedRectangle(bb.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffddaaaa));
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText("Report a bug", bb.toNearestInt(), juce::Justification::centred, false);
    }

    // --- Snapshot Buttons (1-8) ---
    {
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        for (int i = 0; i < 8; ++i)
        {
            auto sb = getSnapshotButtonBounds(i);
            bool filled = snapshotFilled[i];
            bool active = (i == activeSnapshot);

            // Background
            if (active)
                g.setColour(juce::Colour(0xffD4A020));  // gold for active
            else if (filled)
                g.setColour(AppTheme::palette().borderColor);  // grey for filled
            else
                g.setColour(juce::Colour(0xff3a3a3a));  // dark gray for empty

            g.fillRoundedRectangle(sb, 2.0f);

            // Border
            g.setColour(active ? juce::Colour(0xffE8C840) : juce::Colour(0xff666666));
            g.drawRoundedRectangle(sb.reduced(0.5f), 2.0f, 1.0f);

            // Number label
            g.setColour(active || filled ? juce::Colours::white : juce::Colour(0xff888888));
            g.drawText(juce::String(i + 1), sb.toNearestInt(), juce::Justification::centred, false);
        }

        // Interpolation time label (after last button)
        {
            auto last = getSnapshotButtonBounds(7);
            float labelX = last.getRight() + 4.0f;
            float labelY = last.getY();
            g.setFont(juce::FontOptions(9.0f));
            if (snapshotInterpSeconds > 0.0f)
            {
                g.setColour(juce::Colour(0xffD4A020));
                juce::String timeLabel = (snapshotInterpSeconds < 10.0f)
                    ? juce::String(snapshotInterpSeconds, 0) + "s"
                    : juce::String(juce::roundToInt(snapshotInterpSeconds)) + "s";
                g.drawText(timeLabel, juce::Rectangle<float>(labelX, labelY, 24.0f, last.getHeight()),
                           juce::Justification::centredLeft, false);
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
            g.setColour(juce::Colour(0xff333333));
            g.fillRect(first.getX(), barY, barW, barH);
            g.setColour(juce::Colour(0xffD4A020));
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
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(synthName, nameRect.reduced(4, 0), juce::Justification::centredLeft, true);
    }
}

// --- Mouse interaction ---

void PatchHeaderBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

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

    // Snapshot buttons
    int snapIdx = getSnapshotButtonAt(pos);
    if (snapIdx >= 0)
    {
        if (e.mods.isRightButtonDown())
        {
            // Right-click → set interpolation time (works on any snapshot button)
            juce::PopupMenu menu;
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
                [this](int result) {
                    if (result == 1)
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
    if (dragState.morphIndex < 0 || patch == nullptr)
        return;

    int deltaY = dragState.startPos.y - e.getPosition().y;
    int newVal = juce::jlimit(0, 127, dragState.startValue + static_cast<int>(deltaY * 0.5f));
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

void PatchHeaderBar::mouseUp(const juce::MouseEvent&)
{
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
