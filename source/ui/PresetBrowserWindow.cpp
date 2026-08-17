#include "PresetBrowserWindow.h"
#include "../model/PchFileIO.h"
#include "AppTheme.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kPanel  (AppTheme::palette().backgroundPanel)
#define kSep    (AppTheme::palette().buttonActive)
#define kText   (AppTheme::palette().textSecondary)
#define kDim    (AppTheme::palette().textMuted)
// Distinct per-theme accent for each entry kind, from three different color
// families so they never blur together: Patch = cool (blue), Snippet = green,
// Bank = warm (orange). All three follow the active theme.
#define kPatchTag   (AppTheme::palette().accentInfo)
#define kSnippetTag (AppTheme::palette().accentSuccess)
#define kBankTag    (AppTheme::palette().accentWarning)

static juce::Colour legacyPatchTagColour()
{
    return juce::Colour(0xffb07cff);
}

static juce::Colour iconInkFor(juce::Colour background)
{
    return background.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white;
}

DiskPresetBrowserPanel::RefreshIconButton::RefreshIconButton()
    : juce::Button("Refresh")
{
    setTooltip("Refresh disk presets");
}

void DiskPresetBrowserPanel::RefreshIconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced(3.0f);
    auto bg = down ? AppTheme::palette().buttonActive
                  : highlighted ? AppTheme::palette().backgroundElevated
                                : AppTheme::palette().inputBackground;

    g.setColour(bg);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(AppTheme::palette().borderColor);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    auto iconArea = area.reduced(7.0f);
    juce::Path p;
    p.addCentredArc(iconArea.getCentreX(), iconArea.getCentreY(),
                    iconArea.getWidth() * 0.42f, iconArea.getHeight() * 0.42f,
                    0.0f, juce::degreesToRadians(35.0f), juce::degreesToRadians(325.0f), true);
    g.setColour(AppTheme::palette().textSecondary.withAlpha(down ? 0.75f : 1.0f));
    g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

DiskPresetBrowserPanel::FilterIconButton::FilterIconButton(const juce::String& name, Icon iconType)
    : juce::Button(name), icon(iconType)
{
    setClickingTogglesState(true);
    setTooltip(name);
}

void DiskPresetBrowserPanel::FilterIconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const bool selected = getToggleState();
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    auto background = selected || down ? AppTheme::palette().buttonActive
                                      : highlighted ? AppTheme::palette().buttonHover
                                                    : AppTheme::palette().inputBackground;
    g.setColour(background);
    g.fillRoundedRectangle(area, 3.0f);
    g.setColour(selected ? AppTheme::palette().buttonActive : AppTheme::palette().borderColor);
    g.drawRoundedRectangle(area, 3.0f, 1.0f);

    g.setColour(selected ? iconInkFor(background) : AppTheme::palette().textSecondary);
    auto a = area.reduced(7.0f);
    const float stroke = 1.5f;

    if (icon == Icon::All)
    {
        const float cell = juce::jmin(a.getWidth(), a.getHeight()) * 0.34f;
        const float gap = 2.0f;
        const float x = a.getCentreX() - cell - gap * 0.5f;
        const float y = a.getCentreY() - cell - gap * 0.5f;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col)
                g.drawRoundedRectangle(x + col * (cell + gap), y + row * (cell + gap), cell, cell, 1.0f, stroke);
    }
    else if (icon == Icon::Patch)
    {
        auto doc = a.withSizeKeepingCentre(12.0f, 15.0f);
        g.drawRoundedRectangle(doc, 1.0f, stroke);
        g.drawLine(doc.getX() + 3.0f, doc.getY() + 6.0f, doc.getRight() - 3.0f, doc.getY() + 6.0f, stroke);
        g.drawLine(doc.getX() + 3.0f, doc.getY() + 10.0f, doc.getRight() - 3.0f, doc.getY() + 10.0f, stroke);
    }
    else if (icon == Icon::Snippet)
    {
        const float cx = a.getCentreX(), cy = a.getCentreY();
        g.drawLine(cx - 7.0f, cy - 6.0f, cx - 7.0f, cy + 6.0f, stroke);
        g.drawLine(cx - 7.0f, cy - 6.0f, cx - 3.0f, cy - 6.0f, stroke);
        g.drawLine(cx - 7.0f, cy + 6.0f, cx - 3.0f, cy + 6.0f, stroke);
        g.drawLine(cx + 7.0f, cy - 6.0f, cx + 7.0f, cy + 6.0f, stroke);
        g.drawLine(cx + 3.0f, cy - 6.0f, cx + 7.0f, cy - 6.0f, stroke);
        g.drawLine(cx + 3.0f, cy + 6.0f, cx + 7.0f, cy + 6.0f, stroke);
        g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }
    else if (icon == Icon::Bank)
    {
        for (int i = 0; i < 3; ++i)
            g.drawRoundedRectangle(a.getCentreX() - 7.0f + i * 2.0f,
                                   a.getCentreY() - 7.0f + i * 2.0f,
                                   11.0f, 11.0f, 1.0f, stroke);
    }
    else
    {
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        g.drawText("2", a.toNearestInt(), juce::Justification::centred, false);
        g.drawLine(a.getX() + 2.0f, a.getBottom() - 2.0f, a.getRight() - 2.0f, a.getY() + 2.0f, stroke);
    }
}

DiskPresetBrowserPanel::DiskPresetBrowserPanel()
{
    setOpaque(true);

    searchLabel.setText("Search", juce::dontSendNotification);
    searchLabel.setFont(juce::Font(AppTheme::uiFont(12.0f)));
    addAndMakeVisible(searchLabel);

    searchBox.onTextChange = [this]() { rebuildVisibleEntries(); };
    addAndMakeVisible(searchBox);

    for (auto* b : { &allButton, &patchesButton, &snippetsButton, &banksButton })
    {
        b->setRadioGroupId(11);
        addAndMakeVisible(*b);
    }
    allButton.setToggleState(true, juce::dontSendNotification);
    allButton.onClick = [this]() { typeFilter = TypeFilter::All; rebuildVisibleEntries(); };
    patchesButton.onClick = [this]() { typeFilter = TypeFilter::Patches; rebuildVisibleEntries(); };
    snippetsButton.onClick = [this]() { typeFilter = TypeFilter::Snippets; rebuildVisibleEntries(); };
    banksButton.onClick = [this]() { typeFilter = TypeFilter::Banks; rebuildVisibleEntries(); };

    hidePch2Button.setTooltip("Hide legacy 2.10 (.pch2) patches");
    hidePch2Button.onClick = [this]() {
        hidePch2 = hidePch2Button.getToggleState();
        rebuildVisibleEntries();
    };
    addAndMakeVisible(hidePch2Button);

    refreshButton.onClick = [this]() { refresh(); };
    addAndMakeVisible(refreshButton);

    statusLabel.setFont(juce::Font(AppTheme::uiFont(12.0f)));
    addAndMakeVisible(statusLabel);

    listBox.setRowHeight(24);
    addAndMakeVisible(listBox);

    applyTheme();
}

void DiskPresetBrowserPanel::applyTheme()
{
    searchLabel.setColour(juce::Label::textColourId, kText);
    searchBox.setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().inputBackground);
    searchBox.setColour(juce::TextEditor::textColourId, kText);
    searchBox.setColour(juce::TextEditor::outlineColourId, kSep);
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, AppTheme::palette().accentActive);
    searchBox.setTextToShowWhenEmpty("patch or snippet name", kDim);
    statusLabel.setColour(juce::Label::textColourId, kDim);
    listBox.setColour(juce::ListBox::backgroundColourId, kPanel);
    listBox.setColour(juce::ListBox::outlineColourId, kSep);

    for (auto* b : { &allButton, &patchesButton, &snippetsButton, &banksButton, &hidePch2Button })
        b->repaint();
    refreshButton.repaint();
    listBox.repaint();
    repaint();
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
    scanFolder(libraryRoot.getChildFile("Banks"), Entry::Type::Bank);

    std::sort(allEntries.begin(), allEntries.end(), [](const Entry& a, const Entry& b) {
        if (a.type != b.type)
            return static_cast<int>(a.type) < static_cast<int>(b.type);
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
        // Bank backups show their bank subfolder ("Bank3/05 - Name") so the
        // same patch name in different banks stays distinguishable.
        entry.displayName = type == Entry::Type::Bank
            ? file.getRelativePathFrom(folder).dropLastCharacters(4)
            : file.getFileNameWithoutExtension();
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
        auto& e = allEntries[static_cast<size_t>(i)];
        if (!entryPassesTypeFilter(e))
            continue;

        if (hidePch2 && isLegacyPatch210(e))
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
    const int filterWidth = filterRow.getWidth() / 5;
    for (auto* button : { &allButton, &patchesButton, &snippetsButton, &banksButton, &hidePch2Button })
        button->setBounds(filterRow.removeFromLeft(filterWidth).reduced(1));

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

    auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];

    g.fillAll(selected ? AppTheme::palette().backgroundElevated : kPanel);
    g.setColour(selected ? juce::Colour(0xffd8dcdf) : kSep);
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

    auto tagArea = juce::Rectangle<int>(6, 4, 48, height - 8);
    auto tagColour = isLegacyPatch210(entry)            ? legacyPatchTagColour()
                   : entry.type == Entry::Type::Patch   ? kPatchTag
                   : entry.type == Entry::Type::Snippet ? kSnippetTag
                                                        : kBankTag;
    g.setColour(tagColour.withAlpha(0.22f));
    g.fillRoundedRectangle(tagArea.toFloat(), 3.0f);
    g.setColour(tagColour);
    g.drawRoundedRectangle(tagArea.toFloat(), 3.0f, 1.0f);
    g.setFont(juce::Font(AppTheme::uiFont(10.0f)).boldened());
    g.drawText(getTypeLabel(entry), tagArea, juce::Justification::centred, true);

    g.setColour(selected ? juce::Colours::white : kText);
    g.setFont(juce::Font(AppTheme::uiFont(12.0f)));
    g.drawText(entry.displayName, 62, 0, width - 68, height, juce::Justification::centredLeft, true);
}

void DiskPresetBrowserPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= getNumRows())
        return;

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];
    if (entry.type == Entry::Type::Snippet)
    {
        if (onSnippetChosen)
            onSnippetChosen(entry.file);
    }
    else if (onPatchChosen)  // bank backups load exactly like patches
    {
        onPatchChosen(entry.file);
    }
}

juce::var DiskPresetBrowserPanel::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    auto row = selectedRows[0];
    if (row < 0 || row >= getNumRows())
        return {};

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];

    // Snippets go on the canvas, where they merge into the patch at the point
    // they land. Everything else is a whole patch and goes on a slot, where it
    // replaces what is there (issue #50).
    //
    // That includes the BANK-tagged entries, which are not bank files: a bank
    // backup is a folder of ordinary .pch files, one per position, and the tag
    // only says which bank folder this one came out of. Double-clicking one has
    // always loaded it exactly like a patch, and dragging one does the same.
    auto* obj = new juce::DynamicObject();
    obj->setProperty("type", entry.type == Entry::Type::Snippet ? "snippetFile"
                                                                : "patchFile");
    obj->setProperty("path", entry.file.getFullPathName());
    obj->setProperty("name", entry.displayName);
    return juce::var(obj);
}

juce::String DiskPresetBrowserPanel::getTypeLabel(Entry::Type type) const
{
    return type == Entry::Type::Patch   ? "PATCH"
         : type == Entry::Type::Snippet ? "SNIP"
                                        : "BANK";
}

juce::String DiskPresetBrowserPanel::getTypeLabel(Entry& entry) const
{
    return isLegacyPatch210(entry) ? "PCH2" : getTypeLabel(entry.type);
}

bool DiskPresetBrowserPanel::isLegacyPatch210(Entry& entry)
{
    if (entry.legacyPatch210 < 0)
        entry.legacyPatch210 = (entry.type == Entry::Type::Patch
                                && PchFileIO::isLegacyPatch210(entry.file)) ? 1 : 0;

    return entry.legacyPatch210 == 1;
}

bool DiskPresetBrowserPanel::entryPassesTypeFilter(const Entry& entry) const
{
    if (typeFilter == TypeFilter::Patches)
        return entry.type == Entry::Type::Patch;
    if (typeFilter == TypeFilter::Snippets)
        return entry.type == Entry::Type::Snippet;
    if (typeFilter == TypeFilter::Banks)
        return entry.type == Entry::Type::Bank;
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

void PresetBrowserWindow::applyTheme()
{
    setBackgroundColour(kBg);
    browserPanel.applyTheme();
    repaint();
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
