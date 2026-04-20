#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../MainComponent.h"

class NomadPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit NomadPluginEditor(NomadPluginProcessor& processor);
    ~NomadPluginEditor() override;

    void resized() override;

private:
    NomadPluginProcessor& nomadProcessor;
    std::unique_ptr<MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NomadPluginEditor)
};
