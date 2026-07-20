#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SlotWindowContent.h"

// Pop-out window showing one fixed slot's patch in its own canvas, in
// addition to the main window's A/B/C/D tabs — lets 2+ slots be viewed and
// edited side by side. One per slot, lazily created, never destroyed once
// created (mirrors KnobFloaterWindow: setVisible(false) on close, reused on
// next open).
class SlotWindow : public juce::DocumentWindow
{
public:
    explicit SlotWindow(int slot);

    int getSlot() const { return slot_; }

    SlotWindowContent& getContent() { return content; }

    void closeButtonPressed() override;

    std::function<void()> onClosed;

    // App-wide shortcuts (Ctrl+1..8) forwarded while this window has focus,
    // same pattern as the other floater windows.
    std::function<bool(const juce::KeyPress&)> onGlobalKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        return (onGlobalKey && onGlobalKey(key)) || juce::DocumentWindow::keyPressed(key);
    }

private:
    int slot_;
    SlotWindowContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotWindow)
};
