#include "PresetBrowserWindow.h"
#include "BinaryData.h"

static const juce::Colour kBg     { 0xff323232 };
static const juce::Colour kPanel  { 0xff323232 };
static const juce::Colour kSep    { 0xff444A53 };
static const juce::Colour kText   { 0xffcccccc };
static const juce::Colour kDim    { 0xff888899 };
static const juce::Colour kPatchTag   { 0xffd0d4d8 };
static const juce::Colour kSnippetTag { 0xffc9cdd1 };

static void styleButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff25282E));
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff444A53));
    b.setColour(juce::TextButton::textColourOffId, kText);
}

static void styleFilterButton(juce::TextButton& b)
{
    styleButton(b);
    b.setClickingTogglesState(true);
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff444A53));
}

DiskPresetBrowserPanel::RefreshIconButton::RefreshIconButton()
    : juce::Button("Refresh")
{
    setTooltip("Refresh disk presets");
    icon = juce::Drawable::createFromImageData(BinaryData::refreshicon_svg,
                                               BinaryData::refreshicon_svgSize);
    if (icon)
        icon->replaceColour(juce::Colours::black, juce::Colour(0xffc9cdd1));
}

void DiskPresetBrowserPanel::RefreshIconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced(3.0f);
    auto bg = down ? juce::Colour(0xff444A53)
                  : highlighted ? juce::Colour(0xff343941)
                                : juce::Colour(0xff25282E);

    g.setColour(bg);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colour(0xff555B64));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    if (icon)
    {
        icon->drawWithin(g, area.reduced(7.0f), juce::RectanglePlacement::centred, down ? 0.75f : 1.0f);
        return;
    }

    // Fallback if BinaryData fails to provide the SVG.
    auto iconArea = area.reduced(7.0f);
    juce::Path p;
    p.addCentredArc(iconArea.getCentreX(), iconArea.getCentreY(),
                    iconArea.getWidth() * 0.42f, iconArea.getHeight() * 0.42f,
                    0.0f, juce::degreesToRadians(35.0f), juce::degreesToRadians(325.0f), true);
    g.setColour(juce::Colour(0xffc9cdd1));
    g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

DiskPresetBrowserPanel::DiskPresetBrowserPanel()
{
    setOpaque(true);

    searchLabel.setText("Search", juce::dontSendNotification);
    searchLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    searchLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(searchLabel);

    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff25282E));
    searchBox.setColour(juce::TextEditor::textColourId, kText);
    searchBox.setColour(juce::TextEditor::outlineColourId, kSep);
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffffcc44));
    searchBox.setTextToShowWhenEmpty("patch or snippet name", kDim);
    searchBox.onTextChange = [this]() { rebuildVisibleEntries(); };
    addAndMakeVisible(searchBox);

    for (auto* b : { &allButton, &patchesButton, &snippetsButton })
    {
        styleFilterButton(*b);
        b->setRadioGroupId(11);
        addAndMakeVisible(*b);
    }
    allButton.setToggleState(true, juce::dontSendNotification);
    allButton.onClick = [this]() { typeFilter = TypeFilter::All; rebuildVisibleEntries(); };
    patchesButton.onClick = [this]() { typeFilter = TypeFilter::Patches; rebuildVisibleEntries(); };
    snippetsButton.onClick = [this]() { typeFilter = TypeFilter::Snippets; rebuildVisibleEntries(); };

    refreshButton.onClick = [this]() { refresh(); };
    addAndMakeVisible(refreshButton);

    statusLabel.setColour(juce::Label::textColourId, kDim);
    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(statusLabel);

    listBox.setColour(juce::ListBox::backgroundColourId, kPanel);
    listBox.setColour(juce::ListBox::outlineColourId, kSep);
    listBox.setRowHeight(24);
    addAndMakeVisible(listBox);
}

void DiskPresetBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(kPanel);
}

void DiskPresetBrowserPanel::setLibraryRoot(const juce::File& root)
{
    libraryRoot = root;
    refresh();
}

void DiskPresetBrowserPanel::refresh()
{
    allEntries.clear();

    if (libraryRoot == juce::File() || !libraryRoot.isDirectory())
    {
        statusLabel.setText("Choose a preset library folder first.", juce::dontSendNotification);
        rebuildVisibleEntries();
        return;
    }

    scanFolder(libraryRoot.getChildFile("Patches"), Entry::Type::Patch);
    scanFolder(libraryRoot.getChildFile("Snippets"), Entry::Type::Snippet);

    std::sort(allEntries.begin(), allEntries.end(), [](const Entry& a, const Entry& b) {
        if (a.type != b.type)
            return a.type == Entry::Type::Patch;
        return a.displayName.compareIgnoreCase(b.displayName) < 0;
    });

    rebuildVisibleEntries();
}

void DiskPresetBrowserPanel::scanFolder(const juce::File& folder, Entry::Type type)
{
    if (!folder.isDirectory())
        return;

    for (juce::RangedDirectoryIterator it(folder, true, "*.pch", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
    {
        auto file = it->getFile();
        Entry entry;
        entry.type = type;
        entry.file = file;
        entry.displayName = file.getFileNameWithoutExtension();
        entry.relativePath = file.getRelativePathFrom(folder.getParentDirectory());
        allEntries.push_back(std::move(entry));
    }
}

void DiskPresetBrowserPanel::rebuildVisibleEntries()
{
    visibleEntryIndices.clear();
    auto search = searchBox.getText().trim().toLowerCase();

    for (int i = 0; i < static_cast<int>(allEntries.size()); ++i)
    {
        const auto& e = allEntries[static_cast<size_t>(i)];
        if (!entryPassesTypeFilter(e))
            continue;

        auto haystack = (e.displayName + " " + e.relativePath).toLowerCase();
        if (search.isEmpty() || haystack.contains(search))
            visibleEntryIndices.push_back(i);
    }

    auto status = juce::String(visibleEntryIndices.size()) + " of "
        + juce::String(allEntries.size()) + " files";
    if (libraryRoot != juce::File())
        status += " - " + libraryRoot.getFullPathName();
    statusLabel.setText(status, juce::dontSendNotification);

    listBox.updateContent();
    listBox.repaint();
}

void DiskPresetBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto searchRow = area.removeFromTop(28);
    searchLabel.setBounds(searchRow.removeFromLeft(52));
    refreshButton.setBounds(searchRow.removeFromRight(32).reduced(1));
    searchBox.setBounds(searchRow.reduced(2));

    auto filterRow = area.removeFromTop(26);
    allButton.setBounds(filterRow.removeFromLeft(48).reduced(2));
    patchesButton.setBounds(filterRow.removeFromLeft(78).reduced(2));
    snippetsButton.setBounds(filterRow.removeFromLeft(82).reduced(2));

    statusLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);
    listBox.setBounds(area);
}

int DiskPresetBrowserPanel::getNumRows()
{
    return static_cast<int>(visibleEntryIndices.size());
}

void DiskPresetBrowserPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= getNumRows())
        return;

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];

    g.fillAll(selected ? juce::Colour(0xff343941) : kPanel);
    g.setColour(selected ? juce::Colour(0xffd8dcdf) : kSep);
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

    auto tagArea = juce::Rectangle<int>(6, 4, 48, height - 8);
    auto tagColour = entry.type == Entry::Type::Patch ? kPatchTag : kSnippetTag;
    g.setColour(tagColour.withAlpha(0.22f));
    g.fillRoundedRectangle(tagArea.toFloat(), 3.0f);
    g.setColour(tagColour);
    g.drawRoundedRectangle(tagArea.toFloat(), 3.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
    g.drawText(getTypeLabel(entry.type), tagArea, juce::Justification::centred, true);

    g.setColour(selected ? juce::Colours::white : kText);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(entry.displayName, 62, 0, width - 68, height, juce::Justification::centredLeft, true);
}

void DiskPresetBrowserPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= getNumRows())
        return;

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];
    if (entry.type == Entry::Type::Patch)
    {
        if (onPatchChosen)
            onPatchChosen(entry.file);
    }
    else if (onSnippetChosen)
    {
        onSnippetChosen(entry.file);
    }
}

juce::var DiskPresetBrowserPanel::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    auto row = selectedRows[0];
    if (row < 0 || row >= getNumRows())
        return {};

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];
    if (entry.type != Entry::Type::Snippet)
        return {};

    auto* obj = new juce::DynamicObject();
    obj->setProperty("type", "snippetFile");
    obj->setProperty("path", entry.file.getFullPathName());
    obj->setProperty("name", entry.displayName);
    return juce::var(obj);
}

juce::String DiskPresetBrowserPanel::getTypeLabel(Entry::Type type) const
{
    return type == Entry::Type::Patch ? "PATCH" : "SNIP";
}

bool DiskPresetBrowserPanel::entryPassesTypeFilter(const Entry& entry) const
{
    if (typeFilter == TypeFilter::Patches)
        return entry.type == Entry::Type::Patch;
    if (typeFilter == TypeFilter::Snippets)
        return entry.type == Entry::Type::Snippet;
    return true;
}

PresetBrowserWindow::PresetBrowserWindow()
    : juce::DocumentWindow("Preset Browser", kBg, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setResizable(true, true);
    setResizeLimits(420, 320, 1200, 900);
    setContentNonOwned(&browserPanel, false);
    setSize(680, 520);

    browserPanel.onPatchChosen = [this](const juce::File& f) { if (onPatchChosen) onPatchChosen(f); };
    browserPanel.onSnippetChosen = [this](const juce::File& f) { if (onSnippetChosen) onSnippetChosen(f); };
}

void PresetBrowserWindow::setLibraryRoot(const juce::File& root)
{
    browserPanel.setLibraryRoot(root);
}

void PresetBrowserWindow::refresh()
{
    browserPanel.refresh();
}

void PresetBrowserWindow::closeButtonPressed()
{
    setVisible(false);
}
