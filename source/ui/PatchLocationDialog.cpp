#include "PatchLocationDialog.h"
#include "AppTheme.h"

#define kBg     (AppTheme::palette().backgroundMain)
#define kSep    (AppTheme::palette().buttonActive)
#define kGold   (AppTheme::palette().accentActive)
#define kText   (AppTheme::palette().textSecondary)
#define kCtrlBg (AppTheme::palette().inputBackground)
#define kCtrlBd (AppTheme::palette().borderColor)
#define kBtnBg  (AppTheme::palette().buttonBackground)
#define kBtnOn  (AppTheme::palette().buttonActive)
static void styleLabel (juce::Label& l)
{
    l.setFont (juce::Font (AppTheme::uiFont (12.5f).withStyle ("Bold")));
    l.setColour (juce::Label::textColourId,       AppTheme::palette().textMuted);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

static void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId,      kCtrlBg);
    c.setColour (juce::ComboBox::outlineColourId,         kCtrlBd);
    c.setColour (juce::ComboBox::textColourId,            kText);
    c.setColour (juce::ComboBox::arrowColourId,           kText);
    c.setColour (juce::ComboBox::focusedOutlineColourId,  kGold);
}

static void styleBtn (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,   kBtnBg);
    b.setColour (juce::TextButton::buttonOnColourId, kBtnOn);
    b.setColour (juce::TextButton::textColourOffId,  kText);
    b.setColour (juce::TextButton::textColourOnId,   AppTheme::palette().textPrimary);
}

// ─────────────────────────────────────────────────────────────────────────────
PatchLocationDialog::PatchLocationDialog(const juce::String& title,
                                         const std::vector<std::string>& patchList,
                                         bool showSlot,
                                         int  currentSlot,
                                         Callback cb,
                                         int  initialSection,
                                         int  initialPosition)
    : title_ (title), patchList_ (patchList), showSlot_ (showSlot), callback (std::move (cb)),
      initialPosition_ (initialPosition)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this]() { cancel(); };
    addAndMakeVisible (closeButton);

    // Slot
    if (showSlot_)
    {
        if (title_.startsWithIgnoreCase("Copy") || title_.startsWithIgnoreCase("Move"))
            slotLabel.setText("TEMP SLOT", juce::dontSendNotification);

        styleLabel (slotLabel);
        slotCombo.addItem ("A  (Slot 1)", 1);
        slotCombo.addItem ("B  (Slot 2)", 2);
        slotCombo.addItem ("C  (Slot 3)", 3);
        slotCombo.addItem ("D  (Slot 4)", 4);
        slotCombo.setSelectedItemIndex (currentSlot, juce::dontSendNotification);
        styleCombo (slotCombo);
        addAndMakeVisible (slotLabel);
        addAndMakeVisible (slotCombo);
    }

    // Bank
    styleLabel (bankLabel);
    for (int b = 1; b <= 9; ++b)
        bankCombo.addItem ("Bank " + juce::String (b), b);
    bankCombo.setSelectedItemIndex (juce::jlimit (0, 8, initialSection), juce::dontSendNotification);
    bankCombo.onChange = [this]() { updatePositionItems(); };
    styleCombo (bankCombo);
    addAndMakeVisible (bankLabel);
    addAndMakeVisible (bankCombo);

    // Position
    styleLabel (positionLabel);
    styleCombo (positionCombo);
    addAndMakeVisible (positionLabel);
    addAndMakeVisible (positionCombo);
    updatePositionItems();

    // Buttons
    styleBtn (okButton);
    styleBtn (cancelButton);
    okButton.setColour (juce::TextButton::buttonColourId,  kGold);   // OK does the thing
    okButton.setColour (juce::TextButton::textColourOffId, AppTheme::palette().backgroundMain);
    okButton    .onClick = [this]() { confirm(); };
    cancelButton.onClick = [this]() { cancel();  };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (440, 40 + 16 + (showSlot_ ? 3 : 2) * 78 + 12 + 68);
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchLocationDialog::updatePositionItems()
{
    const int bank    = bankCombo.getSelectedItemIndex() + 1;
    const int section = bank - 1;

    positionCombo.clear (juce::dontSendNotification);
    for (int pos = 1; pos <= 99; ++pos)
    {
        int idx = section * 99 + (pos - 1);
        juce::String name = (idx < static_cast<int>(patchList_.size()) && !patchList_[idx].empty())
                            ? juce::String (patchList_[idx]) : "--";
        positionCombo.addItem (juce::String (pos).paddedLeft ('0', 2) + ":  " + name, pos);
    }

    // Preselect the patch's own position the first time round; changing bank
    // afterwards starts from the top of the new bank, as before.
    const int wanted = initialPosition_;
    initialPosition_ = -1;
    positionCombo.setSelectedItemIndex (juce::jlimit (0, 98, wanted), juce::dontSendNotification);
}

void PatchLocationDialog::confirm()
{
    Result r;
    r.slot      = showSlot_ ? slotCombo.getSelectedItemIndex() : 0;
    r.section   = bankCombo.getSelectedItemIndex();
    r.position  = positionCombo.getSelectedItemIndex();
    r.confirmed = true;
    if (callback) callback (r);
    close();
}

void PatchLocationDialog::cancel()
{
    Result r;  r.confirmed = false;
    if (callback) callback (r);
    close();
}

void PatchLocationDialog::close() { closeSelf(); }

bool PatchLocationDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { cancel(); return true; }
    if (key == juce::KeyPress::returnKey) { confirm(); return true; }
    return false;
}

void PatchLocationDialog::mouseDown (const juce::MouseEvent& e)
    { if (e.getPosition().getY() < 40) dragger.startDraggingComponent (this, e); }
void PatchLocationDialog::mouseDrag (const juce::MouseEvent& e)
    { if (e.getMouseDownPosition().getY() < 40) dragger.dragComponent (this, e, nullptr); }  // only a drag that began on the title bar moves the window (the press position says where it began)

// ─────────────────────────────────────────────────────────────────────────────
void PatchLocationDialog::paint (juce::Graphics& g)
{
    AppTheme::drawDialogChrome (g, getLocalBounds(), title_, 40, 68);
}

void PatchLocationDialog::resized()
{
    constexpr int titleH = 40, footerH = 68, pad = 24, secH = 18, rowH = 36;

    closeButton.setBounds (getWidth() - 36, 6, 28, 28);

    int y = titleH + 16;

    auto addRow = [&](juce::Label& lbl, juce::ComboBox& combo)
    {
        lbl.setBounds   (pad, y, getWidth() - pad * 2, secH);
        y += secH + 4;
        combo.setBounds (pad, y, getWidth() - pad * 2, rowH);
        y += rowH + 20;
    };

    if (showSlot_) addRow (slotLabel, slotCombo);
    addRow (bankLabel, bankCombo);
    addRow (positionLabel, positionCombo);

    // Buttons pinned to bottom
    const int btnW = 100, btnH = 36;
    const int btnY = getHeight() - footerH + (footerH - btnH) / 2;
    cancelButton.setBounds (getWidth() - pad - btnW,          btnY, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW * 2 - 12, btnY, btnW, btnH);
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchLocationDialog::show(juce::Component* parent,
                                const juce::String& title,
                                const std::vector<std::string>& patchList,
                                bool showSlot,
                                int  currentSlot,
                                Callback cb,
                                int  initialSection,
                                int  initialPosition)
{
    auto* dlg = new PatchLocationDialog (title, patchList, showSlot, currentSlot, std::move (cb),
                                         initialSection, initialPosition);

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
