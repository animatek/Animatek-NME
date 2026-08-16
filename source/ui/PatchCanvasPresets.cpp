#include "PatchCanvasComponent.h"
#include <cmath>
#include <unordered_map>

// PatchCanvas: the DrumSynth's preset box and the menu behind it.

void PatchCanvas::paintDrumSynthExtras(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds)
{
    // Positions from jMod Modules.res (scaled from Delphi coords to classic-theme px):
    // DisplayDrumSynthPreset: Left=120, Top=116, Width=57
    // SpinnerDrumSynthPreset: Left=178, Top=117, Width=20, Height=12
    // Module bounds are already offset by bounds.getTopLeft()

    int bx = bounds.getX();
    int by = bounds.getY();

    // Preset label "Preset"
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("Preset", bx + 120, by + 105, 60, 10, juce::Justification::centredLeft, true);

    // Display background (dark blue, same style as textDisplays)
    int dispX = bx + 120, dispY = by + 115, dispW = 57, dispH = 13;
    g.setColour(activeScheme_.displayBg);
    g.fillRect(dispX, dispY, dispW, dispH);
    g.setColour(activeScheme_.displayBorder);
    g.drawLine((float)dispX, (float)dispY, (float)(dispX+dispW), (float)dispY, 1.0f);
    g.drawLine((float)dispX, (float)dispY, (float)dispX, (float)(dispY+dispH), 1.0f);
    g.setColour(activeScheme_.displayText);
    g.drawLine((float)dispX, (float)(dispY+dispH), (float)(dispX+dispW), (float)(dispY+dispH), 1.0f);
    g.drawLine((float)(dispX+dispW), (float)dispY, (float)(dispX+dispW), (float)(dispY+dispH), 1.0f);

    // Preset name text. A module nobody has recalled a preset into reads
    // "none", as the original editor's does. Naming the first preset in the
    // list said a preset was loaded when the module was still at its defaults.
    const int presetIdx = resolvedDrumPreset(m);

    juce::String presetName = "none";
    if (presetIdx >= 0 && presetIdx < static_cast<int>(drumPresets().size()))
        presetName = drumPresets()[static_cast<size_t>(presetIdx)].name;

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(8.5f));
    g.drawText(presetName, dispX, dispY, dispW, dispH,
               juce::Justification::centred, true);

    // Up/Down spinner arrows (two stacked mini-buttons)
    int spX = bx + 179, spY = by + 115, spW = 16, spH = 6;
    juce::Colour spBase = activeScheme_.knobBase;

    // Up button
    g.setColour(spBase);
    g.fillRect(spX, spY, spW, spH);
    g.setColour(activeScheme_.buttonBorder);
    g.drawRect(spX, spY, spW, spH, 1);
    {
        float cx = spX + spW * 0.5f, cy = spY + spH * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(cx, cy - 2.0f, cx - 3.0f, cy + 1.5f, cx + 3.0f, cy + 1.5f);
        g.setColour(juce::Colours::black);
        g.fillPath(arrow);
    }

    // Down button
    g.setColour(spBase);
    g.fillRect(spX, spY + spH, spW, spH);
    g.setColour(activeScheme_.buttonBorder);
    g.drawRect(spX, spY + spH, spW, spH, 1);
    {
        float cx = spX + spW * 0.5f, cy = spY + spH * 1.5f;
        juce::Path arrow;
        arrow.addTriangle(cx, cy + 2.0f, cx - 3.0f, cy - 1.5f, cx + 3.0f, cy - 1.5f);
        g.setColour(juce::Colours::black);
        g.fillPath(arrow);
    }
}

// ============================================================
// DrumSynth preset UI
//
// The presets themselves live in the shared ModulePresetLibrary, which is
// injected by the owner. This canvas only draws the display box, offers the
// menu, and applies a recall; it owns no preset data of its own, so what the
// Inspector shows and what this menu shows cannot drift apart.
// ============================================================

const std::vector<ModulePreset>& PatchCanvas::drumPresets() const
{
    static const std::vector<ModulePreset> none;
    return presetLibrary != nullptr ? presetLibrary->forType("DrumSynth") : none;
}

// A patch file does not record which preset a module was built from, so the
// original editor reads the name back off the values. Do the same, or every
// module in a loaded patch would claim to be set to nothing.
//
// A preset may name only the parameters it cares about, so it matches when
// every parameter it does name agrees. An empty preset would match anything.
static bool drumPresetMatches(const ModulePreset& preset,
                              const std::map<juce::String, int>& values)
{
    if (preset.values.empty())
        return false;

    for (const auto& kv : preset.values)
    {
        auto it = values.find(kv.first);
        if (it == values.end() || it->second != kv.second)
            return false;
    }
    return true;
}

int PatchCanvas::resolvedDrumPreset(const Module& m)
{
    const int key = m.getContainerIndex();
    auto it = drumPresetState.find(key);
    if (it != drumPresetState.end())
        return it->second;

    int found = -1;
    const auto values = ModulePresetLibrary::capture(m, {}).values;
    const auto& list = drumPresets();
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (drumPresetMatches(list[i], values))
        {
            found = static_cast<int>(i);
            break;
        }
    }

    // Remembered either way, "matches nothing" included, so the search never
    // runs from paint a second time.
    drumPresetState[key] = found;
    return found;
}

void PatchCanvas::applyDrumPreset(Module& m, int section, int presetIdx)
{
    if (presetIdx < 0 || presetIdx >= static_cast<int>(drumPresets().size())) return;
    auto& preset = drumPresets()[static_cast<size_t>(presetIdx)];

    // Only the parameters the preset actually names are touched, so a preset
    // written by hand with two lines in it stays a two-parameter preset.
    for (const auto& [componentId, value] : preset.values)
    {
        auto* param = findParameter(m, componentId);
        if (param == nullptr) continue;
        int oldVal = param->getValue();
        param->setValue(value);
        int newVal = param->getValue();   // clamped to the parameter's own range
        if (newVal == oldVal) continue;
        if (parameterChangeCallback)
            parameterChangeCallback(section, m.getContainerIndex(), param->getDescriptor()->index, newVal);
        if (paramDragCompleteCallback)
            paramDragCompleteCallback(section, m.getContainerIndex(), param->getDescriptor()->index, oldVal, newVal);
    }
    repaint();
}

// One row of the DrumSynth preset list. Clicking the name recalls the preset,
// clicking the x at its right end deletes it, so the library reads as a list
// rather than as a load menu plus a separate delete menu. Built-in presets get
// no x. The row reports which half was clicked through a shared int, because a
// menu item can only carry the one id that identifies the preset.
class DrumPresetMenuRow : public juce::PopupMenu::CustomComponent
{
public:
    DrumPresetMenuRow(juce::String presetName, bool canDelete, bool isCurrent,
                      std::shared_ptr<int> sharedAction)
        : juce::PopupMenu::CustomComponent(false),
          name(std::move(presetName)), deletable(canDelete), current(isCurrent),
          action(std::move(sharedAction)) {}

    void getIdealSize(int& idealWidth, int& idealHeight) override
    {
        idealWidth  = rowFont().getStringWidth(name) + tickW + deleteW + 16;
        idealHeight = 24;
    }

    void paint(juce::Graphics& g) override
    {
        auto& lf = getLookAndFeel();
        auto textColour = lf.findColour(juce::PopupMenu::textColourId);

        if (isItemHighlighted())
        {
            g.setColour(lf.findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(getLocalBounds());
            textColour = lf.findColour(juce::PopupMenu::highlightedTextColourId);
        }

        g.setFont(rowFont());
        if (current)
        {
            g.setColour(textColour);
            g.drawText(juce::String::fromUTF8("\xe2\x9c\x93"), 4, 0, tickW, getHeight(),
                       juce::Justification::centred);
        }
        g.setColour(textColour);
        g.drawText(name, tickW + 4, 0, getWidth() - tickW - deleteW - 8, getHeight(),
                   juce::Justification::centredLeft, true);

        if (deletable)
        {
            g.setColour(overDelete ? juce::Colour(0xffe05555) : textColour.withAlpha(0.55f));
            g.drawText(juce::String::fromUTF8("\xc3\x97"), getWidth() - deleteW, 0, deleteW - 4,
                       getHeight(), juce::Justification::centred);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override    { updateHover(e.x); }
    void mouseEnter(const juce::MouseEvent& e) override   { updateHover(e.x); }
    void mouseExit(const juce::MouseEvent&) override      { updateHover(-1); }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition()))
            return;
        *action = isOverDelete(e.x) ? 1 : 0;
        triggerMenuItem();
    }

private:
    static juce::Font rowFont() { return juce::Font(juce::FontOptions(14.0f)); }
    bool isOverDelete(int x) const { return deletable && x >= getWidth() - deleteW; }
    void updateHover(int x)
    {
        const bool over = x >= 0 && isOverDelete(x);
        if (over != overDelete) { overDelete = over; repaint(); }
    }

    juce::String name;
    bool deletable = false;
    bool current   = false;
    bool overDelete = false;
    std::shared_ptr<int> action;
    static constexpr int tickW   = 18;
    static constexpr int deleteW = 26;
};

juce::PopupMenu PatchCanvas::buildDrumPresetMenu(Module& m, std::shared_ptr<int> action)
{
    juce::PopupMenu menu;

    const int currentIdx = resolvedDrumPreset(m);

    // The presets that ship with the editor go in a folder of their own. There
    // are 29 of them for the Drum Synthesizer alone, and a flat list buries the
    // handful you saved yourself under a wall of names you rarely pick from.
    juce::PopupMenu factory;
    bool anyFactory = false;
    bool anyUser = false;

    const auto& list = drumPresets();
    for (size_t i = 0; i < list.size(); ++i)
    {
        auto row = std::make_unique<DrumPresetMenuRow>(
            list[i].name,
            !list[i].builtIn,
            static_cast<int>(i) == currentIdx,
            action);
        const int id = drumPresetFirstId + static_cast<int>(i);

        if (list[i].builtIn)
        {
            factory.addCustomItem(id, std::move(row), nullptr, list[i].name);
            anyFactory = true;
        }
        else
        {
            menu.addCustomItem(id, std::move(row), nullptr, list[i].name);
            anyUser = true;
        }
    }

    if (anyFactory)
    {
        if (anyUser)
            menu.addSeparator();
        menu.addSubMenu("Factory", factory);
    }

    if (anyFactory || anyUser)
        menu.addSeparator();
    menu.addItem(drumPresetSaveId, "Save current settings as preset...");
    return menu;
}

void PatchCanvas::handleDrumPresetMenuResult(int result, int action, Module& m, int section)
{
    if (result == drumPresetSaveId)
    {
        saveDrumPresetFromModule(m);
        return;
    }

    const int index = result - drumPresetFirstId;
    if (index < 0 || index >= static_cast<int>(drumPresets().size()))
        return;

    if (action == 1)
        deleteDrumPreset(index);
    else
    {
        drumPresetState[m.getContainerIndex()] = index;
        applyDrumPreset(m, section, index);
    }
}

void PatchCanvas::saveDrumPresetFromModule(Module& m)
{
    if (presetLibrary == nullptr || !presetLibrary->canSave())
        return;

    // Named automatically rather than through a dialog: the name can be changed
    // later, and stopping to invent one is the friction that stops people saving.
    auto preset = ModulePresetLibrary::capture(m, presetLibrary->suggestName("DrumSynth"));
    if (presetLibrary->add(std::move(preset)) >= 0)
        repaint();
}

void PatchCanvas::deleteDrumPreset(int index)
{
    if (presetLibrary == nullptr || !presetLibrary->remove("DrumSynth", index))
        return;   // built-ins and failed writes leave the list untouched

    // Modules pointing past the deleted preset would otherwise show the wrong
    // name, and anything past the end would show none at all.
    const int last = static_cast<int>(drumPresets().size()) - 1;
    for (auto& kv : drumPresetState)
    {
        if (kv.second < 0)
            continue;                      // "none" stays none
        if (kv.second == index)
            kv.second = -1;                // the one it named is gone
        else if (kv.second > index)
            kv.second = juce::jlimit(0, juce::jmax(0, last), kv.second - 1);
    }
    repaint();
}

void PatchCanvas::showDrumPresetContextMenu(Module& m, int section)
{
    auto action = std::make_shared<int>(0);
    buildDrumPresetMenu(m, action).showMenuAsync(
        juce::PopupMenu::Options{},
        [this, &m, section, action](int result)
        {
            if (result != 0)
                handleDrumPresetMenuResult(result, *action, m, section);
        });
}
