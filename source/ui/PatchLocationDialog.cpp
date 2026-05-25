#include "PatchLocationDialog.h"

static const juce::Colour kBg     { 0xff323232 };
static const juce::Colour kSep    { 0xff444A53 };
static const juce::Colour kGold   { 0xffffcc44 };
static const juce::Colour kAmber  { 0xffffaa44 };
static const juce::Colour kText   { 0xffcccccc };
static const juce::Colour kCtrlBg { 0xff25282E };
static const juce::Colour kCtrlBd { 0xff555B64 };
static const juce::Colour kBtnBg  { 0xff25282E };
static const juce::Colour kBtnOn  { 0xff444A53 };
static const juce::Colour kOkBg   { 0xff1e3a1e };
static const juce::Colour kOkOn   { 0xff2a5a2a };

static void styleLabel (juce::Label& l)
{
    l.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId,       kGold);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

static void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId,      kCtrlBg);
    c.setColour (juce::ComboBox::outlineColourId,         kCtrlBd);
    c.setColour (juce::ComboBox::textColourId,            kText);
    c.setColour (juce::ComboBox::arrowColourId,           kAmber);
    c.setColour (juce::ComboBox::focusedOutlineColourId,  kGold);
}

static void styleBtn (juce::TextButton& b, bool isOk = false)
{
    b.setColour (juce::TextButton::buttonColourId,   isOk ? kOkBg : kBtnBg);
    b.setColour (juce::TextButton::buttonOnColourId, isOk ? kOkOn : kBtnOn);
    b.setColour (juce::TextButton::textColourOffId,  isOk ? juce::Colour (0xffaaffaa) : kText);
}

// ─────────────────────────────────────────────────────────────────────────────
PatchLocationDialog::PatchLocationDialog(const juce::String& title,
                                         const std::vector<std::string>& patchList,
                                         bool showSlot,
                                         int  currentSlot,
                                         Callback cb)
    : title_ (title), patchList_ (patchList), showSlot_ (showSlot), callback (std::move (cb))
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
    bankCombo.setSelectedItemIndex (0, juce::dontSendNotification);
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
    styleBtn (okButton, true);
    styleBtn (cancelButton);
    okButton    .onClick = [this]() { confirm(); };
    cancelButton.onClick = [this]() { cancel();  };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (380, showSlot_ ? 250 : 218);
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
    positionCombo.setSelectedItemIndex (0, juce::dontSendNotification);
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

void PatchLocationDialog::close() { removeFromDesktop(); delete this; }

bool PatchLocationDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { cancel(); return true; }
    if (key == juce::KeyPress::returnKey) { confirm(); return true; }
    return false;
}

void PatchLocationDialog::mouseDown (const juce::MouseEvent& e)
    { if (e.getPosition().getY() < 32) dragger.startDraggingComponent (this, e); }
void PatchLocationDialog::mouseDrag (const juce::MouseEvent& e)
    { dragger.dragComponent (this, e, nullptr); }

// ─────────────────────────────────────────────────────────────────────────────
void PatchLocationDialog::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kGold);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText (title_, 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);

    const float x0 = 14.0f, x1 = static_cast<float> (getWidth() - 14);
    const int sepY = getHeight() - 50;
    g.drawHorizontalLine (sepY, x0, x1);
}

void PatchLocationDialog::resized()
{
    constexpr int titleH = 32, pad = 14, secH = 14, rowH = 28, gap = 6;

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = titleH + gap;

    auto addRow = [&](juce::Label& lbl, juce::ComboBox& combo)
    {
        lbl.setBounds   (pad, y, getWidth() - pad * 2, secH);
        y += secH + 2;
        combo.setBounds (pad, y, getWidth() - pad * 2, rowH);
        y += rowH + gap + 4;
    };

    if (showSlot_) addRow (slotLabel, slotCombo);
    addRow (bankLabel, bankCombo);
    addRow (positionLabel, positionCombo);

    // Buttons pinned to bottom
    const int btnW = 80, btnH = 26;
    const int btnY = getHeight() - pad - btnH;
    cancelButton.setBounds (getWidth() - pad - btnW,         btnY, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW * 2 - 8, btnY, btnW, btnH);
}

// ─────────────────────────────────────────────────────────────────────────────
void PatchLocationDialog::show(juce::Component* parent,
                                const juce::String& title,
                                const std::vector<std::string>& patchList,
                                bool showSlot,
                                int  currentSlot,
                                Callback cb)
{
    auto* dlg = new PatchLocationDialog (title, patchList, showSlot, currentSlot, std::move (cb));

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
