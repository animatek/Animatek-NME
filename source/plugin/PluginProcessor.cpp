#include "PluginProcessor.h"
#include "PluginEditor.h"

NomadPluginProcessor::NomadPluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))  // Stereo out so DAWs see it as an instrument
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Nomad2026";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters(options);
}

void NomadPluginProcessor::prepareToPlay(double, int) {}
void NomadPluginProcessor::releaseResources() {}

void NomadPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    buffer.clear();
    // Pass MIDI through unchanged — the editor handles SysEx via ConnectionManager
    juce::ignoreUnused(midiMessages);
}

juce::AudioProcessorEditor* NomadPluginProcessor::createEditor()
{
    return new NomadPluginEditor(*this);
}

void NomadPluginProcessor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
    // TODO: serialize current patch state for DAW session recall
}

void NomadPluginProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
    // TODO: restore patch state from DAW session
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NomadPluginProcessor();
}
