#include "PluginEditor.h"

NmePluginEditor::NmePluginEditor(NmePluginProcessor& processor)
    : AudioProcessorEditor(processor),
      nmeProcessor(processor)
{
    mainComponent = std::make_unique<MainComponent>(nmeProcessor.getAppProperties());
    addAndMakeVisible(mainComponent.get());

    // Default plugin window size
    setSize(1280, 800);
    setResizable(true, true);
}

NmePluginEditor::~NmePluginEditor()
{
    // Destroy MainComponent before the editor's Component base class destructor
    // runs — prevents dangling child-component and async callback crashes.
    mainComponent.reset();
}

void NmePluginEditor::resized()
{
    mainComponent->setBounds(getLocalBounds());
}
