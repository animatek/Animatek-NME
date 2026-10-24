#pragma once

#include <juce_graphics/juce_graphics.h>

// The colours of the Themes window. It keeps its own instead of following the theme: while a theme is being
// edited the palette moves under the mouse, and a window that changes colour as you drag its swatches is not
// one you can read. They are the new look's dark ground and amber accent.
namespace ThemesLook
{
const juce::Colour ground   { 0xff1b1f26 };
const juce::Colour panel    { 0xff222730 };
const juce::Colour field    { 0xff15181e };
const juce::Colour border   { 0xff353c47 };
const juce::Colour text     { 0xffe4e8ee };
const juce::Colour muted    { 0xff7f8896 };
const juce::Colour accent   { 0xffe2b765 };
const juce::Colour button   { 0xff2d343f };
}
