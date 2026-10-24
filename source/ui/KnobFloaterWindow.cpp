#include "KnobFloaterWindow.h"
#include "AppTheme.h"
#include "../protocol/KnobAssignmentMessage.h"
#include "../format/ValueFormatters.h"
#include <cmath>

#define kBg        (AppTheme::palette().backgroundMain)
#define kPanelBg   (AppTheme::palette().backgroundPanel)
#define kBorder    (AppTheme::palette().borderColor)
#define kText      (AppTheme::palette().textPrimary)
#define kTextDim   (AppTheme::palette().textMuted)
#define kAccent    (AppTheme::palette().accentActive)
// A lit assignment lamp is a hardware colour, not a palette role — accentSuccess
// is deliberately darkened on light themes so status text stays readable, which
// left assigned knobs looking unlit. Same fixed green as the Inspector's knob map.
#define kLedOn     (juce::Colour(0xff3ddc7a))

namespace
{
    constexpr int kCellW = 112;
    constexpr int kCellH = 112;
    constexpr int kMargin = 20;
    constexpr int kGroupGap = 20;
    constexpr int kBottomGap = 20;

    // Wire indices of the special bottom-row cells, in display order
    constexpr int kSpecialIndices[3] = { 19, 22, 20 };  // Pedal, On/Off switch, After touch
}

// ============================================================================
// The knob the modules wear on the canvas: a plain circle, a grip line, a tick at each end of its travel
// ============================================================================
namespace
{
class ArcKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                          float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        const auto c = area.getCentre();
        const float sz = juce::jmin(area.getWidth(), area.getHeight()) * 0.78f;
        const float radius = sz * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float fade = s.isEnabled() ? 1.0f : 0.4f;
        const auto& pal = AppTheme::palette();

        g.setColour(pal.textSecondary.withAlpha(fade));
        g.fillEllipse(c.x - radius, c.y - radius, sz, sz);
        g.setColour(pal.borderColor.withAlpha(fade));
        g.drawEllipse(c.x - radius, c.y - radius, sz, sz, 1.0f);

        g.setColour(pal.textMuted.withAlpha(fade));
        for (float a : { startAngle, endAngle })
        {
            const float sa = std::sin(a), ca = std::cos(a);
            g.drawLine(c.x + sa * radius * 1.08f, c.y - ca * radius * 1.08f,
                       c.x + sa * radius * 1.45f, c.y - ca * radius * 1.45f, 1.5f);
        }

        const float sa = std::sin(angle), ca = std::cos(angle);
        g.setColour(pal.backgroundMain);
        g.drawLine(c.x + sa * radius * 0.3f, c.y - ca * radius * 0.3f,
                   c.x + sa * radius * 0.85f, c.y - ca * radius * 0.85f, 1.5f);
    }
};
}

// ============================================================================
// KnobCell
// ============================================================================
class KnobFloaterPanel::KnobCell : public juce::Component
{
public:
    KnobCell(KnobFloaterPanel& ownerPanel, int wireIdx, const juce::String& label)
        : owner(ownerPanel), wireIndex(wireIdx), knobLabel(label)
    {
        moduleLabel.setJustificationType(juce::Justification::centred);
        paramLabel.setJustificationType(juce::Justification::centred);
        moduleLabel.setInterceptsMouseClicks(false, false);
        paramLabel.setInterceptsMouseClicks(false, false);
        moduleLabel.setMinimumHorizontalScale(1.0f);
        paramLabel.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(moduleLabel);
        addAndMakeVisible(paramLabel);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(valueLabel);
        knob.setLookAndFeel(&arcLnF);
        knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);

        knob.cell = this;
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        knob.setRange(0.0, 1.0, 1.0);
        knob.setEnabled(false);
        knob.onDragStart = [this]() { owner.cellDragStart(wireIndex); };
        knob.onValueChange = [this]() {
            owner.cellValueChanged(wireIndex, juce::roundToInt(knob.getValue()));
            updateValueText();
        };
        knob.onDragEnd = [this]() { owner.cellDragEnd(wireIndex); };
        addAndMakeVisible(knob);

        applyTheme();
    }

    ~KnobCell() override { knob.setLookAndFeel(nullptr); }

    // What the knob's value reads as: the parameter's own formatter (C4, 340 ms, -6 dB), or the number
    void setInfo(const juce::String& moduleName, const juce::String& paramName,
                 bool isAssigned, int minValue, int maxValue, int value,
                 const ParameterDescriptor* pd = nullptr)
    {
        assigned = isAssigned;
        if (pd != nullptr)
            formatter = [desc = *pd](int v) { return ValueFormatters::format(desc, v); };
        else
            formatter = nullptr;

        // An empty knob says "Free", with its number where the module's name goes
        setLabelText(moduleLabel, isAssigned ? moduleName : knobLabel);
        setLabelText(paramLabel, isAssigned ? paramName : juce::String("Free"));
        paramLabel.setColour(juce::Label::textColourId, isAssigned ? kText : kTextDim);

        if (assigned && maxValue > minValue)
        {
            knob.setRange(minValue, maxValue, 1.0);
            knob.setValue(value, juce::dontSendNotification);
            knob.setEnabled(true);
        }
        else
        {
            knob.setRange(0.0, 1.0, 1.0);
            knob.setValue(0.0, juce::dontSendNotification);
            knob.setEnabled(assigned);
        }
        updateValueText();
        repaint();
    }

    void updateValueText()
    {
        if (!assigned) { valueLabel.setText(juce::String::fromUTF8("\xe2\x80\x93"), juce::dontSendNotification); return; }
        const int v = juce::roundToInt(knob.getValue());
        valueLabel.setText(formatter ? formatter(v) : juce::String(v), juce::dontSendNotification);
    }

    void applyTheme()
    {
        moduleLabel.setColour(juce::Label::textColourId, kTextDim);
        paramLabel.setColour(juce::Label::textColourId, kText);
        moduleLabel.setFont(juce::Font(AppTheme::uiFont(11.5f)));
        paramLabel.setFont(juce::Font(AppTheme::uiFont(13.5f)).boldened());
        valueLabel.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
        valueLabel.setColour(juce::Label::textColourId, kText);
        knob.setColour(juce::Slider::rotarySliderFillColourId, kAccent);
        knob.setColour(juce::Slider::rotarySliderOutlineColourId, kBorder);
        knob.setColour(juce::Slider::thumbColourId, kText);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Nothing to paint by hand: the labels and the knob are children
        juce::ignoreUnused(g);
    }

    void resized() override
    {
        moduleLabel.setBounds(0, 0, getWidth(), 15);
        paramLabel.setBounds(0, 15, getWidth(), 19);
        knob.setBounds((getWidth() - 50) / 2, 38, 50, 50);
        valueLabel.setBounds(0, 90, getWidth(), 20);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            owner.cellRightClicked(wireIndex);
    }

    void rightClickOnKnob() { owner.cellRightClicked(wireIndex); }

    int getWireIndex() const { return wireIndex; }

private:
    // Forwards right-clicks to the cell so the reassign menu works over the knob itself
    struct CellSlider : public juce::Slider
    {
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (cell != nullptr) cell->rightClickOnKnob();
                return;
            }
            juce::Slider::mouseDown(e);
        }
        KnobCell* cell = nullptr;
    };

    void setLabelText(juce::Label& label, const juce::String& text)
    {
        constexpr int maxChars = 13;
        if (text.length() > maxChars)
        {
            label.setText(text.substring(0, maxChars - 3) + "...", juce::dontSendNotification);
            label.setTooltip(text);
        }
        else
        {
            label.setText(text, juce::dontSendNotification);
            label.setTooltip({});
        }
    }

    KnobFloaterPanel& owner;
    int wireIndex;
    juce::String knobLabel;
    bool assigned = false;

    ArcKnobLookAndFeel arcLnF;
    std::function<juce::String(int)> formatter;
    juce::Label moduleLabel, paramLabel, valueLabel;
    CellSlider knob;
};

// ============================================================================
// KnobFloaterPanel
// ============================================================================
KnobFloaterPanel::KnobFloaterPanel()
{
    // 18 grid knobs: wire index = column * 3 + row
    for (int k = 0; k < 18; ++k)
        cells.push_back(std::make_unique<KnobCell>(*this, k, juce::String(k + 1)));

    for (int idx : kSpecialIndices)
        cells.push_back(std::make_unique<KnobCell>(*this, idx,
                            juce::String(KnobAssignmentMessage::getKnobName(idx))));

    for (auto& cell : cells)
        addAndMakeVisible(*cell);

    setSize(kMargin * 2 + kCellW * 6 + kGroupGap * 2,
            kMargin * 2 + kCellH * 4 + kBottomGap);
}

KnobFloaterPanel::~KnobFloaterPanel() = default;

void KnobFloaterPanel::setPatch(Patch* p)
{
    patch = p;
    draggingKnobIndex = -1;
    refresh();
}

void KnobFloaterPanel::refresh()
{
    for (auto& cellPtr : cells)
    {
        auto* cell = cellPtr.get();
        const int k = cell->getWireIndex();
        if (k == draggingKnobIndex) continue;  // don't fight the user's drag

        if (patch == nullptr || !patch->knobAssignments[static_cast<size_t>(k)].assigned)
        {
            cell->setInfo({}, {}, false, 0, 0, 0);
            continue;
        }

        const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];

        // Morph assignment (section=2, module=1, param=0-3)
        if (ka.section == 2)
        {
            if (ka.param >= 0 && ka.param < 4)
                cell->setInfo("Morph", "Morph " + juce::String(ka.param + 1),
                              true, 0, 127, patch->morphValues[static_cast<size_t>(ka.param)]);   // a plain number
            else
                cell->setInfo({}, {}, false, 0, 0, 0);
            continue;
        }

        auto* module = patch->getContainer(ka.section).getModuleByIndex(ka.module);
        auto* param = module != nullptr ? module->getParameter(ka.param) : nullptr;
        if (param == nullptr)
        {
            cell->setInfo({}, {}, false, 0, 0, 0);
            continue;
        }

        const auto* pd = param->getDescriptor();
        cell->setInfo(module->getTitle(),
                      pd != nullptr ? pd->name : "param " + juce::String(ka.param),
                      true,
                      pd != nullptr ? pd->minValue : 0,
                      pd != nullptr ? pd->maxValue : 127,
                      param->getValue(), pd);
    }
}

void KnobFloaterPanel::cellDragStart(int wireIndex)
{
    if (patch == nullptr) return;
    const auto& ka = patch->knobAssignments[static_cast<size_t>(wireIndex)];
    if (!ka.assigned) return;

    draggingKnobIndex = wireIndex;
    if (ka.section == 2)
        dragOldValue = (ka.param >= 0 && ka.param < 4)
                           ? patch->morphValues[static_cast<size_t>(ka.param)] : 0;
    else
    {
        auto* module = patch->getContainer(ka.section).getModuleByIndex(ka.module);
        auto* param = module != nullptr ? module->getParameter(ka.param) : nullptr;
        dragOldValue = param != nullptr ? param->getValue() : 0;
    }
}

void KnobFloaterPanel::cellValueChanged(int wireIndex, int value)
{
    if (patch == nullptr) return;
    const auto& ka = patch->knobAssignments[static_cast<size_t>(wireIndex)];
    if (!ka.assigned) return;

    if (ka.section == 2)
    {
        if (ka.param >= 0 && ka.param < 4)
        {
            patch->morphValues[static_cast<size_t>(ka.param)] = value;
            if (onMorphChanged) onMorphChanged(ka.param, value);
        }
        return;
    }

    auto* module = patch->getContainer(ka.section).getModuleByIndex(ka.module);
    auto* param = module != nullptr ? module->getParameter(ka.param) : nullptr;
    if (param == nullptr) return;

    param->setValue(value);
    if (onParameterChanged) onParameterChanged(ka.section, ka.module, ka.param, value);
}

void KnobFloaterPanel::cellDragEnd(int wireIndex)
{
    if (draggingKnobIndex != wireIndex) { draggingKnobIndex = -1; return; }
    draggingKnobIndex = -1;

    if (patch == nullptr) return;
    const auto& ka = patch->knobAssignments[static_cast<size_t>(wireIndex)];
    if (!ka.assigned || ka.section == 2) return;  // morphs have no undo (matches header bar)

    auto* module = patch->getContainer(ka.section).getModuleByIndex(ka.module);
    auto* param = module != nullptr ? module->getParameter(ka.param) : nullptr;
    if (param == nullptr) return;

    if (param->getValue() != dragOldValue && onParameterDragComplete)
        onParameterDragComplete(ka.section, ka.module, ka.param, dragOldValue, param->getValue());
}

void KnobFloaterPanel::cellRightClicked(int wireIndex)
{
    if (patch == nullptr || !patch->knobAssignments[static_cast<size_t>(wireIndex)].assigned)
        return;

    juce::PopupMenu reassignMenu;
    for (int k = 0; k < KnobAssignmentMessage::numKnobs; ++k)
    {
        if (!KnobAssignmentMessage::isValidKnob(k) || k == wireIndex) continue;
        if (patch->knobAssignments[static_cast<size_t>(k)].assigned) continue;
        reassignMenu.addItem(1 + k, KnobAssignmentMessage::getKnobName(k));
    }

    juce::PopupMenu menu;
    menu.addSubMenu("Reassign to", reassignMenu, reassignMenu.getNumItems() > 0);

    juce::Component::SafePointer<KnobFloaterPanel> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options(), [safeThis, wireIndex](int result) {
        if (safeThis == nullptr || result <= 0) return;
        if (safeThis->onReassignRequested)
            safeThis->onReassignRequested(wireIndex, result - 1);
    });
}

void KnobFloaterPanel::paint(juce::Graphics& g)
{
    g.fillAll(kBg);

    // Vertical separators between the three knob groups
    g.setColour(kBorder);
    const int gridBottom = kMargin + kCellH * 3;
    for (int group = 1; group <= 2; ++group)
    {
        int x = kMargin + kCellW * 2 * group + kGroupGap * group - kGroupGap / 2;
        g.drawVerticalLine(x, (float) kMargin, (float) gridBottom);
    }

    // Separator above the special row
    g.drawHorizontalLine(gridBottom + kBottomGap / 2, (float) kMargin,
                         (float) (getWidth() - kMargin));
}

void KnobFloaterPanel::resized()
{
    // 18 grid knobs: column-major (knob 1,2,3 in first column)
    for (int k = 0; k < 18; ++k)
    {
        int col = k / 3;
        int row = k % 3;
        int x = kMargin + col * kCellW + (col / 2) * kGroupGap;
        int y = kMargin + row * kCellH;
        cells[static_cast<size_t>(k)]->setBounds(x, y, kCellW, kCellH);
    }

    // Special row: spread across the bottom
    int y = kMargin + kCellH * 3 + kBottomGap;
    int spacing = (getWidth() - 2 * kMargin) / 3;
    for (int i = 0; i < 3; ++i)
        cells[static_cast<size_t>(18 + i)]->setBounds(
            kMargin + i * spacing + (spacing - kCellW) / 2, y, kCellW, kCellH);
}

void KnobFloaterPanel::applyTheme()
{
    for (auto& cell : cells)
        cell->applyTheme();
    repaint();
}

// ============================================================================
// KnobFloaterWindow
// ============================================================================
KnobFloaterWindow::KnobFloaterWindow()
    : juce::DocumentWindow("Knob Floater", kBg, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setResizable(false, false);
    setContentNonOwned(&panel, true);

    panel.onParameterChanged = [this](int s, int m, int p, int v) {
        if (onParameterChanged) onParameterChanged(s, m, p, v);
    };
    panel.onParameterDragComplete = [this](int s, int m, int p, int oldV, int newV) {
        if (onParameterDragComplete) onParameterDragComplete(s, m, p, oldV, newV);
    };
    panel.onMorphChanged = [this](int morph, int v) {
        if (onMorphChanged) onMorphChanged(morph, v);
    };
    panel.onReassignRequested = [this](int from, int to) {
        if (onReassignRequested) onReassignRequested(from, to);
    };
}

void KnobFloaterWindow::setPatch(Patch* p) { panel.setPatch(p); }
void KnobFloaterWindow::refresh()          { panel.refresh(); }

void KnobFloaterWindow::closeButtonPressed()
{
    setVisible(false);
    if (onClosed) onClosed();
}

void KnobFloaterWindow::applyTheme()
{
    setBackgroundColour(kBg);
    panel.applyTheme();
    repaint();
}
