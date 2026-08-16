#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include "FlatCloseButton.h"
#include "SelfOwnedDialog.h"

// Slot chooser shown when opening a .pch (issue #21). Lists the four slots
// A/B/C/D with the name of the patch currently loaded in each, so you can see
// what you are about to overwrite, plus a separate "Local" option that loads
// the patch into the editor only without uploading anything to the synth.
// Cancel aborts the load. Self-owned: it lives on the desktop and takes itself
// down when dismissed, which SelfOwnedDialog makes safe to do from a button
// callback and survivable when the editor quits with it still open.
class SlotSelectDialog : public SelfOwnedDialog
{
public:
    struct Result
    {
        int  slot      = 0;      // 0-3 destination slot/tab; == currentSlot when local
        bool local     = false;  // true = editor-only, do not upload to the synth
        bool confirmed = false;
    };

    using Callback = std::function<void(const Result&)>;

    SlotSelectDialog(const juce::String& title,
                     const std::array<juce::String, 4>& slotNames,
                     int  currentSlot,
                     Callback cb);

    void paint      (juce::Graphics& g) override;
    void resized    () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    static void show(juce::Component* parent,
                     const juce::String& title,
                     const std::array<juce::String, 4>& slotNames,
                     int  currentSlot,
                     Callback cb);

private:
    void confirm();
    void cancel();
    void close();

    juce::String title_;
    int  currentSlot_;
    Callback callback;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    std::array<juce::ToggleButton, 4> slotButtons;
    juce::ToggleButton localButton { "Local (editor only, do not upload)" };

    juce::TextButton okButton     { "OK" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotSelectDialog)
};
