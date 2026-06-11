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
    };

    // Morph callbacks
    std::function<void(Module*, int section, int paramIndex, int group)>        onRemove;
    std::function<void(Module*, int section, int paramIndex, int span, int dir)> onRangeChange;
    // Knob/CC remove callbacks (section, moduleId, paramId)
    std::function<void(int section, int moduleId, int paramId)> onKnobRemove;
    std::function<void(int section, int moduleId, int paramId)> onCtrlRemove;

    AssignmentsListComponent() { setInterceptsMouseClicks(true, false); }

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
    static constexpr int sectionTitleH = 18;
    static constexpr int groupHeaderH = 20;
    static constexpr int rowH         = 22;
    static constexpr int xBtnW        = 18;
    static constexpr int amountW      = 52;
    static constexpr int marginX      = 6;
    static constexpr int sectionGap   = 8;

    // ── Hit testing ──
    enum class HitType { None, MorphX, MorphAmount, KnobX, CtrlX };
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
        }
        return {};
    }

    // ── Paint ──
    void paint(juce::Graphics& g) override
    {
        g.fillAll(AppTheme::palette().backgroundPanel);
        bool isGlobal = (patch != nullptr && module == nullptr);
        bool hasAny = !morphRows.empty() || !knobRows.empty() || !ctrlRows.empty();

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
        }
    }

    // ── Mouse handling (morph rows only for now) ──
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto hr = findHit(e.getPosition());
        if (hr.type == HitType::None) return;

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
            if (onKnobRemove) onKnobRemove(r.section, r.moduleId, r.paramIndex);
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
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
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
        g.setFont(juce::FontOptions(10.0f));
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
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("x", xRect, juce::Justification::centred);

        // Label badge (e.g. "Knob 3")
        int badgeX = marginX + xBtnW + 4;
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        int labelW = g.getCurrentFont().getStringWidth(r.label) + 8;
        juce::Rectangle<int> badge(badgeX, y + 3, labelW, rowH - 6);
        g.setColour(accent.withAlpha(0.25f));
        g.fillRoundedRectangle(badge.toFloat(), 3.0f);
        g.setColour(accent.brighter(0.3f));
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
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(moduleName, x, y, w, rowH / 2, juce::Justification::bottomLeft, true);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(paramName, x, y + rowH / 2, w, rowH / 2, juce::Justification::topLeft, true);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::FontOptions(11.0f));
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
            h += sectionTitleH + (int)ctrlRows.size() * rowH;
        }
        h += topPad;
        return h;
    }

    // ── State ──
public:
    Module*                module        = nullptr;
    Patch*                 patch         = nullptr;
private:
    int                    singleSection = -1;
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
    titleLabel.setColour(juce::Label::textColourId, AppTheme::palette().textPrimary);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    nameLabel.setText("Name", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    nameLabel.setColour(juce::Label::textColourId, AppTheme::palette().textMuted);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    nameEditor.setFont(juce::Font(juce::FontOptions(13.0f)));
    nameEditor.setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().inputBackground);
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId, AppTheme::palette().borderColor);
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, AppTheme::palette().borderColor);
    nameEditor.setInputRestrictions(16);
    nameEditor.addListener(this);
    nameEditor.setEnabled(false);
    addAndMakeVisible(nameEditor);

    sectionLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    sectionLabel.setColour(juce::Label::textColourId, AppTheme::palette().borderColor);
    sectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sectionLabel);

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

    morphViewport.setViewedComponent(assignmentsList.get(), false);
    morphViewport.setScrollBarsShown(true, false);
    morphViewport.setScrollBarThickness(6);
    addAndMakeVisible(morphViewport);
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::setPatch(Patch* p)
{
    currentPatch = p;
    if (currentModule == nullptr && p != nullptr)
    {
        titleLabel.setText("Assignments", juce::dontSendNotification);
        sectionLabel.setText("All modules", juce::dontSendNotification);
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
}

void InspectorPanel::paint(juce::Graphics& g)
{
    g.fillAll(AppTheme::palette().backgroundPanel);

    if (currentModule != nullptr)
    {
        g.setColour(AppTheme::palette().inputBackground);
        g.fillRect(0, margin + rowH + 2 + 14 + 14 + margin * 2 + rowH + 4, getWidth(), 1);
    }
}

void InspectorPanel::resized()
{
    int x = margin;
    int w = getWidth() - margin * 2;
    int y = margin;

    titleLabel.setBounds(x, y, w, rowH);   y += rowH + 2;
    sectionLabel.setBounds(x, y, w, 14);   y += 14 + 4;

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
    if (newName == currentModule->getTitle()) return;
    currentModule->setTitle(newName);
    if (onNameChanged) onNameChanged(currentSection, currentModule, newName);
}
