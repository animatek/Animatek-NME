#include "InspectorPanel.h"
#include "AppTheme.h"
#include "protocol/KnobAssignmentMessage.h"
#include "../format/ValueFormatters.h"
#include "../model/ThemeData.h"
#include <cmath>

// ─── Morph group colours (same as canvas) ────────────────────────────────────
static const juce::Colour kMorphColors[4] = {
    juce::Colour(0xffCB4F4F),  // 1 – red
    juce::Colour(0xff9AC899),  // 2 – green
    juce::Colour(0xff5A5FB3),  // 3 – blue
    juce::Colour(0xffE5DE45),  // 4 – yellow
};
static const char* kGroupNames[4] = { "Macro 1", "Macro 2", "Macro 3", "Macro 4" };

// Which sections are folded away. Shared by every inspector and remembered
// between runs, matching how the main window's own panel toggles behave: a
// display preference, not per-window state.
static juce::PropertiesFile* inspectorSettings = nullptr;
static bool presetsSectionCollapsed = false;
static bool paramsSectionCollapsed  = false;
static bool morphsSectionCollapsed  = false;
static bool knobsSectionCollapsed   = false;
static bool ctrlsSectionCollapsed   = false;

// The presets that ship with the editor sit in a group of their own inside the
// section, folded away by default: there are 29 for the Drum Synthesizer alone,
// and a flat list buries the few you saved yourself under a wall of names.
static bool factoryPresetsCollapsed = true;

static void setCollapsed(bool& flag, const char* key, bool collapsed)
{
    flag = collapsed;
    if (inspectorSettings != nullptr)
    {
        inspectorSettings->setValue(key, collapsed);
        inspectorSettings->saveIfNeeded();
    }
}

// The value a typed string asks for, or -1 when it asks for nothing we can
// place. Values are shown in the parameter's own units, so that is what gets
// typed back: the range is at most 128 steps, so the honest way to read "440Hz"
// or "C#3" is to format every step and look for it. An exact match wins; short
// of that the number at the front of what was typed picks the nearest step,
// which is what makes "440" land on 440Hz and "9.5" on the 9.45s next to it.
static int valueFromText(const ParameterDescriptor& pd, const juce::String& typed)
{
    const auto wanted = typed.trim();
    if (wanted.isEmpty())
        return -1;

    for (int v = pd.minValue; v <= pd.maxValue; ++v)
        if (ValueFormatters::format(pd.formatter, v).trim().equalsIgnoreCase(wanted))
            return v;

    // getDoubleValue stops at the first character that cannot belong to a
    // number, so "9.45s" reads as 9.45 and "C#3" as 0 — hence the digit test,
    // which keeps a mistyped note name from being read as "the lowest value".
    if (!wanted.containsAnyOf("0123456789"))
        return -1;

    const double target = wanted.getDoubleValue();
    int best = -1;
    double bestDistance = 0.0;
    for (int v = pd.minValue; v <= pd.maxValue; ++v)
    {
        const auto text = ValueFormatters::format(pd.formatter, v).trim();
        if (!text.containsAnyOf("0123456789"))
            continue;
        const double distance = std::abs(text.getDoubleValue() - target);
        if (best < 0 || distance < bestDistance)
        {
            best = v;
            bestDistance = distance;
        }
    }
    return best;
}

// Built-ins always sort before user presets, so a count is all the layout needs.
static int countFactory(const std::vector<ModulePreset>& list)
{
    int n = 0;
    for (const auto& p : list)
        if (p.builtIn)
            ++n;
    return n;
}

// A built-in row is hidden while the Factory group is folded.
static bool presetRowHidden(const ModulePreset& p)
{
    return p.builtIn && factoryPresetsCollapsed;
}

// ─── AssignmentsListComponent ────────────────────────────────────────────────
// Shows morph assignments, knob assignments, and MIDI CC assignments.
// Two modes: single module or patch-wide.
class AssignmentsListComponent : public juce::Component
{
public:
    // ── Morph row ──
    struct MorphRow
    {
        int           group;        // 0-3
        int           paramIndex;
        int           section;
        Module*       module = nullptr;   // valid only within one rebuild
        Parameter*    param  = nullptr;
        juce::String  paramName;
        juce::String  moduleName;
    };

    // ── Parameter row ──
    // Every knob, slider and button the selected module has, as a name and the
    // figure it currently reads. The Parameter is held rather than a copy of its
    // value: a knob turned on the canvas has to show here without anyone having
    // to tell the list about it.
    struct ParamRow
    {
        Parameter*   param = nullptr;
        juce::String name;

        // A parameter the module wears as a button rather than a knob. Those
        // read as a state, not a figure, so the row shows a button carrying the
        // state's own name and clicking it steps to the next one, exactly as
        // clicking the button on the module's face does. `labels` comes from the
        // module face and is indexed by raw value, empty on the plain on/off
        // switches that carry no lettering.
        bool                      isSwitch = false;
        std::vector<juce::String> labels;
    };

    // ── Knob/CC row ──
    struct HwRow
    {
        juce::String  label;        // "Knob 3" or "CC 74"
        juce::String  paramName;
        juce::String  moduleName;   // empty in single-module mode
        int           section;
        int           moduleId;
        int           paramIndex;
        bool          isMorphFader = false;  // Morph A/B fader carrier — removed via its own callback
    };

    // Morph callbacks
    std::function<void(Module*, int section, int paramIndex, int group)>        onRemove;
    std::function<void(Module*, int section, int paramIndex, int span, int dir)> onRangeChange;
    // Knob/CC remove callbacks (section, moduleId, paramId)
    std::function<void(int section, int moduleId, int paramId)> onKnobRemove;
    std::function<void(int section, int moduleId, int paramId)> onCtrlRemove;
    std::function<void()> onMorphFaderKnobRemove;   // remove the Morph A/B fader knob
    // Preset callbacks, by index into the selected module type's preset list
    std::function<void(int)> onPresetRecall;
    std::function<void(int)> onPresetDelete;
    std::function<void(int)> onPresetRename;
    std::function<void()>    onPresetSave;
    std::function<void()>    onPresetsCollapsedChanged;
    // Parameter edits: live while the value moves, then once for the whole
    // gesture so it undoes in one step.
    std::function<void(Module*, int section, int paramIndex, int value)> onParamValue;
    std::function<void(Module*, int section, int paramIndex, int oldValue, int newValue)> onParamValueComplete;

    AssignmentsListComponent()
    {
        setInterceptsMouseClicks(true, false);

        addChildComponent(valueEditor);
        valueEditor.setBorder(juce::BorderSize<int>(1));
        valueEditor.onReturnKey = [this] { finishValueEdit(true); };
        valueEditor.onEscapeKey = [this] { finishValueEdit(false); };
        valueEditor.onFocusLost = [this] { finishValueEdit(true); };
    }

    // The module type whose presets belong on screen, empty when nothing single
    // is selected. Kept as its own accessor so paint, hit testing and height all
    // agree on when the section exists.
    juce::String presetType() const
    {
        if (module == nullptr || presetLibrary == nullptr)
            return {};
        auto* desc = module->getDescriptor();
        return desc != nullptr ? desc->name : juce::String();
    }

    const std::vector<ModulePreset>& presets() const
    {
        static const std::vector<ModulePreset> none;
        auto type = presetType();
        return type.isEmpty() ? none : presetLibrary->forType(type);
    }

    // The section carries its Save row even with nothing saved yet, which is the
    // only thing that makes saving discoverable at all.
    bool hasPresetSection() const { return presetType().isNotEmpty(); }
    // Collapsed, only the title row with its chevron is drawn and hit tested.
    bool presetRowsVisible() const { return hasPresetSection() && !presetsSectionCollapsed; }

    // Morph A/B fader carrier knob (-1 = none); shown in the patch-wide view.
    void setMorphFaderKnob(int knobIndex, int carrierGroup)
    {
        if (morphFaderKnob == knobIndex && morphFaderCarrierGroup == carrierGroup) return;
        morphFaderKnob = knobIndex;
        morphFaderCarrierGroup = carrierGroup;
        rebuild();
    }

    void setModule(Module* m, int sec)
    {
        // Held as a reference, not a pointer: the patch can destroy this module
        // between two of our repaints, and the rows below used to be read from
        // it afterwards (issue #61).
        moduleRef = m != nullptr ? ModuleRef { sec, m->getContainerIndex() } : ModuleRef {};
        // Don't clear patch - needed for knob/CC lookups in single-module mode
        singleSection = sec;
        rebuild();
    }

    void setPatchWide(Patch* p)
    {
        moduleRef.clear();
        patch = p;
        singleSection = -1;
        rebuild();
    }

    /** The module on show, or nullptr when there is none or it has been
        deleted since it was chosen. */
    Module* resolveModule() const
    {
        return patch != nullptr ? patch->getModule(moduleRef) : nullptr;
    }

    /** True when the rows were built from a module the patch no longer has.
        A delete, or an undone add, repaints this list before the rebuild that
        follows reaches it, and the rows hold Parameter pointers into that
        module: they must not be drawn from or clicked on in between. */
    bool rowsAreStale() const
    {
        return moduleRef.isValid() && resolveModule() == nullptr;
    }

    void rebuild()
    {
        // A rebuild throws away the rows the editor is anchored to, so whatever
        // was being typed is committed first rather than left hanging over a row
        // that no longer means the same thing.
        finishValueEdit(true);

        morphRows.clear();
        knobRows.clear();
        ctrlRows.clear();
        paramRows.clear();

        // Resolved once for the whole rebuild. The rows below hold Parameter
        // pointers into it, and they are only ever read between one rebuild
        // and the next, which is why they are safe.
        module = resolveModule();

        if (module != nullptr)
        {
            buildParamsFromModule(module);
            buildMorphsFromModule(module, singleSection);
            buildHwFromModule(module, singleSection);
        }
        else if (patch != nullptr)
        {
            buildMorphsFromPatch();
            buildHwFromPatch();
        }
        else
        {
            setSize(1, 1);
            repaint();
            return;
        }

        // Morph A/B fader carrier knob (global — shown in the patch-wide view).
        // Its real assignment lives on the morph pseudo-section, so it never
        // appears in the normal knob list; surface it here so it can be removed.
        if (module == nullptr && morphFaderKnob >= 0
            && KnobAssignmentMessage::isValidKnob(morphFaderKnob))
        {
            HwRow row;
            row.label = KnobAssignmentMessage::getKnobName(morphFaderKnob);
            row.paramName = "Morph A/B Fader";
            row.section = 2;
            row.moduleId = 1;
            row.paramIndex = morphFaderCarrierGroup;
            row.isMorphFader = true;
            knobRows.push_back(row);
        }

        // Sort morph rows by group, then module name, then paramIndex
        std::sort(morphRows.begin(), morphRows.end(), [](const MorphRow& a, const MorphRow& b) {
            if (a.group != b.group) return a.group < b.group;
            if (a.moduleName != b.moduleName) return a.moduleName < b.moduleName;
            return a.paramIndex < b.paramIndex;
        });

        buildLayout();
        setSize(getWidth() > 0 ? getWidth() : 200, juce::jmax(layoutHeight, 10));
        repaint();
    }

    void resized() override { rebuild(); }

    // ── Layout constants ──
    static constexpr int topPad       = 4;
    static constexpr int sectionTitleH = 22;
    static constexpr int groupHeaderH = 22;
    static constexpr int rowH         = 26;
    static constexpr int xBtnW        = 20;
    static constexpr int amountW      = 56;
    // Wider than the morph amount: it holds a formatted reading, not a number,
    // and "1.00kHz" or "-12(Oct)" has to fit.
    static constexpr int valueW       = 70;
    static constexpr int marginX      = 6;
    static constexpr int sectionGap   = 8;

    // Text sizes, named rather than written into each drawText, both because a
    // future editor-wide zoom then has one place to scale and because the old
    // 9-11pt literals made a knob assignment hard to read at a glance.
    static constexpr float fontRow      = 13.0f;  // parameter and preset names
    static constexpr float fontRowSmall = 11.0f;  // module name above a parameter
    static constexpr float fontBadge    = 11.0f;  // "Knob 3" / "CC 74"
    static constexpr float fontTitle    = 12.0f;  // section titles
    static constexpr float fontSmall    = 11.0f;  // x glyphs, amounts

    // ── Layout ──
    // Every row the list can show, worked out once. Painting, hit testing and
    // the overall height all read this rather than each walking the sections
    // and counting pixels for themselves, which is how a fourth section and a
    // fold on each of them would otherwise get three chances to disagree. It is
    // also what lets the value editor be dropped exactly over the cell it edits.
    enum class RowKind
    {
        ParamsTitle, ParamRow,
        MorphsTitle, MorphGroupHeader, MorphRow,
        KnobsTitle, KnobRow,
        CtrlsTitle, CtrlRow,
        PresetsTitle, FactoryHeader, PresetRow, PresetSave
    };
    struct LayoutRow { RowKind kind; int index; juce::Rectangle<int> bounds; };

    void buildLayout()
    {
        layout.clear();
        const int w = juce::jmax(getWidth(), 1);
        int y = topPad;

        auto add = [&](RowKind kind, int index, int h)
        {
            layout.push_back({ kind, index, { 0, y, w, h } });
            y += h;
        };

        if (!paramRows.empty())
        {
            add(RowKind::ParamsTitle, -1, sectionTitleH);
            if (!paramsSectionCollapsed)
                for (int i = 0; i < (int)paramRows.size(); ++i)
                    add(RowKind::ParamRow, i, rowH);
            y += sectionGap;
        }

        if (!morphRows.empty())
        {
            add(RowKind::MorphsTitle, -1, sectionTitleH);
            if (!morphsSectionCollapsed)
            {
                int prevGroup = -1;
                for (int i = 0; i < (int)morphRows.size(); ++i)
                {
                    if (morphRows[size_t(i)].group != prevGroup)
                    {
                        prevGroup = morphRows[size_t(i)].group;
                        add(RowKind::MorphGroupHeader, i, groupHeaderH);
                    }
                    add(RowKind::MorphRow, i, rowH);
                }
            }
            y += sectionGap;
        }

        if (!knobRows.empty())
        {
            add(RowKind::KnobsTitle, -1, sectionTitleH);
            if (!knobsSectionCollapsed)
                for (int i = 0; i < (int)knobRows.size(); ++i)
                    add(RowKind::KnobRow, i, rowH);
            y += sectionGap;
        }

        if (!ctrlRows.empty())
        {
            add(RowKind::CtrlsTitle, -1, sectionTitleH);
            if (!ctrlsSectionCollapsed)
                for (int i = 0; i < (int)ctrlRows.size(); ++i)
                    add(RowKind::CtrlRow, i, rowH);
            y += sectionGap;
        }

        if (hasPresetSection())
        {
            add(RowKind::PresetsTitle, -1, sectionTitleH);
            if (!presetsSectionCollapsed)
            {
                const auto& list = presets();
                if (countFactory(list) > 0)
                    add(RowKind::FactoryHeader, -1, rowH);
                for (int i = 0; i < (int)list.size(); ++i)
                    if (!presetRowHidden(list[size_t(i)]))
                        add(RowKind::PresetRow, i, rowH);
                add(RowKind::PresetSave, -1, rowH);
            }
        }

        layoutHeight = y + topPad;
    }

    // Where one particular row ended up, empty when it is not on screen.
    juce::Rectangle<int> boundsOf(RowKind kind, int index) const
    {
        for (const auto& row : layout)
            if (row.kind == kind && row.index == index)
                return row.bounds;
        return {};
    }

    // The boxed number at the right end of a parameter row: what a drag turns
    // and what a double-click opens for typing.
    juce::Rectangle<int> valueCellFor(juce::Rectangle<int> rowBounds) const
    {
        return { rowBounds.getRight() - marginX - valueW, rowBounds.getY() + 3,
                 valueW, rowBounds.getHeight() - 6 };
    }

    // ── Hit testing ──
    enum class HitType { None, MorphX, MorphAmount, KnobX, CtrlX,
                         PresetRecall, PresetDelete, PresetSave, PresetsHeader,
                         FactoryHeader, ParamValue,
                         ParamsHeader, MorphsHeader, KnobsHeader, CtrlsHeader };
    struct HitResult { HitType type = HitType::None; int rowIdx = -1; };

    HitResult findHit(juce::Point<int> pos) const
    {
        for (const auto& row : layout)
        {
            if (!row.bounds.contains(pos))
                continue;

            switch (row.kind)
            {
                case RowKind::ParamsTitle:  return { HitType::ParamsHeader,  -1 };
                case RowKind::MorphsTitle:  return { HitType::MorphsHeader,  -1 };
                case RowKind::KnobsTitle:   return { HitType::KnobsHeader,   -1 };
                case RowKind::CtrlsTitle:   return { HitType::CtrlsHeader,   -1 };
                case RowKind::PresetsTitle: return { HitType::PresetsHeader, -1 };
                case RowKind::FactoryHeader:return { HitType::FactoryHeader, -1 };

                case RowKind::ParamRow:
                    return { HitType::ParamValue, row.index };

                case RowKind::MorphRow:
                {
                    if (pos.x >= marginX && pos.x < marginX + xBtnW)
                        return { HitType::MorphX, row.index };
                    if (pos.x >= getWidth() - marginX - amountW)
                        return { HitType::MorphAmount, row.index };
                    return {};
                }

                case RowKind::KnobRow:
                    if (pos.x >= marginX && pos.x < marginX + xBtnW)
                        return { HitType::KnobX, row.index };
                    return {};

                case RowKind::CtrlRow:
                    if (pos.x >= marginX && pos.x < marginX + xBtnW)
                        return { HitType::CtrlX, row.index };
                    return {};

                case RowKind::PresetRow:
                {
                    const auto& list = presets();
                    if (row.index < 0 || row.index >= (int)list.size())
                        return {};
                    // The x sits at the right end, mirroring the module's own
                    // preset menu; built-ins have none, so a click there recalls.
                    const bool onX = !list[size_t(row.index)].builtIn
                                   && pos.x >= getWidth() - marginX - xBtnW;
                    return { onX ? HitType::PresetDelete : HitType::PresetRecall, row.index };
                }

                case RowKind::PresetSave:
                    return { HitType::PresetSave, -1 };

                case RowKind::MorphGroupHeader:
                    return {};
            }
        }
        return {};
    }

    // ── Paint ──
    void paint(juce::Graphics& g) override
    {
        g.fillAll(AppTheme::palette().backgroundPanel);

        // Every row holds a Parameter* the patch owns. Deleting the module (or
        // undoing its add) repaints through here before anyone rebuilds, and
        // reading those is reading freed memory: a hard crash on macOS and
        // silently wrong bytes on Linux (issue #61). Draw nothing until the
        // rebuild that is on its way replaces them.
        if (rowsAreStale())
            return;

        bool isGlobal = (patch != nullptr && module == nullptr);
        bool hasAny = !paramRows.empty() || !morphRows.empty() || !knobRows.empty()
                    || !ctrlRows.empty() || hasPresetSection();

        if (!hasAny)
        {
            g.setColour(AppTheme::palette().borderColor);
            g.setFont(AppTheme::uiFont(11.0f));
            g.drawText("No assignments", getLocalBounds().reduced(marginX),
                       juce::Justification::centredTop);
            return;
        }

        // One ink for every section heading, taken from the theme rather than
        // written down here (issue #68). The three colours these used to be
        // were picked against a dark panel: on Nord Classic and the other light
        // themes a pale grey-blue, an amber and a sky blue on a light ground
        // all came out too faint to read, and being three of them made the
        // headings look like they meant three different things when they only
        // divide the panel up. textPrimary is near-white on the dark themes and
        // near-black on the light ones, which is the same rule the macro
        // captions follow (AppTheme::macroLabelInk).
        const juce::Colour sectionInk = AppTheme::palette().textPrimary;

        for (const auto& row : layout)
        {
            const int y = row.bounds.getY();
            switch (row.kind)
            {
                case RowKind::ParamsTitle:
                    paintSectionTitle(g, y, "Parameters", sectionInk);
                    paintCollapseChevron(g, y, sectionInk, paramsSectionCollapsed, sectionTitleH);
                    break;
                case RowKind::ParamRow:
                    paintParamRow(g, row.bounds, row.index);
                    break;

                case RowKind::MorphsTitle:
                    paintSectionTitle(g, y, "Morphs", sectionInk);
                    paintCollapseChevron(g, y, sectionInk, morphsSectionCollapsed, sectionTitleH);
                    break;
                case RowKind::MorphGroupHeader:
                    paintGroupHeader(g, y, morphRows[size_t(row.index)].group);
                    break;
                case RowKind::MorphRow:
                    paintMorphRow(g, y, row.index, morphRows[size_t(row.index)], isGlobal);
                    break;

                case RowKind::KnobsTitle:
                    paintSectionTitle(g, y, "Knobs", sectionInk);
                    paintCollapseChevron(g, y, sectionInk, knobsSectionCollapsed, sectionTitleH);
                    break;
                case RowKind::KnobRow:
                    paintHwRow(g, y, row.index, knobRows[size_t(row.index)], isGlobal,
                               juce::Colour(0xff8D969F));
                    break;

                case RowKind::CtrlsTitle:
                    paintSectionTitle(g, y, "MIDI CC", sectionInk);
                    paintCollapseChevron(g, y, sectionInk, ctrlsSectionCollapsed, sectionTitleH);
                    break;
                case RowKind::CtrlRow:
                    paintHwRow(g, y, row.index, ctrlRows[size_t(row.index)], isGlobal,
                               juce::Colour(0xffaa8844));
                    break;

                case RowKind::PresetsTitle:
                    paintSectionTitle(g, y, "Presets", sectionInk);
                    paintCollapseChevron(g, y, sectionInk, presetsSectionCollapsed, sectionTitleH);
                    break;
                case RowKind::FactoryHeader:
                    paintFactoryHeader(g, y, countFactory(presets()));
                    break;
                case RowKind::PresetRow:
                {
                    const auto& list = presets();
                    if (row.index >= 0 && row.index < (int)list.size())
                        paintPresetRow(g, y, list[size_t(row.index)]);
                    break;
                }
                case RowKind::PresetSave:
                    paintPresetSaveRow(g, y);
                    break;
            }
        }
    }

    // Same shape as the main window's panel chevrons: pointing down while the
    // section is open, up while it is folded away.
    void paintCollapseChevron(juce::Graphics& g, int y, juce::Colour col,
                              bool collapsed, int height)
    {
        const float cx = static_cast<float>(getWidth() - marginX - 8);
        const float cy = static_cast<float>(y) + height * 0.5f;
        const float s  = 3.5f;

        juce::Path chevron;
        if (collapsed)
        {
            chevron.startNewSubPath(cx - s * 1.4f, cy + s * 0.7f);
            chevron.lineTo(cx, cy - s * 0.7f);
            chevron.lineTo(cx + s * 1.4f, cy + s * 0.7f);
        }
        else
        {
            chevron.startNewSubPath(cx - s * 1.4f, cy - s * 0.7f);
            chevron.lineTo(cx, cy + s * 0.7f);
            chevron.lineTo(cx + s * 1.4f, cy - s * 0.7f);
        }
        g.setColour(col);
        g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Reads as a folder inside the section: same indent as the rows, its own
    // chevron, and a count so the size of what is folded away is visible.
    void paintFactoryHeader(juce::Graphics& g, int y, int count)
    {
        g.setColour(AppTheme::palette().textSecondary);
        g.setFont(AppTheme::uiFont(fontRow));
        g.drawText("Factory (" + juce::String(count) + ")",
                   marginX, y, getWidth() - marginX * 2 - 16, rowH,
                   juce::Justification::centredLeft);
        paintCollapseChevron(g, y, AppTheme::palette().textSecondary,
                             factoryPresetsCollapsed, rowH);
    }

    void paintPresetRow(juce::Graphics& g, int y, const ModulePreset& preset)
    {
        const auto& theme = AppTheme::palette();
        g.setColour(theme.textSecondary);
        g.setFont(AppTheme::uiFont(fontRow));
        g.drawText(preset.name, marginX, y, getWidth() - marginX * 2 - xBtnW, rowH,
                   juce::Justification::centredLeft, true);

        if (!preset.builtIn)
        {
            g.setColour(theme.textMuted);
            g.setFont(AppTheme::uiFont(fontRow + 1.0f));
            g.drawText(juce::String::fromUTF8("\xc3\x97"), getWidth() - marginX - xBtnW, y,
                       xBtnW, rowH, juce::Justification::centred);
        }
    }

    // A parameter and what it currently reads, in its own units. The number is
    // a cell rather than plain text because it is a control: drag it to walk the
    // value, double-click it to type one in.
    void paintParamRow(juce::Graphics& g, juce::Rectangle<int> bounds, int i)
    {
        if (i < 0 || i >= (int)paramRows.size())
            return;
        const auto& r = paramRows[size_t(i)];
        const auto& theme = AppTheme::palette();

        g.setColour(i % 2 == 0 ? theme.backgroundPanel : theme.backgroundSecondary);
        g.fillRect(bounds);

        const auto cell = valueCellFor(bounds);

        g.setColour(theme.textPrimary);
        g.setFont(AppTheme::uiFont(fontRow));
        g.drawText(r.name, marginX, bounds.getY(),
                   cell.getX() - marginX - 4, bounds.getHeight(),
                   juce::Justification::centredLeft, true);

        // While one is being typed into, the editor is sitting on top of it.
        if (editingRow == i)
            return;

        if (r.isSwitch)
        {
            paintSwitchCell(g, cell, r);
            return;
        }

        g.setColour(theme.inputBackground);
        g.fillRoundedRectangle(cell.toFloat(), 3.0f);
        g.setColour(theme.borderColor.withAlpha(dragRow == i ? 1.0f : 0.55f));
        g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.0f);
        g.setColour(theme.textPrimary);
        g.setFont(AppTheme::uiFont(fontRowSmall));
        g.drawText(valueTextOf(r), cell, juce::Justification::centred, false);
    }

    juce::String valueTextOf(const ParamRow& r) const
    {
        if (r.param == nullptr)
            return {};
        const auto* pd = r.param->getDescriptor();
        return pd == nullptr ? juce::String(r.param->getValue())
                             : ValueFormatters::format(pd->formatter, r.param->getValue());
    }

    // A parameter the module wears as a button, drawn as one: lit while it is on
    // (anything above its lowest state), carrying the name of the state it is in.
    void paintSwitchCell(juce::Graphics& g, juce::Rectangle<int> cell, const ParamRow& r)
    {
        const auto& theme = AppTheme::palette();
        const auto* pd    = r.param->getDescriptor();
        const int   value = r.param->getValue();
        // Only a two-state switch lights up. A selector carries a different
        // legend for each of its states (LP, BP, HP), so lighting it from the
        // second one on would read as "HP is more on than LP".
        const bool  twoState = pd != nullptr && pd->maxValue - pd->minValue == 1;
        const bool  isOn     = twoState && value > pd->minValue;

        g.setColour(isOn ? theme.buttonActive : theme.buttonBackground);
        g.fillRoundedRectangle(cell.toFloat(), 3.0f);
        g.setColour(theme.borderColor);
        g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.0f);

        // The lit fill is whatever the theme uses for a pressed button, so the
        // lettering has to be picked off the fill rather than from the palette:
        // one of the themes lights it in a colour that dark text belongs on.
        g.setColour(isOn ? theme.buttonActive.contrasting(0.8f)
                         : (twoState ? theme.textSecondary : theme.textPrimary));
        g.setFont(AppTheme::uiFont(fontRowSmall));
        g.drawText(switchTextOf(r), cell.reduced(3, 0), juce::Justification::centred, false);
    }

    // What the button says in the state it is in: the module's own lettering
    // first, then whatever the parameter formats to, and On/Off as the last
    // resort for the plain switches that have neither.
    juce::String switchTextOf(const ParamRow& r) const
    {
        const int value = r.param->getValue();
        if (value >= 0 && value < (int)r.labels.size() && r.labels[size_t(value)].isNotEmpty())
            return r.labels[size_t(value)];

        const auto formatted = valueTextOf(r);
        if (formatted != juce::String(value))
            return formatted;

        const auto* pd = r.param->getDescriptor();
        if (pd != nullptr && pd->maxValue - pd->minValue == 1)
            return value > pd->minValue ? "On" : "Off";
        return formatted;
    }

    // Stepping a switch on to its next state, wrapping at the end, the way
    // clicking the button on the module's face does. Recorded as a whole
    // gesture at once: there is no drag to wait for.
    void toggleSwitch(int rowIdx)
    {
        if (rowIdx < 0 || rowIdx >= (int)paramRows.size())
            return;
        const auto& r = paramRows[size_t(rowIdx)];
        if (r.param == nullptr) return;
        const auto* pd = r.param->getDescriptor();
        if (pd == nullptr) return;

        const int oldValue = r.param->getValue();
        int newValue = oldValue + 1;
        if (newValue > pd->maxValue)
            newValue = pd->minValue;
        if (newValue == oldValue)
            return;

        r.param->setValue(newValue);
        if (onParamValue)         onParamValue(module, singleSection, pd->index, newValue);
        if (onParamValueComplete) onParamValueComplete(module, singleSection, pd->index,
                                                       oldValue, newValue);
        repaint();
    }

    void paintPresetSaveRow(juce::Graphics& g, int y)
    {
        const auto& theme = AppTheme::palette();
        auto row = juce::Rectangle<int>(marginX, y + 2, getWidth() - marginX * 2, rowH - 4);
        g.setColour(theme.borderColor.withAlpha(0.6f));
        g.drawRoundedRectangle(row.toFloat().reduced(0.5f), 3.0f, 1.0f);
        g.setColour(theme.textSecondary);
        g.setFont(AppTheme::uiFont(fontRowSmall));
        g.drawText("+ Save current settings", row, juce::Justification::centred);
    }

    // ── Mouse handling ──
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (rowsAreStale())
            return;   // see paint(): the rows point at a module the patch no longer has

        auto hr = findHit(e.getPosition());

        // Clicking anywhere else puts away whatever was being typed, keeping it.
        if (editingRow >= 0 && !(hr.type == HitType::ParamValue && hr.rowIdx == editingRow))
            finishValueEdit(true);

        if (hr.type == HitType::None) return;

        if (hr.type == HitType::ParamValue)
        {
            if (e.mods.isRightButtonDown()) return;

            // A switch has nothing to type or to drag: every click steps it on,
            // so a two-state one flips and a selector walks round its options.
            if (paramRows[size_t(hr.rowIdx)].isSwitch)
            {
                toggleSwitch(hr.rowIdx);
                return;
            }

            // Double-click types an exact figure in. A knob has 128 steps and no
            // way to say "440Hz"; this is the way to say it.
            if (e.getNumberOfClicks() >= 2)
            {
                beginValueEdit(hr.rowIdx);
                return;
            }

            auto* p = paramRows[size_t(hr.rowIdx)].param;
            if (p == nullptr) return;
            dragKind     = DragKind::ParamValue;
            dragRow      = hr.rowIdx;
            dragStartY   = e.getPosition().y;
            dragStartVal = p->getValue();
            repaint();
            return;
        }

        if (hr.type == HitType::ParamsHeader)
        { toggleSection(paramsSectionCollapsed, "inspectorParamsCollapsed"); return; }
        if (hr.type == HitType::MorphsHeader)
        { toggleSection(morphsSectionCollapsed, "inspectorMorphsCollapsed"); return; }
        if (hr.type == HitType::KnobsHeader)
        { toggleSection(knobsSectionCollapsed, "inspectorKnobsCollapsed"); return; }
        if (hr.type == HitType::CtrlsHeader)
        { toggleSection(ctrlsSectionCollapsed, "inspectorCtrlsCollapsed"); return; }

        // Right-clicking a preset row offers renaming, which has nowhere else to
        // go: the row already recalls on its left and deletes on its right.
        if (e.mods.isRightButtonDown())
        {
            if (hr.type != HitType::PresetRecall && hr.type != HitType::PresetDelete)
                return;
            const auto& list = presets();
            if (hr.rowIdx < 0 || hr.rowIdx >= (int)list.size()) return;
            if (list[size_t(hr.rowIdx)].builtIn) return;   // built-ins are not the user's to name

            juce::PopupMenu menu;
            menu.addItem(1, "Rename...");
            const int row = hr.rowIdx;
            menu.showMenuAsync(juce::PopupMenu::Options{}, [this, row](int result) {
                if (result == 1 && onPresetRename) onPresetRename(row);
            });
            return;
        }

        if (hr.type == HitType::MorphX)
        {
            auto& r = morphRows[size_t(hr.rowIdx)];
            int savedParamIndex = r.paramIndex;
            int savedSection = r.section;
            Module* savedModule = r.module;
            Parameter* p = r.param;
            if (p) { p->setMorphGroup(-1); p->setMorphRange(0); }
            rebuild();
            if (onRemove) onRemove(savedModule, savedSection, savedParamIndex, -1);
            return;
        }

        if (hr.type == HitType::MorphAmount)
        {
            dragKind     = DragKind::MorphAmount;
            dragRow      = hr.rowIdx;
            dragStartY   = e.getPosition().y;
            dragStartVal = morphRows[size_t(hr.rowIdx)].param
                         ? morphRows[size_t(hr.rowIdx)].param->getMorphRange() : 0;
            return;
        }

        if (hr.type == HitType::KnobX)
        {
            auto& r = knobRows[size_t(hr.rowIdx)];
            if (r.isMorphFader)
            {
                if (onMorphFaderKnobRemove) onMorphFaderKnobRemove();
            }
            else if (onKnobRemove)
            {
                onKnobRemove(r.section, r.moduleId, r.paramIndex);
            }
            rebuild();
            return;
        }

        if (hr.type == HitType::CtrlX)
        {
            auto& r = ctrlRows[size_t(hr.rowIdx)];
            if (onCtrlRemove) onCtrlRemove(r.section, r.moduleId, r.paramIndex);
            rebuild();
            return;
        }

        if (hr.type == HitType::PresetsHeader)
        {
            toggleSection(presetsSectionCollapsed, "inspectorPresetsCollapsed");
            return;
        }

        if (hr.type == HitType::FactoryHeader)
        {
            toggleSection(factoryPresetsCollapsed, "inspectorFactoryPresetsCollapsed");
            return;
        }

        // The owner performs these against the library and calls back in to
        // rebuild, so the list on screen can never disagree with what was written.
        if (hr.type == HitType::PresetRecall)
        {
            if (onPresetRecall) onPresetRecall(hr.rowIdx);
            return;
        }
        if (hr.type == HitType::PresetDelete)
        {
            if (onPresetDelete) onPresetDelete(hr.rowIdx);
            return;
        }
        if (hr.type == HitType::PresetSave)
        {
            if (onPresetSave) onPresetSave();
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int dy = dragStartY - e.getPosition().y;   // up is more

        if (dragKind == DragKind::MorphAmount)
        {
            if (dragRow < 0 || dragRow >= (int)morphRows.size()) return;
            auto& r = morphRows[size_t(dragRow)];
            if (r.param == nullptr) return;
            int val = juce::jlimit(-127, 127, dragStartVal + dy);
            r.param->setMorphRange(val);
            int span = std::abs(val);
            int dir  = (val >= 0) ? 0 : 1;
            if (onRangeChange) onRangeChange(r.module, r.section, r.paramIndex, span, dir);
            repaint();
            return;
        }

        if (dragKind == DragKind::ParamValue)
        {
            if (dragRow < 0 || dragRow >= (int)paramRows.size()) return;
            const auto& r = paramRows[size_t(dragRow)];
            if (r.param == nullptr) return;
            const auto* pd = r.param->getDescriptor();
            if (pd == nullptr) return;

            const int val = juce::jlimit(pd->minValue, pd->maxValue, dragStartVal + dy);
            if (val == r.param->getValue()) return;
            r.param->setValue(val);
            if (onParamValue) onParamValue(module, singleSection, pd->index, val);
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        // One undo step for the whole drag, from where it started to where it
        // was let go, the same way the canvas records a knob.
        if (dragKind == DragKind::ParamValue && dragRow >= 0 && dragRow < (int)paramRows.size())
        {
            const auto& r = paramRows[size_t(dragRow)];
            if (r.param != nullptr && r.param->getValue() != dragStartVal)
                if (const auto* pd = r.param->getDescriptor())
                    if (onParamValueComplete)
                        onParamValueComplete(module, singleSection, pd->index,
                                             dragStartVal, r.param->getValue());
        }

        dragKind = DragKind::None;
        dragRow  = -1;
        repaint();
    }

    // ── Typing a value in ──
    void beginValueEdit(int rowIdx)
    {
        if (rowIdx < 0 || rowIdx >= (int)paramRows.size())
            return;
        const auto& r = paramRows[size_t(rowIdx)];
        if (r.param == nullptr || r.param->getDescriptor() == nullptr)
            return;
        if (r.isSwitch)
            return;   // a switch is clicked, never typed into

        const auto cell = valueCellFor(boundsOf(RowKind::ParamRow, rowIdx));
        if (cell.isEmpty())
            return;

        editingRow = rowIdx;
        const auto& theme = AppTheme::palette();
        valueEditor.setColour(juce::TextEditor::backgroundColourId, theme.inputBackground);
        valueEditor.setColour(juce::TextEditor::textColourId, theme.textPrimary);
        valueEditor.setColour(juce::TextEditor::outlineColourId, theme.buttonActive);
        valueEditor.setColour(juce::TextEditor::focusedOutlineColourId, theme.buttonActive);
        valueEditor.setFont(AppTheme::uiFont(fontRowSmall));
        valueEditor.setJustification(juce::Justification::centred);
        valueEditor.setBounds(cell);
        valueEditor.setText(valueTextOf(r), false);
        valueEditor.setVisible(true);
        valueEditor.selectAll();
        valueEditor.grabKeyboardFocus();
        repaint();
    }

    void finishValueEdit(bool commit)
    {
        if (editingRow < 0)
            return;

        // Cleared first: hiding the editor takes the focus away, which calls
        // straight back in here, and the second pass must find nothing to do.
        const int row = editingRow;
        const auto typed = valueEditor.getText();
        editingRow = -1;
        valueEditor.setVisible(false);

        if (commit)
            applyTypedValue(row, typed);
        repaint();
    }

    void applyTypedValue(int rowIdx, const juce::String& typed)
    {
        if (rowIdx < 0 || rowIdx >= (int)paramRows.size())
            return;
        const auto& r = paramRows[size_t(rowIdx)];
        if (r.param == nullptr) return;
        const auto* pd = r.param->getDescriptor();
        if (pd == nullptr) return;

        const int newValue = valueFromText(*pd, typed);
        const int oldValue = r.param->getValue();
        if (newValue < 0 || newValue == oldValue)
            return;

        r.param->setValue(newValue);
        if (onParamValue)         onParamValue(module, singleSection, pd->index, newValue);
        if (onParamValueComplete) onParamValueComplete(module, singleSection, pd->index,
                                                       oldValue, newValue);
    }

private:
    // ── Paint helpers ──
    void paintSectionTitle(juce::Graphics& g, int y, const juce::String& title, juce::Colour col)
    {
        g.setColour(col.withAlpha(0.12f));
        g.fillRect(0, y, getWidth(), sectionTitleH);
        g.setColour(col);
        g.fillRect(0, y, getWidth(), 1);
        g.setFont(AppTheme::uiFont(fontTitle).withStyle("Bold"));
        g.drawText(title.toUpperCase(), marginX, y, getWidth() - marginX * 2, sectionTitleH,
                   juce::Justification::centredLeft);
    }

    void paintGroupHeader(juce::Graphics& g, int y, int group)
    {
        juce::Colour gc = kMorphColors[group];
        g.setColour(gc.withAlpha(0.18f));
        g.fillRect(0, y, getWidth(), groupHeaderH);
        g.setColour(gc);
        g.fillRect(0, y, 3, groupHeaderH);
        g.setFont(AppTheme::uiFont(11.0f).withStyle("Bold"));
        // Ink, not the macro colour: the stripe and the wash already say which
        // macro this is, and green Macro 2 disappeared on light themes.
        g.setColour(AppTheme::macroLabelInk());
        g.drawText(kGroupNames[group], marginX + 6, y, getWidth() - marginX * 2, groupHeaderH,
                   juce::Justification::centredLeft);
    }

    void paintMorphRow(juce::Graphics& g, int y, int i, const MorphRow& r, bool isGlobal)
    {
        int w = getWidth();
        g.setColour(i % 2 == 0 ? AppTheme::palette().backgroundPanel
                                : AppTheme::palette().backgroundSecondary);
        g.fillRect(0, y, w, rowH);

        // X button
        g.setColour(AppTheme::palette().borderColor);
        juce::Rectangle<int> xRect(marginX, y + (rowH - xBtnW) / 2, xBtnW, xBtnW);
        g.drawRoundedRectangle(xRect.toFloat(), 3.0f, 1.0f);
        g.setFont(AppTheme::uiFont(fontSmall));
        g.drawText("x", xRect, juce::Justification::centred);

        // Name
        int nameX = marginX + xBtnW + 4;
        int amX   = w - marginX - amountW;
        int nameW = amX - nameX - 4;
        paintParamName(g, nameX, y, nameW, r.paramName, r.moduleName, isGlobal);

        // Amount bar
        if (r.param != nullptr)
        {
            int morphRange = r.param->getMorphRange();
            juce::Colour gc = kMorphColors[r.group];
            juce::Rectangle<int> amRect(amX, y + 3, amountW, rowH - 6);
            g.setColour(AppTheme::palette().inputBackground);
            g.fillRoundedRectangle(amRect.toFloat(), 3.0f);
            float fraction = static_cast<float>(morphRange) / 127.0f;
            float midXf = amRect.getX() + amRect.getWidth() * 0.5f;
            float barW = std::abs(fraction) * (amRect.getWidth() * 0.5f);
            float barX = (fraction >= 0.0f) ? midXf : midXf - barW;
            g.setColour(gc.withAlpha(0.75f));
            g.fillRoundedRectangle(barX, float(amRect.getY()), barW, float(amRect.getHeight()), 2.0f);
            g.setColour(AppTheme::palette().borderColor);
            g.drawVerticalLine(int(midXf), float(amRect.getY()), float(amRect.getBottom()));
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(AppTheme::uiFont(10.0f));
            g.drawText(juce::String(morphRange), amRect, juce::Justification::centred);
            g.setColour(gc.withAlpha(0.4f));
            g.drawRoundedRectangle(amRect.toFloat(), 3.0f, 1.0f);
        }
    }

    void paintHwRow(juce::Graphics& g, int y, int i, const HwRow& r, bool isGlobal, juce::Colour accent)
    {
        int w = getWidth();
        g.setColour(i % 2 == 0 ? AppTheme::palette().backgroundPanel
                                : AppTheme::palette().backgroundSecondary);
        g.fillRect(0, y, w, rowH);

        // X button
        g.setColour(AppTheme::palette().borderColor);
        juce::Rectangle<int> xRect(marginX, y + (rowH - xBtnW) / 2, xBtnW, xBtnW);
        g.drawRoundedRectangle(xRect.toFloat(), 3.0f, 1.0f);
        g.setFont(AppTheme::uiFont(fontSmall));
        g.drawText("x", xRect, juce::Justification::centred);

        // Label badge (e.g. "Knob 3")
        int badgeX = marginX + xBtnW + 4;
        g.setFont(AppTheme::uiFont(fontBadge).withStyle("Bold"));
        int labelW = g.getCurrentFont().getStringWidth(r.label) + 8;
        juce::Rectangle<int> badge(badgeX, y + 3, labelW, rowH - 6);
        g.setColour(accent.withAlpha(0.32f));
        g.fillRoundedRectangle(badge.toFloat(), 3.0f);
        g.setColour(AppTheme::palette().textPrimary);
        g.drawText(r.label, badge, juce::Justification::centred);

        // Param name
        int nameX = badgeX + labelW + 6;
        int nameW = w - nameX - marginX;
        paintParamName(g, nameX, y, nameW, r.paramName, r.moduleName, isGlobal);
    }

    void paintParamName(juce::Graphics& g, int x, int y, int w,
                        const juce::String& paramName, const juce::String& moduleName, bool isGlobal)
    {
        if (isGlobal && moduleName.isNotEmpty())
        {
            g.setColour(AppTheme::palette().textMuted);
            g.setFont(AppTheme::uiFont(fontRowSmall));
            g.drawText(moduleName, x, y, w, rowH / 2, juce::Justification::bottomLeft, true);
            g.setColour(AppTheme::palette().textPrimary);
            g.setFont(AppTheme::uiFont(fontRowSmall));
            g.drawText(paramName, x, y + rowH / 2, w, rowH / 2, juce::Justification::topLeft, true);
        }
        else
        {
            g.setColour(AppTheme::palette().textPrimary);
            g.setFont(AppTheme::uiFont(fontRow));
            g.drawText(paramName, x, y, w, rowH, juce::Justification::centredLeft, true);
        }
    }

    // Folding a section changes how tall the list is, so the panel relays out
    // and the choice is remembered between runs.
    void toggleSection(bool& flag, const char* key)
    {
        setCollapsed(flag, key, !flag);
        if (onPresetsCollapsedChanged) onPresetsCollapsedChanged();
    }

    // ── Build helpers ──
    // Only the parameters that are really the module's own: "morph" entries are
    // the morph pseudo-parameters and "custom" ones are display state this
    // editor invented, and neither is a knob anybody turns here.
    void buildParamsFromModule(Module* m)
    {
        // The module's own face, when the panel has been given the theme: it is
        // what says which parameters are buttons rather than knobs.
        const ModuleTheme* face = nullptr;
        if (themeData != nullptr && m->getDescriptor() != nullptr)
            face = themeData->getModuleTheme(m->getDescriptor()->componentId);

        for (auto& p : m->getParameters())
        {
            const auto* pd = p.getDescriptor();
            if (pd == nullptr || pd->paramClass != "parameter")
                continue;
            if (pd->maxValue <= pd->minValue)
                continue;

            ParamRow row;
            row.param = &p;
            row.name  = pd->name.isNotEmpty() ? pd->name
                                              : "param " + juce::String(pd->index);

            if (face != nullptr)
                for (const auto& tb : face->buttons)
                {
                    if (tb.componentId != pd->componentId)
                        continue;
                    // Increment pairs are the up/down arrows beside a display and
                    // call buttons run a method rather than hold a value, so
                    // neither is a state this row could show or step.
                    if (tb.isIncrement || tb.isCall)
                        break;
                    row.isSwitch = true;
                    row.labels   = tb.labels;
                    break;
                }

            paramRows.push_back(std::move(row));
        }
    }

    void buildMorphsFromModule(Module* m, int sec)
    {
        for (auto& p : m->getParameters())
        {
            int g = p.getMorphGroup();
            if (g < 0 || g > 3) continue;
            auto* pd = p.getDescriptor();
            if (pd == nullptr) continue;
            morphRows.push_back({ g, pd->index, sec, m, const_cast<Parameter*>(&p),
                                  pd->name, juce::String() });
        }
    }

    void buildMorphsFromPatch()
    {
        if (patch == nullptr) return;
        for (const auto& ma : patch->morphAssignments)
        {
            auto& container = patch->getContainer(ma.section);
            auto* mod = container.getModuleByIndex(ma.module);
            if (mod == nullptr) continue;
            auto* param = mod->getParameter(ma.param);
            if (param == nullptr) continue;
            auto* pd = param->getDescriptor();
            auto* md = mod->getDescriptor();
            morphRows.push_back({ ma.morph, ma.param, ma.section, mod, param,
                                  pd ? pd->name : "?",
                                  md ? md->fullname : mod->getTitle() });
        }
    }

    void buildHwFromModule(Module* m, int sec)
    {
        if (patch == nullptr) return;
        int modId = m->getContainerIndex();

        // Knob assignments for this module
        for (int k = 0; k < 23; ++k)
        {
            if (!KnobAssignmentMessage::isValidKnob(k)) continue;
            const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
            if (!ka.assigned || ka.section != sec || ka.module != modId) continue;
            auto* param = m->getParameter(ka.param);
            auto* pd = param ? param->getDescriptor() : nullptr;
            knobRows.push_back({ KnobAssignmentMessage::getKnobName(k),
                                 pd ? pd->name : "param " + juce::String(ka.param),
                                 juce::String(), sec, modId, ka.param });
        }

        // MIDI CC assignments for this module
        for (const auto& ca : patch->ctrlAssignments)
        {
            if (ca.section != sec || ca.module != modId) continue;
            auto* param = m->getParameter(ca.param);
            auto* pd = param ? param->getDescriptor() : nullptr;
            ctrlRows.push_back({ "CC " + juce::String(ca.control),
                                 pd ? pd->name : "param " + juce::String(ca.param),
                                 juce::String(), sec, modId, ca.param });
        }
    }

    void buildHwFromPatch()
    {
        if (patch == nullptr) return;

        // All knob assignments
        for (int k = 0; k < 23; ++k)
        {
            if (!KnobAssignmentMessage::isValidKnob(k)) continue;
            const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
            if (!ka.assigned) continue;

            // A knob driving one of the four morph groups. There is no module
            // to look up: section 2 is the patch's own morph pseudo-section,
            // module 1, param 0-3, which is why these never used to appear here
            // at all while the Knob Floater showed them (issue #63). The Morph
            // A/B fader's carrier lives on the same section and gets a row of
            // its own further down, so it is left out here.
            if (ka.section == 2)
            {
                if (ka.param >= 0 && ka.param < 4 && k != morphFaderKnob)
                    knobRows.push_back({ KnobAssignmentMessage::getKnobName(k),
                                         kGroupNames[ka.param], "Morph",
                                         ka.section, ka.module, ka.param });
                continue;
            }

            auto& container = patch->getContainer(ka.section);
            auto* mod = container.getModuleByIndex(ka.module);
            if (mod == nullptr) continue;
            auto* param = mod->getParameter(ka.param);
            auto* pd = param ? param->getDescriptor() : nullptr;
            auto* md = mod->getDescriptor();
            knobRows.push_back({ KnobAssignmentMessage::getKnobName(k),
                                 pd ? pd->name : "?",
                                 md ? md->fullname : mod->getTitle(),
                                 ka.section, ka.module, ka.param });
        }

        // All MIDI CC assignments
        for (const auto& ca : patch->ctrlAssignments)
        {
            auto& container = patch->getContainer(ca.section);
            auto* mod = container.getModuleByIndex(ca.module);
            if (mod == nullptr) continue;
            auto* param = mod->getParameter(ca.param);
            auto* pd = param ? param->getDescriptor() : nullptr;
            auto* md = mod->getDescriptor();
            ctrlRows.push_back({ "CC " + juce::String(ca.control),
                                 pd ? pd->name : "?",
                                 md ? md->fullname : mod->getTitle(),
                                 ca.section, ca.module, ca.param });
        }
    }

    // ── State ──
public:
    // Which module the rows were built from. The pointer is re-resolved by
    // every rebuild and is only valid between one rebuild and the next; the
    // reference beside it is what survives (issue #61).
    ModuleRef              moduleRef;
    Module*                module        = nullptr;
    Patch*                 patch         = nullptr;
    // Presets are shown for whichever single module is selected. The list is
    // read straight from the library rather than copied, so a save or a delete
    // made anywhere else is on screen at the next rebuild.
    const ModulePresetLibrary* presetLibrary = nullptr;
    // The module faces, for telling a button apart from a knob. Null until the
    // owner hands them over, and every parameter reads as a number then.
    const ThemeData*           themeData     = nullptr;
private:
    int                    singleSection = -1;
    int                    morphFaderKnob = -1;         // physical knob driving the A/B fader
    int                    morphFaderCarrierGroup = -1; // spare morph group used as carrier
    std::vector<MorphRow>  morphRows;
    std::vector<HwRow>     knobRows;
    std::vector<HwRow>     ctrlRows;
    std::vector<ParamRow>  paramRows;

    std::vector<LayoutRow> layout;
    int layoutHeight = 0;

    enum class DragKind { None, MorphAmount, ParamValue };
    DragKind dragKind = DragKind::None;
    int dragRow      = -1;
    int dragStartY   = 0;
    int dragStartVal = 0;

    // One editor, kept for the life of the list and moved onto whichever value
    // is being typed into. A per-edit editor would have to be destroyed from
    // inside its own focus-lost callback, which is a trap this sidesteps.
    juce::TextEditor valueEditor;
    int editingRow = -1;
};

// ─── InspectorPanel ──────────────────────────────────────────────────────────

InspectorPanel::InspectorPanel()
{
    titleLabel.setText("Inspector", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(AppTheme::uiFont(13.0f).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    nameLabel.setText("Name", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(AppTheme::uiFont(11.0f)));
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    nameEditor.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    nameEditor.setInputRestrictions(16);
    nameEditor.addListener(this);
    nameEditor.setEnabled(false);
    addAndMakeVisible(nameEditor);

    sectionLabel.setFont(juce::Font(AppTheme::uiFont(11.0f)));
    sectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sectionLabel);

    // Shares the section row, right-aligned: what this one module costs the DSP
    // while the header bar shows what the whole patch costs (issue #31).
    dspLabel.setFont(juce::Font(AppTheme::uiFont(11.0f)));
    dspLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(dspLabel);

    // Assignments list (morphs + knobs + CCs)
    assignmentsList = std::make_unique<AssignmentsListComponent>();
    assignmentsList->onRemove = [this](Module* mod, int section, int paramIndex, int /*group*/)
    {
        if (onMorphGroupChanged && mod)
            onMorphGroupChanged(section, mod, paramIndex, -1);
        repaint();
    };
    assignmentsList->onRangeChange = [this](Module* mod, int section, int paramIndex, int span, int dir)
    {
        if (onMorphRangeChanged && mod)
            onMorphRangeChanged(section, mod, paramIndex, span, dir);
        repaint();
    };

    assignmentsList->onKnobRemove = [this](int section, int moduleId, int paramId)
    {
        if (onKnobRemoved) onKnobRemoved(section, moduleId, paramId, -1);
    };
    assignmentsList->onCtrlRemove = [this](int section, int moduleId, int paramId)
    {
        if (onMidiCtrlRemoved) onMidiCtrlRemoved(section, moduleId, paramId, -1);
    };
    assignmentsList->onMorphFaderKnobRemove = [this]()
    {
        if (onMorphFaderKnobRemove) onMorphFaderKnobRemove();
    };

    assignmentsList->onParamValue = [this](Module* mod, int section, int paramIndex, int value)
    {
        if (onParameterChanged && mod) onParameterChanged(section, mod, paramIndex, value);
    };
    assignmentsList->onParamValueComplete = [this](Module* mod, int section, int paramIndex,
                                                   int oldValue, int newValue)
    {
        if (onParameterEditComplete && mod)
            onParameterEditComplete(section, mod, paramIndex, oldValue, newValue);
    };

    assignmentsList->onPresetRecall = [this](int index)
    {
        if (auto* m = currentModule()) if (onPresetRecall) onPresetRecall(currentSection(), m, index);
    };
    assignmentsList->onPresetDelete = [this](int index)
    {
        if (auto* m = currentModule()) if (onPresetDelete) onPresetDelete(currentSection(), m, index);
    };
    assignmentsList->onPresetRename = [this](int index)
    {
        if (auto* m = currentModule()) if (onPresetRename) onPresetRename(currentSection(), m, index);
    };
    assignmentsList->onPresetSave = [this]()
    {
        if (auto* m = currentModule()) if (onPresetSave) onPresetSave(currentSection(), m);
    };
    // Folding the section changes how tall the list is, so the panel relays out.
    assignmentsList->onPresetsCollapsedChanged = [this]() { refreshMorphList(); };

    morphViewport.setViewedComponent(assignmentsList.get(), false);
    morphViewport.setScrollBarsShown(true, false);
    morphViewport.setScrollBarThickness(6);
    addAndMakeVisible(morphViewport);

    applyTheme();
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::applyTheme()
{
    const auto& theme = AppTheme::palette();
    titleLabel.setColour(juce::Label::textColourId, theme.textPrimary);
    nameLabel.setColour(juce::Label::textColourId, theme.textMuted);
    sectionLabel.setColour(juce::Label::textColourId, theme.textMuted);
    dspLabel.setColour(juce::Label::textColourId, theme.textMuted);
    nameEditor.setColour(juce::TextEditor::backgroundColourId, theme.inputBackground);
    nameEditor.setColour(juce::TextEditor::textColourId, theme.textPrimary);
    nameEditor.setColour(juce::TextEditor::outlineColourId, theme.borderColor);
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, theme.buttonActive);
    morphViewport.sendLookAndFeelChange();
    if (assignmentsList)
        assignmentsList->repaint();
    repaint();
}

void InspectorPanel::setPatch(Patch* p)
{
    currentPatch = p;

    // Detaching: the assignments list caches its own Patch and Module pointers,
    // so they must be dropped here or they outlive the patch being replaced.
    if (p == nullptr)
    {
        currentRef.clear();
        assignmentsList->setPatchWide(nullptr);
        return;
    }

    if (currentModule() == nullptr)
    {
        titleLabel.setText("Assignments", juce::dontSendNotification);
        sectionLabel.setText("All modules", juce::dontSendNotification);
        dspLabel.setText({}, juce::dontSendNotification);
        nameLabel.setVisible(false);
        nameEditor.setVisible(false);
        assignmentsList->setPatchWide(p);
        resized();
        repaint();
    }
}

void InspectorPanel::setModule(Module* module, int section)
{
    if (module == nullptr) { clearModule(); return; }

    currentRef = { section, module->getContainerIndex() };

    auto* desc = module->getDescriptor();
    titleLabel.setText(desc ? desc->fullname : "Module", juce::dontSendNotification);
    sectionLabel.setText(section == 1 ? "Poly" : "Common", juce::dontSendNotification);
    dspLabel.setText(desc ? formatDspCost(desc->cycles) + " DSP" : juce::String(),
                     juce::dontSendNotification);
    nameLabel.setVisible(true);
    nameEditor.setVisible(true);
    nameEditor.setEnabled(true);
    nameEditor.setText(module->getTitle(), juce::dontSendNotification);

    // In single-module mode, the list needs access to the patch for knob/CC lookups
    if (currentPatch != nullptr)
        assignmentsList->patch = currentPatch;
    assignmentsList->setModule(module, section);
    resized();
    repaint();
}

void InspectorPanel::clearModule()
{
    currentRef.clear();
    nameEditor.setText("", juce::dontSendNotification);
    nameEditor.setEnabled(false);
    dspLabel.setText({}, juce::dontSendNotification);

    if (currentPatch != nullptr)
    {
        titleLabel.setText("Assignments", juce::dontSendNotification);
        sectionLabel.setText("All modules", juce::dontSendNotification);
        nameLabel.setVisible(false);
        nameEditor.setVisible(false);
        assignmentsList->setPatchWide(currentPatch);
    }
    else
    {
        titleLabel.setText("Inspector", juce::dontSendNotification);
        sectionLabel.setText("", juce::dontSendNotification);
        nameLabel.setVisible(true);
        nameEditor.setVisible(true);
        assignmentsList->setModule(nullptr, -1);
    }
    resized();
    repaint();
}

void InspectorPanel::refreshMorphList()
{
    // Deleting the selected module, or undoing its add, repaints through here.
    // The reference simply stops resolving, and the panel falls back to the
    // patch-wide view instead of reading a module that is gone (issue #61).
    if (currentRef.isValid() && currentModule() == nullptr)
    {
        assignmentsList->moduleRef.clear();
        assignmentsList->module = nullptr;
        clearModule();
        return;
    }

    assignmentsList->rebuild();
    resized();
    repaint();
}

void InspectorPanel::repaintValues()
{
    if (assignmentsList)
        assignmentsList->repaint();
}

void InspectorPanel::setThemeData(const ThemeData* themeData)
{
    assignmentsList->themeData = themeData;
    refreshMorphList();
}

void InspectorPanel::setPresetLibrary(const ModulePresetLibrary* library)
{
    assignmentsList->presetLibrary = library;
    refreshMorphList();
}

void InspectorPanel::setSharedSettings(juce::PropertiesFile* settings)
{
    inspectorSettings = settings;
    if (settings != nullptr)
    {
        presetsSectionCollapsed = settings->getBoolValue("inspectorPresetsCollapsed", false);
        factoryPresetsCollapsed = settings->getBoolValue("inspectorFactoryPresetsCollapsed", true);
        paramsSectionCollapsed  = settings->getBoolValue("inspectorParamsCollapsed", false);
        morphsSectionCollapsed  = settings->getBoolValue("inspectorMorphsCollapsed", false);
        knobsSectionCollapsed   = settings->getBoolValue("inspectorKnobsCollapsed", false);
        ctrlsSectionCollapsed   = settings->getBoolValue("inspectorCtrlsCollapsed", false);
    }
}

void InspectorPanel::setMorphFaderKnob(int knobIndex, int carrierGroup)
{
    assignmentsList->setMorphFaderKnob(knobIndex, carrierGroup);
    resized();
    repaint();
}

void InspectorPanel::paint(juce::Graphics& g)
{
    const auto& theme = AppTheme::palette();
    g.fillAll(theme.backgroundPanel);

    // Miniature of the Nord Modular front-panel knob layout. The first 18
    // assignment slots map directly to the six physical columns, top-to-bottom.
    if (currentPatch != nullptr && getWidth() >= 170)
    {
        constexpr float mapW = 108.0f;
        constexpr float mapH = 38.0f;
        constexpr float groupGap = 3.0f;
        constexpr float cellW = 12.0f;
        constexpr float groupPad = 3.0f;
        const int groupColumns[] = { 2, 2, 1, 1 };
        auto map = juce::Rectangle<float>(static_cast<float>(getWidth() - margin) - mapW,
                                          static_cast<float>(margin), mapW, mapH);
        float x = map.getX();
        int knobColumn = 0;

        for (int columns : groupColumns)
        {
            const float groupW = groupPad * 2.0f + columns * cellW;
            auto group = juce::Rectangle<float>(x, map.getY(), groupW, mapH);
            g.setColour(theme.inputBackground);
            g.fillRoundedRectangle(group, 3.0f);
            g.setColour(theme.borderColor.withAlpha(0.75f));
            g.drawRoundedRectangle(group.reduced(0.5f), 3.0f, 1.0f);

            for (int col = 0; col < columns; ++col, ++knobColumn)
            {
                for (int row = 0; row < 3; ++row)
                {
                    const int knob = knobColumn * 3 + row;
                    const bool assigned = currentPatch->knobAssignments[static_cast<size_t>(knob)].assigned;
                    const float cx = group.getX() + groupPad + cellW * (static_cast<float>(col) + 0.5f);
                    const float cy = group.getY() + 7.0f + static_cast<float>(row) * 12.0f;
                    auto led = juce::Rectangle<float>(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);

                    if (assigned)
                    {
                        // A lit lens is a hardware colour, not a palette role: the
                        // accent green is deliberately darkened on light themes so
                        // status TEXT stays readable, which is the opposite of what
                        // a lamp needs — it made assigned knobs look unlit on Nord
                        // Classic. Fixed bright green instead, matching the fixed
                        // unlit colour below, with a rim highlight for the glass.
                        g.setColour(juce::Colour(0xff3ddc7a).withAlpha(0.28f));
                        g.fillEllipse(led.expanded(2.0f));
                        g.setColour(juce::Colour(0xff3ddc7a));
                        g.fillEllipse(led);
                        g.setColour(juce::Colour(0xffc8ffdf).withAlpha(0.75f));
                        g.drawEllipse(led.reduced(0.5f), 0.7f);
                    }
                    else
                    {
                        // The hardware's unlit lenses retain a deep green tint;
                        // using it consistently also keeps them visible on dark themes.
                        g.setColour(juce::Colour(0xff0b241d));
                        g.fillEllipse(led);
                        g.setColour(juce::Colour(0xff285044));
                        g.drawEllipse(led.reduced(0.5f), 0.7f);
                    }
                }
            }
            x += groupW + groupGap;
        }
    }

    if (currentModule() != nullptr)
    {
        g.setColour(theme.inputBackground);
        g.fillRect(0, margin + rowH + 2 + 14 + 14 + margin * 2 + rowH + 4, getWidth(), 1);
    }
}

void InspectorPanel::paintOverChildren(juce::Graphics& g)
{
    // Right-edge divider so modules sitting next to the inspector stay visually
    // separate from it. Drawn over children so the scrolling content (which spans
    // the full width) can't leave it broken into segments.
    g.setColour(AppTheme::palette().borderColor);
    g.fillRect(getWidth() - 1, 0, 1, getHeight());
}

void InspectorPanel::resized()
{
    int x = margin;
    int w = getWidth() - margin * 2;
    int y = margin;

    const int knobMapSpace = currentPatch != nullptr && getWidth() >= 170 ? 116 : 0;
    titleLabel.setBounds(x, y, juce::jmax(0, w - knobMapSpace), rowH);   y += rowH + 2;
    // The cost shares the section row, right-aligned but kept clear of the knob
    // map, which starts on the title row and hangs down over this one.
    const int rowW = juce::jmax(0, w - knobMapSpace);
    const int dspW = juce::jmin(70, rowW / 2);
    sectionLabel.setBounds(x, y, rowW - dspW, 14);
    dspLabel.setBounds(x + rowW - dspW, y, dspW, 14);   y += 14 + 4;

    if (currentModule() != nullptr)
    {
        nameLabel.setBounds(x, y, w, 14);      y += 16;
        nameEditor.setBounds(x, y, w, rowH);   y += rowH + margin;
        y += 1 + margin;
    }

    int remaining = getHeight() - y - margin;
    if (remaining > 0)
    {
        morphViewport.setBounds(0, y, getWidth(), remaining);
        assignmentsList->setSize(getWidth(), assignmentsList->getHeight());
    }
}

void InspectorPanel::textEditorReturnKeyPressed(juce::TextEditor&)
{
    commitName();
    grabKeyboardFocus();
}

void InspectorPanel::textEditorFocusLost(juce::TextEditor&)
{
    commitName();
}

void InspectorPanel::commitName()
{
    auto* module = currentModule();
    if (module == nullptr) return;
    juce::String newName = nameEditor.getText().trim();
    if (newName.isEmpty()) { nameEditor.setText(module->getTitle(), juce::dontSendNotification); return; }
    juce::String oldName = module->getTitle();
    if (newName == oldName) return;
    // The undoable action applies setTitle; fall back to a direct set if unwired.
    if (onNameChanged) onNameChanged(currentSection(), module, oldName, newName);
    else module->setTitle(newName);
}
