#include "PluginProcessor.h"
#include "PluginEditor.h"

NmePluginProcessor::NmePluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))  // Stereo out so DAWs see it as an instrument
{
    juce::PropertiesFile::Options options;
    options.applicationName = "AnimatekNME";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters(options);
}

void NmePluginProcessor::prepareToPlay(double, int) {}
void NmePluginProcessor::releaseResources() {}

void NmePluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    buffer.clear();
    // Pass MIDI through unchanged — the editor handles SysEx via ConnectionManager
    juce::ignoreUnused(midiMessages);
}

juce::AudioProcessorEditor* NmePluginProcessor::createEditor()
{
    return new NmePluginEditor(*this);
}

void NmePluginProcessor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
    // TODO: serialize current patch state for DAW session recall
}

void NmePluginProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
    // TODO: restore patch state from DAW session
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NmePluginProcessor();
}
