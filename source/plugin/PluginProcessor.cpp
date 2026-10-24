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

void NmePluginProcessor::setSessionHooks(std::function<juce::MemoryBlock()> capture,
                                         std::function<void(const juce::MemoryBlock&)> restore)
{
    captureHook = std::move(capture);
    restoreHook = std::move(restore);
}

void NmePluginProcessor::clearSessionHooks()
{
    captureHook = nullptr;
    restoreHook = nullptr;
}

void NmePluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // The editor's patches live in components, which only the message thread may read: a host
    // that asks from another thread gets the last state seen instead of a race.
    if (captureHook && juce::MessageManager::getInstance()->isThisTheMessageThread())
        if (auto block = captureHook(); block.getSize() > 0)   // empty = the editor cannot say yet
            sessionBlock = std::move(block);
    destData = sessionBlock;
}

void NmePluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    sessionBlock.replaceAll(data, static_cast<size_t>(sizeInBytes));
    // An editor already open takes it now; one opened later reads it as it starts.
    if (restoreHook)
        restoreHook(sessionBlock);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NmePluginProcessor();
}
