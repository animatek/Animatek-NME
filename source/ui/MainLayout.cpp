#include "MainLayout.h"
#include "AppTheme.h"

// ============================================================
// SlotBar implementation
// ============================================================

constexpr const char* SlotBar::slotLetters[];

SlotBar::SlotBar()
{
    setInterceptsMouseClicks(true, false);
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

        // Icon (small fixed-size synth)
        auto iconArea = bounds.removeFromLeft(20).reduced(2);
        drawSlotIcon(g, iconArea, active);

        // Text: "A : PatchName"
        auto textArea = bounds.reduced(4, 0);
        juce::String label = juce::String(slotLetters[i]) + " : ";
        if (slotNames[i].isNotEmpty())
            label += slotNames[i];

        g.setColour(active ? juce::Colours::white : juce::Colour(0xff999999));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(label, textArea, juce::Justification::centredLeft, true);

        // Bottom separator
        g.setColour(AppTheme::palette().buttonActive);
        g.drawHorizontalLine(slotBounds[i].getBottom() - 1,
                             static_cast<float>(slotBounds[i].getX()),
                             static_cast<float>(slotBounds[i].getRight()));
    }
}

void SlotBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    for (int i = 0; i < numSlots; ++i)
    {
        if (slotBounds[i].contains(pos))
        {
            if (i != activeIndex)
            {
                activeIndex = i;
                repaint();
                if (onSlotChanged)
                    onSlotChanged(i);
            }
            break;
        }
    }
}

// ============================================================
// MainLayout implementation
// ============================================================

MainLayout::MainLayout(ModuleDescriptions& /*moduleDescs*/)
{
    slotBar.onSlotChanged = [this](int idx) {
        if (onSlotChanged)
            onSlotChanged(idx);
    };

    // Toolbar buttons
    midiButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().inputBackground);
    midiButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffaaaacc));
    midiButton.onClick = [this]() { if (onMidiSettingsClicked) onMidiSettingsClicked(); };

    libraryButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().inputBackground);
    libraryButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffaaaacc));
    libraryButton.onClick = [this]() { if (onLibraryFolderClicked) onLibraryFolderClicked(); };

    storeButton.setColour(juce::TextButton::buttonColourId, AppTheme::palette().inputBackground);
    storeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffaaaacc));
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

    addAndMakeVisible(leftColumn);
    addAndMakeVisible(headerBar);
    addAndMakeVisible(canvasComponent);
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

    juce::Component* comps[] = {
        &leftColumn, &resizerBar1, &canvasComponent, &resizerBar2, &rightBrowserTabs
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

void MainLayout::showDiskPresetBrowser()
{
    rightBrowserTabs.setCurrentTabIndex(1);
}
