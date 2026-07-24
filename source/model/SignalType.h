#pragma once

#include <juce_graphics/juce_graphics.h>

enum class SignalType
{
    Audio       = 0,
    Control     = 1,
    Logic       = 2,
    MasterSlave = 3,
    User1       = 4,
    User2       = 5,
    None        = 6
};

inline juce::Colour getSignalColour(SignalType type)
{
    switch (type)
    {
        case SignalType::Audio:       return juce::Colour(0xffCB4F4F);
        case SignalType::Control:     return juce::Colour(0xff5A5FB3);
        case SignalType::Logic:       return juce::Colour(0xffE5DE45);
        case SignalType::MasterSlave: return juce::Colour(0xffA8A8A8);
        case SignalType::User1:       return juce::Colour(0xff9AC899);
        case SignalType::User2:       return juce::Colour(0xffBB00D7);
        case SignalType::None:
        default:                      return juce::Colours::white;
    }
}

inline SignalType signalTypeFromString(const juce::String& s)
{
    if (s == "audio")        return SignalType::Audio;
    if (s == "control")      return SignalType::Control;
    if (s == "logic")        return SignalType::Logic;
    if (s == "master-slave") return SignalType::MasterSlave;
    if (s == "user1")        return SignalType::User1;
    if (s == "user2")        return SignalType::User2;
    return SignalType::None;
}
