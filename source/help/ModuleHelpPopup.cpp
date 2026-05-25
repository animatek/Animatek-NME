#include "ModuleHelpPopup.h"
#include <iostream>

// ============================================================================
// Inner content — scrollable list of module description + parameters
// ============================================================================
class HelpContent : public juce::Component
{
public:
    explicit HelpContent(const NordHelp::ModuleHelp& help)
    {
        // Module description
        descLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        descLabel.setColour(juce::Label::textColourId,       juce::Colour(0xffdddddd));
        descLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        descLabel.setText(help.description, juce::dontSendNotification);
        descLabel.setJustificationType(juce::Justification::topLeft);
        descLabel.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(descLabel);

        for (auto& p : help.params)
        {
            auto* nl = names.add(std::make_unique<juce::Label>());
            nl->setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
            nl->setColour(juce::Label::textColourId,       juce::Colour(0xffffaa44));
            nl->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            nl->setText(p.name, juce::dontSendNotification);
            nl->setJustificationType(juce::Justification::topLeft);
            addAndMakeVisible(nl);

            auto* dl = descs.add(std::make_unique<juce::Label>());
            dl->setFont(juce::Font(juce::FontOptions(12.0f)));
            dl->setColour(juce::Label::textColourId,       juce::Colour(0xffcccccc));
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
        const int pad = 12;
        int innerW = w - pad * 2;
        int y = pad;

        auto setLbl = [&](juce::Label& l, float fs, bool bold, int h) {
            l.setBounds(pad, y, innerW, h);
            y += h + 4;
        };
        (void)setLbl; // suppress unused warning

        int dh = textHeight(descLabel.getText(), innerW, 13.0f, false);
        descLabel.setBounds(pad, y, innerW, dh);
        y += dh + 10;

        if (!names.isEmpty())
        {
            y += 8; // space for divider
            for (int i = 0; i < names.size(); ++i)
            {
                int nh = textHeight(names[i]->getText(), innerW, 12.0f, true);
                names[i]->setBounds(pad, y, innerW, nh);
                y += nh + 2;

                int ph = textHeight(descs[i]->getText(), innerW, 12.0f, false);
                descs[i]->setBounds(pad, y, innerW, ph);
                y += ph + 8;
            }
        }

        return y + pad;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff323232));
        if (!names.isEmpty())
        {
            // divider line just above first parameter
            int y = descLabel.getBottom() + 10 + 4;
            g.setColour(juce::Colour(0xff444A53));
            g.fillRect(12, y, getWidth() - 24, 1);
        }
    }

private:
    static int textHeight(const juce::String& text, int width, float fs, bool bold)
    {
        auto f = juce::Font(juce::FontOptions(fs));
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

    // Title bar
    titleLabel.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
    titleLabel.setColour(juce::Label::textColourId,       juce::Colour(0xffffcc44));
    titleLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    titleLabel.setText(help.name, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel);

    closeButton.onClick = [this]() { removeFromDesktop(); delete this; };
    addAndMakeVisible(closeButton);

    // Scrollable content
    auto* content = new HelpContent(help);
    viewport.setViewedComponent(content, true);
    viewport.setScrollBarsShown(true, false);
    viewport.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff555B64));
    addAndMakeVisible(viewport);

    // Size: lay out content at target width first
    const int popupW = 430;
    const int titleH = 32;
    const int maxContentH = 480;

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
    const int titleH = 32;
    titleLabel.setBounds(8, 0, getWidth() - 40, titleH);
    closeButton.setBounds(getWidth() - 32, 2, 28, 28);
    viewport.setBounds(0, titleH, getWidth(), getHeight() - titleH);
}

void ModuleHelpPopup::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff323232));
    // Title bar bottom line
    g.setColour(juce::Colour(0xff444A53));
    g.fillRect(0, 31, getWidth(), 1);
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
    if (e.getPosition().getY() < 32)
        dragger.startDraggingComponent(this, e);
}

void ModuleHelpPopup::mouseDrag(const juce::MouseEvent& e)
{
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
