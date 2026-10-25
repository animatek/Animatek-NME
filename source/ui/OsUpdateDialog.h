#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SelfOwnedDialog.h"
#include "FlatCloseButton.h"

class ConnectionManager;
class SynthHub;

// Device > Update Synth OS: installs an OS image into a G1's flash, as Clavia's
// updater does (midi/OsUpdate.h), on a real G1 or on G1-Emu. Laid out like
// Editor Options: the pages on the left, the page on the right.
//   Warning      the risk, said plainly; Install waits until it is acknowledged
//   Install OS   the image, the synth it goes to, the speed, and the transfer
//   Update mode  how a synth gets ready for it, and whether this one is
//   Recovery     the ways back when an OS does not start
class OsUpdateDialog : public SelfOwnedDialog, private juce::Timer
{
public:
    explicit OsUpdateDialog(SynthHub& hub);
    ~OsUpdateDialog() override;

    static void show(juce::Component* parent, SynthHub& hub);

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    enum class Phase { Idle, Sending, WaitingForAnswer, Accepted, NoAnswer };

    void timerCallback() override;
    void showPage(int newPage);
    juce::Rectangle<int> pageButtonBounds(int index) const;
    static const char* pageName(int page);
    void chooseImage();
    void loadImage(const juce::File& file);
    void refresh();
    void start();
    void stop(const juce::String& why);
    void sendNext();
    void close();
    juce::String synthLine() const;
    juce::String synthState(const ConnectionManager& c) const;
    void fillSynths();
    ConnectionManager* target();
    const ConnectionManager* target() const;
    int intervalMs() const;
    static juce::File emulatorOsFolder();

    SynthHub& hub;
    int targetSynth = -1;         // the synth the OS goes to, -1 none
    bool targetChosen = false;    // the user picked it: no more following update mode

    static constexpr int dialogWidth = 820;
    static constexpr int dialogHeight = 560;
    static constexpr int titleBarH = 40;
    static constexpr int sidebarW = 200;
    static constexpr int footerH = 68;
    static constexpr int numPages = 4;
    int page = 0;

    juce::ComponentDragger dragger;
    bool draggingWindow = false;
    FlatCloseButton closeButton;

    // Warning
    juce::ToggleButton riskToggle { "I have read this and I take the risk" };

    // Install OS
    juce::TextButton browseButton { "Choose..." };
    juce::TextButton checkButton  { "Check again" };
    juce::ComboBox   synthCombo;
    juce::StringArray synthItems;   // what synthCombo shows, to refill it only on a change
    juce::ComboBox   speedCombo;
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };
    juce::Label      statusLabel;
    juce::TextButton installButton { "Install" };
    juce::TextButton closeFooterButton { "Close" };

    juce::File imageFile;
    std::vector<uint8_t> image;
    juce::String imageProblem;
    std::vector<std::vector<uint8_t>> messages;
    size_t nextMessage = 0;
    Phase phase = Phase::Idle;
    juce::uint32 phaseStartMs = 0;
    juce::uint32 lastSendMs = 0;
    bool speedChosen = false;     // the user picked one: no more guessing from the port
    juce::String stoppedNote;     // why the last transfer stopped, until another starts
    int ticks = 0;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OsUpdateDialog)
};
