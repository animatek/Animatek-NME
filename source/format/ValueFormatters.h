#pragma once

#include <juce_core/juce_core.h>

#include "../model/Descriptors.h"

class Module;

// Port of nmedit/libs/nordmodular/data/module-descriptions/nmformat.js
//
// Single entry point: format(formatterName, value) — dispatches by name to
// the 63 fmtXxx functions defined by the original Nomad editor. Formatter
// names come from ParameterDescriptor::formatter (parsed from modules.xml).
//
// Special non-function expressions from modules.xml are also recognized:
//   "value+1"   → value + 1
//   "value-64"  → value - 64
//
// "fmtSeqInterval" is ours rather than the original's: it is the note reading of
// a NoteSeqA step, offered alongside the number the original shows.
//
// Unknown names fall back to String(value).
namespace ValueFormatters
{
    juce::String format (const juce::String& formatterName, int value);

    // Some parameters can be read two ways, and which one the editor shows is a
    // setting rather than a property of the patch (issue #76): the note
    // sequencers' steps read either as the numbers the original editor shows or
    // as note names. It is editor-wide, so it lives here rather than being
    // carried separately to every surface that draws a value.
    void setPreferNoteNames (bool on);
    bool preferNoteNames();

    // What this parameter reads as right now: its own formatter, or its
    // alternative one when there is one and the setting above asks for it.
    juce::String format (const ParameterDescriptor& pd, int value);

    // How the parameter reads inside its module, where another of the module's
    // parameters can change it: the Constant's unipolar switch makes its value
    // read 0 to 64 (issue #122). The same descriptor otherwise. `m` may be null.
    ParameterDescriptor inModule (const Module* m, const ParameterDescriptor& pd);

    // Where a flat knob's value arc starts, 0 to 1 along its travel: the zero of a
    // signed reading, the middle of a centred one, else the minimum (issue #123).
    float arcOrigin (const ParameterDescriptor& pd, const Module* m = nullptr);
}
