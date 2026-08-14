#include "ModuleIconBar.h"
#include "AppTheme.h"
#include "PatchCanvasComponent.h"

// The tabs are labelled the way the original prints them, which is shorter than
// the category names modules.xml uses.
static juce::String tabLabelFor(const juce::String& category)
{
    if (category == "Oscillator") return "Osc";
    if (category == "Envelope")   return "Env";
    if (category == "Control")    return "Ctrl";
    // modules.xml has been spelled both ways: nmedit's original has "Seqencer".
    if (category == "Seqencer" || category == "Sequencer") return "Seq";
    return category;
}

// The order the original lays the tabs out in. Anything modules.xml adds beyond
// this list still appears, after these.
static const juce::StringArray& originalCategoryOrder()
{
    static const juce::StringArray order {
        "In/Out", "Oscillator", "LFO", "Envelope", "Filter",
        "Mixer", "Audio", "Control", "Logic", "Seqencer", "Sequencer"
    };
    return order;
}

// Whatever this editor adds to a patch that the G1 knows nothing about lives
// under its own tab, at the end, so no Clavia category ever gains a member the
// synth would not recognise. Right now that is the text note; anything else of
// ours belongs here too.
static const char* anmeCategory = "ANME";

static juce::Font chipFont()
{
    return juce::Font(AppTheme::uiFont(11.0f));
}

// The chips carry the module's full name, and a handful of words in it cost more
// room than they earn: a whole category has to fit across the window. Every one
// of these reads the same at a glance, and none is a substring of another word
// in modules.xml, nor does any replacement produce a string a later one would
// match, so the order they run in does not matter.
//
// Deliberately NOT abbreviated: Slave, Random, Level, Signal, Portamento. Those
// take a moment to decode, which defeats the point of a bar you glance at. Nor
// is the category's own word dropped when the tab already says it: it would save
// more than everything here put together, but it leaves "Sine Osc" reading
// "Sine" and "ADSR Env" reading "ADSR".
static juce::String chipLabelFor(const juce::String& fullname)
{
    // The Audio area's Quantizer is nothing but that word, so abbreviating it
    // leaves "Quant" standing alone with nothing to lean on, and saves width on
    // the shortest chip in its row, which needed none. (The Amplifier is the
    // other one-word name here and keeps its "Amp": that is what the module has
    // always been called.)
    if (fullname == "Quantizer")
        return fullname;

    return fullname.replace("Oscillator",  "Osc")
                   .replace("Generator",   "Gen")
                   .replace("Envelope",    "Env")
                   .replace("Sequencer",   "Seq")
                   .replace("Control",     "Ctrl")
                   .replace("Frequency",   "Freq")
                   .replace("Equalizer",   "EQ")
                   .replace("Amplifier",   "Amp")
                   .replace("Amplitude",   "Amp")
                   .replace("Modulator",   "Mod")
                   .replace("Synthesizer", "Synth")
                   .replace("Percussion",  "Perc")
                   .replace("Compressor",  "Comp")
                   .replace("Quantizer",   "Quant")
                   .replace("Attenuator",  "Atten")
                   .replace("Sawtooth",    "Saw")
                   .replace("Triangle",    "Tri")
                   .replace("Multimode",   "Multi");
}

ModuleIconBar::ModuleIconBar()
{
    viewport.setViewedComponent(&strip, false);
    viewport.setScrollBarsShown(false, true, false, true);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);
}

void ModuleIconBar::setModuleDescriptions(const ModuleDescriptions* descriptions)
{
    moduleDescs = descriptions;
    rebuildTabs();
}

void ModuleIconBar::rebuildTabs()
{
    categories.clear();
    tabLabels.clear();

    if (moduleDescs != nullptr)
    {
        const auto available = moduleDescs->getCategories();
        for (const auto& c : originalCategoryOrder())
            if (available.contains(c))
                categories.add(c);
        for (const auto& c : available)
            if (!categories.contains(c))
                categories.add(c);
    }

    categories.add(anmeCategory);

    for (const auto& c : categories)
        tabLabels.add(tabLabelFor(c));

    selectedTab = juce::jlimit(0, juce::jmax(0, categories.size() - 1), selectedTab);
    showCategory(selectedTab);
    resized();
    repaint();
}

void ModuleIconBar::showCategory(int tabIndex)
{
    if (categories.isEmpty())
    {
        strip.setModules({});
        return;
    }

    selectedTab = juce::jlimit(0, categories.size() - 1, tabIndex);

    if (categories[selectedTab] == anmeCategory)
        strip.setEditorExtras();
    else if (moduleDescs != nullptr)
        strip.setModules(moduleDescs->getModulesInCategory(categories[selectedTab]));
    else
        strip.setModules({});

    viewport.setViewPosition(0, 0);
    resized();
    repaint();
}

juce::String ModuleIconBar::getSelectedCategory() const
{
    return categories.isEmpty() ? juce::String() : categories[selectedTab];
}

void ModuleIconBar::setSelectedCategory(const juce::String& category)
{
    const int index = categories.indexOf(category);
    if (index >= 0)
        showCategory(index);
}

void ModuleIconBar::resized()
{
    auto area = getLocalBounds();
    auto tabRow = area.removeFromTop(tabRowHeight);

    tabBounds.clear();
    const auto font = chipFont();
    int x = tabRow.getX() + 4;
    for (const auto& label : tabLabels)
    {
        const int w = juce::jmax(30, static_cast<int>(font.getStringWidthFloat(label)) + 16);
        tabBounds.push_back({ x, tabRow.getY(), w, tabRow.getHeight() });
        x += w;
    }

    // Keep the last pixel row for the separator below, or the viewport paints
    // over it and the bar runs straight into the canvas.
    area.removeFromBottom(separatorHeight);

    viewport.setBounds(area);
    strip.setSize(juce::jmax(area.getWidth(), strip.getWidth()), area.getHeight());
}

void ModuleIconBar::applyTheme()
{
    repaint();
    strip.repaint();
}

void ModuleIconBar::paint(juce::Graphics& g)
{
    const auto& p = AppTheme::palette();

    g.fillAll(p.backgroundPanel);
    g.setColour(p.borderColor);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

    g.setFont(chipFont());
    for (size_t i = 0; i < tabBounds.size(); ++i)
    {
        const auto r = tabBounds[i];
        const bool active = (static_cast<int>(i) == selectedTab);
        const bool hovered = (static_cast<int>(i) == hoverTab);

        g.setColour(active ? p.backgroundElevated
                           : hovered ? p.buttonHover
                                     : p.backgroundSecondary);
        g.fillRect(r.reduced(1, 2));
        g.setColour(p.borderColor);
        g.drawRect(r.reduced(1, 2), 1);

        g.setColour(active ? p.textPrimary : p.textSecondary);
        g.drawText(tabLabels[static_cast<int>(i)], r, juce::Justification::centred, false);
    }
}

void ModuleIconBar::mouseDown(const juce::MouseEvent& e)
{
    for (size_t i = 0; i < tabBounds.size(); ++i)
        if (tabBounds[i].contains(e.getPosition()))
        {
            showCategory(static_cast<int>(i));
            if (onCategoryChanged)
                onCategoryChanged(getSelectedCategory());
            return;
        }
}

void ModuleIconBar::mouseMove(const juce::MouseEvent& e)
{
    int found = -1;
    for (size_t i = 0; i < tabBounds.size(); ++i)
        if (tabBounds[i].contains(e.getPosition()))
            found = static_cast<int>(i);

    if (found != hoverTab)
    {
        hoverTab = found;
        repaint();
    }
}

void ModuleIconBar::mouseExit(const juce::MouseEvent&)
{
    if (hoverTab != -1)
    {
        hoverTab = -1;
        repaint();
    }
}

// ── The chip strip ──────────────────────────────────────────────────────────

void ModuleIconBar::ChipStrip::setModules(std::vector<const ModuleDescriptor*> mods)
{
    chips.clear();
    chips.reserve(mods.size());
    for (const auto* d : mods)
        chips.push_back({ d, chipLabelFor(d->fullname), {} });

    hoverIndex = -1;
    layOutChips();
    repaint();
}

void ModuleIconBar::ChipStrip::setEditorExtras()
{
    chips.clear();
    chips.push_back({ nullptr, "Comment", {} });

    hoverIndex = -1;
    layOutChips();
    repaint();
}

void ModuleIconBar::ChipStrip::layOutChips()
{
    // Deliberately not centred: the chips sit high in the row so the gap under
    // them is bigger than the one above and the bar does not feel cramped
    // against the canvas below it.
    const auto font = chipFont();
    const int y = chipTopGap;

    int x = chipGap;
    for (auto& chip : chips)
    {
        const int w = static_cast<int>(font.getStringWidthFloat(chip.label))
                    + chipPadding * 2;
        chip.bounds = { x, y, w, chipHeight };
        x += w + chipGap;
    }

    setSize(juce::jmax(1, x), juce::jmax(getHeight(), chipRowHeight));
}

int ModuleIconBar::ChipStrip::chipIndexAt(juce::Point<int> p) const
{
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i].bounds.contains(p))
            return static_cast<int>(i);
    return -1;
}

void ModuleIconBar::ChipStrip::paint(juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    g.fillAll(pal.backgroundPanel);
    g.setFont(chipFont());

    for (size_t i = 0; i < chips.size(); ++i)
    {
        const auto& chip = chips[i];
        const bool hovered = (static_cast<int>(i) == hoverIndex);
        const auto r = chip.bounds.toFloat();

        // A different fill from the tabs above: both being backgroundSecondary
        // made the row of modules read as a second row of tabs.
        g.setColour(hovered ? pal.buttonHover : pal.buttonBackground);
        g.fillRect(r);
        g.setColour(hovered ? pal.accentActive : pal.borderColor);
        g.drawRect(r, 1.0f);
        g.setColour(hovered ? pal.textPrimary : pal.textSecondary);
        g.drawText(chip.label, chip.bounds, juce::Justification::centred, false);
    }
}

// JUCE takes a snapshot of the source component when no image is given, and the
// source here is the whole strip, so the pointer ended up carrying every chip in
// the category at once. Draw the one chip instead.
juce::Image ModuleIconBar::ChipStrip::renderChipImage(const Chip& chip) const
{
    const auto& pal = AppTheme::palette();
    juce::Image image(juce::Image::ARGB, chip.bounds.getWidth(), chip.bounds.getHeight(), true);
    juce::Graphics g(image);

    const auto r = image.getBounds().toFloat().reduced(0.5f);
    g.setColour(pal.backgroundElevated);
    g.fillRect(r);
    g.setColour(pal.accentActive);
    g.drawRect(r, 1.0f);
    g.setColour(pal.textPrimary);
    g.setFont(chipFont());
    g.drawText(chip.label, image.getBounds(), juce::Justification::centred, false);

    return image;
}

void ModuleIconBar::ChipStrip::mouseMove(const juce::MouseEvent& e)
{
    const int found = chipIndexAt(e.getPosition());
    if (found == hoverIndex)
        return;

    hoverIndex = found;
    repaint();
}

void ModuleIconBar::ChipStrip::mouseExit(const juce::MouseEvent&)
{
    if (hoverIndex == -1)
        return;
    hoverIndex = -1;
    repaint();
}

void ModuleIconBar::ChipStrip::mouseDown(const juce::MouseEvent&)
{
    dragStarted = false;
}

void ModuleIconBar::ChipStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (dragStarted || e.getDistanceFromDragStart() < 4)
        return;

    const int index = chipIndexAt(e.getMouseDownPosition());
    if (index < 0)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (container == nullptr)
        return;

    const auto& chip = chips[static_cast<size_t>(index)];

    // The same payload the module tree sends, so the canvas accepts both
    // without knowing which one the drag came from. The ANME chips have no
    // descriptor, so they name themselves instead.
    auto description = new juce::DynamicObject();
    if (chip.descriptor == nullptr)
    {
        description->setProperty("type", "comment");
    }
    else
    {
        description->setProperty("type", "module");
        description->setProperty("descriptorPtr", reinterpret_cast<juce::int64>(chip.descriptor));
        description->setProperty("typeId", chip.descriptor->index);
        description->setProperty("name", chip.descriptor->name);
    }

    // Hold the chip where it was grabbed, so it does not jump under the pointer.
    const auto grabOffset = chip.bounds.getPosition() - e.getMouseDownPosition();

    dragStarted = true;
    container->startDragging(juce::var(description), this,
                             juce::ScaledImage(renderChipImage(chip)),
                             true, &grabOffset);
}

void ModuleIconBar::ChipStrip::mouseUp(const juce::MouseEvent& e)
{
    if (dragStarted || e.mouseWasDraggedSinceMouseDown())
        return;

    // A plain click hands the module to the pointer, the way Add Module does
    // since 0.14.0: the click that follows chooses the spot and the area.
    const int index = chipIndexAt(e.getPosition());
    if (index < 0)
        return;

    const auto& chip = chips[static_cast<size_t>(index)];
    if (chip.descriptor == nullptr)
        PatchCanvas::beginAddCommentDrop();
    else
        PatchCanvas::beginAddModuleDrop(chip.descriptor->index, chip.descriptor->name);
}
