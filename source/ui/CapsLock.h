#pragma once

// Whether Caps Lock is on right now. JUCE's ModifierKeys does not carry it, so
// this asks the system: GetKeyState on Windows, the event source flags on
// macOS, and the keyboard LEDs under /sys/class/leds on Linux (which follow the
// lock on X11 and Wayland alike). False where none of them answers.
namespace CapsLock
{
    bool isOn();
}
