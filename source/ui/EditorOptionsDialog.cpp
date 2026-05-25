#include "EditorOptionsDialog.h"

// ─── Color palette (same as PatchSettingsDialog) ─────────────────────────────
static const juce::Colour kBg     { 0xff323232 };
static const juce::Colour kSep    { 0xff333355 };
static const juce::Colour kGold   { 0xffffcc44 };
static const juce::Colour kAmber  { 0xffffaa44 };
static const juce::Colour kText   { 0xffcccccc };
static const juce::Colour kDim    { 0xff888899 };
static const juce::Colour kBtnBg  { 0xff252540 };
static const juce::Colour kBtnOn  { 0xff353560 };
static const juce::Colour kOkBg   { 0xff1e3a1e };
static const juce::Colour kOkOn   { 0xff2a5a2a };

static void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId,         kText);
    b.setColour (juce::ToggleButton::tickColourId,         kAmber);
    b.setColour (juce::ToggleButton::tickDisabledColourId, kDim);
}
static void styleLabel (juce::Label& l, bool section = false)
{
    l.setFont (section ? juce::Font (juce::FontOptions (11.0f, juce::Font::bold))
                       : juce::Font (juce::FontOptions (12.0f)));
    l.setColour (juce::Label::textColourId,       section ? kGold : kText);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}
static void styleBtn (juce::TextButton& b, bool isOk = false)
{
    b.setColour (juce::TextButton::buttonColourId,   isOk ? kOkBg : kBtnBg);
    b.setColour (juce::TextButton::buttonOnColourId, isOk ? kOkOn : kBtnOn);
    b.setColour (juce::TextButton::textColourOffId,  isOk ? juce::Colour (0xffaaffaa) : kText);
}
static void styleTextEditor (juce::TextEditor& e)
{
    e.setReadOnly (true);
    e.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff202038));
    e.setColour (juce::TextEditor::textColourId,       kText);
    e.setColour (juce::TextEditor::outlineColourId,    kSep);
    e.setColour (juce::TextEditor::focusedOutlineColourId, kAmber);
}

// ─── EditorOptions persistence ───────────────────────────────────────────────

EditorOptions EditorOptions::load(juce::PropertiesFile* props)
{
    EditorOptions o;
    if (!props) return o;
    o.cableStyle     = static_cast<CableStyle>  (props->getIntValue  ("cableStyle",      0));
    o.knobControl    = static_cast<KnobControl> (props->getIntValue  ("knobControl",     0));
    o.autoUpload     = props->getBoolValue  ("autoUpload",     true);
    o.recycleWindows = props->getBoolValue  ("recycleWindows", true);
    o.cableOpacity   = static_cast<float>   (props->getDoubleValue("cableOpacity", 0.80));
    auto libraryPath = props->getValue ("presetLibraryRoot", {});
    if (libraryPath.isNotEmpty())
        o.presetLibraryRoot = juce::File (libraryPath);
    return o;
}

void EditorOptions::save(juce::PropertiesFile* props) const
{
    if (!props) return;
    props->setValue ("cableStyle",      static_cast<int> (cableStyle));
    props->setValue ("knobControl",     static_cast<int> (knobControl));
    props->setValue ("autoUpload",      autoUpload);
    props->setValue ("recycleWindows",  recycleWindows);
    props->setValue ("cableOpacity",    static_cast<double>(cableOpacity));
    props->setValue ("presetLibraryRoot", presetLibraryRoot.getFullPathName());
    props->saveIfNeeded();
}

juce::File EditorOptions::getPatchesFolder() const
{
    return presetLibraryRoot == juce::File() ? juce::File() : presetLibraryRoot.getChildFile ("Patches");
}

juce::File EditorOptions::getSnippetsFolder() const
{
    return presetLibraryRoot == juce::File() ? juce::File() : presetLibraryRoot.getChildFile ("Snippets");
}

bool EditorOptions::ensureLibraryFolders() const
{
    if (presetLibraryRoot == juce::File())
        return false;

    auto rootOk = presetLibraryRoot.createDirectory().wasOk();
    auto patchesOk = getPatchesFolder().createDirectory().wasOk();
    auto snippetsOk = getSnippetsFolder().createDirectory().wasOk();
    return rootOk && patchesOk && snippetsOk;
}

// ─── EditorOptionsDialog ─────────────────────────────────────────────────────

EditorOptionsDialog::EditorOptionsDialog(const EditorOptions& current)
    : options(current)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    closeButton.onClick = [this]() { close(); };
    addAndMakeVisible (closeButton);

    // Cable Style (radio group 1)
    styleLabel (cableStyleLabel, true);
    for (auto* b : { &cableCurvedThick, &cableStraightThick, &cableCurvedThin, &cableStraightThin })
    {
        b->setRadioGroupId (1);
        styleToggle (*b);
        addAndMakeVisible (*b);
    }
    cableCurvedThick  .setToggleState (options.cableStyle == EditorOptions::CableStyle::CurvedThick,   juce::dontSendNotification);
    cableStraightThick.setToggleState (options.cableStyle == EditorOptions::CableStyle::StraightThick, juce::dontSendNotification);
    cableCurvedThin   .setToggleState (options.cableStyle == EditorOptions::CableStyle::CurvedThin,    juce::dontSendNotification);
    cableStraightThin .setToggleState (options.cableStyle == EditorOptions::CableStyle::StraightThin,  juce::dontSendNotification);
    addAndMakeVisible (cableStyleLabel);

    // Knob Control (radio group 2)
    styleLabel (knobControlLabel, true);
    for (auto* b : { &knobHorizontal, &knobCircular, &knobVertical })
    {
        b->setRadioGroupId (2);
        styleToggle (*b);
        addAndMakeVisible (*b);
    }
    knobHorizontal.setToggleState (options.knobControl == EditorOptions::KnobControl::Horizontal, juce::dontSendNotification);
    knobCircular  .setToggleState (options.knobControl == EditorOptions::KnobControl::Circular,   juce::dontSendNotification);
    knobVertical  .setToggleState (options.knobControl == EditorOptions::KnobControl::Vertical,   juce::dontSendNotification);
    addAndMakeVisible (knobControlLabel);

    // Behaviour toggles
    styleLabel (behaviourLabel, true);
    styleToggle (autoUploadToggle);
    styleToggle (recycleWinToggle);
    autoUploadToggle.setToggleState (options.autoUpload,     juce::dontSendNotification);
    recycleWinToggle.setToggleState (options.recycleWindows, juce::dontSendNotification);
    addAndMakeVisible (behaviourLabel);
    addAndMakeVisible (autoUploadToggle);
    addAndMakeVisible (recycleWinToggle);

    // Preset library
    styleLabel (libraryLabel, true);
    styleTextEditor (libraryPath);
    libraryPath.setText (options.presetLibraryRoot.getFullPathName(), juce::dontSendNotification);
    libraryPath.setTextToShowWhenEmpty ("Choose a root folder. Nomad2026 will create Patches and Snippets inside it.", kDim);
    styleBtn (browseLibraryButton);
    browseLibraryButton.onClick = [this]() { browseLibraryRoot(); };
    addAndMakeVisible (libraryLabel);
    addAndMakeVisible (libraryPath);
    addAndMakeVisible (browseLibraryButton);

    // Buttons
    styleBtn (okButton, true);
    styleBtn (cancelButton);
    okButton    .onClick = [this]() { apply(); };
    cancelButton.onClick = [this]() { close(); };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (560, 450);
}

// ─── paint ───────────────────────────────────────────────────────────────────

void EditorOptionsDialog::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kGold);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText ("Editor Options", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (kSep);
    g.fillRect (0, 31, getWidth(), 1);

    // Separators — positions match resized() math:
    //   after cable:     32 + 8 + (16+2) + 4*22 + 8  = 154
    //   after knob:     154 + 8 + (16+2) + 3*22 + 8  = 254
    //   after behaviour:254 + 8 + (16+2) + 2*22 + 12 = 336
    //   after library: 336 + 8 + (16+2) + 26 + 12 = 400
    const float x0 = 14.0f, x1 = static_cast<float> (getWidth() - 14);
    for (int sy : { 154, 254, 336, 400 })
    {
        g.setColour (kSep);
        g.drawHorizontalLine (sy, x0, x1);
    }
}

// ─── resized ─────────────────────────────────────────────────────────────────

void EditorOptionsDialog::resized()
{
    constexpr int pad   = 14;
    constexpr int rowH  = 22;
    constexpr int secH  = 16;
    constexpr int secGap = 8;   // padding before/after a section label
    constexpr int sepGap = 8;   // padding after a separator

    closeButton.setBounds (getWidth() - 32, 2, 28, 28);

    int y = 32 + secGap;  // below title bar

    // ── Cable Style ──────────────────────────────────────────
    cableStyleLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    for (auto* b : { &cableCurvedThick, &cableStraightThick, &cableCurvedThin, &cableStraightThin })
    {
        b->setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
        y += rowH;
    }
    y += secGap;   // → separator at y=154

    // ── Knob Control ─────────────────────────────────────────
    y += sepGap;
    knobControlLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    for (auto* b : { &knobHorizontal, &knobCircular, &knobVertical })
    {
        b->setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
        y += rowH;
    }
    y += secGap;   // → separator at y=254

    // ── Behaviour ────────────────────────────────────────────
    y += sepGap;
    behaviourLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    autoUploadToggle.setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
    y += rowH;
    recycleWinToggle.setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
    y += rowH + 12;  // → separator at y=336

    // ── Preset Library ───────────────────────────────────────
    y += sepGap;
    libraryLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    auto libraryRow = juce::Rectangle<int> (pad + 8, y, getWidth() - pad * 2 - 8, 26);
    browseLibraryButton.setBounds (libraryRow.removeFromRight (92));
    libraryPath.setBounds (libraryRow.removeFromLeft (libraryRow.getWidth() - 8));
    y += 26 + 12; // → separator at y=400

    // ── OK / Cancel ──────────────────────────────────────────
    y += 10;
    const int btnW = 80, btnH = 26;
    cancelButton.setBounds (getWidth() - pad - btnW,          y, btnW, btnH);
    okButton    .setBounds (getWidth() - pad - btnW * 2 - 8,  y, btnW, btnH);
}

// ─── interaction ─────────────────────────────────────────────────────────────

bool EditorOptionsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::returnKey) { okButton.triggerClick(); return true; }
    return false;
}

void EditorOptionsDialog::mouseDown (const juce::MouseEvent& e)
    { if (e.getPosition().getY() < 32) dragger.startDraggingComponent (this, e); }
void EditorOptionsDialog::mouseDrag (const juce::MouseEvent& e)
    { dragger.dragComponent (this, e, nullptr); }

void EditorOptionsDialog::close() { removeFromDesktop(); delete this; }

void EditorOptionsDialog::browseLibraryRoot()
{
    auto start = options.presetLibraryRoot == juce::File()
        ? juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        : options.presetLibraryRoot;

    folderChooser = std::make_shared<juce::FileChooser> ("Choose Preset Library Folder", start);
    auto chooser = folderChooser;
    juce::Component::SafePointer<EditorOptionsDialog> safeThis (this);
    folderChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safeThis, chooser](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;

            auto folder = fc.getResult();
            if (folder == juce::File())
                return;

            safeThis->options.presetLibraryRoot = folder;
            safeThis->libraryPath.setText (folder.getFullPathName(), juce::dontSendNotification);
        });
}

void EditorOptionsDialog::apply()
{
    if (cableCurvedThick  .getToggleState()) options.cableStyle = EditorOptions::CableStyle::CurvedThick;
    if (cableStraightThick.getToggleState()) options.cableStyle = EditorOptions::CableStyle::StraightThick;
    if (cableCurvedThin   .getToggleState()) options.cableStyle = EditorOptions::CableStyle::CurvedThin;
    if (cableStraightThin .getToggleState()) options.cableStyle = EditorOptions::CableStyle::StraightThin;

    if (knobHorizontal.getToggleState()) options.knobControl = EditorOptions::KnobControl::Horizontal;
    if (knobCircular  .getToggleState()) options.knobControl = EditorOptions::KnobControl::Circular;
    if (knobVertical  .getToggleState()) options.knobControl = EditorOptions::KnobControl::Vertical;

    options.autoUpload     = autoUploadToggle.getToggleState();
    options.recycleWindows = recycleWinToggle .getToggleState();

    if (onChange)
        onChange (options);

    close();
}

// ─── static show ─────────────────────────────────────────────────────────────

void EditorOptionsDialog::show(juce::Component* parent,
                               const EditorOptions& current,
                               std::function<void(const EditorOptions&)> onChangeCb)
{
    auto* dlg = new EditorOptionsDialog (current);
    dlg->onChange = std::move (onChangeCb);

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
