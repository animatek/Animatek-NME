#include "ModuleHelpPopup.h"
#include "../ui/AppTheme.h"
#include <iostream>

// ============================================================================
// Inner content — scrollable list of module description + parameters
// ============================================================================
class HelpContent : public juce::Component
{
public:
    explicit HelpContent(const NordHelp::ModuleHelp& help)
    {
        // Module description. Palette colour, not a hardcoded near-white: on a
        // light theme white-on-white made the text invisible (issue #58).
        descLabel.setFont(juce::Font(AppTheme::uiFont(14.0f)));
        descLabel.setColour(juce::Label::textColourId,       AppTheme::palette().textPrimary);
        descLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        descLabel.setText(help.description, juce::dontSendNotification);
        descLabel.setJustificationType(juce::Justification::topLeft);
        descLabel.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(descLabel);

        for (auto& p : help.params)
        {
            // "$Contents", "$#", "$Edit"...: navigation artifacts the help
            // scraper dragged in from the original manual's section pages,
            // not real controls (issue #57).
            if (p.name.startsWith("$"))
                continue;
            auto* nl = names.add(std::make_unique<juce::Label>());
            nl->setFont(juce::Font(AppTheme::uiFont(13.5f)).boldened());
            nl->setColour(juce::Label::textColourId,       AppTheme::palette().accentActive);
            nl->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            nl->setText(p.name, juce::dontSendNotification);
            nl->setJustificationType(juce::Justification::topLeft);
            addAndMakeVisible(nl);

            auto* dl = descs.add(std::make_unique<juce::Label>());
            dl->setFont(juce::Font(AppTheme::uiFont(13.5f)));
            dl->setColour(juce::Label::textColourId,       AppTheme::palette().textSecondary);
            dl->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            dl->setText(p.description, juce::dontSendNotification);
            dl->setJustificationType(juce::Justification::topLeft);
            dl->setMinimumHorizontalScale(1.0f);
            addAndMakeVisible(dl);
        }
    }

    // Call this after setting width to get the required height
    int layout(int w)
    {
        const int pad = 24;
        int innerW = w - pad * 2;
        int y = pad;

        auto setLbl = [&](juce::Label& l, float fs, bool bold, int h) {
            l.setBounds(pad, y, innerW, h);
            y += h + 4;
        };
        (void)setLbl; // suppress unused warning

        int dh = textHeight(descLabel.getText(), innerW, 14.0f, false);
        descLabel.setBounds(pad, y, innerW, dh);
        y += dh + 14;

        if (!names.isEmpty())
        {
            y += 14; // space for divider
            for (int i = 0; i < names.size(); ++i)
            {
                int nh = textHeight(names[i]->getText(), innerW, 13.5f, true);
                names[i]->setBounds(pad, y, innerW, nh);
                y += nh + 2;

                int ph = textHeight(descs[i]->getText(), innerW, 13.5f, false);
                descs[i]->setBounds(pad, y, innerW, ph);
                y += ph + 14;
            }
        }

        return y + pad;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(AppTheme::palette().backgroundMain);
        if (!names.isEmpty())
        {
            // divider line just above first parameter
            int y = descLabel.getBottom() + 14 + 4;
            g.setColour(AppTheme::palette().borderColor);
            g.fillRect(24, y, getWidth() - 48, 1);
        }
    }

private:
    static int textHeight(const juce::String& text, int width, float fs, bool bold)
    {
        auto f = juce::Font(AppTheme::uiFont(fs));
        if (bold) f = f.boldened();
        // Use AttributedString + TextLayout for accurate multi-line height
        juce::AttributedString as;
        as.setWordWrap(juce::AttributedString::byWord);
        as.append(text, f, juce::Colours::white);
        juce::TextLayout tl;
        tl.createLayout(as, (float)std::max(width, 1));
        return (int)std::ceil(tl.getHeight()) + 6;
    }

    juce::Label descLabel;
    juce::OwnedArray<juce::Label> names, descs;
};

// ============================================================================
// ModuleHelpPopup — plain Component added to desktop
// ============================================================================

ModuleHelpPopup::ModuleHelpPopup(const NordHelp::ModuleHelp& help,
                                 juce::Component* relativeTo)
    : juce::Component("ModuleHelpPopup")
{
    setOpaque(true);

    // The title is drawn by paint(); the label only carries the text
    titleLabel.setText(help.name, juce::dontSendNotification);

    closeButton.onClick = [this]() { removeFromDesktop(); delete this; };
    addAndMakeVisible(closeButton);

    // Scrollable content
    auto* content = new HelpContent(help);
    viewport.setViewedComponent(content, true);
    viewport.setScrollBarsShown(true, false);
    viewport.setColour(juce::ScrollBar::thumbColourId, AppTheme::palette().borderColor);
    addAndMakeVisible(viewport);

    // Size: lay out content at target width first
    const int popupW = 520;
    const int titleH = 40;
    const int maxContentH = 520;

    int prefH = content->layout(popupW);
    content->setSize(popupW, prefH);

    int contentH = juce::jmin(prefH, maxContentH);
    setSize(popupW, titleH + contentH);

    // Position: centred on the top-level window
    if (relativeTo != nullptr)
    {
        auto* top = relativeTo->getTopLevelComponent();
        auto screen = top->localAreaToGlobal(top->getLocalBounds());
        int x = screen.getX() + (screen.getWidth()  - popupW) / 2;
        int y = screen.getY() + (screen.getHeight() - getHeight()) / 2;
        setTopLeftPosition(x, y);
    }

    addToDesktop(juce::ComponentPeer::windowHasDropShadow);
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
}

void ModuleHelpPopup::resized()
{
    const int titleH = 40;
    closeButton.setBounds(getWidth() - 36, 6, 28, 28);
    viewport.setBounds(0, titleH, getWidth(), getHeight() - titleH);
}

void ModuleHelpPopup::paint(juce::Graphics& g)
{
    // The same title bar as every dialog; the text sits on the panel colour below it
    AppTheme::drawDialogChrome(g, getLocalBounds(), titleLabel.getText(), 40, 0);
}

void ModuleHelpPopup::paintOverChildren(juce::Graphics& g)
{
    AppTheme::drawWindowFrame(g, getLocalBounds());
}

bool ModuleHelpPopup::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::F1Key)
    {
        removeFromDesktop();
        delete this;
        return true;
    }
    return false;
}

void ModuleHelpPopup::mouseDown(const juce::MouseEvent& e)
{
    if (e.getPosition().getY() < 40)
        dragger.startDraggingComponent(this, e);
}

void ModuleHelpPopup::mouseDrag(const juce::MouseEvent& e)
{
    // only a drag that began on the title bar moves the window
    if (e.getMouseDownPosition().getY() < 40)
        dragger.dragComponent(this, e, nullptr);
}

// static
ModuleHelpPopup* ModuleHelpPopup::show(const juce::String& moduleFullname,
                                       juce::Component* relativeTo)
{
    // moduleFullname may be "FullName|ShortName" — try each part
    auto parts = juce::StringArray::fromTokens(moduleFullname, "|", "");

    const NordHelp::ModuleHelp* help = nullptr;
    for (auto& part : parts)
    {
        help = NordHelp::findModuleHelp(part.trim());
        if (help != nullptr) break;
    }

    // Fallback: strip parenthesised suffixes from each part
    if (help == nullptr)
    {
        for (auto& part : parts)
        {
            juce::String trimmed = part.upToFirstOccurrenceOf("(", false, false).trim();
            help = NordHelp::findModuleHelp(trimmed);
            if (help != nullptr) break;
        }
    }

    if (help == nullptr)
    {
        std::cout << "[HELP] No help found for: " << moduleFullname << std::endl;
        return nullptr;
    }

    return new ModuleHelpPopup(*help, relativeTo);
}
