#pragma once

#include <array>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "ValueSpinner.h"
#include <functional>

class PatchHeaderBar : public juce::Component,
                       public juce::TooltipClient
{
public:
    PatchHeaderBar();

    // The small icon buttons say what they are; the rest of the bar has no tip.
    juce::String getTooltip() override;

    void setPatch(Patch* p);

    // Set DSP load values (0.0-1.0, or -1 for unknown)
    void setLoadValues(float pva, float e) { loadPva = pva; loadE = e; repaint(); }
    void setSynthName(const juce::String& name) { synthName = name; repaint(); }
    void setSynthDspLoad(int slot0, int slot1, int slot2, int slot3)
    {
        synthDsp[0] = slot0; synthDsp[1] = slot1;
        synthDsp[2] = slot2; synthDsp[3] = slot3;
        repaint();
    }

    using MorphChangeCallback = std::function<void(int morphIndex, int value)>;
    using VoiceChangeCallback = std::function<void(int voices)>;
    using CableVisibilityCallback = std::function<void()>;
    using NameChangeCallback = std::function<void(const juce::String& newName)>;
    using QuickSaveCallback = std::function<void()>;
    using ShakeCablesCallback = std::function<void()>;
    // section=2, module=1, param=morphIndex for morph knobs
    using KnobAssignCallback = std::function<void(int section, int module, int param, int knob)>;
    using MidiCtrlAssignCallback = std::function<void(int section, int module, int param, int cc)>;
    using KeyboardAssignCallback = std::function<void(int morphIndex, int keyboard)>; // 0=disable, 1=velocity, 2=note

    void setMorphChangeCallback(MorphChangeCallback cb) { morphChangeCallback = std::move(cb); }
    void setVoiceChangeCallback(VoiceChangeCallback cb) { voiceChangeCallback = std::move(cb); }
    void setCableVisibilityCallback(CableVisibilityCallback cb) { cableVisCallback = std::move(cb); }
    void setNameChangeCallback(NameChangeCallback cb) { nameChangeCallback = std::move(cb); }
    void setQuickSaveCallback(QuickSaveCallback cb) { quickSaveCallback = std::move(cb); }
    void setShakeCablesCallback(ShakeCablesCallback cb) { shakeCablesCallback = std::move(cb); }
    void setKnobAssignCallback(KnobAssignCallback cb) { knobAssignCallback = std::move(cb); }
    void setMidiCtrlAssignCallback(MidiCtrlAssignCallback cb) { midiCtrlAssignCallback = std::move(cb); }
    void setKeyboardAssignCallback(KeyboardAssignCallback cb) { keyboardAssignCallback = std::move(cb); }

    using ReportBugCallback = std::function<void()>;
    void setReportBugCallback(ReportBugCallback cb) { reportBugCallback = std::move(cb); }

    // Patch Mutator quick-access button (right of the snapshot buttons)
    using MutatorButtonCallback = std::function<void()>;
    void setMutatorButtonCallback(MutatorButtonCallback cb) { mutatorButtonCallback = std::move(cb); }
    void setMutatorOpen(bool open) { mutatorOpen = open; repaint(); }

    // Heart (donate) and Patreon, right of ABCD. Each opens its page through
    // the callback, which is MainComponent's openURL.
    using SupportCallback = std::function<void(const juce::String& url)>;
    void setSupportCallback(SupportCallback cb) { supportCallback = std::move(cb); }

    // Snapshot buttons (click=recall, shift+click=save, right-click=copy/init/interpolation menu)
    using SnapshotClickCallback = std::function<void(int index, bool isShiftClick)>;
    using SnapshotInterpolateCallback = std::function<void(int fromIndex, int toIndex, float seconds)>;
    using SnapshotCopyCallback = std::function<void(int fromIndex, int toIndex)>;
    using SnapshotInitCallback = std::function<void(int index)>;
    void setSnapshotClickCallback(SnapshotClickCallback cb) { snapshotClickCallback = std::move(cb); }
    void setSnapshotInterpolateCallback(SnapshotInterpolateCallback cb) { snapshotInterpolateCallback = std::move(cb); }
    void setSnapshotCopyCallback(SnapshotCopyCallback cb) { snapshotCopyCallback = std::move(cb); }
    void setSnapshotInitCallback(SnapshotInitCallback cb) { snapshotInitCallback = std::move(cb); }
    void setSnapshotFilled(int index, bool filled);
    void setActiveSnapshot(int index) { activeSnapshot = index; repaint(); }
    void setInterpolationProgress(float progress);  // 0-1, <0 = not interpolating

    // Morph A/B fader (editor-side software morph, sits between the snapshot
    // row and the MUT button). Left-drag scrubs A->B; right-click captures the
    // A/B endpoints and Learns a physical panel knob as the drive source.
    using MorphFaderCallback = std::function<void(float pos)>;    // drag -> pos 0..1
    // Set endpoint A (isB=false) or B (isB=true) from snapIndex; snapIndex -1 = current sound
    using MorphSetEndpointCallback = std::function<void(bool isB, int snapIndex)>;
    using MorphLearnCallback = std::function<void()>;             // arm panel-knob Learn
    using MorphClearKnobCallback = std::function<void()>;         // clear knob assignment
    using MorphAssignKnobCallback = std::function<void(int knobIndex)>;  // assign knob editor->synth
    void setMorphFaderCallback(MorphFaderCallback cb) { morphFaderCallback = std::move(cb); }
    void setMorphSetEndpointCallback(MorphSetEndpointCallback cb) { morphSetEndpointCallback = std::move(cb); }
    void setMorphAssignKnobCallback(MorphAssignKnobCallback cb) { morphAssignKnobCallback = std::move(cb); }
    void setMorphLearnCallback(MorphLearnCallback cb) { morphLearnCallback = std::move(cb); }
    void setMorphClearKnobCallback(MorphClearKnobCallback cb) { morphClearKnobCallback = std::move(cb); }
    void setMorphEndpoints(bool hasA, bool hasB) { morphHasA = hasA; morphHasB = hasB; repaint(); }
    void setMorphFaderPos(float pos) { morphFaderPos = juce::jlimit(0.0f, 1.0f, pos); repaint(); }
    void setMorphLearnArmed(bool armed) { morphLearnArmed = armed; repaint(); }
    void setMorphKnobAssigned(bool assigned) { morphKnobAssigned = assigned; repaint(); }

    // Bank location of the patch on screen, shown on the store button next to
    // the patch name and used by the one-click store (-1 = unknown).
    void setCurrentLocation(int section, int position);
    void clearCurrentLocation();
    void setStoreEnabled(bool enabled);   // false = not connected, button greyed
    // Several bank positions carry this patch's name, so the editor has a
    // shortlist but no answer: the button says so with a "?" and a click goes
    // to the dialog rather than overwriting a guess.
    void setStoreUncertain(bool uncertain);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void drawMorphKnob(juce::Graphics& g, float cx, float cy, float size,
                       float normalized, const juce::String& label, juce::Colour colour);
    void drawLoadBar(juce::Graphics& g, juce::Rectangle<float> r,
                     float percent, const juce::String& label);

    int getMorphKnobAt(juce::Point<int> pos) const;
    juce::Rectangle<float> getMorphKnobBounds(int i) const;

    int getCableToggleAt(juce::Point<int> pos) const;
    juce::Rectangle<float> getCableToggleBounds(int i) const;

    juce::Rectangle<int> getPatchNameBounds() const;
    juce::Rectangle<float> getStoreButtonBounds() const;
    bool isStoreButtonAt(juce::Point<int> pos) const;

    enum class ArrowHit { None, Up, Down };
    ArrowHit getVoiceArrowAt(juce::Point<int> pos) const;

    void toggleCableVisibility(int index);
    bool getCableVisibility(int index) const;

    Patch* patch = nullptr;
    float loadPva = -1.0f;
    float loadE = -1.0f;
    juce::String synthName;
    int synthDsp[4] = { -1, -1, -1, -1 };  // per-slot DSP load (0-127), -1=unknown

    struct DragState
    {
        int morphIndex = -1;
        int startValue = 0;
        juce::Point<int> startPos;
        int lastSentValue = -1;
        juce::int64 lastSendTime = 0;
    };
    DragState dragState;

    MorphChangeCallback morphChangeCallback;
    VoiceChangeCallback voiceChangeCallback;
    CableVisibilityCallback cableVisCallback;
    NameChangeCallback nameChangeCallback;
    QuickSaveCallback quickSaveCallback;
    ShakeCablesCallback shakeCablesCallback;
    ReportBugCallback reportBugCallback;
    KnobAssignCallback knobAssignCallback;
    MidiCtrlAssignCallback midiCtrlAssignCallback;
    KeyboardAssignCallback keyboardAssignCallback;
    SnapshotClickCallback snapshotClickCallback;
    SnapshotInterpolateCallback snapshotInterpolateCallback;
    SnapshotCopyCallback snapshotCopyCallback;
    SnapshotInitCallback snapshotInitCallback;
    MutatorButtonCallback mutatorButtonCallback;
    bool mutatorOpen = false;
    SupportCallback supportCallback;
    // Top-level (no parent): the bar is 48px tall and would clip a child tip.
    juce::TooltipWindow tooltipWindow { nullptr, 600 };

    void showMorphKnobContextMenu(int morphIndex);

    juce::Rectangle<float> getShakeButtonBounds() const;
    bool isShakeButtonAt(juce::Point<int> pos) const;
    juce::Rectangle<float> getBugButtonBounds() const;
    bool isBugButtonAt(juce::Point<int> pos) const;

    // Snapshot buttons
    juce::Rectangle<float> getSnapshotButtonBounds(int index) const;
    int getSnapshotButtonAt(juce::Point<int> pos) const;  // -1 if none
    juce::Rectangle<float> getMutatorButtonBounds() const;
    bool isMutatorButtonAt(juce::Point<int> pos) const;
    static constexpr float supportButtonSize = 22.0f;
    static constexpr float supportEdge = 10.0f;      // gap to the bar's right edge
    static constexpr int supportBlockW = 56;         // both buttons, the gap and a margin
    juce::Rectangle<float> getDonateButtonBounds() const;
    juce::Rectangle<float> getPatreonButtonBounds() const;

    // Morph A/B fader
    juce::Rectangle<float> getMorphFaderBounds() const;
    bool isMorphFaderAt(juce::Point<int> pos) const;
    void showMorphFaderMenu();
    void showCablesMenu();
    float morphFaderPosFromX(float x) const;
    MorphFaderCallback morphFaderCallback;
    MorphSetEndpointCallback morphSetEndpointCallback;
    MorphAssignKnobCallback morphAssignKnobCallback;
    MorphLearnCallback morphLearnCallback;
    MorphClearKnobCallback morphClearKnobCallback;
    bool morphHasA = false, morphHasB = false;
    bool morphLearnArmed = false;
    bool morphKnobAssigned = false;
    float morphFaderPos = 0.0f;
    bool morphDragging = false;

    // The morph dials get the same nudge arrows as the canvas knobs, so a macro
    // can be set to an exact figure rather than swept to roughly the right
    // place. They sit inside the dial rather than under it: the caption is
    // directly below, and the bottom of a -135..+135 sweep is dead space.
    ValueSpinner morphSpinner { *this };
    int morphSpinnerIndex = -1;
    void morphSpinnerStep(int delta);

    bool snapshotFilled[8] = {};
    int activeSnapshot = -1;       // -1 = none active
    float interpolationProgress = -1.0f;  // <0 = not interpolating
    float snapshotInterpSeconds = 0.0f;   // 0 = instant recall

    std::unique_ptr<juce::Label> patchNameEditor;
    bool storeHover = false;
    bool storeEnabled = false;
    bool storeUncertain = false;

public:
    int currentSection = -1;  // -1 = no location set (public for quick save access)
    int currentPosition = -1;

private:

    // Every element's rectangle, worked out once in resized(); paint and the hit tests read these.
    struct Layout
    {
        juce::Rectangle<int> name;
        juce::Rectangle<float> voicesLabel, voicesField, voicesUp, voicesDown;
        juce::Rectangle<float> loadPva, loadE, store, xfade;
        std::array<juce::Rectangle<float>, 4> morph;
        std::array<juce::Rectangle<float>, 8> snap;
        std::array<juce::Rectangle<float>, 7> cable;
        juce::Rectangle<float> shake, mutator, donate, patreon;
        float storeWordW = 0.0f;   // measured in resized(), so the word and the location never overlap
        std::vector<float> seps;   // x of each hairline between groups
        bool cablesCompact = false;  // too narrow for the dots: one "Cables" button and a menu
        juce::Rectangle<float> cablesBtn;
    };
    Layout L;

    static constexpr int morphKnobSize = 24;
    static constexpr int morphKnobSpacing = 8;
    static constexpr int cableToggleSize = 16;
    static constexpr int cableToggleSpacing = 6;
    static constexpr int numCableTypes = 7;
    static constexpr int paramSendIntervalMs = 50;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchHeaderBar)
};
