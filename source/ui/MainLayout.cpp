#include "MainLayout.h"
#include "AppTheme.h"

// ============================================================
// SlotBar implementation
// ============================================================

constexpr const char* SlotBar::slotLetters[];

SlotBar::SlotBar()
{
    for (auto& f : loadFraction)
        f = -1.0f;
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
    if (loadProvider)
        for (int synth = 0; synth < kMaxSynths; ++synth)
            setLoad(synth, loadProvider(synth));
    for (int i = 0; i < numSlots; ++i)
        repaint(slotBounds[i]);
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

void SlotBar::setSlotsEnabled(int synth, const std::array<bool, 4>& enabled)
{
    bool changed = false;
    for (int l = 0; l < kSlotsPerSynth; ++l)
    {
        const int i = SynthSlot::global(synth, l);
        if (slotEnabledFlags[i] != enabled[static_cast<size_t>(l)])
        {
            slotEnabledFlags[i] = enabled[static_cast<size_t>(l)];
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void SlotBar::setSynthName(int synth, const juce::String& name)
{
    if (synth < 0 || synth >= kMaxSynths || synthName[synth] == name)
        return;
    synthName[synth] = name;
    repaint(nameBounds[synth]);
}

void SlotBar::setLoad(int synth, float fraction)
{
    if (synth < 0 || synth >= kMaxSynths)
        return;
    fraction = fraction < 0.0f ? -1.0f : juce::jlimit(0.0f, 1.0f, fraction);
    if (juce::approximatelyEqual(loadFraction[synth], fraction))
        return;
    loadFraction[synth] = fraction;
    repaint(loadBounds[synth]);
}

void SlotBar::setRowShown(int synth, bool shown)
{
    if (synth <= 0 || synth >= kMaxSynths || rowShown[synth] == shown)
        return;   // Port 1's row is always there
    rowShown[synth] = shown;
    resized();
    repaint();
    if (onRowsChanged)
        onRowsChanged();
}

int SlotBar::getPreferredHeight() const
{
    int rows = 0;
    for (bool shown : rowShown)
        rows += shown ? 1 : 0;
    return 30 * juce::jmax(1, rows);
}

void SlotBar::resized()
{
    auto all = getLocalBounds();
    static constexpr int buttonW = 20;
    for (int synth = 0; synth < kMaxSynths; ++synth)
    {
        if (! rowShown[synth])
        {
            // Nothing on screen, so nothing to click or drop onto either.
            for (int l = 0; l < kSlotsPerSynth; ++l)
                slotBounds[SynthSlot::global(synth, l)] = {};
            nameBounds[synth] = loadBounds[synth] = {};
            continue;
        }
        auto area = all.removeFromTop(30).reduced(4, 4);
        for (int l = 0; l < kSlotsPerSynth; ++l)
            slotBounds[SynthSlot::global(synth, l)] = area.removeFromLeft(buttonW).withTrimmedRight(2);
        area.removeFromLeft(2);
        // The name takes about half of what is left, the load bar the rest.
        nameBounds[synth] = area.removeFromLeft(juce::jmax(40, area.getWidth() * 11 / 20));
        area.removeFromLeft(6);
        loadBounds[synth] = area;
    }
}

void SlotBar::paint(juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    g.fillAll(pal.backgroundPanel);

    for (int i = 0; i < numSlots; ++i)
    {
        if (slotBounds[i].isEmpty())
            continue;   // its synth's row is hidden
        auto b = slotBounds[i].toFloat();
        const bool active = (i == activeIndex);

        // A patch is being dragged over this button and would land in this slot.
        if (i == dropTargetSlot)
        {
            g.setColour(pal.accentActive.withAlpha(0.35f));
            g.fillRoundedRectangle(b.expanded(1.0f), 3.0f);
        }

        g.setColour(active ? pal.buttonActive : pal.inputBackground);
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(i == dropTargetSlot ? pal.accentActive : pal.borderColor);
        g.drawRoundedRectangle(b, 3.0f, 1.0f);

        g.setColour(active ? juce::Colours::white
                           : slotEnabledFlags[i] ? pal.textPrimary : pal.textSecondary.withAlpha(0.5f));
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        g.drawText(slotLetters[SynthSlot::localOf(i)], slotBounds[i], juce::Justification::centred);

        // The slot LED, as a bar under the letter: blinking = focused, fixed =
        // enabled, off = disabled. Ctrl+click the button to toggle enable.
        const bool ledOn = active ? blinkPhase : slotEnabledFlags[i];
        g.setColour(ledOn ? juce::Colour(0xff44cc44) : juce::Colour(0xff2a3a2a));
        g.fillRect(b.getX() + 3.0f, b.getBottom() - 4.0f, b.getWidth() - 6.0f, 2.0f);

        // "LOCAL": this slot's editor patch is not known to match the synth
        // (loaded Local, or edited/loaded while disconnected).
        if (slotLocalFlags[i])
        {
            g.setColour(juce::Colour(0xffd08a2c));
            g.fillEllipse(b.getRight() - 6.0f, b.getY() + 1.0f, 5.0f, 5.0f);
        }
    }

    for (int synth = 0; synth < kMaxSynths; ++synth)
    {
        if (! rowShown[synth])
            continue;
        // The synth's name: dark when it is the synth being edited, as in the original.
        const bool hasSynth = synthName[synth].isNotEmpty();
        const bool beingEdited = (synth == SynthSlot::synthOf(activeIndex));
        auto name = nameBounds[synth].toFloat();
        g.setColour(! hasSynth ? pal.inputBackground
                               : beingEdited ? juce::Colour(0xff3a2f7a) : juce::Colour(0xffa9c8c8));
        g.fillRoundedRectangle(name, 2.0f);
        g.setColour(pal.borderColor);
        g.drawRoundedRectangle(name, 2.0f, 1.0f);
        g.setColour(! hasSynth ? pal.textSecondary : beingEdited ? juce::Colours::white : juce::Colours::black);
        g.setFont(AppTheme::uiFont(12.0f));
        g.drawText(hasSynth ? synthName[synth] : "No synth", nameBounds[synth].reduced(3, 0),
                   juce::Justification::centred, true);

        // The load: a green-to-red track, darkened past the current value.
        const auto lb = loadBounds[synth];
        const float load = loadFraction[synth];
        if (lb.getWidth() > 8)
        {
            auto bar = lb.toFloat().withSizeKeepingCentre(static_cast<float>(lb.getWidth()), 7.0f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2fae3f), bar.getX(), 0.0f,
                                                   juce::Colour(0xffc8352b), bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle(bar, 2.0f);
            if (load >= 0.0f)
            {
                auto rest = bar;
                rest.removeFromLeft(bar.getWidth() * load);
                g.setColour(pal.backgroundPanel.darker(0.6f).withAlpha(0.85f));
                g.fillRect(rest);
                const float x = bar.getX() + bar.getWidth() * load;
                g.setColour(juce::Colours::white);
                g.fillRect(juce::jlimit(bar.getX(), bar.getRight() - 2.0f, x - 1.0f), bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
            }
            else
            {
                g.setColour(pal.backgroundPanel.withAlpha(0.7f));
                g.fillRoundedRectangle(bar, 2.0f);
            }
            g.setColour(pal.borderColor);
            g.drawRoundedRectangle(bar, 2.0f, 1.0f);
            g.setColour(pal.textSecondary);
            g.setFont(AppTheme::uiFont(9.0f));
            g.drawText(load >= 0.0f ? juce::String(juce::roundToInt(load * 100.0f)) + "%" : "--",
                       lb.withTrimmedTop(lb.getCentreY() + 5 - lb.getY()),
                       juce::Justification::centred, false);
        }
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

void SlotBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (int synth = 0; synth < kMaxSynths; ++synth)
        if (nameBounds[synth].contains(e.getPosition()) && synthName[synth].isNotEmpty())
        {
            if (onSynthRenameRequested)
                onSynthRenameRequested(synth);
            return;
        }
}

void SlotBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    // A click on a synth's name makes it the synth being edited.
    for (int synth = 0; synth < kMaxSynths; ++synth)
        if (nameBounds[synth].contains(pos))
        {
            if (synthName[synth].isNotEmpty() && onSynthSelected)
                onSynthSelected(synth);
            return;
        }
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
    slotBar.onRowsChanged = [this] { resized(); };
    slotBar.onSynthRenameRequested = [this](int synth) {
        if (onSynthRenameRequested)
            onSynthRenameRequested(synth);
    };
    slotBar.onSynthSelected = [this](int synth) {
        if (onSynthSelected)
            onSynthSelected(synth);
    };
    slotBar.onSlotViewToggled = [this](int idx) {
        if (onSlotViewToggled)
            onSlotViewToggled(idx);
    };




    // Left column: inspector + slots
    leftColumn.addAndMakeVisible(inspectorPanel);
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

    // Layout left column: inspector | slot bar
    auto leftArea = leftColumn.getLocalBounds();
    slotBar.setBounds(leftArea.removeFromBottom(slotBar.getPreferredHeight()));
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
