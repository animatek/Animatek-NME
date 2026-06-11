#include "QuickAddPopup.h"
#include "AppTheme.h"

juce::PropertiesFile* QuickAddPopup::sharedSettings = nullptr;

void QuickAddPopup::setSharedSettings(juce::PropertiesFile* settings)
{
    sharedSettings = settings;
}

QuickAddPopup::QuickAddPopup(const ModuleDescriptions& descs_, juce::Point<int> screenPos,
                             int gx, int gy, OnSelectCallback cb, OnDismissCallback dismissCb)
    : descs(descs_), spawnGridPos(gx, gy), onSelect(std::move(cb)), onDismiss(std::move(dismissCb))
{
    if (sharedSettings != nullptr)
        favorites.addTokens(sharedSettings->getValue("favoriteModules"), "|", "");

    searchField.setFont(juce::Font(juce::FontOptions(14.0f)));
    searchField.setTextToShowWhenEmpty("Type module name...", juce::Colour(0xff666666));
    searchField.setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().backgroundPanel);
    searchField.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchField.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    searchField.addListener(this);
    searchField.addKeyListener(this);
    addAndMakeVisible(searchField);

    rebuildFiltered("");

    // Add as desktop component (screen coordinates)
    addToDesktop(juce::ComponentPeer::windowIsTemporary
                 | juce::ComponentPeer::windowHasDropShadow);

    // Clamp to screen so it doesn't go off-edge
    auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    auto screen = display ? display->userArea : juce::Rectangle<int>(0, 0, 1920, 1080);
    int px = juce::jlimit(screen.getX(), screen.getRight()  - popupWidth, screenPos.x);
    int py = juce::jlimit(screen.getY(), screen.getBottom() - 300,        screenPos.y);
    setTopLeftPosition(px, py);
    setVisible(true);

    // Dismiss on any click outside the popup (see mouseDown)
    juce::Desktop::getInstance().addGlobalMouseListener(this);
}

QuickAddPopup::~QuickAddPopup()
{
    juce::Desktop::getInstance().removeGlobalMouseListener(this);
    if (onDismiss) onDismiss();
}

int QuickAddPopup::totalHeight() const
{
    int rows = filtered.empty() ? 1 : juce::jmin((int)filtered.size(), maxVisible);
    return fieldHeight + rows * rowHeight + 4;
}

void QuickAddPopup::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(AppTheme::palette().backgroundPanel);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(AppTheme::palette().borderColor);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.5f);

    // Separator under search field
    g.setColour(AppTheme::palette().buttonActive);
    g.fillRect(0, fieldHeight, popupWidth, 1);

    // Module list rows (window of maxVisible rows starting at scrollOffset)
    int y = fieldHeight + 1;
    const int visible = juce::jmin((int)filtered.size() - scrollOffset, maxVisible);
    for (int v = 0; v < visible; ++v)
    {
        const int i = scrollOffset + v;
        auto& entry = filtered[(size_t)i];
        juce::Rectangle<int> row(0, y, popupWidth, rowHeight);

        if (i == selectedIdx)
        {
            g.setColour(AppTheme::palette().backgroundElevated);
            g.fillRect(row.reduced(2, 1));
        }

        // Category (dim, left)
        g.setColour(AppTheme::palette().borderColor);
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(entry.category, row.withTrimmedLeft(8).withWidth(80),
                   juce::Justification::centredLeft);

        // Module fullname
        g.setColour(i == selectedIdx ? juce::Colours::white : AppTheme::palette().textSecondary);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(entry.desc->fullname, row.withTrimmedLeft(92).withTrimmedRight(26),
                   juce::Justification::centredLeft);

        // Favorite star: gold when favorite, outline hint on the selected row
        const bool fav = isFavorite(entry.desc->name);
        if (fav || i == selectedIdx)
        {
            g.setColour(fav ? juce::Colour(0xffe8c84a) : AppTheme::palette().textSecondary);
            g.setFont(juce::Font(juce::FontOptions(15.0f)));
            g.drawText(juce::String::fromUTF8(fav ? "\xe2\x98\x85" : "\xe2\x98\x86"),
                       row.removeFromRight(24).withTrimmedRight(6),
                       juce::Justification::centred);
        }

        y += rowHeight;
    }

    // Scrollbar indicator when the list overflows
    if ((int)filtered.size() > maxVisible)
    {
        const float listH  = (float)(maxVisible * rowHeight);
        const float ratio  = (float)maxVisible / (float)filtered.size();
        const float thumbH = juce::jmax(18.0f, listH * ratio);
        const float maxOff = (float)((int)filtered.size() - maxVisible);
        const float thumbY = (float)(fieldHeight + 1)
                           + (listH - thumbH) * ((float)scrollOffset / maxOff);
        g.setColour(AppTheme::palette().borderColor);
        g.fillRoundedRectangle((float)popupWidth - 5.0f, thumbY, 3.0f, thumbH, 1.5f);
    }

    if (filtered.empty())
    {
        g.setColour(AppTheme::palette().borderColor);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("No modules found", 0, fieldHeight + 4, popupWidth, rowHeight,
                   juce::Justification::centred);
    }
}

void QuickAddPopup::resized()
{
    searchField.setBounds(6, 4, popupWidth - 12, fieldHeight - 8);
}

void QuickAddPopup::rebuildFiltered(const juce::String& text)
{
    filtered.clear();
    auto lower = text.toLowerCase();

    // Rank matches: name prefix > name > fullname > category/tags, keeping
    // category order within each rank
    std::vector<std::pair<int, Entry>> scored;
    for (auto& cat : descs.getCategories())
    {
        for (auto* desc : descs.getModulesInCategory(cat))
        {
            int score = -1;
            if (lower.isEmpty())
                score = 0;
            else if (desc->name.toLowerCase().startsWith(lower))
                score = 0;
            else if (desc->name.toLowerCase().contains(lower))
                score = 1;
            else if (desc->fullname.toLowerCase().contains(lower))
                score = 2;
            else if (cat.toLowerCase().contains(lower) || desc->tags.contains(lower))
                score = 3;

            // Favorites sort ahead of non-favorites within the same rank
            if (score >= 0)
                scored.push_back({ score * 2 + (isFavorite(desc->name) ? 0 : 1),
                                   { desc, cat } });
        }
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& s : scored)
        filtered.push_back(s.second);

    selectedIdx = 0;
    scrollOffset = 0;
    setSize(popupWidth, totalHeight());
    repaint();
}

void QuickAddPopup::textEditorTextChanged(juce::TextEditor& ed)
{
    rebuildFiltered(ed.getText());
}

void QuickAddPopup::textEditorReturnKeyPressed(juce::TextEditor&)
{
    confirmSelection();
}

void QuickAddPopup::textEditorEscapeKeyPressed(juce::TextEditor&)
{
    dismiss();
}

bool QuickAddPopup::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (key.getKeyCode() == juce::KeyPress::upKey)
    {
        if (selectedIdx > 0) { --selectedIdx; scrollSelectedIntoView(); repaint(); }
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        if (selectedIdx < (int)filtered.size() - 1) { ++selectedIdx; scrollSelectedIntoView(); repaint(); }
        return true;
    }
    return false;
}

void QuickAddPopup::scrollSelectedIntoView()
{
    if (selectedIdx < scrollOffset)
        scrollOffset = selectedIdx;
    else if (selectedIdx >= scrollOffset + maxVisible)
        scrollOffset = selectedIdx - maxVisible + 1;
}

int QuickAddPopup::rowIndexAt(juce::Point<int> localPos) const
{
    if (!getLocalBounds().contains(localPos) || localPos.y <= fieldHeight)
        return -1;
    int v = (localPos.y - fieldHeight - 1) / rowHeight;
    if (v >= juce::jmin((int)filtered.size() - scrollOffset, maxVisible))
        return -1;
    return scrollOffset + v;
}

void QuickAddPopup::mouseDown(const juce::MouseEvent& e)
{
    // Clicks on the popup arrive twice: direct dispatch plus the global mouse
    // listener. Handle each physical click once or star toggles double-fire.
    if (e.eventTime == lastMouseDownTime)
        return;
    lastMouseDownTime = e.eventTime;

    auto local = e.getEventRelativeTo(this).getPosition();

    // Defer dismissal/selection: we may be inside the global mouse dispatch,
    // and dismiss() deletes this component
    juce::Component::SafePointer<QuickAddPopup> safe(this);

    if (!getLocalBounds().contains(local))  // global listener: click outside
    {
        juce::MessageManager::callAsync([safe]() {
            if (safe != nullptr) safe->dismiss();
        });
        return;
    }

    int row = rowIndexAt(local);
    if (row >= 0)
    {
        if (isStarZone(local))
        {
            toggleFavorite(filtered[(size_t)row].desc->name);
            rebuildFiltered(searchField.getText());  // reorder with new favorites
            return;
        }

        selectedIdx = row;
        repaint();
        juce::MessageManager::callAsync([safe]() {
            if (safe != nullptr) safe->confirmSelection();
        });
    }
}

bool QuickAddPopup::isStarZone(juce::Point<int> localPos) const
{
    return localPos.x >= popupWidth - 26;
}

bool QuickAddPopup::isFavorite(const juce::String& moduleName) const
{
    return favorites.contains(moduleName);
}

void QuickAddPopup::toggleFavorite(const juce::String& moduleName)
{
    if (favorites.contains(moduleName))
        favorites.removeString(moduleName);
    else
        favorites.add(moduleName);

    if (sharedSettings != nullptr)
    {
        sharedSettings->setValue("favoriteModules", favorites.joinIntoString("|"));
        sharedSettings->saveIfNeeded();
    }
}

void QuickAddPopup::mouseMove(const juce::MouseEvent& e)
{
    int row = rowIndexAt(e.getEventRelativeTo(this).getPosition());
    if (row >= 0 && row != selectedIdx)
    {
        selectedIdx = row;
        repaint();
    }
}

void QuickAddPopup::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Global listener also forwards wheel events from elsewhere — only react
    // when the cursor is over the popup
    auto local = e.getEventRelativeTo(this).getPosition();
    if (!getLocalBounds().contains(local))
        return;

    const int maxOffset = juce::jmax(0, (int)filtered.size() - maxVisible);
    if (maxOffset == 0)
        return;

    const int step = wheel.deltaY < 0 ? 3 : -3;
    int newOffset = juce::jlimit(0, maxOffset, scrollOffset + step);
    if (newOffset == scrollOffset)
        return;

    scrollOffset = newOffset;
    // Keep the highlight under the cursor while the rows slide beneath it
    int row = rowIndexAt(local);
    if (row >= 0)
        selectedIdx = row;
    repaint();
}

void QuickAddPopup::confirmSelection()
{
    if (!filtered.empty() && selectedIdx >= 0 && selectedIdx < (int)filtered.size())
    {
        if (onSelect)
            onSelect(filtered[selectedIdx].desc, spawnGridPos.x, spawnGridPos.y);
    }
    dismiss();
}

void QuickAddPopup::dismiss()
{
    removeFromDesktop();
    delete this;
}
