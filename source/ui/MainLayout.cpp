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
    if (synth < 0 || synth >= kMaxSynths || rowShown[synth] == shown)
        return;
    rowShown[synth] = shown;
    resized();
    repaint();
    if (onRowsChanged)
        onRowsChanged();
}

void SlotBar::setSlotCount(int synth, int count)
{
    count = juce::jlimit(1, kSlotsPerSynth, count);
    if (synth < 0 || synth >= kMaxSynths || slotCounts[synth] == count)
        return;
    slotCounts[synth] = count;
    resized();
    repaint();
}

int SlotBar::getPreferredHeight() const
{
    int rows = 0;
    for (bool shown : rowShown)
        rows += shown ? 1 : 0;
    return kHeaderH + kRowH * juce::jmax(1, rows) + kBottomPad;
}

void SlotBar::setWaitingEmulators(int count)
{
    if (waitingEmulators == count)
        return;
    waitingEmulators = count;
    repaint(0, 0, getWidth(), kHeaderH);
}

void SlotBar::resized()
{
    auto all = getLocalBounds().withTrimmedTop(kHeaderH).withTrimmedBottom(kBottomPad);
    static constexpr int numW = 16, buttonW = 25;
    for (int synth = 0; synth < kMaxSynths; ++synth)
    {
        if (! rowShown[synth])
        {
            // Nothing on screen, so nothing to click or drop onto either.
            for (int l = 0; l < kSlotsPerSynth; ++l)
                slotBounds[SynthSlot::global(synth, l)] = {};
            numBounds[synth] = nameBounds[synth] = loadBounds[synth] = {};
            continue;
        }
        // A row: the synth's number, its four slot buttons, its name; the DSP load is a thin bar
        // under the buttons and the name.
        auto row = all.removeFromTop(kRowH).reduced(10, 4);
        auto load = row.removeFromBottom(4);
        row.removeFromBottom(3);
        numBounds[synth] = row.removeFromLeft(numW);
        row.removeFromLeft(2);
        const int firstButtonX = row.getX();
        for (int l = 0; l < kSlotsPerSynth; ++l)
            slotBounds[SynthSlot::global(synth, l)] =
                l < slotCounts[synth] ? row.removeFromLeft(buttonW).withTrimmedRight(3) : juce::Rectangle<int>();
        row.removeFromLeft(3);
        nameBounds[synth] = row;
        loadBounds[synth] = load.withTrimmedLeft(firstButtonX - load.getX());
    }
}

void SlotBar::paint(juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    g.fillAll(pal.backgroundPanel);

    // The rule that ends the inspector, then the header: what this is, and how many synths.
    g.setColour(pal.borderColor);
    g.fillRect(0, 0, getWidth(), 1);
    auto header = juce::Rectangle<int>(10, 6, getWidth() - 20, kHeaderH - 8);
    g.setColour(pal.textMuted);
    g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
    g.drawText("SYNTHS", header, juce::Justification::centredLeft, false);
    int connected = 0;
    for (int synth = 0; synth < kMaxSynths; ++synth)
        if (synthName[synth].isNotEmpty())
            ++connected;
    juce::String count = juce::String(connected) + " / " + juce::String(kMaxSynths);
    g.setColour(pal.textSecondary);
    g.setFont(AppTheme::uiFont(11.0f));
    if (waitingEmulators > 0)
    {
        // More emulators than ports: the ones left out are counted where it shows.
        count = "+" + juce::String(waitingEmulators) + " G1-Emu waiting, ports full   " + count;
        g.setColour(pal.accentWarning);
    }
    g.drawText(count, header, juce::Justification::centredRight, true);

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
            g.fillRoundedRectangle(b.expanded(1.0f), 5.0f);
        }

        g.setColour(active ? pal.buttonActive : pal.buttonBackground);
        g.fillRoundedRectangle(b, 5.0f);
        g.setColour(i == dropTargetSlot ? pal.accentActive : active ? pal.buttonActive : pal.borderColor);
        g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 1.0f);

        g.setColour(active ? juce::Colours::white
                           : slotEnabledFlags[i] ? pal.textPrimary : pal.textSecondary.withAlpha(0.5f));
        g.setFont(AppTheme::uiFont(12.0f).withStyle("Bold"));
        g.drawText(slotLetters[SynthSlot::localOf(i)], slotBounds[i], juce::Justification::centred);

        // The slot LED, as a bar under the letter: blinking = focused, fixed =
        // enabled, off = disabled. Ctrl+click the button to toggle enable.
        const bool ledOn = active ? blinkPhase : slotEnabledFlags[i];
        g.setColour(ledOn ? juce::Colour(0xff44cc44) : juce::Colour(0xff2a3a2a));
        g.fillRoundedRectangle(b.getX() + 5.0f, b.getBottom() - 5.0f, b.getWidth() - 10.0f, 2.0f, 1.0f);

        // "LOCAL": this slot's editor patch is not known to match the synth
        // (loaded Local, or edited/loaded while disconnected).
        if (slotLocalFlags[i])
        {
            g.setColour(juce::Colour(0xffd08a2c));
            g.fillEllipse(b.getRight() - 7.0f, b.getY() + 2.0f, 5.0f, 5.0f);
        }
    }

    for (int synth = 0; synth < kMaxSynths; ++synth)
    {
        if (! rowShown[synth])
            continue;
        const bool hasSynth = synthName[synth].isNotEmpty();
        const bool beingEdited = (synth == SynthSlot::synthOf(activeIndex));

        // The synth's number, lit when it is the synth being edited
        g.setColour(beingEdited ? pal.accentActive : pal.textMuted);
        g.setFont(AppTheme::uiFont(12.0f).withStyle("Bold"));
        g.drawText(juce::String(synth + 1), numBounds[synth], juce::Justification::centred, false);

        // Its name: a pill with a connection dot; filled when it is the synth being edited
        auto name = nameBounds[synth].toFloat();
        g.setColour(! hasSynth ? pal.inputBackground
                               : beingEdited ? pal.buttonActive : pal.buttonBackground);
        g.fillRoundedRectangle(name, 5.0f);
        g.setColour(beingEdited && hasSynth ? pal.buttonActive : pal.borderColor);
        g.drawRoundedRectangle(name.reduced(0.5f), 5.0f, 1.0f);

        g.setColour(hasSynth ? juce::Colour(0xff7bc47f) : pal.textMuted.withAlpha(0.6f));
        g.fillEllipse(name.getX() + 8.0f, name.getCentreY() - 3.5f, 7.0f, 7.0f);

        const float load = loadFraction[synth];
        g.setColour(! hasSynth ? pal.textMuted : beingEdited ? juce::Colours::white : pal.textPrimary);
        g.setFont(AppTheme::uiFont(12.0f));
        auto text = name.withTrimmedLeft(21.0f).withTrimmedRight(6.0f);
        g.drawText(hasSynth ? synthName[synth] : "No synth", text.toNearestInt(), juce::Justification::centredLeft, true);

        // The load: a thin track under the row, green to amber to red
        const auto lb = loadBounds[synth];
        if (lb.getWidth() > 8)
        {
            auto bar = lb.toFloat().withSizeKeepingCentre(static_cast<float>(lb.getWidth()), 3.0f);
            g.setColour(pal.inputBackground);
            g.fillRoundedRectangle(bar, 1.5f);
            if (load >= 0.0f)
            {
                g.setColour(load < 0.6f ? juce::Colour(0xff5a9a5a)
                          : load < 0.85f ? juce::Colour(0xffb0a030) : juce::Colour(0xffcb4f4f));
                g.fillRoundedRectangle(bar.withWidth(juce::jmax(2.0f, bar.getWidth() * juce::jlimit(0.0f, 1.0f, load))), 1.5f);
            }
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

    rightBrowserTabs.setTabBarDepth(40);
    rightBrowserTabs.addTab("Synth", AppTheme::palette().backgroundPanel, &patchBrowserPanel, false);
    rightBrowserTabs.addTab("Disk", AppTheme::palette().backgroundPanel, &diskPresetBrowserPanel, false);

    // The switches live in the status bar, as in Bitwig: an "i" for the inspector, a lens for the browser
    statusBar.onInspectorClicked = [this]() {
        if (onPanelToggleRequested) onPanelToggleRequested(true);
        else                        setLeftPanelVisible(!leftPanelVisible);
    };
    statusBar.onBrowserClicked = [this]() {
        if (onPanelToggleRequested) onPanelToggleRequested(false);
        else                        setRightPanelVisible(!rightPanelVisible);
    };

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
    statusBar.setPanelStates(leftPanelVisible, rightPanelVisible);

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
    statusBar.setPanelStates(leftPanelVisible, rightPanelVisible);

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
