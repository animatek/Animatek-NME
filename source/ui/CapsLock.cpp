#include "CapsLock.h"

#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #include <windows.h>
#elif JUCE_MAC
 #include <CoreGraphics/CoreGraphics.h>
#endif

bool CapsLock::isOn()
{
   #if JUCE_WINDOWS
    return (GetKeyState(VK_CAPITAL) & 1) != 0;
   #elif JUCE_MAC
    return (CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState)
            & kCGEventFlagMaskAlphaShift) != 0;
   #elif JUCE_LINUX || JUCE_BSD
    // One LED per keyboard (input2::capslock, input8::capslock…); the lock is
    // shared, so any of them lit means it is on.
    for (const auto& entry : juce::RangedDirectoryIterator(juce::File("/sys/class/leds"),
                                                          false, "*capslock",
                                                          juce::File::findDirectories))
    {
        auto brightness = entry.getFile().getChildFile("brightness");
        if (brightness.loadFileAsString().trim().getIntValue() > 0)
            return true;
    }
    return false;
   #else
    return false;
   #endif
}
