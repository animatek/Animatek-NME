#include "PresetBrowserWindow.h"
#include <algorithm>
#include <filesystem>
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
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(AppTheme::palette().borderColor);
    g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);

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
    return juce::Font(AppTheme::uiFont(11.0f)).boldened();
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
    auto area = getLocalBounds().toFloat().reduced(0.5f, 2.0f);

    // A pill: the chosen one is filled with the accent, the others are outlined
    auto fill = selected || down ? AppTheme::palette().accentActive
              : highlighted      ? AppTheme::palette().buttonHover
                                 : AppTheme::palette().buttonBackground;
    g.setColour(fill);
    g.fillRoundedRectangle(area, area.getHeight() * 0.5f);
    if (!selected)
    {
        g.setColour(AppTheme::palette().borderColor);
        g.drawRoundedRectangle(area.reduced(0.5f), area.getHeight() * 0.5f, 1.0f);
    }

    g.setColour(selected ? iconInkFor(fill) : AppTheme::palette().textSecondary);
    g.setFont(chipFont());
    g.drawText(label, area.toNearestInt(), juce::Justification::centred, false);
}

DiskPresetBrowserPanel::DiskPresetBrowserPanel()
{
    setOpaque(true);

    // Centred in the field, both ways, and in a size that fills it
    searchBox.setFont(juce::Font(AppTheme::uiFont(15.0f)));
    searchBox.setJustification(juce::Justification::centredLeft);
    searchBox.setIndents(12, 0);

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
        if (onHidePch2Changed)
            onHidePch2Changed(hidePch2);
        rebuildVisibleEntries();
        if (hidePch2)
            sniffLegacyInBackground();
    };
    addAndMakeVisible(hidePch2Button);

    refreshButton.onClick = [this]() { refresh(); };
    addAndMakeVisible(refreshButton);

    statusLabel.setFont(juce::Font(AppTheme::uiFont(12.0f)));
    addAndMakeVisible(statusLabel);

    listBox.setRowHeight(32);
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
    searchBox.setTextToShowWhenEmpty("Search patch, snippet or set...", kDim);
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

    // The two hairlines, edge to edge, like the rules between the inspector's sections
    g.setColour(AppTheme::palette().borderColor);
    g.fillRect(0, rule1Y, getWidth(), 1);
    g.fillRect(0, rule2Y, getWidth(), 1);
}

void DiskPresetBrowserPanel::setLibraryRoot(const juce::File& root)
{
    // The same folder asked for again while it is still being read is the same request: starting
    // a second read would only make both slower
    if (root == libraryRoot && scanning)
        return;
    libraryRoot = root;
    refresh();
}

void DiskPresetBrowserPanel::refresh()
{
    if (libraryRoot == juce::File() || !libraryRoot.isDirectory())
    {
        ++scanGeneration;   // whatever was being read is no longer wanted
        scanning = false;
        allEntries.clear();
        statusLabel.setText("Choose a preset library folder first.", juce::dontSendNotification);
        rebuildVisibleEntries();
        return;
    }

    // The list on screen stays as it is until the new one is ready
    scanning = true;
    statusLabel.setText("Reading the library...", juce::dontSendNotification);

    const int generation = ++scanGeneration;
    const auto root = libraryRoot;
    juce::Component::SafePointer<DiskPresetBrowserPanel> safeThis(this);
    juce::Thread::launch([safeThis, root, generation]()
    {
        // Only the names are read here, so the list can appear at once. Which patches are old is
        // found out afterwards (sniffLegacyInBackground), because that means opening every file.
        auto entries = scanLibrary(root, false);
        juce::MessageManager::callAsync([safeThis, generation, entries = std::move(entries)]() mutable
        {
            if (safeThis != nullptr && safeThis->scanGeneration.load() == generation)
                safeThis->scanFinished(std::move(entries));
        });
    });
}

std::vector<DiskPresetBrowserPanel::Entry> DiskPresetBrowserPanel::scanLibrary(const juce::File& root, bool sniffLegacy)
{
    std::vector<Entry> entries;

    // A patch is any .pch under the library folder, not only what is inside
    // Patches/: people point the library at the folder they already keep their
    // patches in (#106). The folders with a meaning of their own are left out,
    // so a snippet or a bank backup is not listed twice.
    const std::vector<juce::File> ownFolders {
        root.getChildFile("Snippets"), root.getChildFile("Banks"),
        root.getChildFile("Sets"),     root.getChildFile("Presets") };
    scanFolder(root, root, Entry::Type::Patch, ownFolders, entries);
    scanFolder(root, root.getChildFile("Snippets"), Entry::Type::Snippet, {}, entries);
    scanFolder(root, root.getChildFile("Banks"), Entry::Type::Bank, {}, entries);
    scanFolder(root, root.getChildFile("Sets"), Entry::Type::Set, {}, entries);

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.type != b.type)
            return static_cast<int>(a.type) < static_cast<int>(b.type);
        return a.displayName.compareIgnoreCase(b.displayName) < 0;
    });

    if (sniffLegacy)
        for (auto& e : entries)
            if (e.type == Entry::Type::Patch)
                e.legacyPatch210 = PchFileIO::isLegacyPatch210(e.file) ? 1 : 0;

    return entries;
}

void DiskPresetBrowserPanel::scanFinished(std::vector<Entry> entries)
{
    scanning = false;
    allEntries = std::move(entries);
    rebuildVisibleEntries();
    // "Hide 2.10" may have been switched on after this scan began: find out about what it missed
    if (hidePch2)
        sniffLegacyInBackground();
}

// Switching "Hide 2.10" on needs to know which patches are old, which means opening them: done
// off the message thread too, and the list is rebuilt when it is known. Until then nothing is hidden.
void DiskPresetBrowserPanel::sniffLegacyInBackground()
{
    std::vector<juce::File> files;
    std::vector<size_t> indices;
    for (size_t i = 0; i < allEntries.size(); ++i)
        if (allEntries[i].type == Entry::Type::Patch && allEntries[i].legacyPatch210 < 0)
        {
            files.push_back(allEntries[i].file);
            indices.push_back(i);
        }
    if (files.empty())
        return;

    const int generation = scanGeneration.load();
    juce::Component::SafePointer<DiskPresetBrowserPanel> safeThis(this);
    juce::Thread::launch([safeThis, generation, files = std::move(files), indices = std::move(indices)]()
    {
        std::vector<int> result;
        result.reserve(files.size());
        for (const auto& f : files)
            result.push_back(PchFileIO::isLegacyPatch210(f) ? 1 : 0);

        juce::MessageManager::callAsync([safeThis, generation, indices, result]()
        {
            if (safeThis == nullptr || safeThis->scanGeneration.load() != generation)
                return;   // the list was read again meanwhile: the indices mean something else now
            for (size_t k = 0; k < indices.size() && indices[k] < safeThis->allEntries.size(); ++k)
                safeThis->allEntries[indices[k]].legacyPatch210 = result[k];
            safeThis->rebuildVisibleEntries();
        });
    });
}

void DiskPresetBrowserPanel::scanFolder(const juce::File& root, const juce::File& folder, Entry::Type type,
                                        const std::vector<juce::File>& skipFolders, std::vector<Entry>& out)
{
    if (!folder.isDirectory())
        return;

    // std::filesystem reads the type of each entry from the directory listing itself, where the
    // JUCE iterator stats every file: on a library of 50,000 patches that is ten seconds against
    // a fraction of one.
    namespace fs = std::filesystem;
    const juce::String wanted = type == Entry::Type::Set ? ".nmset" : ".pch";
    std::error_code ec;
    fs::recursive_directory_iterator it(fs::u8path(folder.getFullPathName().toStdString()),
                                        fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;

    for (; !ec && it != end; it.increment(ec))
    {
        std::error_code entryError;
        if (!it->is_regular_file(entryError))
            continue;

        const auto ext = juce::String(it->path().extension().u8string());
        if (!ext.equalsIgnoreCase(wanted))
            continue;

        juce::File file(juce::String::fromUTF8(it->path().u8string().c_str()));
        if (std::any_of(skipFolders.begin(), skipFolders.end(),
                        [&file](const juce::File& skip) { return file.isAChildOf(skip); }))
            continue;

        Entry entry;
        entry.type = type;
        entry.displayName = type == Entry::Type::Bank
            ? file.getRelativePathFrom(folder).dropLastCharacters(4)
            : file.getFileNameWithoutExtension();
        entry.relativePath = file.getRelativePathFrom(root);
        entry.file = std::move(file);
        out.push_back(std::move(entry));
    }
}

void DiskPresetBrowserPanel::rebuildVisibleEntries()
{
    visibleEntryIndices.clear();
    auto search = searchBox.getText().trim().toLowerCase();

    for (int i = 0; i < static_cast<int>(allEntries.size()); ++i)
    {
        auto& e = allEntries[static_cast<size_t>(i)];
        if (hidePch2 && e.legacyPatch210 == 1)
            continue;

        auto haystack = (e.displayName + " " + e.relativePath).toLowerCase();
        if (search.isNotEmpty() && !haystack.contains(search))
            continue;

        if (entryPassesTypeFilter(e))
            visibleEntryIndices.push_back(i);
    }

    // Just the count: the library path never fit beside it, so it moves to
    // the tooltip for whoever needs to know which folder this is.
    statusLabel.setText(scanning && allEntries.empty()
                            ? juce::String("Reading the library...")
                            : visibleEntryIndices.size() == allEntries.size()
                                ? juce::String(allEntries.size()) + " files"
                                : juce::String(visibleEntryIndices.size()) + " of " + juce::String(allEntries.size()),
                        juce::dontSendNotification);
    statusLabel.setTooltip(libraryRoot != juce::File() ? libraryRoot.getFullPathName() : juce::String());

    listBox.updateContent();
    listBox.repaint();
    updateStepButtons();
}

void DiskPresetBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced(12, 10);

    auto searchRow = area.removeFromTop(36);
    refreshButton.setBounds(searchRow.removeFromRight(36));
    searchBox.setBounds(searchRow.withTrimmedRight(8));
    area.removeFromTop(10);

    filterRowArea = area.removeFromTop(28);
    layoutFilterRow();
    area.removeFromTop(10);

    // A hairline under the search and the filters, then the count row, then another over the list
    rule1Y = area.getY();
    area.removeFromTop(1 + 5);
    auto statusRow = area.removeFromTop(30);
    nextButton.setBounds(statusRow.removeFromRight(28));
    statusRow.removeFromRight(4);
    prevButton.setBounds(statusRow.removeFromRight(28));
    statusRow.removeFromRight(8);
    hidePch2Button.setBounds(statusRow.removeFromRight(juce::jmin(116, statusRow.getWidth() * 3 / 5)));
    statusLabel.setBounds(statusRow);
    area.removeFromTop(5);
    rule2Y = area.getY();
    area.removeFromTop(1 + 2);
    listBox.setBounds(area.withX(0).withWidth(getWidth()));
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
    const int gap = 6;
    auto totalWidth = [&](int padding) {
        int w = gap * (numChips - 1);
        for (auto* c : chips)
            w += c->getIdealWidth(padding);
        return w;
    };

    // Each chip is as wide as its word needs, with room around it, and they sit side by side
    // from the left. On a narrow panel the padding tightens instead: the labels are never cut.
    int padding = 20;
    while (padding > 8 && totalWidth(padding) > row.getWidth())
        padding -= 2;

    for (auto* c : chips)
    {
        c->setBounds(row.removeFromLeft(c->getIdealWidth(padding)));
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

    g.fillAll(kPanel);
    if (selected)
    {
        g.setColour(AppTheme::palette().backgroundElevated);
        g.fillRoundedRectangle(4.0f, 1.0f, static_cast<float>(width) - 8.0f, static_cast<float>(height) - 2.0f, 5.0f);
    }

    auto tagArea = juce::Rectangle<int>(10, (height - 18) / 2, 50, 18);
    auto tagColour = isLegacyPatch210(entry)            ? legacyPatchTagColour()
                   : entry.type == Entry::Type::Patch   ? kPatchTag
                   : entry.type == Entry::Type::Snippet ? kSnippetTag
                   : entry.type == Entry::Type::Set     ? kSetTag
                                                        : kBankTag;
    g.setColour(tagColour.withAlpha(0.22f));
    g.fillRoundedRectangle(tagArea.toFloat(), 4.0f);
    g.setColour(tagColour);
    g.setFont(juce::Font(AppTheme::uiFont(10.0f)).boldened());
    g.drawText(getTypeLabel(entry), tagArea, juce::Justification::centred, true);

    g.setColour(selected ? AppTheme::palette().textPrimary : AppTheme::palette().textPrimary.withAlpha(0.9f));
    g.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    g.drawText(entry.displayName, 70, 0, width - 78, height, juce::Justification::centredLeft, true);
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
