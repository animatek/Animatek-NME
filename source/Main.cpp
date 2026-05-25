#include "MainComponent.h"
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>

class NomadApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Nomad2026"; }
    const juce::String getApplicationVersion() override { return "0.5.6"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise(const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Nomad2026";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters(options);

        applyDarkTheme();
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), appProperties);
    }

    static void applyDarkTheme()
    {
        auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();

        // PopupMenu / ComboBox dropdowns / context menus / main menu bar
        lf.setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff14142a));
        lf.setColour (juce::PopupMenu::textColourId,                  juce::Colour (0xffcccccc));
        lf.setColour (juce::PopupMenu::headerTextColourId,            juce::Colour (0xffffcc44));
        lf.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff353560));
        lf.setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colour (0xffffcc44));

        // ComboBox widget itself
        lf.setColour (juce::ComboBox::backgroundColourId,             juce::Colour (0xff22223a));
        lf.setColour (juce::ComboBox::outlineColourId,                juce::Colour (0xff3a3a5a));
        lf.setColour (juce::ComboBox::textColourId,                   juce::Colour (0xffcccccc));
        lf.setColour (juce::ComboBox::arrowColourId,                  juce::Colour (0xffffaa44));
        lf.setColour (juce::ComboBox::focusedOutlineColourId,         juce::Colour (0xffffcc44));

        // TextButton defaults
        lf.setColour (juce::TextButton::buttonColourId,               juce::Colour (0xff252540));
        lf.setColour (juce::TextButton::buttonOnColourId,             juce::Colour (0xff353560));
        lf.setColour (juce::TextButton::textColourOffId,              juce::Colour (0xffcccccc));
        lf.setColour (juce::TextButton::textColourOnId,               juce::Colour (0xffffcc44));

        // ScrollBar
        lf.setColour (juce::ScrollBar::thumbColourId,                 juce::Colour (0xff353560));
        lf.setColour (juce::ScrollBar::trackColourId,                 juce::Colour (0xff14142a));

        // ToggleButton
        lf.setColour (juce::ToggleButton::textColourId,               juce::Colour (0xffcccccc));
        lf.setColour (juce::ToggleButton::tickColourId,               juce::Colour (0xffffaa44));
        lf.setColour (juce::ToggleButton::tickDisabledColourId,       juce::Colour (0xff888899));

        // AlertWindow
        lf.setColour (juce::AlertWindow::backgroundColourId,          juce::Colour (0xff14142a));
        lf.setColour (juce::AlertWindow::textColourId,                juce::Colour (0xffcccccc));
        lf.setColour (juce::AlertWindow::outlineColourId,             juce::Colour (0xff333355));
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name, juce::ApplicationProperties& props)
            : DocumentWindow(name, juce::Colour(0xff1a1a2e), allButtons)
        {
            setUsingNativeTitleBar(true);
            applyWindowIcon();
            setContentOwned(new MainComponent(props), true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            applyWindowIcon();
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        void applyWindowIcon()
        {
            auto icon = juce::ImageFileFormat::loadFrom(BinaryData::appicondark_png,
                                                        BinaryData::appicondark_pngSize);
            if (!icon.isValid())
                return;

            constexpr int maxWindowIconSize = 256;
            if (icon.getWidth() > maxWindowIconSize || icon.getHeight() > maxWindowIconSize)
            {
                const auto scale = static_cast<double>(maxWindowIconSize)
                                 / static_cast<double>(juce::jmax(icon.getWidth(), icon.getHeight()));
                icon = icon.rescaled(juce::roundToInt(icon.getWidth() * scale),
                                     juce::roundToInt(icon.getHeight() * scale),
                                     juce::Graphics::highResamplingQuality);
            }

            setIcon(icon);

            if (auto* peer = getPeer())
                peer->setIcon(icon);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(NomadApplication)
