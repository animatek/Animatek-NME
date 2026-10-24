#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

class NmePluginProcessor : public juce::AudioProcessor
{
public:
    NmePluginProcessor();
    ~NmePluginProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Animatek NME"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::ApplicationProperties& getAppProperties() { return appProperties; }

    // DAW project recall (#94). The editor, while it is open, says how to read the editor's
    // state and how to put one back; when it is closed the last state it left is kept, so
    // closing the window does not lose what the project is to save.
    void setSessionHooks(std::function<juce::MemoryBlock()> capture,
                         std::function<void(const juce::MemoryBlock&)> restore);
    void clearSessionHooks();
    const juce::MemoryBlock& getSessionBlock() const { return sessionBlock; }
    void keepSessionBlock(const juce::MemoryBlock& block) { sessionBlock = block; }

private:
    juce::ApplicationProperties appProperties;
    juce::MemoryBlock sessionBlock;
    std::function<juce::MemoryBlock()> captureHook;
    std::function<void(const juce::MemoryBlock&)> restoreHook;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NmePluginProcessor)
};
