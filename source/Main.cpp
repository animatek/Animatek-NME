#include "MainComponent.h"
#include "BinaryData.h"
#include "ui/AppTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

class NmeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Animatek NME"; }
    const juce::String getApplicationVersion() override { return "0.6.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise(const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "AnimatekNME";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";

        // Migrate settings written under the old Nomad2026 name
        auto oldOptions = options;
        oldOptions.applicationName = "Nomad2026";
        const auto oldFile = oldOptions.getDefaultFile();
        const auto newFile = options.getDefaultFile();
        if (oldFile.existsAsFile() && !newFile.existsAsFile())
        {
            newFile.getParentDirectory().createDirectory();
            oldFile.copyFileTo(newFile);
        }

        appProperties.setStorageParameters(options);

        const auto savedTheme = AppTheme::themeFromInt(
            appProperties.getUserSettings()->getIntValue("appearanceTheme", 0));
        AppTheme::setTheme(savedTheme);
        mainWindow = std::make_unique<MainWindow>("Animatek NME - Nord Modular Editor G1", appProperties);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
        {
            if (auto* settings = appProperties.getUserSettings())
            {
                settings->setValue("mainWindowState", mainWindow->getWindowStateAsString());
                settings->saveIfNeeded();
            }
        }
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

            // Restore last size/position/maximized state; JUCE clamps it back
            // on-screen if the monitor layout changed since last run
            if (auto* settings = props.getUserSettings())
            {
                const auto state = settings->getValue("mainWindowState");
                if (state.isNotEmpty())
                    restoreWindowStateFromString(state);
            }

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

START_JUCE_APPLICATION(NmeApplication)
