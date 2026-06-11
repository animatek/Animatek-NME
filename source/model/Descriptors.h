#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "SignalType.h"
#include <vector>

struct ParameterDescriptor
{
    juce::String name;
    juce::String componentId;
    int index = 0;
    int minValue = 0;
    int maxValue = 127;
    int defaultValue = 0;
    juce::String paramClass;   // "parameter", "morph", "custom"
    juce::String formatter;
    juce::String extension;    // linked morph parameter component-id
    juce::String role;
};

struct ConnectorDescriptor
{
    juce::String name;
    juce::String componentId;
    int index = 0;
    bool isOutput = false;
    SignalType signalType = SignalType::None;
};

struct LightDescriptor
{
    juce::String name;
    juce::String componentId;
    int index = 0;
    enum Type { Led, LedArray, Meter };
    Type type = Meter;
    int minValue = 0;
    int maxValue = 127;
};

struct ModuleDescriptor
{
    juce::String name;
    juce::String fullname;
    juce::String category;
    juce::String componentId;
    juce::String tags;        // lowercase search synonyms (see ModuleTags.cpp)
    int index = 0;
    double cycles = 0;
    double progMem = 0;
    double xMem = 0;
    double yMem = 0;
    double dynMem = 0;
    int height = 2;
    int limit = -1;           // -1 = unlimited
    bool instantiable = true;
    juce::Colour background { 0xff888888 };

    std::vector<ParameterDescriptor> parameters;
    std::vector<ConnectorDescriptor> connectors;
    std::vector<LightDescriptor> lights;
};
