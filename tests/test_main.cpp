#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <juce_events/juce_events.h>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    return doctest::Context(argc, argv).run();
}
