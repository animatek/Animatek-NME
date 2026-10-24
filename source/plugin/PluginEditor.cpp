#include "PluginEditor.h"

NmePluginEditor::NmePluginEditor(NmePluginProcessor& processor)
    : AudioProcessorEditor(processor),
      nmeProcessor(processor)
{
    mainComponent = std::make_unique<MainComponent>(nmeProcessor.getAppProperties());
    addAndMakeVisible(mainComponent.get());

    // Project recall (#94): the patches the project was saved with come back into the editor,
    // and the project is saved with what the editor holds. The restore waits a moment for the
    // editor to finish setting itself up; a project loading while the window is open (the
    // host calls the restore hook) goes in at once.
    juce::Component::SafePointer<MainComponent> main(mainComponent.get());
    juce::Component::SafePointer<NmePluginEditor> self(this);
    // While the restore is pending the editor is still empty: reading it then would replace
    // the project's state with nothing, so it answers "unknown" (an empty block) instead.
    nmeProcessor.setSessionHooks(
        [main, self]() { return main != nullptr && self != nullptr && !self->restorePending
                                    ? main->captureSessionState() : juce::MemoryBlock(); },
        [main](const juce::MemoryBlock& block) { if (main != nullptr) main->restoreSessionState(block); });
    if (nmeProcessor.getSessionBlock().getSize() > 0)
    {
        restorePending = true;
        juce::Timer::callAfterDelay(500, [main, self, block = nmeProcessor.getSessionBlock()]()
        {
            if (self != nullptr)
                self->restorePending = false;
            if (main != nullptr)
                main->restoreSessionState(block);
        });
    }

    // Default plugin window size
    setSize(1280, 800);
    setResizable(true, true);
}

NmePluginEditor::~NmePluginEditor()
{
    // What the editor holds is kept for the project before it goes: the host may save after
    // the window is closed, and a window opened again starts from it.
    if (!restorePending)
        nmeProcessor.keepSessionBlock(mainComponent->captureSessionState());
    nmeProcessor.clearSessionHooks();

    // Destroy MainComponent before the editor's Component base class destructor
    // runs — prevents dangling child-component and async callback crashes.
    mainComponent.reset();
}

void NmePluginEditor::resized()
{
    mainComponent->setBounds(getLocalBounds());
}
