#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// About box: what version is running, who built it on whose work, and who
// keeps reporting the bugs. The version block doubles as bug-report material,
// which is why it can be copied to the clipboard in one click.
class AboutDialog : public juce::Component
{
public:
    using UrlCallback = std::function<void(const juce::String&)>;

    explicit AboutDialog(UrlCallback openUrl);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Launches it as a modal dialog window, styled like the other dialogs.
    // Returns the dialog window, so a caller can watch for it closing.
    static juce::Component* show(juce::Component* parent, UrlCallback openUrl);

    // "Animatek NME 0.11.0 (Linux, JUCE 8.0.12)" — also used by the copy button.
    static juce::String versionSummary();

private:
    void copyVersionToClipboard();

    UrlCallback openUrl_;

    juce::Image icon;
    juce::TextEditor credits;

    juce::TextButton websiteButton { "Website" };
    juce::TextButton patreonButton { "Patreon" };
    juce::TextButton githubButton  { "GitHub" };
    juce::TextButton copyButton    { "Copy version" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutDialog)
};
