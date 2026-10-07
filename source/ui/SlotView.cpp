#include "SlotView.h"
#include "../midi/SynthHub.h"
#include "AppTheme.h"

SlotView::SlotView(int slot)
    : slot_(slot)
{
    addAndMakeVisible(canvas);
    setPatchTitle({});
}

void SlotView::resized()
{
    canvas.setBounds(getLocalBounds());
}

void SlotView::paintOverChildren(juce::Graphics& g)
{
    if (!dropArmed_)
        return;

    // Over the canvas rather than behind it: the canvas fills this component,
    // so anything painted underneath would never be seen.
    g.setColour(AppTheme::palette().accentActive);
    g.drawRect(getLocalBounds(), 3);
}

bool SlotView::isInterestedInDragSource(const SourceDetails& details)
{
    return SlotDrop::isAccepted(details.description);
}

void SlotView::itemDragEnter(const SourceDetails&)
{
    dropArmed_ = true;
    repaint();
}

void SlotView::itemDragExit(const SourceDetails&)
{
    dropArmed_ = false;
    repaint();
}

void SlotView::itemDropped(const SourceDetails& details)
{
    dropArmed_ = false;
    repaint();

    const auto& d = details.description;

    if (SlotDrop::isSynthPatch(d) && onPatchDropped)
        onPatchDropped((int) d.getProperty("section", -1),
                       (int) d.getProperty("position", -1),
                       slot_);
    else if (SlotDrop::isPatchFile(d) && onPatchFileDropped)
        onPatchFileDropped(SlotDrop::fileOf(d), slot_);
}

void SlotView::setPatchTitle(const juce::String& patchName)
{
    if (patchName_ == patchName)
        return;
    patchName_ = patchName;
    refreshTitle();
}

void SlotView::setLocal(bool isLocal)
{
    if (local_ == isLocal)
        return;
    local_ = isLocal;
    refreshTitle();
}

void SlotView::setSynthName(const juce::String& synthName)
{
    if (synthName_ == synthName)
        return;
    synthName_ = synthName;
    refreshTitle();
}

void SlotView::refreshTitle()
{
    // "Slot A - Bella - animatek": with up to four synths there are sixteen windows,
    // so each one says whose slot it is. Before the synth has said its name, the
    // port stands in for it.
    // juce::String has no char constructor — String(char) silently picks the
    // int overload and prints the ASCII code. Always charToString.
    auto title = "Slot " + juce::String::charToString(static_cast<char>('A' + SynthSlot::localOf(slot_)));
    if (patchName_.isNotEmpty())
        title += " - " + patchName_;
    if (synthName_.isNotEmpty())
        title += " - " + synthName_;
    else if (kMaxSynths > 1)
        title += " - Port " + juce::String(SynthSlot::synthOf(slot_) + 1);
    if (local_)
        title += "  [LOCAL]";
    setName(title);
}
