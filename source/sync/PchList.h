#pragma once

#include <juce_core/juce_core.h>
#include <array>

// The .pchList file Clavia's editor writes beside a saved bank and reads back to
// send it: plain text, CRLF, the patch files of one bank in position order.
//   - a line that is a number (1-99) sets the position of the next file;
//   - a line ending in ".pch" is a file name, relative to the list's folder,
//     for the current position, which then moves on by one;
//   - empty lines are skipped (they are not empty positions: a gap is a number).
// The original writes "1 " as the first line. Read in NME303.exe at 0x00437e30.
namespace PchList
{
    constexpr int positions = 99;
    using Entries = std::array<juce::String, positions>;   // file name per position, empty = none

    // Text of a list for these entries, in the original's layout.
    juce::String write(const Entries& entries);

    // Entries of a list. Returns false, with a reason, for a list the original
    // would also refuse ("List file corrupt": a position past 99, or a line
    // that is neither a number nor a .pch name).
    bool parse(const juce::String& text, Entries& entries, juce::String& error);

    // The lists in a folder of .pch files.
    juce::Array<juce::File> findIn(const juce::File& folder);
}
