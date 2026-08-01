#include "InspectorPanel.h"
#include "AppTheme.h"
#include "protocol/KnobAssignmentMessage.h"
#include <cmath>

// ─── Morph group colours (same as canvas) ────────────────────────────────────
static const juce::Colour kMorphColors[4] = {
    juce::Colour(0xffCB4F4F),  // 1 – red
    juce::Colour(0xff9AC899),  // 2 – green
    juce::Colour(0xff5A5FB3),  // 3 – blue
    juce::Colour(0xffE5DE45),  // 4 – yellow
};
static const char* kGroupNames[4] = { "Macro 1", "Macro 2", "Macro 3", "Macro 4" };

// Whether the Presets section is folded away. Shared by every inspector and
// remembered between runs, matching how the main window's own panel toggles
// behave: a display preference, not per-window state.
static juce::PropertiesFile* inspectorSettings = nullptr;
static bool presetsSectionCollapsed = false;

static void setPresetsCollapsed(bool collapsed)
{
    presetsSectionCollapsed = collapsed;
    if (inspectorSettings != nullptr)
    {
        inspectorSettings->setValue("inspectorPresetsCollapsed", collapsed);
        inspectorSettings->saveIfNeeded();
    }
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
        Module*       module = nullptr;
        Parameter*    param  = nullptr;
        juce::String  paramName;
        juce::String  moduleName;
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

    AssignmentsListComponent() { setInterceptsMouseClicks(true, false); }

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
        module = m;
        // Don't clear patch — needed for knob/CC lookups in single-module mode
        singleSection = sec;
        rebuild();
    }

    void setPatchWide(Patch* p)
    {
        module = nullptr;
        patch = p;
        singleSection = -1;
        rebuild();
    }

    void rebuild()
    {
        morphRows.clear();
        knobRows.clear();
        ctrlRows.clear();

        if (module != nullptr)
        {
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

        // Compute required height
        int h = computeHeight();
        setSize(getWidth() > 0 ? getWidth() : 200, juce::jmax(h, 10));
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

    // ── Hit testing ──
    enum class HitType { None, MorphX, MorphAmount, KnobX, CtrlX,
                         PresetRecall, PresetDelete, PresetSave, PresetsHeader };
    struct HitResult { HitType type = HitType::None; int rowIdx = -1; };

    HitResult findHit(juce::Point<int> pos) const
    {
        int y = topPad;
        if (!morphRows.empty())
        {
            y += sectionTitleH;
            int prevGroup = -1;
            for (int i = 0; i < (int)morphRows.size(); ++i)
            {
                if (morphRows[size_t(i)].group != prevGroup)
                { y += groupHeaderH; prevGroup = morphRows[size_t(i)].group; }
                juce::Rectangle<int> rowRect(0, y, getWidth(), rowH);
                if (rowRect.contains(pos))
                {
                    bool xBtn = (pos.x >= marginX && pos.x < marginX + xBtnW);
                    int amX = getWidth() - marginX - amountW;
                    if (xBtn) return { HitType::MorphX, i };
                    if (pos.x >= amX) return { HitType::MorphAmount, i };
                    return {};
                }
                y += rowH;
            }
            y += sectionGap;
        }
        if (!knobRows.empty())
        {
            y += sectionTitleH;
            for (int i = 0; i < (int)knobRows.size(); ++i)
            {
                juce::Rectangle<int> rowRect(0, y, getWidth(), rowH);
                if (rowRect.contains(pos) && pos.x >= marginX && pos.x < marginX + xBtnW)
                    return { HitType::KnobX, i };
                y += rowH;
            }
            y += sectionGap;
        }
        if (!ctrlRows.empty())
        {
            y += sectionTitleH;
            for (int i = 0; i < (int)ctrlRows.size(); ++i)
            {
                juce::Rectangle<int> rowRect(0, y, getWidth(), rowH);
                if (rowRect.contains(pos) && pos.x >= marginX && pos.x < marginX + xBtnW)
                    return { HitType::CtrlX, i };
                y += rowH;
            }
            y += sectionGap;
        }
        if (hasPresetSection())
        {
            juce::Rectangle<int> headerRect(0, y, getWidth(), sectionTitleH);
            if (headerRect.contains(pos))
                return { HitType::PresetsHeader, -1 };
            y += sectionTitleH;

            if (!presetsSectionCollapsed)
            {
                const auto& list = presets();
                for (int i = 0; i < (int)list.size(); ++i)
                {
                    juce::Rectangle<int> rowRect(0, y, getWidth(), rowH);
                    if (rowRect.contains(pos))
                    {
                        // The x sits at the right end, mirroring the module's own
                        // preset menu; built-ins have none, so a click there recalls.
                        const bool onX = !list[size_t(i)].builtIn
                                       && pos.x >= getWidth() - marginX - xBtnW;
                        return { onX ? HitType::PresetDelete : HitType::PresetRecall, i };
                    }
                    y += rowH;
                }
                juce::Rectangle<int> saveRect(0, y, getWidth(), rowH);
                if (saveRect.contains(pos))
                    return { HitType::PresetSave, -1 };
            }
        }
        return {};
    }

    // ── Paint ──
    void paint(juce::Graphics& g) override
    {
        g.fillAll(AppTheme::palette().backgroundPanel);
        bool isGlobal = (patch != nullptr && module == nullptr);
        bool hasAny = !morphRows.empty() || !knobRows.empty() || !ctrlRows.empty()
                    || hasPresetSection();

        if (!hasAny)
        {
            g.setColour(AppTheme::palette().borderColor);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("No assignments", getLocalBounds().reduced(marginX),
                       juce::Justification::centredTop);
            return;
        }

        int y = topPad;

        // ── Morph section ──
        if (!morphRows.empty())
        {
            paintSectionTitle(g, y, "Morphs", juce::Colour(0xff9CA3AA));
            y += sectionTitleH;
            int prevGroup = -1;
            int w = getWidth();

            for (int i = 0; i < (int)morphRows.size(); ++i)
            {
                const auto& r = morphRows[size_t(i)];
                if (r.group != prevGroup)
                {
                    prevGroup = r.group;
                    paintGroupHeader(g, y, r.group);
                    y += groupHeaderH;
                }
                paintMorphRow(g, y, i, r, isGlobal);
                y += rowH;
            }
            y += sectionGap;
        }

        // ── Knob section ──
        if (!knobRows.empty())
        {
            paintSectionTitle(g, y, "Knobs", juce::Colour(0xff9CA3AA));
            y += sectionTitleH;
            for (int i = 0; i < (int)knobRows.size(); ++i)
            {
                paintHwRow(g, y, i, knobRows[size_t(i)], isGlobal, juce::Colour(0xff8D969F));
                y += rowH;
            }
            y += sectionGap;
        }

        // ── MIDI CC section ──
        if (!ctrlRows.empty())
        {
            paintSectionTitle(g, y, "MIDI CC", juce::Colour(0xffccaa66));
            y += sectionTitleH;
            for (int i = 0; i < (int)ctrlRows.size(); ++i)
            {
                paintHwRow(g, y, i, ctrlRows[size_t(i)], isGlobal, juce::Colour(0xffaa8844));
                y += rowH;
            }
            y += sectionGap;
        }

        // ── Presets section ──
        if (hasPresetSection())
        {
            paintSectionTitle(g, y, "Presets", juce::Colour(0xff7fb2d4));
            paintCollapseChevron(g, y, juce::Colour(0xff7fb2d4));
            y += sectionTitleH;
            if (!presetsSectionCollapsed)
            {
                const auto& list = presets();
                for (int i = 0; i < (int)list.size(); ++i)
                {
                    paintPresetRow(g, y, list[size_t(i)]);
                    y += rowH;
                }
                paintPresetSaveRow(g, y);
            }
        }
    }

    // Same shape as the main window's panel chevrons: pointing down while the
    // section is open, up while it is folded away.
    void paintCollapseChevron(juce::Graphics& g, int y, juce::Colour col)
    {
        const float cx = static_cast<float>(getWidth() - marginX - 8);
        const float cy = static_cast<float>(y) + sectionTitleH * 0.5f;
        const float s  = 3.5f;

        juce::Path chevron;
        if (presetsSectionCollapsed)
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

    void paintPresetRow(juce::Graphics& g, int y, const ModulePreset& preset)
    {
        const auto& theme = AppTheme::palette();
        g.setColour(theme.textSecondary);
        g.setFont(juce::FontOptions(fontRow));
        g.drawText(preset.name, marginX, y, getWidth() - marginX * 2 - xBtnW, rowH,
                   juce::Justification::centredLeft, true);

        if (!preset.builtIn)
        {
            g.setColour(theme.textMuted);
            g.setFont(juce::FontOptions(fontRow + 1.0f));
            g.drawText(juce::String::fromUTF8("\xc3\x97"), getWidth() - marginX - xBtnW, y,
                       xBtnW, rowH, juce::Justification::centred);
        }
    }

    void paintPresetSaveRow(juce::Graphics& g, int y)
    {
        const auto& theme = AppTheme::palette();
        auto row = juce::Rectangle<int>(marginX, y + 2, getWidth() - marginX * 2, rowH - 4);
        g.setColour(theme.borderColor.withAlpha(0.6f));
        g.drawRoundedRectangle(row.toFloat().reduced(0.5f), 3.0f, 1.0f);
        g.setColour(theme.textSecondary);
        g.setFont(juce::FontOptions(fontRowSmall));
        g.drawText("+ Save current settings", row, juce::Justification::centred);
    }

    // ── Mouse handling (morph rows only for now) ──
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto hr = findHit(e.getPosition());
        if (hr.type == HitType::None) return;

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
            dragRowIdx   = hr.rowIdx;
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
            setPresetsCollapsed(!presetsSectionCollapsed);
            if (onPresetsCollapsedChanged) onPresetsCollapsedChanged();
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
        if (dragRowIdx < 0 || dragRowIdx >= (int)morphRows.size()) return;
        auto& r = morphRows[size_t(dragRowIdx)];
        if (r.param == nullptr) return;
        int dy  = dragStartY - e.getPosition().y;
        int val = juce::jlimit(-127, 127, dragStartVal + dy);
        r.param->setMorphRange(val);
        int span = std::abs(val);
        int dir  = (val >= 0) ? 0 : 1;
        if (onRangeChange) onRangeChange(r.module, r.section, r.paramIndex, span, dir);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { dragRowIdx = -1; }

private:
    // ── Paint helpers ──
    void paintSectionTitle(juce::Graphics& g, int y, const juce::String& title, juce::Colour col)
    {
        g.setColour(col.withAlpha(0.12f));
        g.fillRect(0, y, getWidth(), sectionTitleH);
        g.setColour(col);
        g.fillRect(0, y, getWidth(), 1);
        g.setFont(juce::FontOptions(fontTitle).withStyle("Bold"));
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
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.setColour(gc.brighter(0.3f));
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
        g.setFont(juce::FontOptions(fontSmall));
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
            g.setFont(juce::FontOptions(10.0f));
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
        g.setFont(juce::FontOptions(fontSmall));
        g.drawText("x", xRect, juce::Justification::centred);

        // Label badge (e.g. "Knob 3")
        int badgeX = marginX + xBtnW + 4;
        g.setFont(juce::FontOptions(fontBadge).withStyle("Bold"));
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
            g.setFont(juce::FontOptions(fontRowSmall));
            g.drawText(moduleName, x, y, w, rowH / 2, juce::Justification::bottomLeft, true);
            g.setColour(AppTheme::palette().textPrimary);
            g.setFont(juce::FontOptions(fontRowSmall));
            g.drawText(paramName, x, y + rowH / 2, w, rowH / 2, juce::Justification::topLeft, true);
        }
        else
        {
            g.setColour(AppTheme::palette().textPrimary);
            g.setFont(juce::FontOptions(fontRow));
            g.drawText(paramName, x, y, w, rowH, juce::Justification::centredLeft, true);
        }
    }

    // ── Build helpers ──
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

    int computeHeight()
    {
        int h = topPad;
        if (!morphRows.empty())
        {
            h += sectionTitleH;
            int prevGroup = -1;
            for (auto& r : morphRows)
            {
                if (r.group != prevGroup) { h += groupHeaderH; prevGroup = r.group; }
                h += rowH;
            }
            h += sectionGap;
        }
        if (!knobRows.empty())
        {
            h += sectionTitleH + (int)knobRows.size() * rowH + sectionGap;
        }
        if (!ctrlRows.empty())
        {
            h += sectionTitleH + (int)ctrlRows.size() * rowH + sectionGap;
        }
        if (hasPresetSection())
        {
            h += sectionTitleH;
            // Rows plus the Save row that always closes the section.
            if (!presetsSectionCollapsed)
                h += ((int)presets().size() + 1) * rowH;
        }
        h += topPad;
        return h;
    }

    // ── State ──
public:
    Module*                module        = nullptr;
    Patch*                 patch         = nullptr;
    // Presets are shown for whichever single module is selected. The list is
    // read straight from the library rather than copied, so a save or a delete
    // made anywhere else is on screen at the next rebuild.
    const ModulePresetLibrary* presetLibrary = nullptr;
private:
    int                    singleSection = -1;
    int                    morphFaderKnob = -1;         // physical knob driving the A/B fader
    int                    morphFaderCarrierGroup = -1; // spare morph group used as carrier
    std::vector<MorphRow>  morphRows;
    std::vector<HwRow>     knobRows;
    std::vector<HwRow>     ctrlRows;
    int dragRowIdx   = -1;
    int dragStartY   = 0;
    int dragStartVal = 0;
};

// ─── InspectorPanel ──────────────────────────────────────────────────────────

InspectorPanel::InspectorPanel()
{
    titleLabel.setText("Inspector", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    nameLabel.setText("Name", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    nameEditor.setFont(juce::Font(juce::FontOptions(13.0f)));
    nameEditor.setInputRestrictions(16);
    nameEditor.addListener(this);
    nameEditor.setEnabled(false);
    addAndMakeVisible(nameEditor);

    sectionLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    sectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sectionLabel);

    // Shares the section row, right-aligned: what this one module costs the DSP
    // while the header bar shows what the whole patch costs (issue #31).
    dspLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
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

    assignmentsList->onPresetRecall = [this](int index)
    {
        if (onPresetRecall && currentModule) onPresetRecall(currentSection, currentModule, index);
    };
    assignmentsList->onPresetDelete = [this](int index)
    {
        if (onPresetDelete && currentModule) onPresetDelete(currentSection, currentModule, index);
    };
    assignmentsList->onPresetRename = [this](int index)
    {
        if (onPresetRename && currentModule) onPresetRename(currentSection, currentModule, index);
    };
    assignmentsList->onPresetSave = [this]()
    {
        if (onPresetSave && currentModule) onPresetSave(currentSection, currentModule);
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
        currentModule  = nullptr;
        currentSection = -1;
        assignmentsList->setPatchWide(nullptr);
        return;
    }

    if (currentModule == nullptr)
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
    currentModule  = module;
    currentSection = section;

    if (module == nullptr) { clearModule(); return; }

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
    currentModule  = nullptr;
    currentSection = -1;
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
    assignmentsList->rebuild();
    resized();
    repaint();
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
        presetsSectionCollapsed = settings->getBoolValue("inspectorPresetsCollapsed", false);
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

    if (currentModule != nullptr)
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

    if (currentModule != nullptr)
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
    if (currentModule == nullptr) return;
    juce::String newName = nameEditor.getText().trim();
    if (newName.isEmpty()) { nameEditor.setText(currentModule->getTitle(), juce::dontSendNotification); return; }
    juce::String oldName = currentModule->getTitle();
    if (newName == oldName) return;
    // The undoable action applies setTitle; fall back to a direct set if unwired.
    if (onNameChanged) onNameChanged(currentSection, currentModule, oldName, newName);
    else currentModule->setTitle(newName);
}
