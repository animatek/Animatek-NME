#include "PluginEditor.h"

NomadPluginEditor::NomadPluginEditor(NomadPluginProcessor& processor)
    : AudioProcessorEditor(processor),
      nomadProcessor(processor)
{
    mainComponent = std::make_unique<MainComponent>(nomadProcessor.getAppProperties());
    addAndMakeVisible(mainComponent.get());

    // Default plugin window size
    setSize(1280, 800);
    setResizable(true, true);
}

NomadPluginEditor::~NomadPluginEditor()
{
    // Destroy MainComponent before the editor's Component base class destructor
    // runs — prevents dangling child-component and async callback crashes.
    mainComponent.reset();
}

void NomadPluginEditor::resized()
{
    mainComponent->setBounds(getLocalBounds());
}
