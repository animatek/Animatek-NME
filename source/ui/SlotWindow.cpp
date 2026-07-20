#include "SlotWindow.h"
#include "AppTheme.h"

SlotWindow::SlotWindow(int slot)
    : juce::DocumentWindow("Slot " + juce::String::charToString(static_cast<char>('A' + (slot & 0x03))),
                          AppTheme::palette().backgroundMain,
                          juce::DocumentWindow::allButtons)
    , slot_(slot)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setContentNonOwned(&content, true);
    centreWithSize(900, 700);
}

void SlotWindow::closeButtonPressed()
{
    setVisible(false);
    if (onClosed) onClosed();
}
