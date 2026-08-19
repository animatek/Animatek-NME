#include "EditorOptionsDialog.h"
#include "ThemeRegistry.h"

// ─── Color palette ───────────────────────────────────────────────────────────
static const AppThemePalette& p() { return AppTheme::palette(); }
static void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId,         p().textSecondary);
    b.setColour (juce::ToggleButton::tickColourId,         p().textPrimary);
    b.setColour (juce::ToggleButton::tickDisabledColourId, p().textMuted);
}
static void styleLabel (juce::Label& l, bool section = false)
{
    l.setFont (section ? juce::Font (AppTheme::uiFont (11.0f).withStyle ("Bold"))
                       : juce::Font (AppTheme::uiFont (12.0f)));
    l.setColour (juce::Label::textColourId,       section ? p().textPrimary : p().textSecondary);
    l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}
static void styleBtn (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,   p().buttonBackground);
    b.setColour (juce::TextButton::buttonOnColourId, p().buttonActive);
    b.setColour (juce::TextButton::textColourOffId,  p().textSecondary);
    b.setColour (juce::TextButton::textColourOnId,   p().textPrimary);
}
static void styleTextEditor (juce::TextEditor& e)
{
    e.setReadOnly (true);
    e.setColour (juce::TextEditor::backgroundColourId, p().inputBackground);
    e.setColour (juce::TextEditor::textColourId,       p().textSecondary);
    e.setColour (juce::TextEditor::outlineColourId,    p().borderColor);
    e.setColour (juce::TextEditor::focusedOutlineColourId, p().buttonActive);
}

// ─── EditorOptions persistence ───────────────────────────────────────────────

const std::vector<EditorOptions::SendRate>& EditorOptions::sendRates()
{
    // batch params per 'intervalMs' tick. Approx rate = batch * 1000 / intervalMs.
    static const std::vector<SendRate> rates {
        { "Safe (~160/s)",     4, 25 },
        { "Balanced (~400/s)", 8, 20 },
        { "Fast (~800/s)",    12, 15 },
        { "Maximum (~1600/s)",16, 10 },
    };
    return rates;
}

EditorOptions EditorOptions::load(juce::PropertiesFile* props)
{
    EditorOptions o;
    if (!props) return o;
    // Migrate pre-0.7 "appearanceTheme" (0=Soft chrome, 1=Deep chrome; canvas was always Dark);
    // a fresh install with neither key defaults to "Nord" (index 6).
    const int legacyIndex = props->getIntValue("appearanceTheme", -1) == 1 ? 2 : 6;
    o.uiThemeIndex   = props->getIntValue("uiThemeIndex", legacyIndex);
    o.cableStyle     = static_cast<CableStyle>  (props->getIntValue  ("cableStyle",      0));
    // Vertical is the default: it is how the original editor behaves, and
    // what people reach for first.
    o.knobControl    = static_cast<KnobControl> (props->getIntValue  ("knobControl",     2));
    o.autoUpload     = props->getBoolValue  ("autoUpload",     true);
    o.askSlotOnOpen  = props->getBoolValue  ("askSlotOnOpen",  true);
    o.wireframe      = props->getBoolValue  ("wireframe",      false);
    o.animateTiling  = props->getBoolValue  ("animateTiling",  true);
    o.synthDisplayCaptions = props->getBoolValue ("synthDisplayCaptions", false);
    o.moduleIconBar  = props->getBoolValue  ("moduleIconBar",  true);
    o.moduleIconBarCategory = props->getValue ("moduleIconBarCategory", "In/Out");
    o.mcpBridgeEnabled = props->getBoolValue("mcpBridgeEnabled", false);
    o.cableOpacity   = static_cast<float>   (props->getDoubleValue("cableOpacity", 0.80));
    o.sendRateIndex  = juce::jlimit (0, static_cast<int> (sendRates().size()) - 1,
                                     props->getIntValue ("sendRateIndex", 1));
    auto libraryPath = props->getValue ("presetLibraryRoot", {});
    if (libraryPath.isNotEmpty())
        o.presetLibraryRoot = juce::File (libraryPath);
    return o;
}

void EditorOptions::save(juce::PropertiesFile* props) const
{
    if (!props) return;
    props->setValue ("uiThemeIndex",    uiThemeIndex);
    props->setValue ("cableStyle",      static_cast<int> (cableStyle));
    props->setValue ("knobControl",     static_cast<int> (knobControl));
    props->setValue ("autoUpload",      autoUpload);
    props->setValue ("askSlotOnOpen",   askSlotOnOpen);
    props->setValue ("wireframe",       wireframe);
    props->setValue ("animateTiling",   animateTiling);
    props->setValue ("synthDisplayCaptions", synthDisplayCaptions);
    props->setValue ("moduleIconBar",   moduleIconBar);
    props->setValue ("moduleIconBarCategory", moduleIconBarCategory);
    props->setValue ("mcpBridgeEnabled", mcpBridgeEnabled);
    props->setValue ("cableOpacity",    static_cast<double>(cableOpacity));
    props->setValue ("sendRateIndex",   sendRateIndex);
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

juce::File EditorOptions::getBanksFolder() const
{
    return presetLibraryRoot == juce::File() ? juce::File() : presetLibraryRoot.getChildFile ("Banks");
}

// Module presets ("<Type>.pchp" packs) sit alongside patches, snippets and
// banks so a preset library is shared the same way everything else is.
juce::File EditorOptions::getPresetsFolder() const
{
    return presetLibraryRoot == juce::File() ? juce::File() : presetLibraryRoot.getChildFile ("Presets");
}

bool EditorOptions::ensureLibraryFolders() const
{
    if (presetLibraryRoot == juce::File())
        return false;

    auto rootOk = presetLibraryRoot.createDirectory().wasOk();
    auto patchesOk = getPatchesFolder().createDirectory().wasOk();
    auto snippetsOk = getSnippetsFolder().createDirectory().wasOk();
    auto banksOk = getBanksFolder().createDirectory().wasOk();
    auto presetsOk = getPresetsFolder().createDirectory().wasOk();
    return rootOk && patchesOk && snippetsOk && banksOk && presetsOk;
}

// ─── EditorOptionsDialog ─────────────────────────────────────────────────────

EditorOptionsDialog::EditorOptionsDialog(const EditorOptions& current,
                                         McpBridgeStatusKind mcpStatus,
                                         const juce::String& mcpStatusText,
                                         const juce::String& mcpCommand)
    : options(current), openingOpacity(current.cableOpacity)
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

    // Cable opacity. This lived on a slider inside the View menu, which the
    // macOS menu bar cannot host at all: it is a real NSMenu and JUCE drops any
    // custom component put in one, so the setting was out of reach there
    // (issue #74). It belongs next to the cable style anyway.
    styleLabel (cableOpacityLabel);
    cableOpacitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    cableOpacitySlider.setRange (0.0, 100.0, 1.0);
    cableOpacitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    cableOpacitySlider.setTextValueSuffix ("%");
    cableOpacitySlider.setValue (juce::roundToInt (options.cableOpacity * 100.0f),
                                 juce::dontSendNotification);
    cableOpacitySlider.setColour (juce::Slider::textBoxTextColourId,       p().textSecondary);
    cableOpacitySlider.setColour (juce::Slider::textBoxBackgroundColourId, p().inputBackground);
    cableOpacitySlider.setColour (juce::Slider::textBoxOutlineColourId,    p().borderColor);
    cableOpacitySlider.setColour (juce::Slider::thumbColourId,             p().accentActive);
    cableOpacitySlider.setColour (juce::Slider::trackColourId,             p().buttonActive);
    cableOpacitySlider.setTooltip ("How solid the cables are drawn. Lower it to read the modules "
                                   "under a dense patch. The canvas follows the slider while you "
                                   "drag it; Cancel puts it back.");
    cableOpacitySlider.onValueChange = [this]() {
        if (onCableOpacityPreview)
            onCableOpacityPreview (static_cast<float> (cableOpacitySlider.getValue()) * 0.01f);
    };
    addAndMakeVisible (cableOpacityLabel);
    addAndMakeVisible (cableOpacitySlider);

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
    styleToggle (wireframeToggle);
    styleToggle (animateTilingToggle);
    styleToggle (synthCaptionToggle);
    styleToggle (askSlotToggle);
    autoUploadToggle.setToggleState (options.autoUpload,     juce::dontSendNotification);
    askSlotToggle   .setToggleState (options.askSlotOnOpen,  juce::dontSendNotification);
    wireframeToggle .setToggleState (options.wireframe,      juce::dontSendNotification);
    animateTilingToggle.setToggleState (options.animateTiling, juce::dontSendNotification);
    synthCaptionToggle.setToggleState (options.synthDisplayCaptions, juce::dontSendNotification);
    addAndMakeVisible (behaviourLabel);
    addAndMakeVisible (autoUploadToggle);
    addAndMakeVisible (wireframeToggle);
    addAndMakeVisible (animateTilingToggle);
    addAndMakeVisible (synthCaptionToggle);
    addAndMakeVisible (askSlotToggle);

    // Send speed selector — synth parameter throughput (Mutator/Random)
    styleLabel (sendRateLabel);
    const auto& rates = EditorOptions::sendRates();
    for (int i = 0; i < static_cast<int> (rates.size()); ++i)
        sendRateSelector.addItem (rates[static_cast<size_t> (i)].label, i + 1);
    sendRateSelector.setSelectedId (options.sendRateIndex + 1, juce::dontSendNotification);
    sendRateSelector.setColour (juce::ComboBox::backgroundColourId, p().inputBackground);
    sendRateSelector.setColour (juce::ComboBox::outlineColourId,    p().borderColor);
    sendRateSelector.setColour (juce::ComboBox::textColourId,       p().textSecondary);
    sendRateSelector.setColour (juce::ComboBox::arrowColourId,      p().textSecondary);
    sendRateSelector.setTooltip ("How fast parameter changes are streamed to the synth. "
                                 "Higher is more responsive for the Mutator but may overrun "
                                 "the G1 on big patches. Lower it if the connection drops.");
    addAndMakeVisible (sendRateLabel);
    addAndMakeVisible (sendRateSelector);

    // MCP Bridge
    styleLabel (mcpBridgeLabel, true);
    styleToggle (mcpBridgeToggle);
    mcpBridgeToggle.setToggleState (options.mcpBridgeEnabled, juce::dontSendNotification);
    mcpBridgeToggle.setTooltip ("Lets an MCP client (e.g. an AI agent) add modules and connect "
                                "cables in the live patch over a localhost-only control socket. "
                                "See mcp-bridge/README.md.");
    styleLabel (mcpBridgeStatusLabel);
    mcpBridgeStatusLabel.setText (mcpStatusText, juce::dontSendNotification);
    {
        juce::Colour col = p().textMuted;
        if (mcpStatus == McpBridgeStatusKind::Listening) col = p().accentSuccess;
        else if (mcpStatus == McpBridgeStatusKind::Failed) col = p().accentWarning;
        mcpBridgeStatusLabel.setColour (juce::Label::textColourId, col);
    }
    addAndMakeVisible (mcpBridgeLabel);
    addAndMakeVisible (mcpBridgeToggle);
    addAndMakeVisible (mcpBridgeStatusLabel);

    styleTextEditor (mcpBridgeCommand);
    mcpBridgeCommand.setText (mcpCommand, juce::dontSendNotification);
    mcpBridgeCommand.setTextToShowWhenEmpty ("mcp-bridge/server.py not found next to the executable", p().textMuted);
    mcpBridgeCommand.setTooltip ("The stdio command an MCP client (Claude Code, Claude Desktop, ...) "
                                 "needs to register this bridge. Select all + copy, or use the button.");
    styleBtn (mcpBridgeCopyButton);
    mcpBridgeCopyButton.onClick = [this]() {
        juce::SystemClipboard::copyTextToClipboard (mcpBridgeCommand.getText());
        mcpBridgeCopyButton.setButtonText ("Copied!");
        juce::Component::SafePointer<EditorOptionsDialog> safeThis (this);
        juce::Timer::callAfterDelay (1500, [safeThis]() {
            if (safeThis != nullptr) safeThis->mcpBridgeCopyButton.setButtonText ("Copy");
        });
    };
    addAndMakeVisible (mcpBridgeCommand);
    addAndMakeVisible (mcpBridgeCopyButton);

    // Preset library
    styleLabel (libraryLabel, true);
    styleTextEditor (libraryPath);
    libraryPath.setText (options.presetLibraryRoot.getFullPathName(), juce::dontSendNotification);
    libraryPath.setTextToShowWhenEmpty ("Choose a root folder. Animatek NME will create Patches and Snippets inside it.", p().textMuted);
    styleBtn (browseLibraryButton);
    browseLibraryButton.onClick = [this]() { browseLibraryRoot(); };
    addAndMakeVisible (libraryLabel);
    addAndMakeVisible (libraryPath);
    addAndMakeVisible (browseLibraryButton);

    // Buttons
    styleBtn (okButton);
    styleBtn (cancelButton);
    okButton    .onClick = [this]() { apply(); };
    cancelButton.onClick = [this]() { close(); };
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (dialogWidth, layoutComponents (false));
}

void EditorOptionsDialog::populateThemeSelector()
{
    const auto themeNames = ThemeRegistry::names();
    for (int i = 0; i < themeNames.size(); ++i)
        themeSelector.addItem(themeNames[i], i + 1);
    themeSelector.setSelectedId(options.uiThemeIndex + 1, juce::dontSendNotification);
    themeSelector.setColour(juce::ComboBox::backgroundColourId, p().inputBackground);
    themeSelector.setColour(juce::ComboBox::outlineColourId, p().borderColor);
    themeSelector.setColour(juce::ComboBox::textColourId, p().textSecondary);
    themeSelector.setColour(juce::ComboBox::arrowColourId, p().textSecondary);
}

// ─── paint ───────────────────────────────────────────────────────────────────

void EditorOptionsDialog::paint (juce::Graphics& g)
{
    g.fillAll (p().backgroundMain);

    g.setColour (p().textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
    g.drawText ("Editor Options", 10, 0, getWidth() - 44, 32, juce::Justification::centredLeft);

    g.setColour (p().buttonActive);
    g.fillRect (0, 31, getWidth(), 1);

    // Separators are computed by layoutComponents() (the same pass that
    // places every component), never hand-copied here - so a line can never
    // drift out of sync with what resized() actually did.
    const float x0 = 14.0f, x1 = static_cast<float> (getWidth() - 14);
    g.setColour (p().buttonActive);
    for (int sy : sectionSeparatorsY)
        g.drawHorizontalLine (sy, x0, x1);
}

// ─── resized ─────────────────────────────────────────────────────────────────

void EditorOptionsDialog::resized()
{
    layoutComponents (true);
}

int EditorOptionsDialog::layoutComponents (bool apply)
{
    constexpr int pad   = 14;
    constexpr int rowH  = 22;
    constexpr int secH  = 16;
    constexpr int secGap = 8;   // padding before/after a section label
    constexpr int sepGap = 8;   // padding before a separator line
    const int w = dialogWidth;

    sectionSeparatorsY.clear();
    auto place = [apply] (juce::Component& c, int x, int y, int cw, int ch) {
        if (apply) c.setBounds (x, y, cw, ch);
    };
    // Marks a section boundary: adds the gap, records where the divider
    // line goes, and returns the (possibly updated) y - call right before
    // laying out each section's header label (except the very first one).
    auto sectionSep = [this, &sepGap] (int yIn) {
        int y = yIn + sepGap;
        sectionSeparatorsY.push_back (y);
        return y + 4;
    };

    if (apply) closeButton.setBounds (w - 32, 2, 28, 28);

    int y = 32 + secGap;  // below title bar

    // ── Appearance ───────────────────────────────────────────
    place (appearanceLabel, pad, y, w - pad * 2, secH);
    y += secH + 6;
    place (themeLabel,    pad + 8,  y, 80, rowH);
    place (themeSelector, pad + 92, y, w - pad * 2 - 100, rowH);
    y += rowH + secGap;

    // ── Cable Style ──────────────────────────────────────────
    y = sectionSep (y);
    place (cableStyleLabel, pad, y, w - pad * 2, secH);
    y += secH + 2;
    for (auto* b : { &cableCurvedThick, &cableStraightThick, &cableCurvedThin, &cableStraightThin })
    {
        place (*b, pad + 8, y, w - pad * 2 - 8, rowH);
        y += rowH;
    }
    y += 4;
    place (cableOpacityLabel,  pad + 8,  y, 80, rowH);
    place (cableOpacitySlider, pad + 92, y, w - pad * 2 - 100, rowH);
    y += rowH + secGap;

    // ── Knob Control ─────────────────────────────────────────
    y = sectionSep (y);
    place (knobControlLabel, pad, y, w - pad * 2, secH);
    y += secH + 2;
    for (auto* b : { &knobHorizontal, &knobCircular, &knobVertical })
    {
        place (*b, pad + 8, y, w - pad * 2 - 8, rowH);
        y += rowH;
    }
    y += secGap;

    // ── Behaviour ────────────────────────────────────────────
    y = sectionSep (y);
    place (behaviourLabel, pad, y, w - pad * 2, secH);
    y += secH + 2;
    place (autoUploadToggle, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH;
    place (wireframeToggle, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH;
    place (animateTilingToggle, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH;
    place (askSlotToggle, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH;
    place (synthCaptionToggle, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH + 4;
    place (sendRateLabel,    pad + 8,  y, 80, rowH);
    place (sendRateSelector, pad + 92, y, w - pad * 2 - 100, rowH);
    y += rowH + 12;

    // ── MCP Bridge ────────────────────────────────────────────
    y = sectionSep (y);
    place (mcpBridgeLabel, pad, y, w - pad * 2, secH);
    y += secH + 2;
    place (mcpBridgeToggle, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH;
    place (mcpBridgeStatusLabel, pad + 8, y, w - pad * 2 - 8, rowH);
    y += rowH + 6;
    place (mcpBridgeCopyButton, w - pad - 72, y, 72, 26);
    place (mcpBridgeCommand,    pad + 8, y, w - pad * 2 - 8 - 72 - 8, 26);
    y += 26 + secGap;

    // ── Preset Library ───────────────────────────────────────
    y = sectionSep (y);
    place (libraryLabel, pad, y, w - pad * 2, secH);
    y += secH + 2;
    place (browseLibraryButton, w - pad - 92, y, 92, 26);
    place (libraryPath,         pad + 8, y, w - pad * 2 - 8 - 92 - 8, 26);
    y += 26 + 12;

    // ── OK / Cancel ──────────────────────────────────────────
    y += 10;
    const int btnW = 80, btnH = 26;
    place (cancelButton, w - pad - btnW,         y, btnW, btnH);
    place (okButton,     w - pad - btnW * 2 - 8, y, btnW, btnH);
    y += btnH;

    return y + pad;
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

void EditorOptionsDialog::close()
{
    // Escape, the X and Cancel all land here. Anything but OK has to undo the
    // opacity the canvas has been previewing while the slider moved.
    if (! applied && onCableOpacityPreview)
        onCableOpacityPreview (openingOpacity);

    removeFromDesktop();
    delete this;
}

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

    options.uiThemeIndex   = themeSelector.getSelectedId() - 1;
    options.autoUpload     = autoUploadToggle.getToggleState();
    options.wireframe      = wireframeToggle  .getToggleState();
    options.animateTiling  = animateTilingToggle.getToggleState();
    options.askSlotOnOpen  = askSlotToggle.getToggleState();
    options.synthDisplayCaptions = synthCaptionToggle.getToggleState();
    options.sendRateIndex  = sendRateSelector.getSelectedId() - 1;
    options.mcpBridgeEnabled = mcpBridgeToggle.getToggleState();
    options.cableOpacity   = static_cast<float> (cableOpacitySlider.getValue()) * 0.01f;

    applied = true;
    if (onChange)
        onChange (options);

    close();
}

// ─── static show ─────────────────────────────────────────────────────────────

EditorOptionsDialog* EditorOptionsDialog::show(juce::Component* parent,
                               const EditorOptions& current,
                               McpBridgeStatusKind mcpStatus,
                               const juce::String& mcpStatusText,
                               const juce::String& mcpCommand,
                               std::function<void(const EditorOptions&)> onChangeCb)
{
    auto* dlg = new EditorOptionsDialog (current, mcpStatus, mcpStatusText, mcpCommand);
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
    return dlg;
}
