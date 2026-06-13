#include "MutatorWindow.h"
#include "AppTheme.h"

#include <cmath>

namespace
{
constexpr int kBoxW = 86, kBoxH = 92;
constexpr int kTempH = 46, kVarH = 52;
constexpr int kPad = 10;

const juce::Colour& bg()       { return AppTheme::palette().backgroundMain; }
const juce::Colour& panelBg()  { return AppTheme::palette().backgroundPanel; }
const juce::Colour& border()   { return AppTheme::palette().borderColor; }
const juce::Colour& text()     { return AppTheme::palette().textPrimary; }
const juce::Colour& textDim()  { return AppTheme::palette().textMuted; }
const juce::Colour& accent()   { return AppTheme::palette().accentActive; }
}

// ============================================================================
// KnobLookAndFeel — same drawing as the canvas module knobs (e.g. the Level
// knob on "2 Outputs"): compact circle at 0.78 scale, grip line, ±135° ticks.
// ============================================================================
class MutatorPanel::KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                 static_cast<float>(width), static_cast<float>(height));
        const auto centre = area.getCentre();
        // Same proportions as PatchCanvas::paintKnobs for a size-21 XML knob
        const float sz = juce::jmin(area.getWidth(), area.getHeight()) * 0.78f;
        const float radius = sz * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(AppTheme::palette().textSecondary);
        g.fillEllipse(centre.x - radius, centre.y - radius, sz, sz);
        g.setColour(AppTheme::palette().borderColor);
        g.drawEllipse(centre.x - radius, centre.y - radius, sz, sz, 1.0f);

        // Travel-limit tick marks outside the circle
        g.setColour(AppTheme::palette().textMuted);
        for (float a : { rotaryStartAngle, rotaryEndAngle })
        {
            const float sa = std::sin(a), ca = std::cos(a);
            g.drawLine(centre.x + sa * radius * 1.08f, centre.y - ca * radius * 1.08f,
                       centre.x + sa * radius * 1.45f, centre.y - ca * radius * 1.45f, 1.5f);
        }

        // Grip line
        const float sa = std::sin(angle), ca = std::cos(angle);
        g.setColour(AppTheme::palette().backgroundMain);
        g.drawLine(centre.x + sa * radius * 0.3f, centre.y - ca * radius * 0.3f,
                   centre.x + sa * radius * 0.85f, centre.y - ca * radius * 0.85f, 1.5f);
    }
};

// ============================================================================
// SnapshotBox — one sound: Mother, Child, Father, Temp Storage or Variation
// ============================================================================
class MutatorPanel::SnapshotBox : public juce::Component,
                                  public juce::DragAndDropTarget
{
public:
    SnapshotBox(MutatorPanel& ownerPanel, int boxIndex, juce::String boxLabel)
        : owner(ownerPanel), index(boxIndex), label(std::move(boxLabel)) {}

    ParamSnapshot snap;
    bool focused = false;

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(snap.filled ? panelBg().brighter(0.06f) : panelBg().darker(0.15f));
        g.fillRoundedRectangle(r, 4.0f);

        // Chromosome graph: one bar per sampled parameter value
        if (snap.filled && !snap.entries.empty())
        {
            auto plot = r.reduced(4.0f);
            plot.removeFromTop(12.0f);
            const int bars = juce::jmin(48, static_cast<int>(snap.entries.size()));
            const float bw = plot.getWidth() / static_cast<float>(bars);
            g.setColour(accent().withAlpha(0.75f));
            for (int i = 0; i < bars; ++i)
            {
                const auto& e = snap.entries[static_cast<size_t>(
                    i * static_cast<int>(snap.entries.size()) / bars)];
                const float norm = juce::jlimit(0.0f, 1.0f, static_cast<float>(e.value) / 127.0f);
                const float h = plot.getHeight() * norm;
                g.fillRect(plot.getX() + bw * static_cast<float>(i),
                           plot.getBottom() - h, juce::jmax(1.0f, bw - 1.0f), h);
            }
        }

        g.setColour(focused ? accent() : (dragOver ? text() : border()));
        g.drawRoundedRectangle(r, 4.0f, focused ? 2.0f : 1.0f);

        g.setColour(snap.filled ? text() : textDim());
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(label, getLocalBounds().removeFromTop(13), juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { owner.showBoxMenu(index); return; }
        owner.focusBox(index, true);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (!snap.filled) return;
        owner.copyToParent(index, kMother);
        owner.generate(GenOp::Mutate);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (snap.filled && !owner.isDragAndDropActive() && e.getDistanceFromDragStart() > 6)
            owner.startDragging(juce::var(index), this);
    }

    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        return details.description.isInt() && static_cast<int>(details.description) != index;
    }
    void itemDragEnter(const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit(const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped(const SourceDetails& details) override
    {
        dragOver = false;
        owner.boxDropped(static_cast<int>(details.description), index,
                         juce::ModifierKeys::getCurrentModifiers());
        repaint();
    }

private:
    MutatorPanel& owner;
    int index;
    juce::String label;
    bool dragOver = false;
};

// ============================================================================
// MutatorPanel
// ============================================================================
MutatorPanel::MutatorPanel()
{
    setWantsKeyboardFocus(true);
    knobLnF = std::make_unique<KnobLookAndFeel>();

    auto addBox = [this](int i, const juce::String& lbl) {
        auto b = std::make_unique<SnapshotBox>(*this, i, lbl);
        addAndMakeVisible(*b);
        boxes.push_back(std::move(b));
    };
    addBox(kMother, "MOTHER (1)");
    for (int c = 1; c <= 6; ++c)
        addBox(c, "Child " + juce::String(c + 1));
    addBox(kFather, "FATHER (8)");
    for (int t = 0; t < 24; ++t)
        addBox(kTempFirst + t, juce::String(t + 1));
    for (int v = 0; v < 8; ++v)
        addBox(kVarFirst + v, "Var " + juce::String(v + 1));

    for (auto* b : { &motherFromPatchBtn, &mutateBtn, &randomizeBtn, &interpolateBtn, &crossBtn })
        addAndMakeVisible(*b);

    motherFromPatchBtn.onClick = [this] {
        if (onCaptureCurrent) { box(kMother).snap = onCaptureCurrent(); box(kMother).repaint(); }
    };
    mutateBtn.onClick      = [this] { generate(GenOp::Mutate); };
    randomizeBtn.onClick   = [this] { generate(GenOp::Randomize); };
    interpolateBtn.onClick = [this] { generate(GenOp::Interpolate); };
    crossBtn.onClick       = [this] { generate(GenOp::Cross); };

    mutateBtn.setTooltip("Mutate: 6 children from Mother (U)");
    randomizeBtn.setTooltip("Randomize: 6 random children (N)");
    interpolateBtn.setTooltip("Interpolate Mother -> Father (I)");
    crossBtn.setTooltip("Cross Mother + Father (X)");

    auto setupKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name,
                            double init, bool plusMinus) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        // % readout above the knob, G2-style
        s.setTextBoxStyle(juce::Slider::TextBoxAbove, true, 46, 13);
        s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                              juce::MathConstants<float>::pi * 2.75f, true);
        s.setRange(0.0, 1.0, 0.01);
        s.textFromValueFunction = [plusMinus](double v) {
            return (plusMinus ? juce::String("+/-") : juce::String())
                 + juce::String(juce::roundToInt(v * 100)) + "%";
        };
        s.setValue(init, juce::dontSendNotification);
        s.updateText();
        s.setLookAndFeel(knobLnF.get());
        addAndMakeVisible(s);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::FontOptions(10.5f));
        addAndMakeVisible(l);
    };
    setupKnob(probSlider, probLabel, "Probability", 0.20, false);
    setupKnob(rangeSlider, rangeLabel, "Range", 0.40, true);
    setupKnob(crossProbSlider, crossProbLabel, "Probability", 0.15, false);

    // Linked (G2 default): low probability <-> large changes
    linkToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(linkToggle);

    // Cross mode: Sequential (default) or Independent
    crossModeToggle.setButtonText("Seq / Ind");
    crossModeToggle.setTooltip("Sequential (default): parent switches at each gene\nIndependent: each gene picks parent separately");
    addAndMakeVisible(crossModeToggle);
    probSlider.onValueChange = [this] {
        if (linkToggle.getToggleState() && !adjustingLinked)
        {
            adjustingLinked = true;
            rangeSlider.setValue(1.0 - probSlider.getValue(), juce::sendNotificationSync);
            adjustingLinked = false;
        }
    };
    rangeSlider.onValueChange = [this] {
        if (linkToggle.getToggleState() && !adjustingLinked)
        {
            adjustingLinked = true;
            probSlider.setValue(1.0 - rangeSlider.getValue(), juce::sendNotificationSync);
            adjustingLinked = false;
        }
    };

    // Audition morph time (applies when clicking a sound)
    auditionLabel.setText("Audition", juce::dontSendNotification);
    auditionLabel.setJustificationType(juce::Justification::centredRight);
    auditionLabel.setFont(juce::FontOptions(10.5f));
    addAndMakeVisible(auditionLabel);
    auditionTimeBox.addItem("Instant", 1);
    auditionTimeBox.addItem("0.5 s", 2);
    auditionTimeBox.addItem("1 s", 3);
    auditionTimeBox.addItem("2 s", 4);
    auditionTimeBox.addItem("5 s", 5);
    auditionTimeBox.addItem("10 s", 6);
    auditionTimeBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(auditionTimeBox);

    tempLabel.setText("TEMPORARY STORAGE", juce::dontSendNotification);
    variationsLabel.setText("VARIATIONS  (drag a sound here to store it)", juce::dontSendNotification);
    quickLockLabel.setText("QUICK LOCKS during one-parent operations  (lit = not evolved; Solo = only this)",
                           juce::dontSendNotification);
    for (auto* l : { &tempLabel, &variationsLabel, &quickLockLabel })
    {
        l->setFont(juce::FontOptions(10.5f, juce::Font::bold));
        addAndMakeVisible(*l);
    }

    for (int i = 0; i < kNumMutCategories; ++i)
    {
        lockBtn[i] = std::make_unique<juce::TextButton>(
            mutCategoryName(static_cast<MutCategory>(i)));
        lockBtn[i]->setClickingTogglesState(true);
        addAndMakeVisible(*lockBtn[i]);

        soloBtn[i] = std::make_unique<juce::TextButton>("Solo");
        soloBtn[i]->setClickingTogglesState(true);
        addAndMakeVisible(*soloBtn[i]);
    }

    applyTheme();
    setSize(kPad * 2 + 8 * kBoxW + 7 * 6, 516);
}

MutatorPanel::~MutatorPanel()
{
    for (auto* s : { &probSlider, &rangeSlider, &crossProbSlider })
        s->setLookAndFeel(nullptr);
}

void MutatorPanel::applyTheme()
{
    auto styleBtn = [](juce::TextButton& b) {
        b.setColour(juce::TextButton::buttonColourId, AppTheme::palette().buttonBackground);
        b.setColour(juce::TextButton::buttonOnColourId, AppTheme::palette().buttonActive);
        b.setColour(juce::TextButton::textColourOffId, AppTheme::palette().textSecondary);
        b.setColour(juce::TextButton::textColourOnId, AppTheme::palette().accentActive);
    };
    for (auto* b : { &motherFromPatchBtn, &mutateBtn, &randomizeBtn, &interpolateBtn, &crossBtn })
        styleBtn(*b);
    for (int i = 0; i < kNumMutCategories; ++i) { styleBtn(*lockBtn[i]); styleBtn(*soloBtn[i]); }

    for (auto* s : { &probSlider, &rangeSlider, &crossProbSlider })
    {
        s->setColour(juce::Slider::textBoxTextColourId, AppTheme::palette().textSecondary);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour(juce::Slider::textBoxBackgroundColourId, AppTheme::palette().inputBackground);
    }
    for (auto* l : { &probLabel, &rangeLabel, &crossProbLabel, &auditionLabel })
        l->setColour(juce::Label::textColourId, AppTheme::palette().textSecondary);
    for (auto* l : { &quickLockLabel, &tempLabel, &variationsLabel })
        l->setColour(juce::Label::textColourId, AppTheme::palette().accentActive);
    linkToggle.setColour(juce::ToggleButton::textColourId, AppTheme::palette().textSecondary);
    linkToggle.setColour(juce::ToggleButton::tickColourId, AppTheme::palette().accentWarning);
    auditionTimeBox.setColour(juce::ComboBox::backgroundColourId, AppTheme::palette().inputBackground);
    auditionTimeBox.setColour(juce::ComboBox::outlineColourId, AppTheme::palette().borderColor);
    auditionTimeBox.setColour(juce::ComboBox::textColourId, AppTheme::palette().textSecondary);
    auditionTimeBox.setColour(juce::ComboBox::arrowColourId, AppTheme::palette().accentWarning);
    repaint();
}

void MutatorPanel::clearAll()
{
    for (auto& b : boxes) { b->snap = ParamSnapshot(); b->focused = false; b->repaint(); }
    focusedIndex = -1;
}

void MutatorPanel::setVariations(const PatchVariations& vars)
{
    for (int v = 0; v < PatchVariations::kNumSlots; ++v)
    {
        box(kVarFirst + v).snap = vars.slots[v];
        box(kVarFirst + v).repaint();
    }
}

void MutatorPanel::paint(juce::Graphics& g)
{
    g.fillAll(bg());
}

void MutatorPanel::resized()
{
    const int gap = 6;
    const int w = getWidth();

    // ── Controls row: prob/range + Link | audition | cross prob ─────────────
    // Knobs sized like the canvas module knobs (size-21 XML knob ≈ 17 px circle)
    probLabel.setBounds(kPad - 6, 6, 70, 12);
    probSlider.setBounds(kPad + 6, 18, 46, 38);
    linkToggle.setBounds(kPad + 56, 30, 50, 18);
    rangeLabel.setBounds(kPad + 104, 6, 70, 12);
    rangeSlider.setBounds(kPad + 116, 18, 46, 38);

    auditionLabel.setBounds(w / 2 - 110, 26, 62, 18);
    auditionTimeBox.setBounds(w / 2 - 44, 24, 92, 22);

    crossProbLabel.setBounds(w - kPad - 130, 6, 70, 12);
    crossProbSlider.setBounds(w - kPad - 112, 18, 46, 38);
    crossModeToggle.setBounds(w - kPad - 58, 30, 48, 18);

    // ── Operation buttons (centered) ─────────────────────────────────────────
    {
        const int widths[5] = { 108, 76, 88, 92, 64 };
        int total = 4 * gap;
        for (int bw : widths) total += bw;
        int x = (w - total) / 2;
        juce::TextButton* btns[5] = { &motherFromPatchBtn, &mutateBtn, &randomizeBtn,
                                      &interpolateBtn, &crossBtn };
        for (int i = 0; i < 5; ++i)
        {
            btns[i]->setBounds(x, 62, widths[i], 24);
            x += widths[i] + gap;
        }
    }

    // ── Mother | Children | Father ───────────────────────────────────────────
    int y = 94;
    int x = kPad;
    for (int i = 0; i <= kFather; ++i)
    {
        box(i).setBounds(x, y, kBoxW, kBoxH);
        x += kBoxW + gap;
    }
    y += kBoxH + 8;

    // ── Temporary storage 3 x 8 ──────────────────────────────────────────────
    tempLabel.setBounds(kPad, y, 460, 16);
    y += 18;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 8; ++col)
            box(kTempFirst + row * 8 + col)
                .setBounds(kPad + col * (kBoxW + gap), y + row * (kTempH + gap), kBoxW, kTempH);
    y += 3 * (kTempH + gap) + 2;

    // ── Variations row ───────────────────────────────────────────────────────
    variationsLabel.setBounds(kPad, y, 460, 16);
    y += 18;
    for (int v = 0; v < 8; ++v)
        box(kVarFirst + v).setBounds(kPad + v * (kBoxW + gap), y, kBoxW, kVarH);
    y += kVarH + 8;

    // ── Quick locks ──────────────────────────────────────────────────────────
    quickLockLabel.setBounds(kPad, y, w - kPad * 2, 16);
    y += 18;
    const int qlW = (w - kPad * 2 - 6 * 4) / 7;
    for (int i = 0; i < kNumMutCategories; ++i)
    {
        lockBtn[i]->setBounds(kPad + i * (qlW + 4), y, qlW, 22);
        soloBtn[i]->setBounds(kPad + i * (qlW + 4), y + 24, qlW, 16);
    }
}

MutatorPanel::GenParams MutatorPanel::currentParams(float interpT) const
{
    GenParams p;
    p.mutateProb  = static_cast<float>(probSlider.getValue());
    p.mutateRange = static_cast<float>(rangeSlider.getValue());
    p.crossProb   = static_cast<float>(crossProbSlider.getValue());
    p.interpT     = interpT;
    p.independentCross = crossModeToggle.getToggleState();
    for (int i = 0; i < kNumMutCategories; ++i)
    {
        p.lock[i] = lockBtn[i]->getToggleState();
        p.solo[i] = soloBtn[i]->getToggleState();
    }
    return p;
}

float MutatorPanel::auditionSeconds() const
{
    switch (auditionTimeBox.getSelectedId())
    {
        case 2: return 0.5f;
        case 3: return 1.0f;
        case 4: return 2.0f;
        case 5: return 5.0f;
        case 6: return 10.0f;
        default: return 0.0f;
    }
}

void MutatorPanel::focusBox(int index, bool audition)
{
    if (index < 0 || index >= kNumBoxes) return;
    if (focusedIndex >= 0) { box(focusedIndex).focused = false; box(focusedIndex).repaint(); }
    focusedIndex = index;
    box(index).focused = true;
    box(index).repaint();
    if (audition && box(index).snap.filled && onAudition)
        onAudition(box(index).snap, auditionSeconds());
    grabKeyboardFocus();
}

void MutatorPanel::copyToParent(int srcIndex, int parentIndex)
{
    if (srcIndex < 0 || srcIndex >= kNumBoxes || !box(srcIndex).snap.filled) return;
    box(parentIndex).snap = box(srcIndex).snap;
    box(parentIndex).repaint();
}

void MutatorPanel::saveFocusedToTemp()
{
    if (focusedIndex < 0 || !box(focusedIndex).snap.filled) return;
    for (int t = kTempFirst; t < kVarFirst; ++t)
    {
        if (!box(t).snap.filled)
        {
            box(t).snap = box(focusedIndex).snap;
            box(t).repaint();
            return;
        }
    }
}

void MutatorPanel::ensureMother()
{
    if (!box(kMother).snap.filled && onCaptureCurrent)
    {
        box(kMother).snap = onCaptureCurrent();
        box(kMother).repaint();
    }
}

void MutatorPanel::generate(GenOp op)
{
    if (!onGenerate) return;
    ensureMother();

    const auto& mother = box(kMother).snap;
    const auto& father = box(kFather).snap;
    if (!mother.filled) return;
    if ((op == GenOp::Interpolate || op == GenOp::Cross) && !father.filled) return;

    for (int c = 1; c <= 6; ++c)
    {
        const float t = static_cast<float>(c) / 7.0f;  // interpolation position
        box(c).snap = onGenerate(op, mother, father, currentParams(t));
        box(c).repaint();
    }
}

void MutatorPanel::boxDropped(int srcIndex, int dstIndex, const juce::ModifierKeys& mods)
{
    if (srcIndex < 0 || srcIndex >= kNumBoxes || !box(srcIndex).snap.filled) return;

    // Dropping on the Variations row stores the sound into that patch variation
    if (dstIndex >= kVarFirst)
    {
        if (onStoreToVariation)
            onStoreToVariation(dstIndex - kVarFirst, box(srcIndex).snap);
        return;
    }

    if (mods.isShiftDown() || mods.isCtrlDown())
    {
        // G2: Shift+drag = interpolate the two sounds, Ctrl+drag = cross them
        if (!box(dstIndex).snap.filled) return;
        box(kMother).snap = box(srcIndex).snap;
        box(kFather).snap = box(dstIndex).snap;
        box(kMother).repaint();
        box(kFather).repaint();
        generate(mods.isShiftDown() ? GenOp::Interpolate : GenOp::Cross);
        return;
    }

    box(dstIndex).snap = box(srcIndex).snap;   // plain drag = copy (overwrite)
    box(dstIndex).repaint();
}

void MutatorPanel::showBoxMenu(int index)
{
    juce::PopupMenu menu;
    const bool filled = box(index).snap.filled;

    menu.addItem(1, "Set as Mother", filled);
    menu.addItem(2, "Set as Father", filled);
    menu.addSeparator();
    juce::PopupMenu storeMenu;
    for (int v = 0; v < 8; ++v)
        storeMenu.addItem(100 + v, "Variation " + juce::String(v + 1));
    menu.addSubMenu("Store to", storeMenu, filled);
    menu.addItem(3, "Save to Temporary Storage", filled && index < kTempFirst);
    menu.addSeparator();
    menu.addItem(4, "Clear", filled && index < kVarFirst);

    menu.showMenuAsync(juce::PopupMenu::Options{}, [this, index](int result) {
        if (result == 1) copyToParent(index, kMother);
        else if (result == 2) copyToParent(index, kFather);
        else if (result == 3) { focusBox(index, false); saveFocusedToTemp(); }
        else if (result == 4) { box(index).snap = ParamSnapshot(); box(index).repaint(); }
        else if (result >= 100 && result < 108 && onStoreToVariation)
            onStoreToVariation(result - 100, box(index).snap);
    });
}

bool MutatorPanel::keyPressed(const juce::KeyPress& key)
{
    // Ctrl-modified keys are app-wide shortcuts — let the window forward them
    if (key.getModifiers().isCommandDown())
        return false;

    const auto c = key.getTextCharacter();
    const int code = key.getKeyCode();

    // 1..8 = focus Mother / Children / Father (G2 layout)
    if (c >= '1' && c <= '8') { focusBox(static_cast<int>(c - '1'), true); return true; }

    if (c == 'o' || c == 'O') { copyToParent(focusedIndex, kMother); return true; }
    if (c == 't' || c == 'T') { copyToParent(focusedIndex, kFather); return true; }
    if (c == 'e' || c == 'E')
    {
        copyToParent(focusedIndex, kMother);
        generate(GenOp::Mutate);
        return true;
    }
    if (c == 'u' || c == 'U') { generate(GenOp::Mutate); return true; }
    if (c == 'n' || c == 'N') { generate(GenOp::Randomize); return true; }
    if (c == 'i' || c == 'I') { generate(GenOp::Interpolate); return true; }
    if (c == 'x' || c == 'X') { generate(GenOp::Cross); return true; }
    if (c == 's' || c == 'S') { saveFocusedToTemp(); return true; }

    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int delta = (code == juce::KeyPress::rightKey) ? 1 : -1;
        int next = focusedIndex < 0 ? kMother : juce::jlimit(0, kNumBoxes - 1, focusedIndex + delta);
        focusBox(next, true);
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        int next = focusedIndex;
        if (code == juce::KeyPress::downKey)
            next = focusedIndex < kTempFirst ? kTempFirst : juce::jmin(kNumBoxes - 1, focusedIndex + 8);
        else
            next = focusedIndex >= kTempFirst + 8 ? focusedIndex - 8
                 : (focusedIndex >= kTempFirst ? kMother : focusedIndex);
        focusBox(juce::jlimit(0, kNumBoxes - 1, next), true);
        return true;
    }
    return false;
}

// ============================================================================
// MutatorWindow
// ============================================================================
MutatorWindow::MutatorWindow()
    : juce::DocumentWindow("Patch Mutator", bg(), juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setResizable(false, false);
    setAlwaysOnTop(true);  // keep the mutator in front of the editor while breeding
    setContentNonOwned(&panel, true);
}

void MutatorWindow::closeButtonPressed()
{
    setVisible(false);
    if (onClosed) onClosed();
}

void MutatorWindow::applyTheme()
{
    setBackgroundColour(bg());
    panel.applyTheme();
}
