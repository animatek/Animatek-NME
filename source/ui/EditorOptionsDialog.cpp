#include "EditorOptionsDialog.h"

// ─── Color palette ───────────────────────────────────────────────────────────
static const AppThemePalette& p() { return AppTheme::palette(); }
static const juce::Colour kOkBg { 0xff1e3a1e };
static const juce::Colour kOkOn { 0xff2a5a2a };

static void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId,         p().textSecondary);
    b.setColour (juce::ToggleButton::tickColourId,         p().accentWarning);
    b.setColour (juce::ToggleButton::tickDisabledColourId, p().textMuted);
}
static void styleLabel (juce::Label& l, bool section = false)
{
    l.setFont (section ? juce::Font (juce::FontOptions (11.0f, juce::Font::bold))
                       : juce::Font (juce::FontOptions (12.0f)));
    l.setColour (juce::Label::textColourId,       section ? p().accentActive : p().textSecondary);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}
static void styleBtn (juce::TextButton& b, bool isOk = false)
{
    b.setColour (juce::TextButton::buttonColourId,   isOk ? kOkBg : p().buttonBackground);
    b.setColour (juce::TextButton::buttonOnColourId, isOk ? kOkOn : p().buttonActive);
    b.setColour (juce::TextButton::textColourOffId,  isOk ? juce::Colour (0xffaaffaa) : p().textSecondary);
}
static void styleTextEditor (juce::TextEditor& e)
{
    e.setReadOnly (true);
    e.setColour (juce::TextEditor::backgroundColourId, p().inputBackground);
    e.setColour (juce::TextEditor::textColourId,       p().textSecondary);
    e.setColour (juce::TextEditor::outlineColourId,    p().borderColor);
    e.setColour (juce::TextEditor::focusedOutlineColourId, p().accentWarning);
}

// ─── EditorOptions persistence ───────────────────────────────────────────────

EditorOptions EditorOptions::load(juce::PropertiesFile* props)
{
    EditorOptions o;
    if (!props) return o;
    o.appearanceTheme = AppTheme::themeFromInt(props->getIntValue("appearanceTheme", 0));
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
    props->setValue ("appearanceTheme", static_cast<int> (appearanceTheme));
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

    // Appearance
    styleLabel (appearanceLabel, true);
    styleLabel (themeLabel);
    populateThemeSelector();
    addAndMakeVisible (appearanceLabel);
    addAndMakeVisible (themeLabel);
    addAndMakeVisible (themeSelector);

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
    libraryPath.setTextToShowWhenEmpty ("Choose a root folder. Nomad2026 will create Patches and Snippets inside it.", p().textMuted);
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

    setSize (560, 510);
}

void EditorOptionsDialog::populateThemeSelector()
{
    themeSelector.addItem(AppTheme::displayName(AppThemeId::SoftDarkGrey), 1);
    themeSelector.addItem(AppTheme::displayName(AppThemeId::DeepDarkGrey), 2);
    themeSelector.setSelectedId(static_cast<int>(options.appearanceTheme) + 1, juce::dontSendNotification);
    themeSelector.setColour(juce::ComboBox::backgroundColourId, p().inputBackground);
    themeSelector.setColour(juce::ComboBox::outlineColourId, p().borderColor);
    themeSelector.setColour(juce::ComboBox::textColourId, p().textSecondary);
    themeSelector.setColour(juce::ComboBox::arrowColourId, p().accentWarning);
}

// ─── paint ───────────────────────────────────────────────────────────────────

void EditorOptionsDialog::paint (juce::Graphics& g)
{
    g.fillAll (p().backgroundMain);

    g.setColour (p().accentActive);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText ("Editor Options", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (p().buttonActive);
    g.fillRect (0, 31, getWidth(), 1);

    // Separators — positions match resized() math:
    // Separators match resized() section starts.
    const float x0 = 14.0f, x1 = static_cast<float> (getWidth() - 14);
    for (int sy : { 104, 226, 326, 408, 472 })
    {
        g.setColour (p().buttonActive);
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

    // ── Appearance ───────────────────────────────────────────
    appearanceLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 6;
    themeLabel.setBounds (pad + 8, y, 80, rowH);
    themeSelector.setBounds (pad + 92, y, getWidth() - pad * 2 - 100, rowH);
    y += rowH + secGap;

    // ── Cable Style ──────────────────────────────────────────
    y += sepGap;
    cableStyleLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    for (auto* b : { &cableCurvedThick, &cableStraightThick, &cableCurvedThin, &cableStraightThin })
    {
        b->setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
        y += rowH;
    }
    y += secGap;

    // ── Knob Control ─────────────────────────────────────────
    y += sepGap;
    knobControlLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    for (auto* b : { &knobHorizontal, &knobCircular, &knobVertical })
    {
        b->setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
        y += rowH;
    }
    y += secGap;

    // ── Behaviour ────────────────────────────────────────────
    y += sepGap;
    behaviourLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    autoUploadToggle.setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
    y += rowH;
    recycleWinToggle.setBounds (pad + 8, y, getWidth() - pad * 2 - 8, rowH);
    y += rowH + 12;

    // ── Preset Library ───────────────────────────────────────
    y += sepGap;
    libraryLabel.setBounds (pad, y, getWidth() - pad * 2, secH);
    y += secH + 2;
    auto libraryRow = juce::Rectangle<int> (pad + 8, y, getWidth() - pad * 2 - 8, 26);
    browseLibraryButton.setBounds (libraryRow.removeFromRight (92));
    libraryPath.setBounds (libraryRow.removeFromLeft (libraryRow.getWidth() - 8));
    y += 26 + 12;

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

    options.appearanceTheme = AppTheme::themeFromInt(themeSelector.getSelectedId() - 1);
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
