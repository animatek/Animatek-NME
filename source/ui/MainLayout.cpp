#include "MainLayout.h"
#include "AppTheme.h"

// ============================================================
// SlotBar implementation
// ============================================================

constexpr const char* SlotBar::slotLetters[];

SlotBar::SlotBar()
{
    setInterceptsMouseClicks(true, false);
    startTimer(450);  // hardware-style blink for the focused slot's LED
}

SlotBar::~SlotBar()
{
    stopTimer();
}

void SlotBar::timerCallback()
{
    blinkPhase = !blinkPhase;
    for (int i = 0; i < numSlots; ++i)
        repaint(ledBounds(i));
}

void SlotBar::setCurrentTab(int index)
{
    if (index >= 0 && index < numSlots && index != activeIndex)
    {
        activeIndex = index;
        repaint();
    }
}

void SlotBar::setSlotName(int slot, const juce::String& patchName)
{
    if (slot >= 0 && slot < numSlots)
    {
        slotNames[slot] = patchName;
        repaint();
    }
}

void SlotBar::setSlotLocal(int slot, bool local)
{
    if (slot >= 0 && slot < numSlots && slotLocalFlags[slot] != local)
    {
        slotLocalFlags[slot] = local;
        repaint();
    }
}

void SlotBar::setSlotsEnabled(const std::array<bool, 4>& enabled)
{
    bool changed = false;
    for (int i = 0; i < numSlots; ++i)
    {
        if (slotEnabledFlags[i] != enabled[static_cast<size_t>(i)])
        {
            slotEnabledFlags[i] = enabled[static_cast<size_t>(i)];
            changed = true;
        }
    }
    if (changed)
        repaint();
}

juce::Rectangle<int> SlotBar::ledBounds(int slot) const
{
    // Small round LED at the right edge of the slot row
    auto bounds = slotBounds[slot];
    return { bounds.getRight() - 16, bounds.getCentreY() - 5, 10, 10 };
}

void SlotBar::resized()
{
    auto area = getLocalBounds();
    static constexpr int rowH = 24;
    for (int i = 0; i < numSlots; ++i)
        slotBounds[i] = area.removeFromTop(rowH);
}

void SlotBar::drawSlotIcon(juce::Graphics& g, juce::Rectangle<int> area, bool active)
{
    // Simple synth/keyboard icon
    auto iconArea = area.toFloat().reduced(1.0f);
    float x = iconArea.getX(), y = iconArea.getY();
    float w = iconArea.getWidth(), h = iconArea.getHeight();

    // Body
    g.setColour(active ? juce::Colour(0xffcc3333) : AppTheme::palette().borderColor);
    g.fillRoundedRectangle(x, y, w, h, 2.0f);

    // Keys (bottom half)
    float keyY = y + h * 0.55f;
    float keyH = h * 0.35f;
    int nKeys = 5;
    float keyW = (w - 4.0f) / nKeys;
    g.setColour(juce::Colours::white.withAlpha(active ? 0.9f : 0.5f));
    for (int k = 0; k < nKeys; ++k)
    {
        float kx = x + 2.0f + k * keyW;
        g.fillRect(kx + 0.5f, keyY, keyW - 1.0f, keyH);
    }

    // Knobs (top half)
    float knobY = y + h * 0.15f;
    float knobR = juce::jmin(keyW * 0.3f, h * 0.12f);
    g.setColour(juce::Colours::white.withAlpha(active ? 0.7f : 0.35f));
    for (int k = 0; k < 3; ++k)
    {
        float kx = x + w * 0.2f + k * w * 0.25f;
        g.fillEllipse(kx - knobR, knobY - knobR, knobR * 2, knobR * 2);
    }
}

void SlotBar::paint(juce::Graphics& g)
{
    g.fillAll(AppTheme::palette().backgroundPanel);

    for (int i = 0; i < numSlots; ++i)
    {
        auto bounds = slotBounds[i];
        bool active = (i == activeIndex);

        // Background
        if (active)
            g.setColour(juce::Colour(0xff3a3a3a));
        else
            g.setColour(AppTheme::palette().backgroundPanel);
        g.fillRect(bounds);

        // Left border highlight for active
        if (active)
        {
            g.setColour(AppTheme::palette().borderColor);
            g.fillRect(bounds.getX(), bounds.getY(), 3, bounds.getHeight());
        }

        // A patch is being dragged over this row and would land here. Drawn over
        // the row's own background and under everything else, so the letter and
        // the name stay readable.
        if (i == dropTargetSlot)
        {
            g.setColour(AppTheme::palette().accentActive.withAlpha(0.25f));
            g.fillRect(bounds);
            g.setColour(AppTheme::palette().accentActive);
            g.drawRect(bounds, 2);
        }

        // Icon (small fixed-size synth)
        auto iconArea = bounds.removeFromLeft(20).reduced(2);
        drawSlotIcon(g, iconArea, active);

        // Text: "A : PatchName" (leave room for the LED on the right, and for
        // the LOCAL badge when this slot is not synced to the synth)
        auto textArea = bounds.reduced(4, 0).withTrimmedRight(slotLocalFlags[i] ? 62 : 16);
        juce::String label = juce::String(slotLetters[i]) + " : ";
        if (slotNames[i].isNotEmpty())
            label += slotNames[i];

        g.setColour(active ? juce::Colours::white : AppTheme::palette().textSecondary);
        g.setFont(AppTheme::uiFont(12.0f));
        g.drawText(label, textArea, juce::Justification::centredLeft, true);

        // "LOCAL" badge: this slot's editor patch is not known to match the
        // synth (loaded Local, or edited/loaded while disconnected).
        if (slotLocalFlags[i])
        {
            auto row = slotBounds[i];
            juce::Rectangle<int> badge(row.getRight() - 16 - 44, row.getCentreY() - 8, 42, 16);
            g.setColour(juce::Colour(0xffd08a2c));
            g.fillRoundedRectangle(badge.toFloat(), 3.0f);
            g.setColour(juce::Colours::black);
            g.setFont(AppTheme::uiFont(9.0f).withStyle("Bold"));
            g.drawText("LOCAL", badge, juce::Justification::centred);
        }

        // Slot LED, mirroring the hardware: blinking = focused, fixed =
        // enabled, off = disabled. Ctrl+click the row to toggle enable.
        auto led = ledBounds(i).toFloat();
        bool ledOn = active ? blinkPhase : slotEnabledFlags[i];
        g.setColour(ledOn ? juce::Colour(0xff44cc44) : juce::Colour(0xff2a3a2a));
        g.fillEllipse(led);
        g.setColour(AppTheme::palette().borderColor);
        g.drawEllipse(led, 1.0f);

        // Bottom separator
        g.setColour(AppTheme::palette().buttonActive);
        g.drawHorizontalLine(slotBounds[i].getBottom() - 1,
                             static_cast<float>(slotBounds[i].getX()),
                             static_cast<float>(slotBounds[i].getRight()));
    }
}

int SlotBar::slotAt(juce::Point<int> pos) const
{
    for (int i = 0; i < numSlots; ++i)
        if (slotBounds[i].contains(pos))
            return i;
    return -1;
}

void SlotBar::updateDropTarget(int slot)
{
    if (dropTargetSlot == slot)
        return;

    // Repaint both rows rather than the whole bar: the highlight moves between
    // two of them and the LEDs on the others are on a blink timer.
    const int previous = dropTargetSlot;
    dropTargetSlot = slot;
    if (previous >= 0) repaint(slotBounds[previous]);
    if (slot >= 0)     repaint(slotBounds[slot]);
}

bool SlotBar::isInterestedInDragSource(const SourceDetails& details)
{
    return SlotDrop::isAccepted(details.description);
}

void SlotBar::itemDragEnter(const SourceDetails& details)
{
    updateDropTarget(slotAt(details.localPosition));
}

void SlotBar::itemDragMove(const SourceDetails& details)
{
    updateDropTarget(slotAt(details.localPosition));
}

void SlotBar::itemDragExit(const SourceDetails&)
{
    updateDropTarget(-1);
}

void SlotBar::itemDropped(const SourceDetails& details)
{
    const int slot = slotAt(details.localPosition);
    updateDropTarget(-1);

    // Dropped on the gap under the last row: no slot was named, so nothing is
    // loaded. Silently, because the highlight already said no target was armed.
    if (slot < 0)
        return;

    const auto& d = details.description;

    if (SlotDrop::isSynthPatch(d) && onPatchDroppedOnSlot)
        onPatchDroppedOnSlot((int) d.getProperty("section", -1),
                             (int) d.getProperty("position", -1),
                             slot);
    else if (SlotDrop::isPatchFile(d) && onPatchFileDroppedOnSlot)
        onPatchFileDroppedOnSlot(SlotDrop::fileOf(d), slot);
}

void SlotBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    for (int i = 0; i < numSlots; ++i)
    {
        if (!slotBounds[i].contains(pos))
            continue;

        // Ctrl+click toggles the slot's enable state without changing focus,
        // matching the original 3.3 editor (like holding the slot button on
        // the hardware). Plain click moves focus.
        // Ctrl/Cmd+click keeps its existing meaning (enable toggle) even on
        // platforms where that combination is itself reported as a popup-menu
        // click — check it first so a plain right-click (no modifier) is the
        // only thing that shows or hides the slot's sub-window.
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            if (onSlotEnableToggled)
                onSlotEnableToggled(i);
        }
        else if (e.mods.isPopupMenu())
        {
            if (onSlotViewToggled)
                onSlotViewToggled(i);
        }
        else if (i != activeIndex)
        {
            activeIndex = i;
            repaint();
            if (onSlotChanged)
                onSlotChanged(i);
        }
        break;
    }
}

// ============================================================
// PanelToggleStrip implementation
// ============================================================

PanelToggleStrip::PanelToggleStrip(bool leftEdge)
    : isLeftEdge(leftEdge)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void PanelToggleStrip::setPanelOpen(bool open)
{
    if (panelOpen == open)
        return;
    panelOpen = open;
    repaint();
}

void PanelToggleStrip::paint(juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    auto area = getLocalBounds();

    g.setColour(hovered ? pal.buttonActive : pal.backgroundSecondary);
    g.fillRect(area);

    // Hairline on the canvas-facing edge only, so the strip reads as part of
    // the panel's chrome rather than as a floating bar.
    g.setColour(pal.borderColor);
    if (isLeftEdge)
        g.drawVerticalLine(area.getRight() - 1, (float) area.getY(), (float) area.getBottom());
    else
        g.drawVerticalLine(area.getX(), (float) area.getY(), (float) area.getBottom());

    // Chevron points toward the panel while it is open (click collapses it)
    // and away from it once collapsed (click brings it back), matching the
    // slot windows.
    const float cx = (float) area.getCentreX();
    const float cy = (float) area.getCentreY();
    const float s = 4.0f;
    const bool pointsLeft = (isLeftEdge == panelOpen);

    juce::Path chevron;
    if (pointsLeft)
    {
        chevron.startNewSubPath(cx + s, cy - s * 1.4f);
        chevron.lineTo(cx - s, cy);
        chevron.lineTo(cx + s, cy + s * 1.4f);
    }
    else
    {
        chevron.startNewSubPath(cx - s, cy - s * 1.4f);
        chevron.lineTo(cx + s, cy);
        chevron.lineTo(cx - s, cy + s * 1.4f);
    }

    g.setColour(hovered ? pal.textPrimary : pal.textSecondary);
    g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void PanelToggleStrip::mouseDown(const juce::MouseEvent&)
{
    if (onToggle)
        onToggle();
}

void PanelToggleStrip::mouseEnter(const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void PanelToggleStrip::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

// ============================================================
// MainLayout implementation
// ============================================================

MainLayout::MainLayout(ModuleDescriptions& moduleDescs)
{
    slotBar.onSlotChanged = [this](int idx) {
        if (onSlotChanged)
            onSlotChanged(idx);
    };
    slotBar.onSlotViewToggled = [this](int idx) {
        if (onSlotViewToggled)
            onSlotViewToggled(idx);
    };

    midiButton.onClick = [this]() { if (onMidiSettingsClicked) onMidiSettingsClicked(); };

    libraryButton.onClick = [this]() { if (onLibraryFolderClicked) onLibraryFolderClicked(); };

    storeButton.onClick = [this]() { if (onStoreToBankClicked) onStoreToBankClicked(); };

    // Left column: inspector + toolbar + slots
    leftColumn.addAndMakeVisible(inspectorPanel);
    leftColumn.addAndMakeVisible(midiButton);
    leftColumn.addAndMakeVisible(libraryButton);
    leftColumn.addAndMakeVisible(storeButton);
    leftColumn.addAndMakeVisible(slotBar);

    rightBrowserTabs.setTabBarDepth(28);
    rightBrowserTabs.addTab("Synth", AppTheme::palette().backgroundPanel, &patchBrowserPanel, false);
    rightBrowserTabs.addTab("Disk", AppTheme::palette().backgroundPanel, &diskPresetBrowserPanel, false);

    leftToggleStrip.onToggle = [this]() {
        if (onPanelToggleRequested) onPanelToggleRequested(true);
        else                        setLeftPanelVisible(!leftPanelVisible);
    };
    rightToggleStrip.onToggle = [this]() {
        if (onPanelToggleRequested) onPanelToggleRequested(false);
        else                        setRightPanelVisible(!rightPanelVisible);
    };

    addAndMakeVisible(leftToggleStrip);
    addAndMakeVisible(rightToggleStrip);
    addAndMakeVisible(leftColumn);
    addAndMakeVisible(headerBar);
    moduleIconBar.setModuleDescriptions(&moduleDescs);
    addAndMakeVisible(moduleIconBar);
    addAndMakeVisible(patchArea);
    addAndMakeVisible(rightBrowserTabs);
    addAndMakeVisible(statusBar);
    addAndMakeVisible(resizerBar1);
    addAndMakeVisible(resizerBar2);

    // Layout: [leftColumn | bar | canvas | bar | patchBrowser]
    layoutManager.setItemLayout(0, 150, 350, 210);   // left column
    layoutManager.setItemLayout(1, 4, 4, 4);          // resizer
    layoutManager.setItemLayout(2, 200, -1.0, -0.6);  // canvas (most space)
    layoutManager.setItemLayout(3, 4, 4, 4);          // resizer
    layoutManager.setItemLayout(4, 150, 400, 220);    // patch browser (right)

    applyTheme();
}

void MainLayout::applyTheme()
{
    midiButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().inputBackground);
    midiButton.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
    midiButton.setColour(juce::TextButton::textColourOffId, AppTheme::palette().textSecondary);
    libraryButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().inputBackground);
    libraryButton.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
    libraryButton.setColour(juce::TextButton::textColourOffId, AppTheme::palette().textSecondary);
    storeButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().inputBackground);
    storeButton.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
    storeButton.setColour(juce::TextButton::textColourOffId, AppTheme::palette().textSecondary);

    moduleIconBar.applyTheme();

    rightBrowserTabs.setTabBackgroundColour(0, AppTheme::palette().backgroundPanel);
    rightBrowserTabs.setTabBackgroundColour(1, AppTheme::palette().backgroundPanel);

    // The work area behind the slot sub-windows, their own backgrounds and the
    // focus outline all follow the theme like any other chrome.
    patchArea.applyTheme();

    patchBrowserPanel.applyTheme();
    diskPresetBrowserPanel.applyTheme();
    inspectorPanel.applyTheme();
    statusBar.applyTheme();
    leftToggleStrip.repaint();
    rightToggleStrip.repaint();
    repaint();
}

void MainLayout::paint(juce::Graphics& g)
{
    g.fillAll(AppTheme::palette().backgroundPanel);
}

void MainLayout::resized()
{
    auto area = getLocalBounds();

    statusBar.setBounds(area.removeFromBottom(statusBarHeight));
    headerBar.setBounds(area.removeFromTop(headerBarHeight));
    if (moduleIconBarVisible)
        moduleIconBar.setBounds(area.removeFromTop(ModuleIconBar::preferredHeight));

    // The chevron strips sit outside the stretchable layout, at both edges, so
    // they keep their place whether or not the panel behind them is showing.
    leftToggleStrip.setBounds(area.removeFromLeft(PanelToggleStrip::stripWidth));
    rightToggleStrip.setBounds(area.removeFromRight(PanelToggleStrip::stripWidth));

    juce::Component* comps[] = {
        &leftColumn, &resizerBar1, &patchArea, &resizerBar2, &rightBrowserTabs
    };
    layoutManager.layOutComponents(comps, 5,
                                   area.getX(), area.getY(),
                                   area.getWidth(), area.getHeight(),
                                   false, true);

    // Layout left column: inspector | toolbar buttons | slot bar
    auto leftArea = leftColumn.getLocalBounds();
    slotBar.setBounds(leftArea.removeFromBottom(slotBarHeight));
    auto toolRow = leftArea.removeFromBottom(toolbarHeight);
    auto thirdW = toolRow.getWidth() / 3;
    midiButton.setBounds(toolRow.removeFromLeft(thirdW).reduced(2));
    libraryButton.setBounds(toolRow.removeFromLeft(thirdW).reduced(2));
    storeButton.setBounds(toolRow.reduced(2));
    inspectorPanel.setBounds(leftArea);
}

void MainLayout::setModuleIconBarVisible(bool visible)
{
    if (moduleIconBarVisible == visible)
        return;

    moduleIconBarVisible = visible;
    moduleIconBar.setVisible(visible);
    resized();
}

void MainLayout::showDiskPresetBrowser()
{
    rightBrowserTabs.setCurrentTabIndex(1);
}

void MainLayout::setLeftPanelVisible(bool visible)
{
    if (leftPanelVisible == visible)
        return;

    if (!visible && leftColumn.getWidth() > 0)
        savedLeftWidth = leftColumn.getWidth();

    leftPanelVisible = visible;
    leftColumn.setVisible(visible);
    resizerBar1.setVisible(visible);
    leftToggleStrip.setPanelOpen(visible);

    // Zero out the item and its resizer so the canvas absorbs the width.
    if (visible)
    {
        layoutManager.setItemLayout(0, 150, 350, savedLeftWidth);
        layoutManager.setItemLayout(1, 4, 4, 4);
    }
    else
    {
        layoutManager.setItemLayout(0, 0, 0, 0);
        layoutManager.setItemLayout(1, 0, 0, 0);
    }

    resized();
}

void MainLayout::setRightPanelVisible(bool visible)
{
    if (rightPanelVisible == visible)
        return;

    if (!visible && rightBrowserTabs.getWidth() > 0)
        savedRightWidth = rightBrowserTabs.getWidth();

    rightPanelVisible = visible;
    rightBrowserTabs.setVisible(visible);
    resizerBar2.setVisible(visible);
    rightToggleStrip.setPanelOpen(visible);

    if (visible)
    {
        layoutManager.setItemLayout(3, 4, 4, 4);
        layoutManager.setItemLayout(4, 150, 400, savedRightWidth);
    }
    else
    {
        layoutManager.setItemLayout(3, 0, 0, 0);
        layoutManager.setItemLayout(4, 0, 0, 0);
    }

    resized();
}
