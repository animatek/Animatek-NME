#include "SlotSelectDialog.h"
#include "AppTheme.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kGold   (AppTheme::palette().accentActive)
#define kText   (AppTheme::palette().textSecondary)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)

static constexpr int kRadioGroup = 0x51075E1; // "SLOTSEl" — any unique non-zero id

static void styleBtn (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,   kBtnBg);
    b.setColour (juce::TextButton::buttonOnColourId, kBtnOn);
    b.setColour (juce::TextButton::textColourOffId,  kText);
    b.setColour (juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
}

static void styleToggle (juce::ToggleButton& t)
{
    t.setColour (juce::ToggleButton::textColourId,        AppTheme::palette().textPrimary);
    t.setColour (juce::ToggleButton::tickColourId,        kGold);
    t.setColour (juce::ToggleButton::tickDisabledColourId, AppTheme::palette().borderColor);
    t.setRadioGroupId (kRadioGroup);
}

// ─────────────────────────────────────────────────────────────────────────────
SlotSelectDialog::SlotSelectDialog(const juce::String& title,
                                   const std::array<juce::String, 4>& slotNames,
                                   int  currentSlot,
                                   Callback cb)
    : title_ (title), currentSlot_ (juce::jlimit (0, 3, currentSlot)), callback (std::move (cb))
{
    static const char* letters[] = { "A", "B", "C", "D" };

    closeButton.onClick = [this] { cancel(); };
    addAndMakeVisible (closeButton);

    for (int i = 0; i < 4; ++i)
    {
        auto name = slotNames[static_cast<size_t> (i)].isNotEmpty()
                        ? slotNames[static_cast<size_t> (i)]
                        : juce::String ("(empty)");
        slotButtons[static_cast<size_t> (i)].setButtonText (juce::String (letters[i]) + " :  " + name);
        styleToggle (slotButtons[static_cast<size_t> (i)]);
        addAndMakeVisible (slotButtons[static_cast<size_t> (i)]);
    }

    styleToggle (localButton);
    addAndMakeVisible (localButton);

    slotButtons[static_cast<size_t> (currentSlot_)].setToggleState (true, juce::dontSendNotification);

    styleBtn (okButton);
    styleBtn (cancelButton);
    okButton.onClick     = [this] { confirm(); };
    cancelButton.onClick = [this] { cancel();  };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (320, 262);
    setWantsKeyboardFocus (true);
}

void SlotSelectDialog::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (AppTheme::palette().textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
    g.drawText (title_, 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);

    const int sepY = getHeight() - 50;
    g.drawHorizontalLine (sepY, 14.0f, static_cast<float> (getWidth() - 14));
}

void SlotSelectDialog::resized()
{
    constexpr int titleH = 32, pad = 14, rowH = 26, gap = 4;

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = titleH + gap + 2;
    for (auto& b : slotButtons)
    {
        b.setBounds (pad + 4, y, getWidth() - pad * 2 - 8, rowH);
        y += rowH + gap;
    }
    y += 4;
    localButton.setBounds (pad + 4, y, getWidth() - pad * 2 - 8, rowH);

    const int btnW = 80, btnH = 26;
    const int btnY = getHeight() - pad - btnH;
    cancelButton.setBounds (getWidth() - pad - btnW,         btnY, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW * 2 - 8, btnY, btnW, btnH);
}

bool SlotSelectDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { cancel();  return true; }
    if (key == juce::KeyPress::returnKey) { confirm(); return true; }
    return false;
}

void SlotSelectDialog::mouseDown (const juce::MouseEvent& e) { dragger.startDraggingComponent (this, e); }
void SlotSelectDialog::mouseDrag (const juce::MouseEvent& e) { dragger.dragComponent (this, e, nullptr); }

void SlotSelectDialog::confirm()
{
    Result r;
    r.confirmed = true;
    if (localButton.getToggleState())
    {
        r.local = true;
        r.slot  = currentSlot_;
    }
    else
    {
        for (int i = 0; i < 4; ++i)
            if (slotButtons[static_cast<size_t> (i)].getToggleState())
                r.slot = i;
    }
    if (callback) callback (r);
    close();
}

void SlotSelectDialog::cancel()
{
    Result r; r.confirmed = false;
    if (callback) callback (r);
    close();
}

void SlotSelectDialog::close() { closeSelf(); }

// ─────────────────────────────────────────────────────────────────────────────
void SlotSelectDialog::show(juce::Component* parent,
                            const juce::String& title,
                            const std::array<juce::String, 4>& slotNames,
                            int  currentSlot,
                            Callback cb)
{
    auto* dlg = new SlotSelectDialog (title, slotNames, currentSlot, std::move (cb));

    if (parent != nullptr)
    {
        auto* top    = parent->getTopLevelComponent();
        auto  screen = top->localAreaToGlobal (top->getLocalBounds());
        dlg->setTopLeftPosition (screen.getX() + (screen.getWidth()  - dlg->getWidth())  / 2,
                                 screen.getY() + (screen.getHeight() - dlg->getHeight()) / 2);
    }

    dlg->addToDesktop (juce::ComponentPeer::windowHasDropShadow);
    dlg->setVisible (true);
    dlg->toFront (true);
    dlg->grabKeyboardFocus();
}
