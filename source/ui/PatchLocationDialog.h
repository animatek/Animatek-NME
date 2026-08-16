#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SelfOwnedDialog.h"
#include <vector>
#include <string>
#include <functional>
#include "FlatCloseButton.h"

class PatchLocationDialog : public SelfOwnedDialog
{
public:
    struct Result
    {
        int  slot      = 0;
        int  section   = 0;
        int  position  = 0;
        bool confirmed = false;
    };

    using Callback = std::function<void(const Result&)>;

    // initialSection/initialPosition preselect a bank location (-1 = bank 1,
    // position 1), so Store to Bank opens on the patch's own place instead of
    // making you find it again every time.
    PatchLocationDialog(const juce::String& title,
                        const std::vector<std::string>& patchList,
                        bool showSlot,
                        int  currentSlot,
                        Callback cb,
                        int  initialSection = -1,
                        int  initialPosition = -1);

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    static void show(juce::Component* parent,
                     const juce::String& title,
                     const std::vector<std::string>& patchList,
                     bool showSlot,
                     int  currentSlot,
                     Callback cb,
                     int  initialSection = -1,
                     int  initialPosition = -1);

private:
    void updatePositionItems();
    void confirm();
    void cancel();
    void close();

    juce::String title_;
    const std::vector<std::string>& patchList_;
    bool showSlot_;
    Callback callback;
    int initialPosition_ = -1;  // consumed by the first updatePositionItems()
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    juce::Label    slotLabel     { {}, "SLOT" };
    juce::ComboBox slotCombo;
    juce::Label    bankLabel     { {}, "BANK" };
    juce::ComboBox bankCombo;
    juce::Label    positionLabel { {}, "POSITION" };
    juce::ComboBox positionCombo;

    juce::TextButton okButton     { "OK" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchLocationDialog)
};
