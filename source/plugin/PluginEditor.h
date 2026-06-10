#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../MainComponent.h"

class NmePluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit NmePluginEditor(NmePluginProcessor& processor);
    ~NmePluginEditor() override;

    void resized() override;

private:
    NmePluginProcessor& nmeProcessor;
    std::unique_ptr<MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NmePluginEditor)
};
