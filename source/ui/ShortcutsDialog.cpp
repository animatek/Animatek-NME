#include "ShortcutsDialog.h"

// Where the "toggle a slot's window" chord differs by platform
#if JUCE_MAC
 #define NME_SC_ZOOM_WHEEL   "Cmd+wheel, pinch"
 #define NME_SC_WIREFRAME    "Cmd+Shift+W"
 #define NME_SC_HARDWARE     "Cmd+Shift+H"
 #define NME_SC_REROUTE      "Cmd+drag connector"
 #define NME_SC_SLOT_TOGGLE  "Cmd+Alt+1..4"
#else
 #define NME_SC_ZOOM_WHEEL   "Ctrl+wheel"
 #define NME_SC_WIREFRAME    "Ctrl+W"
 #define NME_SC_HARDWARE     "Ctrl+H"
 #define NME_SC_REROUTE      "Ctrl+drag connector"
 #define NME_SC_SLOT_TOGGLE  "Ctrl+Shift+1..4"
#endif

// Keep in sync with manual/07-shortcuts.md
const std::vector<ShortcutsDialog::Group>& ShortcutsDialog::groups()
{
    static const std::vector<Group> g = {
        { "File", {}, {
            { "Ctrl+N", "New patch" },
            { "Ctrl+O", "Open patch" },
            { "Ctrl+S", "Save" },
            { "Ctrl+Shift+S", "Save as" },
            { "Ctrl+B", "Preset browser" },
            { "Ctrl+P", "Patch settings" },
            { "Ctrl+G", "Synth settings" },
            { "Ctrl+M", "MIDI setup" },
            { "Ctrl+,", "Editor options" },
            { "Ctrl+Q", "Quit" } } },
        { "Edit", {}, {
            { "Ctrl+Z", "Undo" },
            { "Ctrl+Shift+Z, Ctrl+Y", "Redo" },
            { "Ctrl+A", "Select all modules (section)" },
            { "Ctrl+X / C / V", "Cut / copy / paste modules" },
            { "Ctrl+D", "Duplicate with cables" },
            { "Delete, Backspace", "Delete selection" },
            { "Escape", "Clear selection" },
            { "Arrow keys", "Nudge selected modules" },
            { "Ctrl+R", "Randomize parameters" },
            { "Ctrl+Shift+R", "Randomize (gaussian)" } } },
        { "Canvas", {}, {
            { "Enter, Double-click", "Quick Add module at the mouse" },
            { "F1", "Module help (hovered or selected)" },
            { "F5", "Parameter values overlay" },
            { "F7", "Morph groups overlay" },
            { "F8", "Knob assignments overlay" },
            { "F9", "MIDI CC assignments overlay" },
            { "F3, F10", "Module DSP cost overlay" },
            { "Double-click module", "Module DSP cost" },
            { "+ / -", "Step the control under the pointer" },
            { "Z", "Zoom to the selection; again, back where it was" },
            { "Shift+Z", "Reset zoom to 100%" },
            { "Ctrl++ / Ctrl+-", "Zoom in / out" },
            { NME_SC_ZOOM_WHEEL, "Zoom around the pointer" },
            { "Ctrl+T", "Cycle colour theme" },
            { NME_SC_WIREFRAME, "Toggle wireframe modules" },
            { NME_SC_HARDWARE, "Toggle hardware look" },
            { "Ctrl+I", "Toggle inspector panel (left)" },
            { "Ctrl+Shift+I", "Toggle patch browser (right)" },
            { "S", "Shake cables" },
            { "Middle-drag", "Pan canvas" },
            { NME_SC_REROUTE, "Re-route: lift the cable off that end and drop it on another connector (Alt works too)" } } },
        { "Slots", "Open slots tile themselves: one fills the area, two split it, three go in thirds, four go 2x2. "
                   "Dragging or resizing a sub-window leaves the windows where you put them; View > Slots > Tile Slots "
                   "re-flows. The ABCD button of the module bar goes further and puts the slots back in A, B, C, D order "
                   "within the tiling.", {
            { "Ctrl+1..4", "Switch to slot A..D (opens it if closed)" },
            { NME_SC_SLOT_TOGGLE, "Show or hide slot A..D's sub-window" },
            { "F4, F11", "Focus mode: blow the focused slot up, and back" },
            { "F6", "Zen mode: panels away, every module zoomed to fill the area; again to restore" },
            { "Shift+F6", "Performance mode: no panels, bars or cables, only the modules and the header macros; Esc or Shift+F6 to leave" },
            { "Maximise button", "The same, on that sub-window's title bar" },
            { "Ctrl+Shift+arrows", "Move the focused slot to the neighbouring tile (up and down only in the 2x2)" },
            { "Right-click slot", "Show or hide that slot's sub-window" },
            { "Ctrl+click slot", "Enable or disable without selecting" } } },
        { "Floaters", {}, {
            { "Ctrl+5", "Knob Floater" },
            { "Ctrl+6", "Keyboard Floater" },
            { "Ctrl+7", "Patch Notes" },
            { "Ctrl+8", "Patch Mutator" },
            { "Ctrl+9", "SysEx Monitor" } } },
        { "Sub-window", "These act on the slot and the selection of the sub-window that has the focus.", {
            { "Ctrl+R / Ctrl+Shift+R", "Randomize (uniform / gaussian)" },
            { "Ctrl+S / Ctrl+Shift+S", "Save / Save as" } } },
        { "Mutator", "With the Patch Mutator window focused.", {
            { "1-8", "Focus Mother / Children / Father" },
            { "O / T", "Copy the focused sound to Mother / Father" },
            { "E / U", "Mutate from the focused sound / from Mother" },
            { "N", "Randomize" },
            { "I / X", "Interpolate / Cross (Mother + Father)" },
            { "S", "Save the focused sound to Temporary Storage" },
            { "Shift+drag", "Interpolate two sounds" },
            { "Ctrl+drag", "Cross two sounds" } } },
    };
    return g;
}

namespace
{
constexpr int kRowH = 44, kHeadingH = 40, kPad = 32;

// "Ctrl+Shift+Z, Ctrl+Y" is two ways to do it: one key cap each
juce::StringArray alternatives (const juce::String& keys)
{
    juce::StringArray out;
    out.addTokens (keys, ",", "");
    out.trim();
    out.removeEmptyStrings();
    return out;
}
}

// ─── the list ────────────────────────────────────────────────────────────────

int ShortcutsDialog::List::heightOf (const Line& l)
{
    switch (l.kind)
    {
        case Line::Heading:  return kHeadingH;
        case Line::Note:     return 64;
        default:             return kRowH;
    }
}

int ShortcutsDialog::List::totalHeight() const
{
    int h = 12;
    for (const auto& l : lines) h += heightOf (l);
    return h + 12;
}

void ShortcutsDialog::List::paint (juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    int y = 12;
    const int w = getWidth();
    for (const auto& l : lines)
    {
        const int h = heightOf (l);
        const juce::Rectangle<int> r (0, y, w, h);
        if (l.kind == Line::Heading)
        {
            g.setColour (pal.textMuted);
            g.setFont (juce::Font (AppTheme::uiFont (12.5f)).boldened());
            g.drawText (l.a.toUpperCase(), r.withTrimmedTop (12), juce::Justification::centredLeft, false);
        }
        else if (l.kind == Line::Note)
        {
            g.setColour (pal.textMuted);
            g.setFont (juce::Font (AppTheme::uiFont (12.5f)));
            g.drawFittedText (l.a, r.withTrimmedTop (6).withTrimmedBottom (8), juce::Justification::topLeft, 3, 1.0f);
        }
        else
        {
            g.setColour (pal.borderColor.withAlpha (0.5f));
            g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);

            // The keys, right-aligned, one cap per alternative
            const auto font = juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            auto alts = alternatives (l.a);
            int x = r.getRight();
            for (int i = alts.size(); --i >= 0;)
            {
                const int tw = static_cast<int> (std::ceil (font.getStringWidthFloat (alts[i]))) + 26;
                const auto cap = juce::Rectangle<float> (static_cast<float> (x - tw), static_cast<float> (r.getCentreY()) - 13.0f, static_cast<float> (tw), 26.0f);
                g.setColour (pal.inputBackground);
                g.fillRoundedRectangle (cap, 6.0f);
                g.setColour (pal.borderColor);
                g.drawRoundedRectangle (cap.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (pal.textPrimary);
                g.setFont (font);
                g.drawText (alts[i], cap.toNearestInt(), juce::Justification::centred, false);
                x -= tw + 8;
            }
            g.setColour (pal.textPrimary);
            g.setFont (juce::Font (AppTheme::uiFont (14.0f)));
            g.drawText (l.b, r.getX(), r.getY(), x - r.getX() - 12, r.getHeight() - 1, juce::Justification::centredLeft, true);
        }
        y += h;
    }
}

// ─── the dialog ──────────────────────────────────────────────────────────────

ShortcutsDialog::ShortcutsDialog()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);

    const auto& pal = AppTheme::palette();
    filter.setFont (juce::Font (AppTheme::uiFont (14.0f)));
    filter.setJustification (juce::Justification::centredLeft);
    filter.setIndents (14, 0);
    filter.setTextToShowWhenEmpty ("Filter shortcuts...", pal.textMuted);
    filter.setColour (juce::TextEditor::backgroundColourId, pal.inputBackground);
    filter.setColour (juce::TextEditor::textColourId, pal.textPrimary);
    filter.setColour (juce::TextEditor::outlineColourId, pal.borderColor);
    filter.setColour (juce::TextEditor::focusedOutlineColourId, pal.accentActive);
    filter.onTextChange = [this] { rebuildLines(); };
    filter.onEscapeKey  = [this] { if (filter.getText().isNotEmpty()) filter.setText ({}); else close(); };
    addAndMakeVisible (filter);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    rebuildLines();
    setSize (dialogW, dialogH);
}

juce::Rectangle<int> ShortcutsDialog::groupButtonBounds (int index) const
{
    return { 8, titleBarH + 14 + index * 42, sidebarW - 16, 38 };
}

void ShortcutsDialog::showGroup (int index)
{
    group = juce::jlimit (0, static_cast<int> (groups().size()) - 1, index);
    filter.setText ({}, juce::dontSendNotification);
    rebuildLines();
    viewport.setViewPosition (0, 0);
    repaint();
}

void ShortcutsDialog::rebuildLines()
{
    list.lines.clear();
    const auto text = filter.getText().trim();
    if (text.isEmpty())
    {
        const auto& g = groups()[static_cast<size_t> (group)];
        if (g.note.isNotEmpty()) list.lines.push_back ({ Line::Note, g.note, {} });
        for (const auto& e : g.entries) list.lines.push_back ({ Line::Shortcut, e.keys, e.action });
    }
    else
    {
        // A search goes through every group, under the name of each one that has a match
        for (const auto& g : groups())
        {
            std::vector<Line> found;
            for (const auto& e : g.entries)
                if (e.action.containsIgnoreCase (text) || e.keys.containsIgnoreCase (text))
                    found.push_back ({ Line::Shortcut, e.keys, e.action });
            if (found.empty()) continue;
            list.lines.push_back ({ Line::Heading, g.name, {} });
            list.lines.insert (list.lines.end(), found.begin(), found.end());
        }
        if (list.lines.empty())
            list.lines.push_back ({ Line::Note, "No shortcut matches \"" + text + "\".", {} });
    }
    list.setSize (juce::jmax (1, viewport.getWidth() - viewport.getScrollBarThickness()), list.totalHeight());
    list.repaint();
    repaint();
}

void ShortcutsDialog::resized()
{
    closeButton.setBounds (getWidth() - 36, 6, 28, 28);
    const int x0 = sidebarW + kPad, cw = getWidth() - x0 - kPad;
    filter.setBounds (x0, titleBarH + 20, cw, 38);
    viewport.setBounds (x0, titleBarH + 72, cw + 8, getHeight() - titleBarH - 72 - 12);
    list.setSize (juce::jmax (1, viewport.getWidth() - viewport.getScrollBarThickness()), list.totalHeight());
}

void ShortcutsDialog::paint (juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    g.fillAll (pal.backgroundMain);

    g.setColour (pal.textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (15.0f)).boldened());
    g.drawText ("Keyboard Shortcuts", 18, 0, getWidth() - 60, titleBarH, juce::Justification::centredLeft);
    g.setColour (pal.borderColor);
    g.fillRect (0, titleBarH - 1, getWidth(), 1);

    g.setColour (pal.backgroundPanel);
    g.fillRect (0, titleBarH, sidebarW, getHeight() - titleBarH);
    g.setColour (pal.borderColor);
    g.fillRect (sidebarW - 1, titleBarH, 1, getHeight() - titleBarH);

    const bool searching = filter.getText().trim().isNotEmpty();
    for (int i = 0; i < static_cast<int> (groups().size()); ++i)
    {
        const auto r = groupButtonBounds (i).toFloat();
        const bool on = (i == group) && ! searching;
        if (on)
        {
            g.setColour (pal.buttonBackground);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (pal.accentActive);
            g.fillRoundedRectangle (r.getX(), r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);
        }
        g.setColour (on ? pal.textPrimary : pal.textSecondary);
        g.setFont (juce::Font (AppTheme::uiFont (14.0f).withStyle (on ? "Bold" : "Regular")));
        g.drawText (groups()[static_cast<size_t> (i)].name, r.toNearestInt().withTrimmedLeft (18), juce::Justification::centredLeft, false);
    }
}

bool ShortcutsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}

void ShortcutsDialog::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < static_cast<int> (groups().size()); ++i)
        if (groupButtonBounds (i).contains (e.getPosition())) { showGroup (i); return; }

    // Only a press on the title bar drags the window
    draggingWindow = e.getPosition().getY() < titleBarH;
    if (draggingWindow)
        dragger.startDraggingComponent (this, e);
}

void ShortcutsDialog::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingWindow)
        dragger.dragComponent (this, e, nullptr);
}

void ShortcutsDialog::close()
{
    removeFromDesktop();
    delete this;
}

ShortcutsDialog* ShortcutsDialog::show (juce::Component* parent)
{
    auto* dlg = new ShortcutsDialog();
    if (parent != nullptr)
    {
        auto* top = parent->getTopLevelComponent();
        auto screen = top->localAreaToGlobal (top->getLocalBounds());
        dlg->setTopLeftPosition (screen.getX() + (screen.getWidth()  - dlg->getWidth())  / 2,
                                 screen.getY() + (screen.getHeight() - dlg->getHeight()) / 2);
    }
    dlg->addToDesktop (juce::ComponentPeer::windowHasDropShadow);
    dlg->setVisible (true);
    dlg->toFront (true);
    dlg->grabKeyboardFocus();
    return dlg;
}
