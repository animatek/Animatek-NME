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
    // A size that fills the field, and the text centred in it, both ways
    e.setFont (juce::Font (AppTheme::uiFont (14.0f)));
    e.setJustification (juce::Justification::centredLeft);
    e.setIndents (12, 0);
    e.setColour (juce::TextEditor::backgroundColourId, p().inputBackground);
    e.setColour (juce::TextEditor::textColourId,       p().textSecondary);
    e.setColour (juce::TextEditor::outlineColourId,    p().borderColor);
    e.setColour (juce::TextEditor::focusedOutlineColourId, p().buttonActive);
}

// ─── EditorOptions persistence ───────────────────────────────────────────────

const std::vector<float>& EditorOptions::uiScales()
{
    static const std::vector<float> scales { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f };
    return scales;
}

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
    o.uiThemeName    = props->getValue("uiThemeName");
    if (const int byName = o.uiThemeName.isNotEmpty() ? ThemeRegistry::indexOfName(o.uiThemeName) : -1;
        byName >= 0)
        o.uiThemeIndex = byName;
    o.cableStyle     = static_cast<CableStyle>  (props->getIntValue  ("cableStyle",      0));
    // Vertical is the default: it is how the original editor behaves, and
    // what people reach for first.
    o.knobControl    = static_cast<KnobControl> (props->getIntValue  ("knobControl",     2));
    o.autoUpload     = props->getBoolValue  ("autoUpload",     true);
    o.askSlotOnOpen  = props->getBoolValue  ("askSlotOnOpen",  true);
    o.wireframe      = props->getBoolValue  ("wireframe",      false);
    o.hardwareLook   = props->getBoolValue  ("hardwareLook",   false);
    o.hardwareKnobStyle = juce::jlimit (0, 2, props->getIntValue ("hardwareKnobStyle", 2));
    o.flatKnobs      = props->getBoolValue  ("flatKnobs",      false);
    // "wireframe,hardwareLook,knobStyle,flatKnobs" as 0/1/0-2/0/1; empty when no theme sets one.
    if (const auto t = juce::StringArray::fromTokens (props->getValue ("renderBeforeTheme"), ",", {});
        t.size() == 4)
        o.renderBeforeTheme = RenderStyle { t[0].getIntValue() != 0, t[1].getIntValue() != 0,
                                            juce::jlimit (0, 2, t[2].getIntValue()), t[3].getIntValue() != 0 };
    o.animateTiling  = props->getBoolValue  ("animateTiling",  true);
    o.restoreAllWindows = props->getBoolValue ("restoreAllWindows", false);
    o.synthDisplayCaptions = props->getBoolValue ("synthDisplayCaptions", false);
    o.seqNoteNames   = props->getBoolValue  ("seqNoteNames",   false);
    o.checkCommunityThemes   = props->getBoolValue ("checkCommunityThemes", true);
    o.communityThemesChecked = props->getValue ("communityThemesChecked", "0").getLargeIntValue();
    o.communityThemesSeen    = props->getValue ("communityThemesSeen");
    o.checkNewVersion        = props->getBoolValue ("checkNewVersion", true);
    o.newVersionChecked      = props->getValue ("newVersionChecked", "0").getLargeIntValue();
    o.latestVersion          = props->getValue ("latestVersion");
    o.newVersionSeen         = props->getValue ("newVersionSeen");
    o.moduleIconBar  = props->getBoolValue  ("moduleIconBar",  true);
    o.moduleIconBarCategory = props->getValue ("moduleIconBarCategory", "In/Out");
    o.mcpBridgeEnabled = props->getBoolValue("mcpBridgeEnabled", false);
    o.cableOpacity   = static_cast<float>   (props->getDoubleValue("cableOpacity", 0.80));
    o.uiScale        = juce::jlimit (1.0f, 3.0f, static_cast<float> (props->getDoubleValue ("uiScale", 1.0)));
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
    props->setValue ("uiThemeName",     ThemeRegistry::get (uiThemeIndex).name);
    props->setValue ("cableStyle",      static_cast<int> (cableStyle));
    props->setValue ("knobControl",     static_cast<int> (knobControl));
    props->setValue ("autoUpload",      autoUpload);
    props->setValue ("askSlotOnOpen",   askSlotOnOpen);
    props->setValue ("wireframe",       wireframe);
    props->setValue ("hardwareLook",    hardwareLook);
    props->setValue ("hardwareKnobStyle", hardwareKnobStyle);
    props->setValue ("flatKnobs",       flatKnobs);
    props->setValue ("renderBeforeTheme",
                     renderBeforeTheme ? juce::String (int (renderBeforeTheme->wireframe)) + ","
                                             + juce::String (int (renderBeforeTheme->hardwareLook)) + ","
                                             + juce::String (renderBeforeTheme->knobStyle) + ","
                                             + juce::String (int (renderBeforeTheme->flatKnobs))
                                       : juce::String());
    props->setValue ("animateTiling",   animateTiling);
    props->setValue ("restoreAllWindows", restoreAllWindows);
    props->setValue ("synthDisplayCaptions", synthDisplayCaptions);
    props->setValue ("seqNoteNames",    seqNoteNames);
    props->setValue ("checkCommunityThemes",   checkCommunityThemes);
    props->setValue ("communityThemesChecked", juce::String (communityThemesChecked));
    props->setValue ("communityThemesSeen",    communityThemesSeen);
    props->setValue ("checkNewVersion",        checkNewVersion);
    props->setValue ("newVersionChecked",      juce::String (newVersionChecked));
    props->setValue ("latestVersion",          latestVersion);
    props->setValue ("newVersionSeen",         newVersionSeen);
    props->setValue ("moduleIconBar",   moduleIconBar);
    props->setValue ("moduleIconBarCategory", moduleIconBarCategory);
    props->setValue ("mcpBridgeEnabled", mcpBridgeEnabled);
    props->setValue ("cableOpacity",    static_cast<double>(cableOpacity));
    props->setValue ("sendRateIndex",   sendRateIndex);
    props->setValue ("uiScale",         static_cast<double> (uiScale));
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

// Slot sets: one folder per set, holding its patches and a .nmset manifest.
juce::File EditorOptions::getSetsFolder() const
{
    return presetLibraryRoot == juce::File() ? juce::File() : presetLibraryRoot.getChildFile ("Sets");
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
    auto setsOk = getSetsFolder().createDirectory().wasOk();
    return rootOk && patchesOk && snippetsOk && banksOk && presetsOk && setsOk;
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

    // Hardware look + knob style
    styleToggle (hardwareLookToggle);
    hardwareLookToggle.setToggleState (options.hardwareLook, juce::dontSendNotification);
    hardwareLookToggle.setTooltip ("Draws knobs, jacks, module panels, displays and cables as "
                                   "physical hardware. Works with any theme.");
    hardwareLookToggle.onClick = [this]() {
        knobStyleSelector.setEnabled (hardwareLookToggle.getToggleState());
        flatKnobsToggle.setEnabled (! hardwareLookToggle.getToggleState());
    };
    addAndMakeVisible (hardwareLookToggle);

    styleLabel (knobStyleLabel);
    knobStyleSelector.addItem ("Domed cap",       1);
    knobStyleSelector.addItem ("Black knurled",      2);
    knobStyleSelector.addItem ("Aluminium knurled",  3);
    knobStyleSelector.setSelectedId (options.hardwareKnobStyle + 1, juce::dontSendNotification);
    knobStyleSelector.setEnabled (options.hardwareLook);
    knobStyleSelector.setColour (juce::ComboBox::backgroundColourId, p().inputBackground);
    knobStyleSelector.setColour (juce::ComboBox::outlineColourId,    p().borderColor);
    knobStyleSelector.setColour (juce::ComboBox::textColourId,       p().textSecondary);
    knobStyleSelector.setColour (juce::ComboBox::arrowColourId,      p().textSecondary);
    addAndMakeVisible (knobStyleLabel);
    addAndMakeVisible (knobStyleSelector);

    styleToggle (flatKnobsToggle);
    flatKnobsToggle.setToggleState (options.flatKnobs, juce::dontSendNotification);
    flatKnobsToggle.setEnabled (! options.hardwareLook);
    flatKnobsToggle.setTooltip ("Knobs drawn like the ones on the Animatek modules for VCV Rack: "
                                "a dark disc with a thin rim, a white pointer and the value "
                                "painted in blue on the rim. Works with any theme.");
    addAndMakeVisible (flatKnobsToggle);
    addAndMakeVisible (themeLabel);
    addAndMakeVisible (themeSelector);

    // Interface size: the whole UI, for 4K screens where the text is too small to read.
    styleLabel (uiScaleLabel);
    {
        const auto& scales = EditorOptions::uiScales();
        int selected = 1;
        for (int i = 0; i < static_cast<int> (scales.size()); ++i)
        {
            uiScaleSelector.addItem (juce::String (juce::roundToInt (scales[static_cast<size_t> (i)] * 100.0f)) + "%", i + 1);
            if (std::abs (scales[static_cast<size_t> (i)] - options.uiScale) < 0.01f)
                selected = i + 1;
        }
        uiScaleSelector.setSelectedId (selected, juce::dontSendNotification);
    }
    uiScaleSelector.setColour (juce::ComboBox::backgroundColourId, p().inputBackground);
    uiScaleSelector.setColour (juce::ComboBox::outlineColourId,    p().borderColor);
    uiScaleSelector.setColour (juce::ComboBox::textColourId,       p().textSecondary);
    uiScaleSelector.setColour (juce::ComboBox::arrowColourId,      p().textSecondary);
    uiScaleSelector.setTooltip ("Makes every window, menu and text bigger, for 4K screens. "
                                "Applies when you press OK; windows grow with it.");
    addAndMakeVisible (uiScaleLabel);
    addAndMakeVisible (uiScaleSelector);

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
    styleToggle (restoreWindowsToggle);
    styleToggle (synthCaptionToggle);
    styleToggle (askSlotToggle);
    styleToggle (seqNoteNamesToggle);
    styleToggle (communityThemesToggle);
    communityThemesToggle.setToggleState (options.checkCommunityThemes, juce::dontSendNotification);
    communityThemesToggle.setTooltip ("On startup, at most once a day, NME reads the list of themes in "
                                      "github.com/animatek/nme-themes and marks the new ones in View > Theme. "
                                      "Nothing is sent but the request. Off: only when you choose Check Now.");
    addAndMakeVisible (communityThemesToggle);
    styleToggle (newVersionToggle);
    newVersionToggle.setToggleState (options.checkNewVersion, juce::dontSendNotification);
    newVersionToggle.setTooltip ("On startup, at most once a day, NME reads the release tags of "
                                 "github.com/animatek/Animatek-NME. When a newer version is out, About "
                                 "gets a red dot and an item that opens Patreon. It never downloads anything.");
    addAndMakeVisible (newVersionToggle);
    autoUploadToggle.setToggleState (options.autoUpload,     juce::dontSendNotification);
    askSlotToggle   .setToggleState (options.askSlotOnOpen,  juce::dontSendNotification);
    wireframeToggle .setToggleState (options.wireframe,      juce::dontSendNotification);
    animateTilingToggle.setToggleState (options.animateTiling, juce::dontSendNotification);
    restoreWindowsToggle.setToggleState (options.restoreAllWindows, juce::dontSendNotification);
    restoreWindowsToggle.setTooltip ("Off: on startup only Port 1's slot windows come back, and another synth's "
                                     "windows open when it connects and close when it goes. On: every window "
                                     "left open last time comes back, and they stay open.");
    synthCaptionToggle.setToggleState (options.synthDisplayCaptions, juce::dontSendNotification);
    seqNoteNamesToggle.setToggleState (options.seqNoteNames, juce::dontSendNotification);
    addAndMakeVisible (behaviourLabel);
    addAndMakeVisible (autoUploadToggle);
    addAndMakeVisible (wireframeToggle);
    addAndMakeVisible (animateTilingToggle);
    addAndMakeVisible (restoreWindowsToggle);
    addAndMakeVisible (synthCaptionToggle);
    addAndMakeVisible (askSlotToggle);
    addAndMakeVisible (seqNoteNamesToggle);

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
    libraryPath.setTextToShowWhenEmpty ("Choose a root folder. Every .pch under it shows in the Disk tab.", p().textMuted);
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
    showPage (0);
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
    const auto& pal = p();
    g.fillAll (pal.backgroundMain);

    // Title bar
    g.setColour (pal.textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (15.0f)).boldened());
    g.drawText ("Editor Options", 18, 0, getWidth() - 60, titleBarH, juce::Justification::centredLeft);
    g.setColour (pal.borderColor);
    g.fillRect (0, titleBarH - 1, getWidth(), 1);

    // Sidebar
    g.setColour (pal.backgroundPanel);
    g.fillRect (0, titleBarH, sidebarW, getHeight() - titleBarH);
    g.setColour (pal.borderColor);
    g.fillRect (sidebarW - 1, titleBarH, 1, getHeight() - titleBarH);
    for (int i = 0; i < numPages; ++i)
    {
        const auto r = pageButtonBounds (i).toFloat();
        const bool on = (i == page);
        if (on)
        {
            g.setColour (pal.buttonBackground);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (pal.accentActive);
            g.fillRoundedRectangle (r.getX(), r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);
        }
        g.setColour (on ? pal.textPrimary : pal.textSecondary);
        g.setFont (juce::Font (AppTheme::uiFont (14.0f).withStyle (on ? "Bold" : "Regular")));
        g.drawText (pageName (i), r.toNearestInt().withTrimmedLeft (18), juce::Justification::centredLeft, false);
    }

    // The page: its name, and the rows that are a name and a control
    const int x0 = sidebarW + 32;
    g.setColour (pal.textPrimary);
    g.setFont (juce::Font (AppTheme::uiFont (20.0f)).boldened());
    g.drawText (pageName (page), x0, titleBarH + 16, 400, 36, juce::Justification::centredLeft);

    for (const auto& row : infoRows)
    {
        if (row.page != page)
            continue;
        const int textW = row.rect.getWidth() - row.controlW - 28;
        g.setColour (pal.borderColor.withAlpha (0.5f));
        g.fillRect (row.rect.getX(), row.rect.getBottom() - 1, row.rect.getWidth(), 1);
        g.setColour (pal.textPrimary);
        g.setFont (juce::Font (AppTheme::uiFont (14.0f)).boldened());
        g.drawText (row.title, row.rect.getX(), row.rect.getY() + 10, textW, 22, juce::Justification::centredLeft, true);
        g.setColour (pal.textMuted);
        g.setFont (juce::Font (AppTheme::uiFont (12.5f)));
        g.drawFittedText (row.description, row.rect.getX(), row.rect.getY() + 32, textW, row.rect.getHeight() - 34,
                          juce::Justification::topLeft, 2, 1.0f);
    }

    // Footer
    g.setColour (pal.borderColor);
    g.fillRect (sidebarW, getHeight() - footerH, getWidth() - sidebarW, 1);
}


// ─── resized ─────────────────────────────────────────────────────────────────

void EditorOptionsDialog::resized()
{
    layoutComponents (true);
}

const char* EditorOptionsDialog::pageName (int page)
{
    static const char* const names[numPages] = { "Appearance", "Cables", "Knobs", "Behaviour",
                                                 "Synth link", "Library", "Updates", "Advanced" };
    return names[juce::jlimit (0, numPages - 1, page)];
}

juce::Rectangle<int> EditorOptionsDialog::pageButtonBounds (int index) const
{
    return { 8, titleBarH + 14 + index * 42, sidebarW - 16, 38 };
}

// Only the controls of the page that is showing are on screen; every page is laid out all the
// same, so the dialog does not change size and nothing jumps when one is picked.
void EditorOptionsDialog::showPage (int newPage)
{
    page = juce::jlimit (0, numPages - 1, newPage);
    auto only = [this] (std::initializer_list<juce::Component*> controls, int forPage)
    {
        for (auto* c : controls)
            c->setVisible (page == forPage);
    };
    only ({ &themeSelector, &uiScaleSelector, &hardwareLookToggle, &knobStyleSelector, &flatKnobsToggle, &wireframeToggle }, 0);
    only ({ &cableCurvedThick, &cableStraightThick, &cableCurvedThin, &cableStraightThin, &cableOpacitySlider }, 1);
    only ({ &knobHorizontal, &knobCircular, &knobVertical }, 2);
    only ({ &askSlotToggle, &animateTilingToggle, &restoreWindowsToggle, &seqNoteNamesToggle }, 3);
    only ({ &autoUploadToggle, &synthCaptionToggle, &sendRateSelector }, 4);
    only ({ &libraryPath, &browseLibraryButton }, 5);
    only ({ &communityThemesToggle, &newVersionToggle }, 6);
    only ({ &mcpBridgeToggle, &mcpBridgeStatusLabel, &mcpBridgeCommand, &mcpBridgeCopyButton }, 7);

    // The old section and row labels are drawn by paint() now
    for (auto* l : { &appearanceLabel, &themeLabel, &uiScaleLabel, &knobStyleLabel, &cableStyleLabel,
                     &cableOpacityLabel, &knobControlLabel, &behaviourLabel, &sendRateLabel,
                     &mcpBridgeLabel, &libraryLabel })
        l->setVisible (false);

    repaint();
}

int EditorOptionsDialog::layoutComponents (bool apply)
{
    const int w = dialogWidth, h = dialogHeight;
    const int x0 = sidebarW + 32;
    const int cw = w - x0 - 32;                  // width of a page's rows
    const int top = titleBarH + 70;              // under the page's name
    constexpr int rowD = 66;                     // a row with a name and a line saying what it does
    constexpr int rowS = 48;                     // a row with just a name

    infoRows.clear();
    auto place = [apply] (juce::Component& c, juce::Rectangle<int> r) { if (apply) c.setBounds (r); };

    if (apply) closeButton.setBounds (w - 36, 6, 28, 28);

    // A switch or radio row is the whole row
    auto option = [&] (OptionRow& o, int y, int rowH) { place (o, { x0, y, cw, rowH }); return y + rowH; };
    // A row of text on the left and a control on the right
    auto info = [&] (int pg, const juce::String& title, const juce::String& description, int y, int rowH,
                     juce::Component& control, int controlW, int controlH)
    {
        const juce::Rectangle<int> r (x0, y, cw, rowH);
        infoRows.push_back ({ pg, r, title, description, controlW });
        place (control, { r.getRight() - controlW, r.getCentreY() - controlH / 2, controlW, controlH });
        return y + rowH;
    };

    int y = top;   // Appearance
    y = info (0, "Theme", "Colours of the editor and the modules.", y, rowD, themeSelector, 230, 34);
    y = info (0, "UI size", "Makes every window, menu and text bigger, for 4K screens. Applies when you press OK.", y, rowD, uiScaleSelector, 140, 34);
    y = option (hardwareLookToggle, y, rowD);
    y = info (0, "Knob style", "Used by the hardware look.", y, rowD, knobStyleSelector, 210, 34);
    y = option (flatKnobsToggle, y, rowD);
    y = option (wireframeToggle, y, rowD);

    y = top;       // Cables
    for (auto* b : { &cableCurvedThick, &cableStraightThick, &cableCurvedThin, &cableStraightThin })
        y = option (*b, y, rowS);
    y = info (1, "Opacity", "How solid the cables are. The canvas follows the slider; Cancel puts it back.", y + 10, rowD,
              cableOpacitySlider, 280, 28);

    y = top;       // Knobs
    for (auto* b : { &knobHorizontal, &knobCircular, &knobVertical })
        y = option (*b, y, rowD);

    y = top;       // Behaviour
    y = option (askSlotToggle, y, rowD);
    y = option (animateTilingToggle, y, rowD);
    y = option (restoreWindowsToggle, y, rowD);
    y = option (seqNoteNamesToggle, y, rowD);

    y = top;       // Synth link
    y = option (autoUploadToggle, y, rowD);
    y = option (synthCaptionToggle, y, rowD);
    y = info (4, "Send speed", "How fast parameter changes are streamed to the synth. Lower it if the connection drops.", y, rowD,
              sendRateSelector, 230, 34);

    y = top;       // Library
    y = info (5, "Preset library", "Every .pch under this folder shows in the Disk tab. Snippets, banks and sets get their own folders in it.",
              y, rowD, browseLibraryButton, 120, 34);
    place (libraryPath, { x0, y + 4, cw, 38 });

    y = top;       // Updates
    y = option (communityThemesToggle, y, rowD);
    y = option (newVersionToggle, y, rowD);

    y = top;       // Advanced
    y = option (mcpBridgeToggle, y, rowD);
    place (mcpBridgeStatusLabel, { x0, y + 2, cw, 24 });
    y += 36;
    y = info (7, "Command for your MCP client", "Register this in Claude Code, Claude Desktop or any client that speaks MCP.", y, rowD,
              mcpBridgeCopyButton, 100, 34);
    place (mcpBridgeCommand, { x0, y + 2, cw, 38 });

    // OK and Cancel, at the right of the footer
    const int by = h - footerH + (footerH - 36) / 2;
    place (cancelButton, { w - 32 - 100, by, 100, 36 });
    place (okButton,     { w - 32 - 100 - 12 - 100, by, 100, 36 });

    return h;
}

// ─── interaction ─────────────────────────────────────────────────────────────

bool EditorOptionsDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { close(); return true; }
    if (key == juce::KeyPress::returnKey) { okButton.triggerClick(); return true; }
    return false;
}

void EditorOptionsDialog::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numPages; ++i)
        if (pageButtonBounds (i).contains (e.getPosition()))
        {
            showPage (i);
            return;
        }

    // Only a press on the title bar drags the window; any other press must not, or the next
    // little movement of the mouse would carry the window off by where the last drag started
    draggingWindow = e.getPosition().getY() < titleBarH;
    if (draggingWindow)
        dragger.startDraggingComponent (this, e);
}

void EditorOptionsDialog::mouseUp (const juce::MouseEvent&)
{
    draggingWindow = false;
}
void EditorOptionsDialog::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingWindow)
        dragger.dragComponent (this, e, nullptr);
}

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
    options.uiScale        = EditorOptions::uiScales()[static_cast<size_t> (juce::jlimit (
                                 0, static_cast<int> (EditorOptions::uiScales().size()) - 1,
                                 uiScaleSelector.getSelectedId() - 1))];
    options.autoUpload     = autoUploadToggle.getToggleState();
    options.wireframe      = wireframeToggle  .getToggleState();
    options.hardwareLook   = hardwareLookToggle.getToggleState();
    options.hardwareKnobStyle = juce::jmax (0, knobStyleSelector.getSelectedId() - 1);
    options.flatKnobs      = flatKnobsToggle.getToggleState();
    options.animateTiling  = animateTilingToggle.getToggleState();
    options.restoreAllWindows = restoreWindowsToggle.getToggleState();
    options.askSlotOnOpen  = askSlotToggle.getToggleState();
    options.synthDisplayCaptions = synthCaptionToggle.getToggleState();
    options.seqNoteNames   = seqNoteNamesToggle.getToggleState();
    options.checkCommunityThemes = communityThemesToggle.getToggleState();
    options.checkNewVersion = newVersionToggle.getToggleState();
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
