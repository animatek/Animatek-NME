#include "QuickAddPopup.h"

QuickAddPopup::QuickAddPopup(const ModuleDescriptions& descs_, juce::Point<int> screenPos,
                             int gx, int gy, OnSelectCallback cb, OnDismissCallback dismissCb)
    : descs(descs_), spawnGridPos(gx, gy), onSelect(std::move(cb)), onDismiss(std::move(dismissCb))
{
    searchField.setFont(juce::Font(juce::FontOptions(14.0f)));
    searchField.setTextToShowWhenEmpty("Type module name...", juce::Colour(0xff666666));
    searchField.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff323232));
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
}

QuickAddPopup::~QuickAddPopup()
{
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

    g.setColour(juce::Colour(0xff323232));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(0xff6B737C));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.5f);

    // Separator under search field
    g.setColour(juce::Colour(0xff444A53));
    g.fillRect(0, fieldHeight, popupWidth, 1);

    // Module list rows
    int y = fieldHeight + 1;
    for (int i = 0; i < juce::jmin((int)filtered.size(), maxVisible); ++i)
    {
        auto& entry = filtered[i];
        juce::Rectangle<int> row(0, y, popupWidth, rowHeight);

        if (i == selectedIdx)
        {
            g.setColour(juce::Colour(0xff343941));
            g.fillRect(row.reduced(2, 1));
        }

        // Category (dim, left)
        g.setColour(juce::Colour(0xff6B737C));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(entry.category, row.withTrimmedLeft(8).withWidth(80),
                   juce::Justification::centredLeft);

        // Module fullname
        g.setColour(i == selectedIdx ? juce::Colours::white : juce::Colour(0xffcccccc));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(entry.desc->fullname, row.withTrimmedLeft(92).withTrimmedRight(8),
                   juce::Justification::centredLeft);

        y += rowHeight;
    }

    if (filtered.empty())
    {
        g.setColour(juce::Colour(0xff555B64));
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
    for (auto& cat : descs.getCategories())
    {
        for (auto* desc : descs.getModulesInCategory(cat))
        {
            if (lower.isEmpty()
                || desc->name.toLowerCase().contains(lower)
                || desc->fullname.toLowerCase().contains(lower)
                || cat.toLowerCase().contains(lower))
            {
                filtered.push_back({ desc, cat });
            }
        }
    }
    selectedIdx = 0;
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
        if (selectedIdx > 0) { --selectedIdx; repaint(); }
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        if (selectedIdx < (int)filtered.size() - 1) { ++selectedIdx; repaint(); }
        return true;
    }
    return false;
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
