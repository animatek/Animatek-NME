#include "AboutDialog.h"
#include "AppTheme.h"
#include "VersionCheck.h"
#include <BinaryData.h>

namespace
{
constexpr int dialogWidth  = 520;
constexpr int dialogHeight = 620;
constexpr int headerHeight = 124;
constexpr int buttonRowH   = 38;
constexpr int pad          = 22;
constexpr int footerHeight = pad * 2 + buttonRowH * 2 + 10;

// Same destinations as the About menu items in MainComponent.
const char* const kWebsiteUrl = "https://animatek.net/animatek-nme-eng/";
const char* const kPatreonUrl = "https://www.patreon.com/collection/2038913";
const char* const kGithubUrl  = "https://github.com/animatek/Animatek-NME/";

void styleButton(juce::TextButton& b, bool primary = false)
{
    b.setColour(juce::TextButton::buttonColourId,   primary ? AppTheme::palette().accentActive
                                                            : AppTheme::palette().buttonBackground);
    b.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
    b.setColour(juce::TextButton::textColourOffId,  primary ? AppTheme::palette().backgroundMain
                                                            : AppTheme::palette().textSecondary);
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
                                                 12.5f, juce::Font::plain)));
    credits.setIndents(2, 4);
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
    donateButton.onClick  = [this]() { if (openUrl_) openUrl_(VersionCheck::kDonateUrl); };
    githubButton.onClick  = [this]() { if (openUrl_) openUrl_(kGithubUrl); };
    copyButton.onClick    = [this]() { copyVersionToClipboard(); };

    for (auto* b : { &websiteButton, &patreonButton, &donateButton, &githubButton, &copyButton })
    {
        styleButton(*b, b == &donateButton);   // Donate is the one thing asked of you here
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

    auto line = header.removeFromTop(34);
    g.setColour(pal.textPrimary);
    g.setFont(juce::Font(AppTheme::uiFont(25.0f)).boldened());
    g.drawText("Animatek NME", line, juce::Justification::centredLeft, true);

    line = header.removeFromTop(24);
    g.setColour(pal.accentActive);
    g.setFont(juce::Font(AppTheme::uiFont(15.0f)).boldened());
    g.drawText("Nord Modular Editor G1", line, juce::Justification::centredLeft, true);

    line = header.removeFromTop(22);
    g.setColour(pal.textMuted);
    g.setFont(juce::Font(AppTheme::uiFont(12.5f)));
    g.drawText("Version " + juce::String(JUCE_APPLICATION_VERSION_STRING)
                   + "  -  built " + juce::String(__DATE__),
               line, juce::Justification::centredLeft, true);

    g.setColour(pal.borderColor);
    g.fillRect(0, headerHeight - 1, getWidth(), 1);

    g.setColour(pal.backgroundPanel);
    g.fillRect(0, getHeight() - footerHeight, getWidth(), footerHeight);
    g.setColour(pal.borderColor);
    g.fillRect(0, getHeight() - footerHeight, getWidth(), 1);
}

void AboutDialog::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(headerHeight);

    // Two rows of buttons in a footer: Donate and Patreon, the asks, above Website, GitHub and Copy version
    auto footer = area.removeFromBottom(footerHeight).reduced(pad, pad);
    const int gap = 8;
    auto top = footer.removeFromTop(buttonRowH);
    footer.removeFromTop(10);
    auto bottom = footer.removeFromTop(buttonRowH);

    {
        const int w = (top.getWidth() - gap) / 2;
        donateButton.setBounds(top.removeFromLeft(w));
        top.removeFromLeft(gap);
        patreonButton.setBounds(top);
    }
    {
        const int w = (bottom.getWidth() - gap * 2) / 3;
        websiteButton.setBounds(bottom.removeFromLeft(w));
        bottom.removeFromLeft(gap);
        githubButton.setBounds(bottom.removeFromLeft(w));
        bottom.removeFromLeft(gap);
        copyButton.setBounds(bottom);
    }

    credits.setBounds(area.reduced(pad, 8));
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
