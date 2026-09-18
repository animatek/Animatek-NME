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
#define kSetTag     (AppTheme::palette().accentActive)

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

DiskPresetBrowserPanel::StepIconButton::StepIconButton(const juce::String& name, int direction)
    : juce::Button(name), step(direction)
{
    setTooltip(name);
}

void DiskPresetBrowserPanel::StepIconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    auto bg = down ? AppTheme::palette().buttonActive
                  : highlighted ? AppTheme::palette().backgroundElevated
                                : AppTheme::palette().inputBackground;

    g.setColour(bg);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(AppTheme::palette().borderColor);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    auto icon = area.reduced(area.getWidth() * 0.32f, area.getHeight() * 0.28f);
    juce::Path p;
    if (step < 0)
    {
        p.addTriangle(icon.getRight(), icon.getY(),
                      icon.getRight(), icon.getBottom(),
                      icon.getX(),     icon.getCentreY());
    }
    else
    {
        p.addTriangle(icon.getX(),     icon.getY(),
                      icon.getX(),     icon.getBottom(),
                      icon.getRight(), icon.getCentreY());
    }

    g.setColour(AppTheme::palette().textSecondary.withAlpha(isEnabled() ? (down ? 0.75f : 1.0f) : 0.35f));
    g.fillPath(p);
}

DiskPresetBrowserPanel::TypeChipButton::TypeChipButton(const juce::String& chipLabel, const juce::String& tooltip)
    : juce::Button(chipLabel), label(chipLabel)
{
    setClickingTogglesState(true);
    setTooltip(tooltip);
}

static juce::Font chipFont()
{
    return juce::Font(AppTheme::uiFont(10.0f)).boldened();
}

int DiskPresetBrowserPanel::TypeChipButton::getIdealWidth(int padding) const
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(chipFont(), label, 0.0f, 0.0f);
    return juce::roundToInt(std::ceil(glyphs.getBoundingBox(0, -1, true).getWidth())) + padding;
}

// Neutral like the rest of the toolbar: only the active chip stands out, so
// the row tags in the list stay the one place where the kinds have colours.
void DiskPresetBrowserPanel::TypeChipButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const bool selected = getToggleState();
    auto area = getLocalBounds().toFloat().reduced(0.5f, 3.0f);

    auto fill = selected || down ? AppTheme::palette().buttonActive
              : highlighted      ? AppTheme::palette().buttonHover
                                 : AppTheme::palette().inputBackground;
    g.setColour(fill);
    g.fillRoundedRectangle(area, 3.0f);
    g.setColour(selected ? AppTheme::palette().accentActive : AppTheme::palette().borderColor);
    g.drawRoundedRectangle(area, 3.0f, 1.0f);

    g.setColour(selected ? iconInkFor(fill) : AppTheme::palette().textSecondary);
    g.setFont(chipFont());
    g.drawText(label, area.toNearestInt(), juce::Justification::centred, false);
}

DiskPresetBrowserPanel::DiskPresetBrowserPanel()
{
    setOpaque(true);

    searchLabel.setText("Search", juce::dontSendNotification);
    searchLabel.setFont(juce::Font(AppTheme::uiFont(12.0f)));
    addAndMakeVisible(searchLabel);

    searchBox.onTextChange = [this]() { rebuildVisibleEntries(); };
    addAndMakeVisible(searchBox);

    for (auto* b : { &allButton, &patchesButton, &snippetsButton, &banksButton, &setsButton })
    {
        b->setRadioGroupId(11);
        addAndMakeVisible(*b);
    }
    allButton.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    prevButton.onClick = [this]() { stepSelection(-1); };
    nextButton.onClick = [this]() { stepSelection(1); };

    allButton.onClick = [this]() { typeFilter = TypeFilter::All; rebuildVisibleEntries(); };
    patchesButton.onClick = [this]() { typeFilter = TypeFilter::Patches; rebuildVisibleEntries(); };
    snippetsButton.onClick = [this]() { typeFilter = TypeFilter::Snippets; rebuildVisibleEntries(); };
    banksButton.onClick = [this]() { typeFilter = TypeFilter::Banks; rebuildVisibleEntries(); };
    setsButton.onClick = [this]() { typeFilter = TypeFilter::Sets; rebuildVisibleEntries(); };

    hidePch2Button.setTooltip("Hide legacy 2.10 patches (tagged PCH2 in the list)");
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
    searchBox.setTextToShowWhenEmpty("patch, snippet or set name", kDim);
    statusLabel.setColour(juce::Label::textColourId, kDim);
    listBox.setColour(juce::ListBox::backgroundColourId, kPanel);
    listBox.setColour(juce::ListBox::outlineColourId, kSep);

    for (auto* b : std::initializer_list<juce::Component*> { &allButton, &patchesButton, &snippetsButton, &banksButton, &setsButton, &hidePch2Button })
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
    scanFolder(libraryRoot.getChildFile("Sets"), Entry::Type::Set);

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

    for (juce::RangedDirectoryIterator it(folder, true, type == Entry::Type::Set ? "*.nmset" : "*.pch", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
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
        if (hidePch2 && isLegacyPatch210(e))
            continue;

        auto haystack = (e.displayName + " " + e.relativePath).toLowerCase();
        if (search.isNotEmpty() && !haystack.contains(search))
            continue;

        if (entryPassesTypeFilter(e))
            visibleEntryIndices.push_back(i);
    }

    // Just the count: the library path never fit beside it, so it moves to
    // the tooltip for whoever needs to know which folder this is.
    statusLabel.setText(juce::String(visibleEntryIndices.size()) + " of "
                            + juce::String(allEntries.size()) + " files",
                        juce::dontSendNotification);
    statusLabel.setTooltip(libraryRoot != juce::File() ? libraryRoot.getFullPathName() : juce::String());

    listBox.updateContent();
    listBox.repaint();
    updateStepButtons();
}

void DiskPresetBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto searchRow = area.removeFromTop(28);
    searchLabel.setBounds(searchRow.removeFromLeft(52));
    refreshButton.setBounds(searchRow.removeFromRight(32).reduced(1));
    searchBox.setBounds(searchRow.reduced(2));

    filterRowArea = area.removeFromTop(26);
    layoutFilterRow();

    auto statusRow = area.removeFromTop(24);
    nextButton.setBounds(statusRow.removeFromRight(26));
    prevButton.setBounds(statusRow.removeFromRight(26));
    statusRow.removeFromRight(4);
    hidePch2Button.setBounds(statusRow.removeFromRight(juce::jmin(84, statusRow.getWidth() / 2)));
    statusLabel.setBounds(statusRow);
    area.removeFromTop(4);
    listBox.setBounds(area);
}

// Chips take the width their word needs and share out any room left over.
// On a narrow panel the padding tightens instead: the labels are never cut.
void DiskPresetBrowserPanel::layoutFilterRow()
{
    auto row = filterRowArea;
    if (row.isEmpty())
        return;

    TypeChipButton* chips[] = { &allButton, &patchesButton, &snippetsButton, &banksButton, &setsButton };
    constexpr int numChips = static_cast<int>(std::size(chips));
    const int gap = 3;
    auto totalWidth = [&](int padding) {
        int w = gap * (numChips - 1);
        for (auto* c : chips)
            w += c->getIdealWidth(padding);
        return w;
    };

    int padding = 14;
    while (padding > 6 && totalWidth(padding) > row.getWidth())
        padding -= 2;

    const int spare = juce::jmax(0, row.getWidth() - totalWidth(padding));
    for (int i = 0; i < numChips; ++i)
    {
        const int extra = spare / numChips + (i < spare % numChips ? 1 : 0);
        chips[i]->setBounds(row.removeFromLeft(chips[i]->getIdealWidth(padding) + extra));
        row.removeFromLeft(gap);
    }
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
                   : entry.type == Entry::Type::Set     ? kSetTag
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

void DiskPresetBrowserPanel::loadRow(int row)
{
    if (row < 0 || row >= getNumRows())
        return;

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];
    if (entry.type == Entry::Type::Snippet)
    {
        if (onSnippetChosen)
            onSnippetChosen(entry.file);
    }
    else if (entry.type == Entry::Type::Set)
    {
        if (onSetChosen)
            onSetChosen(entry.file);
    }
    else if (onPatchChosen)  // bank backups load exactly like patches
    {
        onPatchChosen(entry.file);
    }
}

// Walks the *visible* rows, so whatever the search box and the type filters
// left on screen is exactly what the arrows step through. Nothing wraps: at
// either end the button greys out rather than jumping to the far side of the
// library, which on a list this long is never what was meant.
void DiskPresetBrowserPanel::stepSelection(int delta)
{
    const int rows = getNumRows();
    if (rows == 0)
        return;

    const int current = listBox.getSelectedRow();
    // No selection yet: the first press picks an end rather than nothing, so a
    // freshly opened browser can be auditioned straight from the keyboard.
    const int target = current < 0 ? (delta > 0 ? 0 : rows - 1)
                                   : current + delta;
    if (target < 0 || target >= rows)
        return;

    listBox.selectRow(target);
    loadRow(target);
}

void DiskPresetBrowserPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    loadRow(row);
}

// Enter loads whatever is selected, so the arrow keys the ListBox already
// handles become a way through the library on their own.
void DiskPresetBrowserPanel::returnKeyPressed(int lastRowSelected)
{
    loadRow(lastRowSelected);
}

void DiskPresetBrowserPanel::selectedRowsChanged(int)
{
    updateStepButtons();
}

void DiskPresetBrowserPanel::updateStepButtons()
{
    const int rows = getNumRows();
    const int current = listBox.getSelectedRow();

    // With nothing selected both arrows are live: either one is a way in.
    prevButton.setEnabled(rows > 0 && (current < 0 || current > 0));
    nextButton.setEnabled(rows > 0 && (current < 0 || current < rows - 1));
}

juce::var DiskPresetBrowserPanel::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    auto row = selectedRows[0];
    if (row < 0 || row >= getNumRows())
        return {};

    const auto& entry = allEntries[static_cast<size_t>(visibleEntryIndices[static_cast<size_t>(row)])];

    // A set is four slots at once: there is no one place to drop it.
    if (entry.type == Entry::Type::Set)
        return {};

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
         : type == Entry::Type::Set     ? "SET"
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
    if (typeFilter == TypeFilter::Sets)
        return entry.type == Entry::Type::Set;
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
    browserPanel.onSetChosen = [this](const juce::File& f) { if (onSetChosen) onSetChosen(f); };
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
