#include "SlotView.h"

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

void SlotView::refreshTitle()
{
    // juce::String has no char constructor — String(char) silently picks the
    // int overload and prints the ASCII code. Always charToString.
    auto title = "Slot " + juce::String::charToString(static_cast<char>('A' + slot_));
    if (patchName_.isNotEmpty())
        title += " - " + patchName_;
    if (local_)
        title += "  [LOCAL]";
    setName(title);
}
