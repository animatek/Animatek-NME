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
    // juce::String has no char constructor — String(char) silently picks the
    // int overload and prints the ASCII code. Always charToString.
    auto letter = juce::String::charToString(static_cast<char>('A' + slot_));
    setName(patchName.isEmpty() ? "Slot " + letter
                                : "Slot " + letter + " - " + patchName);
}
