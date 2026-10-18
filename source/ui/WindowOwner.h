#pragma once

// NME's dialogs and floaters are top-level windows of their own, and JUCE does not tie them to
// the main window: a click on the main window raised it over them, and a dialog that had just
// opened (the slot chooser the patch browser brings up) went behind, so NME looked stuck (#98).
// Making the main window their owner lets the system keep them above it: an owned window on
// Windows, a transient one (WM_TRANSIENT_FOR) on Linux. Not on macOS, where it returns false.
namespace WindowOwner
{
    // Both are native handles, as juce::ComponentPeer::getNativeHandle() gives them.
    bool setOwner (void* window, void* owner);
}
