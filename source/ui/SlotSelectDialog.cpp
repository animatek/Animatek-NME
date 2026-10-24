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
    t.setRadioGroupId (kRadioGroup);
}

// ─────────────────────────────────────────────────────────────────────────────
SlotSelectDialog::SlotSelectDialog(const juce::String& title,
                                   const std::array<juce::String, 4>& slotNames,
                                   int  synth,
                                   int  currentSlot,
                                   Callback cb)
    : title_ (title), currentSlot_ (juce::jlimit (0, 3, currentSlot)), callback (std::move (cb))
{
    closeButton.onClick = [this] { cancel(); };
    addAndMakeVisible (closeButton);

    for (int i = 0; i < 4; ++i)
    {
        auto name = slotNames[static_cast<size_t> (i)].isNotEmpty()
                        ? slotNames[static_cast<size_t> (i)]
                        : juce::String ("(empty)");
        slotButtons[static_cast<size_t> (i)].setButtonText (SynthSlot::label (SynthSlot::global (synth, i)) + " :  " + name);
        styleToggle (slotButtons[static_cast<size_t> (i)]);
        addAndMakeVisible (slotButtons[static_cast<size_t> (i)]);
    }

    styleToggle (localButton);
    addAndMakeVisible (localButton);

    slotButtons[static_cast<size_t> (currentSlot_)].setToggleState (true, juce::dontSendNotification);

    styleBtn (okButton);
    styleBtn (cancelButton);
    okButton.setColour (juce::TextButton::buttonColourId,  AppTheme::palette().accentActive);   // OK does the thing
    okButton.setColour (juce::TextButton::textColourOffId, AppTheme::palette().backgroundMain);
    okButton.onClick     = [this] { confirm(); };
    cancelButton.onClick = [this] { cancel();  };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (400, 40 + 12 + 4 * 48 + 76 + 20 + 68);
    setWantsKeyboardFocus (true);
}

void SlotSelectDialog::paint (juce::Graphics& g)
{
    AppTheme::drawDialogChrome (g, getLocalBounds(), title_, 40, 68);
}

void SlotSelectDialog::resized()
{
    constexpr int titleH = 40, footerH = 68, side = 24;

    closeButton.setBounds (getWidth() - 36, 6, 28, 28);

    // Each slot is a row with a round radio at the right; the editor-only choice is the last row
    int y = titleH + 12;
    for (auto& b : slotButtons)
    {
        b.setBounds (side, y, getWidth() - side * 2, 48);
        y += 48;
    }
    localButton.setBounds (side, y, getWidth() - side * 2, 76);   // room for its two lines of text, and a gap above the footer

    const int btnW = 100, btnH = 36;
    const int btnY = getHeight() - footerH + (footerH - btnH) / 2;
    cancelButton.setBounds (getWidth() - side - btnW,          btnY, btnW, btnH);
    okButton    .setBounds (getWidth() - side - btnW * 2 - 12, btnY, btnW, btnH);
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
                            int  synth,
                            int  currentSlot,
                            Callback cb)
{
    auto* dlg = new SlotSelectDialog (title, slotNames, synth, currentSlot, std::move (cb));

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
