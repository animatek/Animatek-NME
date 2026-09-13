#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include "FlatCloseButton.h"
#include "SelfOwnedDialog.h"
#include "../model/SlotSet.h"

// File > Save Slot Set: a name, an optional note, and for each of the four
// slots whether the set takes its patch, switches the slot off, or leaves it
// alone. Self-owned, like the slot chooser it is modelled on.
class SaveSlotSetDialog : public SelfOwnedDialog
{
public:
    struct SlotInfo
    {
        juce::String patchName;
        bool hasPatch = false;
        bool enabledKnown = false;   // the synth has told us its enable mask
        bool enabled = false;
    };

    struct Result
    {
        bool confirmed = false;
        juce::String name;
        juce::String notes;
        std::array<SlotSet::Mode, 4> modes { SlotSet::Mode::Keep, SlotSet::Mode::Keep,
                                             SlotSet::Mode::Keep, SlotSet::Mode::Keep };
    };

    using Callback = std::function<void(const Result&)>;

    SaveSlotSetDialog(const juce::String& suggestedName,
                      const std::array<SlotInfo, 4>& slots,
                      Callback cb);

    void paint      (juce::Graphics& g) override;
    void resized    () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    static void show(juce::Component* parent,
                     const juce::String& suggestedName,
                     const std::array<SlotInfo, 4>& slots,
                     Callback cb);

private:
    enum { kInclude = 1, kDisable = 2, kKeep = 3 };

    void updateSaveButton();
    void confirm();
    void cancel();

    Callback callback;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    juce::Label nameLabel { {}, "Name" };
    juce::TextEditor nameEditor;
    std::array<juce::Label, 4> slotLabels;
    std::array<juce::ComboBox, 4> slotModes;
    juce::Label notesLabel { {}, "Notes" };
    juce::TextEditor notesEditor;

    juce::TextButton saveButton   { "Save" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveSlotSetDialog)
};
