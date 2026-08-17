#include "AboutDialog.h"
#include "AppTheme.h"
#include <BinaryData.h>

namespace
{
constexpr int dialogWidth  = 460;
constexpr int dialogHeight = 560;
constexpr int headerHeight = 108;
constexpr int buttonRowH   = 30;
constexpr int pad          = 16;

// Same destinations as the About menu items in MainComponent.
const char* const kWebsiteUrl = "https://animatek.net/animatek-nme-eng/";
const char* const kPatreonUrl = "https://www.patreon.com/collection/2038913";
const char* const kGithubUrl  = "https://github.com/animatek/Animatek-NME/";

void styleButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId,   AppTheme::palette().buttonBackground);
    b.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
    b.setColour(juce::TextButton::textColourOffId,  AppTheme::palette().textSecondary);
    b.setColour(juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
}
}

juce::String AboutDialog::versionSummary()
{
    return "Animatek NME " + juce::String(JUCE_APPLICATION_VERSION_STRING)
         + " (" + juce::SystemStats::getOperatingSystemName()
         + ", JUCE " + juce::String(JUCE_MAJOR_VERSION) + "." + juce::String(JUCE_MINOR_VERSION)
         + "." + juce::String(JUCE_BUILDNUMBER) + ")";
}

AboutDialog::AboutDialog(UrlCallback openUrl)
    : openUrl_(std::move(openUrl))
{
    setOpaque(true);

    icon = juce::ImageCache::getFromMemory(BinaryData::appicon_png, BinaryData::appicon_pngSize);

    // Word wrap on: the text is hand-wrapped to fit, and wrapping is the
    // fallback that keeps a long line from adding a horizontal scrollbar.
    credits.setMultiLine(true, true);
    credits.setReadOnly(true);
    credits.setScrollbarsShown(true);
    credits.setCaretVisible(false);
    credits.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                                 12.0f, juce::Font::plain)));
    credits.setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().backgroundMain);
    credits.setColour(juce::TextEditor::textColourId,       AppTheme::palette().textSecondary);
    credits.setColour(juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);
    credits.setText(juce::String::createStringFromData(BinaryData::credits_txt,
                                                       BinaryData::credits_txtSize),
                    false);
    credits.moveCaretToTop(false);  // otherwise it opens part-scrolled
    addAndMakeVisible(credits);

    websiteButton.onClick = [this]() { if (openUrl_) openUrl_(kWebsiteUrl); };
    patreonButton.onClick = [this]() { if (openUrl_) openUrl_(kPatreonUrl); };
    githubButton.onClick  = [this]() { if (openUrl_) openUrl_(kGithubUrl); };
    copyButton.onClick    = [this]() { copyVersionToClipboard(); };

    for (auto* b : { &websiteButton, &patreonButton, &githubButton, &copyButton })
    {
        styleButton(*b);
        addAndMakeVisible(*b);
    }

    setSize(dialogWidth, dialogHeight);
}

void AboutDialog::copyVersionToClipboard()
{
    juce::SystemClipboard::copyTextToClipboard(versionSummary());
    // Confirm in place: the dialog is modal, so the status bar is not visible.
    copyButton.setButtonText("Copied");
    juce::Component::SafePointer<AboutDialog> safeThis(this);
    juce::Timer::callAfterDelay(1500, [safeThis]() {
        if (safeThis != nullptr)
            safeThis->copyButton.setButtonText("Copy version");
    });
}

void AboutDialog::paint(juce::Graphics& g)
{
    const auto& pal = AppTheme::palette();
    g.fillAll(pal.backgroundMain);

    auto header = juce::Rectangle<int>(0, 0, getWidth(), headerHeight).reduced(pad, pad);

    if (icon.isValid())
    {
        auto iconArea = header.removeFromLeft(header.getHeight());
        g.drawImage(icon, iconArea.toFloat(), juce::RectanglePlacement::centred);
        header.removeFromLeft(pad);
    }

    auto line = header.removeFromTop(26);
    g.setColour(pal.textPrimary);
    g.setFont(juce::Font(AppTheme::uiFont(20.0f)).boldened());
    g.drawText("Animatek NME", line, juce::Justification::centredLeft, true);

    line = header.removeFromTop(20);
    g.setColour(pal.accentActive);
    g.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    g.drawText("Nord Modular Editor G1", line, juce::Justification::centredLeft, true);

    line = header.removeFromTop(18);
    g.setColour(pal.textSecondary);
    g.setFont(juce::Font(AppTheme::uiFont(11.5f)));
    g.drawText("Version " + juce::String(JUCE_APPLICATION_VERSION_STRING)
                   + "  -  built " + juce::String(__DATE__),
               line, juce::Justification::centredLeft, true);

    g.setColour(pal.buttonActive);
    g.fillRect(0, headerHeight - 1, getWidth(), 1);
}

void AboutDialog::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(headerHeight);

    auto buttons = area.removeFromBottom(buttonRowH + pad).reduced(pad, 0);
    buttons.removeFromBottom(pad);

    // Four equal buttons across the row.
    const int gap = 6;
    const int w = (buttons.getWidth() - gap * 3) / 4;
    websiteButton.setBounds(buttons.removeFromLeft(w));
    buttons.removeFromLeft(gap);
    patreonButton.setBounds(buttons.removeFromLeft(w));
    buttons.removeFromLeft(gap);
    githubButton.setBounds(buttons.removeFromLeft(w));
    buttons.removeFromLeft(gap);
    copyButton.setBounds(buttons);

    credits.setBounds(area.reduced(pad, pad / 2));
}

juce::Component* AboutDialog::show(juce::Component* parent, UrlCallback openUrl)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(new AboutDialog(std::move(openUrl)));
    opts.dialogTitle = "About Animatek NME";
    opts.dialogBackgroundColour = AppTheme::palette().backgroundMain;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = false;
    opts.resizable = false;
    opts.componentToCentreAround = parent;
    return opts.launchAsync();
}
