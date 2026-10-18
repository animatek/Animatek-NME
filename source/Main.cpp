#include "MainComponent.h"
#include "BinaryData.h"
#include "ui/AppTheme.h"
#include "ui/PatchCanvasComponent.h"
#include "ui/ThemeRegistry.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <set>
#include "ui/WindowOwner.h"

class NmeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Animatek NME"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
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

        // No display to draw on (on Linux: $DISPLAY unset or unreachable, a stale xauth cookie):
        // JUCE has no screen to put the window on, and centring it crashed (#101). Say so and quit.
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
        {
            std::cerr << "Animatek NME: no display to open the window on. "
                         "On Linux, check that $DISPLAY is set and reachable (xauth, ssh -X)." << std::endl;
            setApplicationReturnValue(1);
            quit();
            return;
        }

        const auto savedOptions = EditorOptions::load(appProperties.getUserSettings());
        AppTheme::setPalette(ThemeRegistry::get(savedOptions.uiThemeIndex).app);
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

        // NME's other windows (dialogs, floaters) are top-level windows of their own, and JUCE
        // does not tie them to this one: a click here raised it over them, and a dialog that
        // had just opened (the slot chooser a double-click in the browser brings up) went
        // behind, so NME looked stuck waiting for an answer nobody could see (#98).
       #if JUCE_MAC
        // macOS: none of the system's own ownership is used. When this window comes forward
        // the others are put back above it, without the focus, once the button is let go; the
        // menu bar is the system's there, so no menu of ours is in the way.
        void activeWindowStatusChanged() override
        {
            DocumentWindow::activeWindowStatusChanged();
            if (isActiveWindow())
                raiseOwnWindowsWhenFree (0);
        }

        void raiseOwnWindowsWhenFree (int tries)
        {
            juce::Component::SafePointer<MainWindow> self (this);
            juce::Timer::callAfterDelay (40, [self, tries]
            {
                if (self == nullptr)
                    return;
                if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
                {
                    if (tries < 250)
                        self->raiseOwnWindowsWhenFree (tries + 1);
                    return;
                }
                auto& desktop = juce::Desktop::getInstance();
                for (int i = 0; i < desktop.getNumComponents(); ++i)
                    if (auto* c = desktop.getComponent (i);
                        c != nullptr && c != self.getComponent() && c->isVisible() && !c->isAlwaysOnTop()
                        && !isTemporaryWindow (*c))
                        c->toFront (false);
            });
        }
       #else
        // Windows and Linux: this window becomes their owner (an owned window, a transient one
        // for the window manager), and the system keeps them above it, as it does any app's
        // dialogs. Raising them by hand fought the menu bar: the raise took the focus, and the
        // next click on File or Edit only brought this window back. They are looked for a few
        // times a second, as they come and go on their own.
        struct OwnerTimer : juce::Timer
        {
            MainWindow& owner;
            std::set<void*> owned;
            explicit OwnerTimer (MainWindow& w) : owner (w) { startTimer (250); }
            void timerCallback() override
            {
                auto* mainPeer = owner.getPeer();
                if (mainPeer == nullptr)
                    return;
                std::set<void*> seen;
                auto& desktop = juce::Desktop::getInstance();
                for (int i = 0; i < desktop.getNumComponents(); ++i)
                {
                    auto* c = desktop.getComponent (i);
                    if (c == nullptr || c == &owner || isTemporaryWindow (*c) || c->getPeer() == nullptr)
                        continue;
                    void* handle = c->getPeer()->getNativeHandle();
                    seen.insert (handle);
                    if (owned.count (handle) == 0
                        && WindowOwner::setOwner (handle, mainPeer->getNativeHandle()))
                        owned.insert (handle);
                }
                for (auto it = owned.begin(); it != owned.end();)   // gone: forget them
                    it = seen.count (*it) ? std::next (it) : owned.erase (it);
            }
        };
        OwnerTimer ownerTimer { *this };
       #endif

        // Menus, tooltips and Quick Add: they come and go on their own, and are left alone.
        static bool isTemporaryWindow (juce::Component& c)
        {
            auto* peer = c.getPeer();
            return peer != nullptr && (peer->getStyleFlags() & juce::ComponentPeer::windowIsTemporary) != 0;
        }

        // Closing a component leaves JUCE with nothing focused, and keys then
        // arrive at the window rather than bubbling up from a child. Quick Add
        // is a window of its own, so that is exactly the state it leaves behind:
        // without this, Enter could not put down the module it had just handed
        // to the pointer, though a click could.
        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey && PatchCanvas::isDropPending())
            {
                PatchCanvas::cancelPendingDrop();
                return true;
            }
            if (key == juce::KeyPress::returnKey && PatchCanvas::dropPendingAtPointer())
                return true;

            return DocumentWindow::keyPressed(key);
        }

    private:
        void applyWindowIcon()
        {
            auto icon = juce::ImageFileFormat::loadFrom(BinaryData::appicon_png,
                                                        BinaryData::appicon_pngSize);
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
