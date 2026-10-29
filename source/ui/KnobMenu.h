#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../protocol/KnobAssignmentMessage.h"

// The "Knob" submenu of a parameter's or a morph's context menu: one entry per front-panel knob,
// ids 100 + the wire index, grouped the way the panel is (6, 6, 3, 3) and then the pedal,
// after touch and on/off switch.
//
// A Micro Modular has three knobs and no pedals, so with one as the synth being edited the list is
// those three and after touch (it arrives over MIDI). Whatever knob the target already has stays
// in the list, ticked, so an assignment made on a G1 can still be seen and changed (issue #111).
struct KnobMenu
{
    inline static bool microModel = false;

    static void fill(juce::PopupMenu& menu, const Patch* patch, int currentKnob)
    {
        auto add = [&](int k, const juce::String& name)
        {
            juce::String label = name;
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            menu.addItem(100 + k, label, true, k == currentKnob);
        };
        auto addKnobs = [&](int from, int to)
        {
            for (int k = from; k < to; ++k)
                add(k, "Knob " + juce::String(k + 1));
        };

        if (!microModel)
        {
            addKnobs(0, 6);   menu.addSeparator();
            addKnobs(6, 12);  menu.addSeparator();
            addKnobs(12, 15); menu.addSeparator();
            addKnobs(15, 18); menu.addSeparator();
            for (int k : { 19, 20, 22 })
                add(k, KnobAssignmentMessage::getKnobName(k));
            return;
        }

        addKnobs(0, 3);
        const bool elsewhere = currentKnob >= 3 && currentKnob != 20;
        if (elsewhere)
        {
            menu.addSeparator();
            add(currentKnob, KnobAssignmentMessage::getKnobName(currentKnob));
        }
        menu.addSeparator();
        add(20, KnobAssignmentMessage::getKnobName(20));
    }
};
