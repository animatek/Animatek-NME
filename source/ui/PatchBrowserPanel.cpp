#include "PatchBrowserPanel.h"
#include <iostream>

PatchBrowserPanel::PatchBrowserPanel()
{
    // Status label for loading state
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    addAndMakeVisible(statusLabel);

    // Search label
    searchLabel.setText("Search:", juce::dontSendNotification);
    searchLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    searchLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(searchLabel);

    // Search box
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff25282E));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffcccccc));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff555B64));
    searchBox.setFont(juce::Font(juce::FontOptions(12.0f)));
    searchBox.onTextChange = [this]() { onSearchTextChanged(); };
    addAndMakeVisible(searchBox);

    // Hide empty button
    hideEmptyButton.setButtonText("Hide Empty");
    hideEmptyButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
    hideEmptyButton.onClick = [this]() { onHideEmptyToggled(); };
    addAndMakeVisible(hideEmptyButton);

    // Refresh button
    refreshButton.setButtonText("Refresh");
    refreshButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555B64));
    refreshButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcccccc));
    refreshButton.onClick = [this]() { onRefreshClicked(); };
    addAndMakeVisible(refreshButton);

    // TreeView for patch browser
    treeView = std::make_unique<juce::TreeView>();
    treeView->setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff323232));
    treeView->setColour(juce::TreeView::linesColourId, juce::Colour(0xff555B64));
    treeView->setDefaultOpenness(false);  // Banks start collapsed
    treeView->setIndentSize(20);
    addAndMakeVisible(*treeView);

    setLoadingState(false);
}

void PatchBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff323232));
}

void PatchBrowserPanel::resized()
{
    auto bounds = getLocalBounds();
    std::cout << "[INSPECTOR] resized: bounds=" << bounds.getWidth() << "x" << bounds.getHeight()
              << " isLoading=" << isLoading << " hasRoot=" << (rootItem != nullptr) << std::endl;

    if (isLoading || !rootItem)
    {
        statusLabel.setBounds(bounds);
        statusLabel.setVisible(true);
        treeView->setVisible(false);
        searchLabel.setVisible(false);
        searchBox.setVisible(false);
        hideEmptyButton.setVisible(false);
        refreshButton.setVisible(false);
    }
    else
    {
        statusLabel.setVisible(false);

        // Header area with search and filters
        auto headerArea = bounds.removeFromTop(70);
        headerArea.reduce(8, 8);

        // First row: Search
        auto searchRow = headerArea.removeFromTop(24);
        searchLabel.setBounds(searchRow.removeFromLeft(50));
        searchBox.setBounds(searchRow.withTrimmedLeft(4));

        headerArea.removeFromTop(4);  // Spacing

        // Second row: Hide empty + Refresh
        auto filterRow = headerArea.removeFromTop(24);
        hideEmptyButton.setBounds(filterRow.removeFromLeft(100));
        filterRow.removeFromLeft(8);  // Spacing
        refreshButton.setBounds(filterRow.removeFromLeft(80));

        // TreeView gets remaining space
        treeView->setBounds(bounds.withTrimmedTop(4));
        treeView->setVisible(true);
        searchLabel.setVisible(true);
        searchBox.setVisible(true);
        hideEmptyButton.setVisible(true);
        refreshButton.setVisible(true);
    }
}

void PatchBrowserPanel::setPatchList(const std::vector<std::string>& names)
{
    std::cout << "[INSPECTOR] setPatchList called with " << names.size() << " entries" << std::endl;
    cachedPatchList = names;  // Cache the list
    applyFilters();  // Build tree with current filters
    setLoadingState(false);
    resized();  // Force re-layout now that rootItem exists
    std::cout << "[INSPECTOR] Tree rebuilt and visible" << std::endl;
}

void PatchBrowserPanel::applyFilters()
{
    if (cachedPatchList.empty())
        return;

    rebuildTree(cachedPatchList);
}

void PatchBrowserPanel::onSearchTextChanged()
{
    currentSearchText = searchBox.getText().toLowerCase();
    applyFilters();
}

void PatchBrowserPanel::onHideEmptyToggled()
{
    hideEmptySlots = hideEmptyButton.getToggleState();
    applyFilters();
}

void PatchBrowserPanel::onRefreshClicked()
{
    if (onRefreshRequested)
        onRefreshRequested();
}

void PatchBrowserPanel::setLoadingState(bool loading)
{
    std::cout << "[INSPECTOR] setLoadingState: loading=" << loading << std::endl;
    isLoading = loading;

    if (loading)
    {
        statusLabel.setText("Loading patches from synth...", juce::dontSendNotification);
        statusLabel.setVisible(true);
        treeView->setVisible(false);
    }
    else
    {
        statusLabel.setVisible(false);
        if (rootItem)
            treeView->setVisible(true);
    }

    resized();
    repaint();
}

void PatchBrowserPanel::setLoadedPatch(int section, int position)
{
    if (loadedSection == section && loadedPosition == position)
        return;
    loadedSection = section;
    loadedPosition = position;
    treeView->repaint();
}

void PatchBrowserPanel::setSelectedPatch(int section, int position)
{
    if (selectedSection == section && selectedPosition == position)
        return;

    selectedSection = section;
    selectedPosition = position;
    treeView->repaint();
}

void PatchBrowserPanel::rebuildTree(const std::vector<std::string>& names)
{
    std::cout << "[INSPECTOR] rebuildTree starting, names.size()=" << names.size() << std::endl;

    // IMPORTANT: Clear old tree first to avoid double-free
    treeView->setRootItem(nullptr);
    rootItem.reset();

    // Create new root item
    rootItem = std::make_unique<PatchTreeItem>("Synth Patches");

    // Expected: 891 entries (9 banks × 99 positions)
    // names[section * 99 + position]

    bool hasSearchFilter = currentSearchText.isNotEmpty();

    for (int section = 0; section < 9; ++section)
    {
        // Create bank node (e.g., "Bank 1 (101-199)")
        int displaySection = section + 1;
        juce::String bankName = "Bank " + juce::String(displaySection)
                              + " (" + juce::String(displaySection * 100 + 1)
                              + "-" + juce::String(displaySection * 100 + 99) + ")";

        auto* bankItem = new PatchTreeItem(bankName, section, -1);
        int patchesAdded = 0;

        // Add patches within this bank
        for (int position = 0; position < 99; ++position)
        {
            int index = section * 99 + position;
            if (index >= static_cast<int>(names.size()))
                break;

            juce::String patchName = names[static_cast<size_t>(index)];
            bool isEmpty = patchName.isEmpty();

            // Apply filters
            if (hideEmptySlots && isEmpty)
                continue;

            if (hasSearchFilter)
            {
                juce::String searchTarget = patchName.toLowerCase();
                if (!searchTarget.contains(currentSearchText))
                    continue;
            }

            // Display location (e.g., "101: PatchName" or "101: --")
            int displayLocation = displaySection * 100 + position + 1;
            juce::String displayName = isEmpty ? "--" : patchName;
            juce::String itemName = juce::String(displayLocation).paddedLeft('0', 3)
                                  + ": " + displayName;

            auto* patchItem = new PatchTreeItem(itemName, section, position, this);
            bankItem->addSubItem(patchItem);
            patchesAdded++;
        }

        // Only add bank if it has visible patches
        if (patchesAdded > 0)
        {
            rootItem->addSubItem(bankItem);
            std::cout << "[INSPECTOR]   Added bank " << displaySection << " with " << patchesAdded << " patches" << std::endl;
        }
        else
        {
            // Don't add to tree - just let it be deleted automatically
            // JUCE will clean it up since it wasn't added anywhere
            delete bankItem;
        }
    }

    std::cout << "[INSPECTOR] Setting root item in tree view, rootItem=" << (void*)rootItem.get() << std::endl;
    std::cout << "[INSPECTOR] Root item has " << rootItem->getNumSubItems() << " sub-items" << std::endl;

    treeView->setRootItem(rootItem.get());
    treeView->setRootItemVisible(false);  // Don't show "Synth Patches" root

    std::cout << "[INSPECTOR] TreeView visible=" << treeView->isVisible()
              << " bounds=" << treeView->getBounds().toString().toStdString() << std::endl;
    std::cout << "[INSPECTOR] rebuildTree complete" << std::endl;
}

// --- PatchTreeItem implementation ---

PatchBrowserPanel::PatchTreeItem::PatchTreeItem(const juce::String& name, int sec, int pos, PatchBrowserPanel* parent)
    : itemName(name), section(sec), position(pos), panel(parent)
{
}

bool PatchBrowserPanel::PatchTreeItem::mightContainSubItems()
{
    // Bank nodes (section >= 0, position == -1) can contain patches
    return (section >= 0 && position == -1);
}

void PatchBrowserPanel::PatchTreeItem::paintItem(juce::Graphics& g, int width, int height)
{
    bool isLoaded = (panel != nullptr
                     && section >= 0 && position >= 0
                     && section == panel->loadedSection
                     && position == panel->loadedPosition);
    bool isSelected = (panel != nullptr
                       && section >= 0 && position >= 0
                       && section == panel->selectedSection
                       && position == panel->selectedPosition);

    if (isSelected)
    {
        g.setColour(juce::Colour(isLoaded ? 0x44ffaa00 : 0x33343941));
        g.fillRect(0, 0, width, height);

        g.setColour(juce::Colour(isLoaded ? 0xffffcc44 : 0xffd0d4d8));
        g.drawRect(0, 0, width, height);
    }
    else if (isLoaded)
    {
        // Subtle highlight background for the loaded patch
        g.setColour(juce::Colour(0x33ffaa00));
        g.fillRect(0, 0, width, height);
    }

    juce::Colour textColor = isLoaded ? juce::Colour(0xffffcc44)
                            : isSelected ? juce::Colour(0xffd0d4d8)
                                         : juce::Colour(0xffcccccc);

    if (section >= 0 && position == -1)
    {
        // Bank node — bold
        g.setColour(textColor);
        g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
        g.drawText(itemName, 4, 0, width - 4, height, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour(textColor);
        g.setFont(juce::Font(juce::FontOptions(isLoaded ? 12.5f : 12.0f)));

        if (isLoaded)
        {
            // Draw play indicator
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), 2, 0, 12, height, juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions(12.5f)));
            g.drawText(itemName, 16, 0, width - 16, height, juce::Justification::centredLeft, true);
        }
        else
        {
            g.drawText(itemName, 4, 0, width - 4, height, juce::Justification::centredLeft, true);
        }
    }
}

void PatchBrowserPanel::PatchTreeItem::itemClicked(const juce::MouseEvent& e)
{
    // Right-click: show context menu for patches (not banks)
    if (e.mods.isPopupMenu() && section >= 0 && position >= 0)
    {
        if (panel)
            panel->setSelectedPatch(section, position);
        showContextMenu();
        return;
    }

    // Single-click: select patches so users get immediate feedback before double-click loading
    if (section >= 0 && position >= 0 && panel)
    {
        panel->setSelectedPatch(section, position);
        return;
    }

    // Single-click: toggle open/close for banks
    if (section >= 0 && position == -1)
    {
        setOpen(!isOpen());
    }
}

void PatchBrowserPanel::PatchTreeItem::itemDoubleClicked(const juce::MouseEvent&)
{
    // Double-click on a patch (not a bank): load it
    if (section >= 0 && position >= 0 && panel && panel->onPatchDoubleClicked)
    {
        std::cout << "[INSPECTOR] Double-clicked patch: section=" << section << " position=" << position << std::endl;
        panel->onPatchDoubleClicked(section, position);
    }
}

void PatchBrowserPanel::PatchTreeItem::showContextMenu()
{
    if (!panel)
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Delete (clear)");
    menu.addSeparator();
    menu.addItem(2, "Copy to...");
    menu.addItem(3, "Move to...");

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result) {
            if (!panel)
                return;

            switch (result)
            {
                case 1:  // Delete
                    if (panel->onPatchDelete)
                        panel->onPatchDelete(section, position);
                    break;

                case 2:  // Copy
                    if (panel->onPatchCopy)
                        panel->onPatchCopy(section, position);
                    break;

                case 3:  // Move
                    if (panel->onPatchMove)
                        panel->onPatchMove(section, position);
                    break;

                default:
                    break;
            }
        });
}
