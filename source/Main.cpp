#include "MainComponent.h"
#include "BinaryData.h"
#include "ui/AppTheme.h"
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

        const auto savedTheme = AppTheme::themeFromInt(
            appProperties.getUserSettings()->getIntValue("appearanceTheme", 0));
        AppTheme::setTheme(savedTheme);
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), appProperties);
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
            : DocumentWindow(name, AppTheme::palette().backgroundMain, allButtons)
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
