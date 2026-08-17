#include "PatchCanvasComponent.h"
#include "QuickAddPopup.h"
#include "KnobDrag.h"
#include "../model/ModulePlacement.h"
#include "../protocol/KnobAssignmentMessage.h"
#include <cmath>
#include <set>
#include <unordered_map>

// PatchCanvas: the pointer and the keyboard. Hit testing, dragging, the hover
// readout, the nudge arrows and the context menus they open.

static std::set<juce::String> sequencerStepIds(const ModuleTheme& theme)
{
    std::set<juce::String> ids;
    for (auto& ts : theme.sliders)
        ids.insert(ts.componentId);
    for (auto& cd : theme.customDisplays)
        if (cd.type == "note-seq-editor")
            for (auto& id : cd.noteStepIds)
                if (id.isNotEmpty())
                    ids.insert(id);
    return ids;
}

static bool isSequencerStepParam(const ParameterDescriptor& pd,
                                 const std::set<juce::String>& stepIds)
{
    return stepIds.count(pd.componentId) != 0
        || (pd.name.startsWithIgnoreCase("seq ") && pd.name.containsIgnoreCase("step "));
}

// Decoration bitmap cache: iconName ("decoration-N") → loaded juce::Image.
// PNGs are embedded via juce_add_binary_data in CMakeLists.txt.
void PatchCanvas::zoomAnchoredToPointer(float newZoom, const juce::MouseEvent& e)
{
    newZoom = juce::jlimit(zoomMin, zoomMax, newZoom);
    if (std::abs(newZoom - zoomLevel) < 0.001f)
        return;

    // Get canvas-space point under cursor before zoom
    auto canvasPt = screenToCanvas(e.getPosition());

    zoomLevel = newZoom;
    updateSizeForZoom();

    // Adjust viewport so the same canvas point stays under the cursor
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        auto vpMouse = e.getEventRelativeTo(vp).getPosition();
        int newVpX = juce::roundToInt(canvasPt.x * newZoom) - vpMouse.x;
        int newVpY = juce::roundToInt(canvasPt.y * newZoom) - vpMouse.y;
        vp->setViewPosition(juce::jmax(0, newVpX), juce::jmax(0, newVpY));
    }

    repaint();
}

void PatchCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomAnchoredToPointer(zoomLevel + wheel.deltaY * zoomStep * 3.0f, e);
        return;
    }

    // Default: let viewport handle normal scrolling
    juce::Component::mouseWheelMove(e, wheel);
}

void PatchCanvas::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    // Trackpad pinch (issue #72). scaleFactor is a multiplier for the gesture
    // step, not an increment, so it multiplies the zoom: pinching out and back
    // in lands on the level you started from, which adding would not do. No
    // modifier here, unlike the wheel: on a trackpad the pinch is the gesture.
    if (scaleFactor > 0.0f)
        zoomAnchoredToPointer(zoomLevel * scaleFactor, e);
}

bool PatchCanvas::findControlAt(juce::Point<int> canvasPos, HoverTarget& out) const
{
    if (patch == nullptr || themeData == nullptr)
        return false;

    const auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();

    for (auto& modulePtr : container.getModules())
    {
        const auto& m = *modulePtr;
        auto bounds = getModuleBounds(m, 0);
        if (!bounds.contains(canvasPos))
            continue;

        const auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
        if (theme == nullptr)
            return false;

        const auto rel = canvasPos - bounds.getPosition();

        auto hit = [&](const juce::String& componentId, juce::Rectangle<int> r) -> bool
        {
            if (componentId.isEmpty() || !r.contains(rel))
                return false;
            if (findParameter(m, componentId) == nullptr)
                return false;
            out.module        = refTo(m);
            out.componentId   = componentId;
            out.controlBounds = r.translated(bounds.getX(), bounds.getY()).toFloat();
            out.moduleBounds  = bounds;
            return true;
        };

        for (const auto& tk : theme->knobs)
            if (hit(tk.componentId, { tk.x, tk.y, tk.size, tk.size })) return true;
        for (const auto& ts : theme->sliders)
            if (hit(ts.componentId, { ts.x, ts.y, ts.width, ts.height })) return true;
        for (const auto& tb : theme->buttons)
            if (hit(tb.componentId, { tb.x, tb.y, tb.width, tb.height })) return true;
        for (const auto& td : theme->textDisplays)
            if (hit(td.componentId, { td.x, td.y, td.width, td.height })) return true;

        // Over the module but not over a control: the module itself is the
        // target, and what it has to say is what it costs the DSP (issue #31,
        // which the original editor answers on a double-click).
        out.module        = refTo(m);
        out.componentId   = {};
        out.controlBounds = bounds.removeFromTop(1).toFloat();
        out.moduleBounds  = getModuleBounds(m, 0);
        return true;
    }
    return false;
}

// Same walk as findControlAt, but starting from a parameter rather than from a
// point: while a control is being dragged we know which one it is and only need
// to know where it sits.
bool PatchCanvas::controlBoundsFor(const Module& m, const juce::String& componentId,
                                   juce::Rectangle<float>& outControl,
                                   juce::Rectangle<int>& outModule) const
{
    if (themeData == nullptr || componentId.isEmpty())
        return false;
    const auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
    if (theme == nullptr)
        return false;

    const auto bounds = getModuleBounds(m, 0);
    auto take = [&](const juce::String& id, juce::Rectangle<int> r) -> bool
    {
        if (id != componentId)
            return false;
        outControl = r.translated(bounds.getX(), bounds.getY()).toFloat();
        outModule  = bounds;
        return true;
    };

    for (const auto& tk : theme->knobs)
        if (take(tk.componentId, { tk.x, tk.y, tk.size, tk.size })) return true;
    for (const auto& ts : theme->sliders)
        if (take(ts.componentId, { ts.x, ts.y, ts.width, ts.height })) return true;
    for (const auto& tb : theme->buttons)
        if (take(tb.componentId, { tb.x, tb.y, tb.width, tb.height })) return true;
    for (const auto& td : theme->textDisplays)
        if (take(td.componentId, { td.x, td.y, td.width, td.height })) return true;
    return false;
}

// While a control is being dragged its value is read out with no delay: the
// point of turning a knob is watching where it lands.
void PatchCanvas::mouseMove(const juce::MouseEvent& e)
{
    // Modules waiting to be dropped follow the pointer, into this canvas from
    // whichever one it was over before. Nothing else is worth reading out while
    // they do.
    if (pendingDrop.active())
    {
        updatePendingGhost(screenToCanvas(e.getPosition()));
        clearHover();
        clearSpinner();
        return;
    }

    // Text notes take the pointer before modules do, the way they take a click.
    // Over a corner grip the cursor says so, which is the only hint that a note
    // can be pulled bigger.
    {
        const auto canvasPos = screenToCanvas(e.getPosition());
        auto* overComment = getCommentAt(canvasPos);
        const int id = overComment != nullptr ? overComment->id : -1;
        const auto grip = overComment != nullptr ? commentGripAt(*overComment, canvasPos)
                                                 : CommentGrip::None;

        if (id != hoverCommentId || grip != hoverCommentGrip)
        {
            // Only the note that lost the hover and the one that gained it
            // change: this fires on every pointer move across a note's edge,
            // and redrawing the whole area for two grip handles was the most
            // expensive thing the mouse could do while hovering.
            const int previousId = hoverCommentId;
            hoverCommentId = id;
            hoverCommentGrip = grip;
            setMouseCursor(grip == CommentGrip::BottomRight
                               ? juce::MouseCursor::BottomRightCornerResizeCursor
                               : grip == CommentGrip::BottomLeft
                                     ? juce::MouseCursor::BottomLeftCornerResizeCursor
                                     : juce::MouseCursor::NormalCursor);
            repaintComment(previousId);
            repaintComment(id);
        }

        if (overComment != nullptr)
        {
            clearHover();
            clearSpinner();
            return;
        }
    }

    // The arrows answer the pointer straight away, the way the original's do —
    // the tooltip pause below is for the value box, which is a different thing.
    updateSpinner(screenToCanvas(e.getPosition()));

    HoverTarget target;
    const bool found = findControlAt(screenToCanvas(e.getPosition()), target);

    if (found && target.sameControlAs(hoverTarget))
    {
        hoverTarget = target;   // same control, refresh its bounds after a zoom
        return;
    }

    const bool wasVisible = hoverBadgeVisible;
    hoverBadgeVisible = false;
    hoverTarget = found ? target : HoverTarget{};

    // Delayed like a tooltip: without it, dragging the cursor across a module
    // flashes a box over every knob it crosses.
    if (found)
        startTimer(hoverDelayMs);
    else
        stopTimer();

    if (wasVisible)
        repaint();
}

void PatchCanvas::mouseExit(const juce::MouseEvent&)
{
    clearHover();
    clearSpinner();

    if (hoverCommentId != -1 || hoverCommentGrip != CommentGrip::None)
    {
        const int previousId = hoverCommentId;
        hoverCommentId = -1;
        hoverCommentGrip = CommentGrip::None;
        // Not while a block is hanging off the pointer: that cursor is the copy
        // one, and it belongs to the drop, not to the note we just left.
        if (!pendingDrop.active())
            setMouseCursor(juce::MouseCursor::NormalCursor);
        repaintComment(previousId);
    }

    // The block stays on the pointer when it leaves; it just stops being drawn
    // here, and the next canvas it enters picks it up.
    if (pendingHost == this)
    {
        pendingHost = nullptr;
        repaint();
    }
}

void PatchCanvas::clearHover()
{
    stopTimer();
    hoverTarget = HoverTarget{};
    if (hoverBadgeVisible)
    {
        hoverBadgeVisible = false;
        repaint();
    }
}

void PatchCanvas::timerCallback()
{
    stopTimer();
    if (!hoverTarget.module.isValid())
        return;
    hoverBadgeVisible = true;
    repaint();
}

// ── Nudge arrows ─────────────────────────────────────────────────────────────

// Same walk as findControlAt, but only over the things worth nudging: a button
// has two states and a text display is driven by the knob beside it, so neither
// has a step to take.
bool PatchCanvas::findSpinnerAt(juce::Point<int> canvasPos, SpinnerTarget& out)
{
    out = SpinnerTarget{};
    if (patch == nullptr || themeData == nullptr)
        return false;

    auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();

    for (auto& modulePtr : container.getModules())
    {
        auto& m = *modulePtr;
        const auto bounds = getModuleBounds(m, 0);
        if (!bounds.contains(canvasPos))
            continue;

        const auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
        if (theme == nullptr)
            return false;

        const auto rel = canvasPos - bounds.getPosition();

        auto take = [&](const juce::String& componentId, juce::Rectangle<int> r) -> bool
        {
            if (componentId.isEmpty() || !r.contains(rel))
                return false;
            const auto* param = findParameter(m, componentId);
            if (param == nullptr)
                return false;
            const auto* pd = param->getDescriptor();
            if (pd == nullptr || pd->maxValue <= pd->minValue)
                return false;

            out.module       = refTo(m);
            out.componentId  = componentId;
            out.moduleBounds = bounds;
            out.control      = r.translated(bounds.getX(), bounds.getY()).toFloat();
            return true;
        };

        for (const auto& tk : theme->knobs)
            if (take(tk.componentId, { tk.x, tk.y, tk.size, tk.size })) return true;
        for (const auto& ts : theme->sliders)
            if (take(ts.componentId, { ts.x, ts.y, ts.width, ts.height })) return true;

        return false;   // over the module, but not over anything that steps
    }
    return false;
}

void PatchCanvas::updateSpinner(juce::Point<int> canvasPos)
{
    const auto p = canvasPos.toFloat();

    // The buttons hang below the control they belong to, so the pointer being
    // on one of them is not the same as being on the control. Ask them first or
    // the pair vanishes the moment you reach for it.
    if (spinner.contains(p) || spinner.isHeld())
    {
        spinner.updateHover(p);
        return;
    }

    SpinnerTarget next;
    findSpinnerAt(canvasPos, next);
    spinnerTarget = next;

    // The module and the id together name the control: two modules of the same
    // type both have a "p1", and one module outlives a zoom that moves it.
    const juce::String key = !next.module.isValid() ? juce::String()
        : juce::String(next.module.section) + ":"
              + juce::String(next.module.containerIndex) + "/" + next.componentId;

    spinner.showFor(key, next.control, ValueSpinner::Placement::BelowEdge);
    spinner.updateHover(p);
}

void PatchCanvas::clearSpinner()
{
    if (spinner.isHeld())
        return;   // a press outlives the hover that started it
    spinner.hide();
    spinnerTarget = SpinnerTarget{};
}

bool PatchCanvas::spinnerMouseDown(juce::Point<int> canvasPos)
{
    auto* module = resolve(spinnerTarget.module);
    if (module == nullptr)
        return false;

    auto* param = findParameter(*module, spinnerTarget.componentId);
    if (param == nullptr)
        return false;

    spinnerValueBeforePress = param->getValue();
    return spinner.mouseDown(canvasPos.toFloat(), [this](int delta) { spinnerStep(delta); });
}

void PatchCanvas::spinnerStep(int delta)
{
    auto* module = resolve(spinnerTarget.module);
    if (module == nullptr)
        return;

    auto* param = findParameter(*module, spinnerTarget.componentId);
    if (param == nullptr)
        return;
    auto* pd = param->getDescriptor();
    if (pd == nullptr)
        return;

    const int oldValue = param->getValue();
    const int newValue = juce::jlimit(pd->minValue, pd->maxValue, oldValue + delta);
    if (newValue == oldValue)
    {
        spinner.stopRepeat();   // at the end of the range there is no more
        return;
    }

    param->setValue(newValue);
    if (parameterChangeCallback)
        parameterChangeCallback(mySection, spinnerTarget.module.containerIndex,
                                pd->index, newValue);

    // Read the value out while it is being stepped. Chasing an exact number is
    // the whole point of the arrows, so the number had better be on screen,
    // without the tooltip pause a plain hover waits through.
    hoverTarget.module        = spinnerTarget.module;
    hoverTarget.componentId   = spinnerTarget.componentId;
    hoverTarget.controlBounds = spinnerTarget.control;
    hoverTarget.moduleBounds  = spinnerTarget.moduleBounds;
    hoverBadgeVisible = true;
    stopTimer();

    repaint();
}

// A run of `+` or `-` presses on one control is one gesture. The value it
// started from is kept so the whole run undoes in a single step, exactly as a
// held nudge arrow does; moving to another control closes the run first.
void PatchCanvas::beginKeyStep()
{
    auto* module = resolve(spinnerTarget.module);
    if (module == nullptr)
        return;
    if (keyStepModule == spinnerTarget.module && keyStepComponentId == spinnerTarget.componentId)
        return;   // already stepping this one

    endKeyStep();

    if (const auto* param = findParameter(*module, spinnerTarget.componentId))
    {
        keyStepModule      = spinnerTarget.module;
        keyStepComponentId = spinnerTarget.componentId;
        keyStepStartValue  = param->getValue();
    }
}

void PatchCanvas::endKeyStep()
{
    if (!keyStepModule.isValid())
        return;

    // The module may have been deleted while the key was held; resolving the
    // reference answers that on its own.
    auto* module = resolve(keyStepModule);
    const auto componentId = keyStepComponentId;
    const int startValue = keyStepStartValue;
    keyStepModule.clear();
    keyStepComponentId.clear();

    if (module == nullptr)
        return;

    if (paramDragCompleteCallback)
        if (auto* param = findParameter(*module, componentId))
            if (auto* pd = param->getDescriptor())
                if (param->getValue() != startValue)
                    paramDragCompleteCallback(mySection, module->getContainerIndex(),
                                              pd->index, startValue, param->getValue());
}

bool PatchCanvas::keyStateChanged(bool isKeyDown)
{
    // Nothing to close while a step key is still held: the key repeating is one
    // gesture, not one per repeat.
    if (!isKeyDown
        && !juce::KeyPress::isKeyCurrentlyDown('+')
        && !juce::KeyPress::isKeyCurrentlyDown('=')
        && !juce::KeyPress::isKeyCurrentlyDown('-')
        && !juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::numberPadAdd)
        && !juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::numberPadSubtract))
    {
        endKeyStep();
    }
    return false;   // this is a notification, not a key we are claiming
}

void PatchCanvas::spinnerRelease()
{
    if (!spinner.mouseUp())
        return;

    // One undo step for the whole press, however many times it repeated, the
    // way a knob drag records itself from where it started to where it landed.
    if (auto* module = resolve(spinnerTarget.module))
        if (paramDragCompleteCallback)
            if (auto* param = findParameter(*module, spinnerTarget.componentId))
                if (auto* pd = param->getDescriptor())
                    if (param->getValue() != spinnerValueBeforePress)
                        paramDragCompleteCallback(mySection, spinnerTarget.module.containerIndex,
                                                  pd->index, spinnerValueBeforePress, param->getValue());
}

void PatchCanvas::openQuickAddAtMouse()
{
    if (patch == nullptr || moduleDescs == nullptr)
        return;
    if (activeQuickAdd != nullptr)
        return;  // already open

    auto mousePos = screenToCanvas(getMouseXYRelative());

    int gx = juce::jlimit(0, 39, mousePos.x / gridX);
    int gy = juce::jlimit(0, 127, mousePos.y / gridY);

    auto screenPos = localPointToGlobal(getMouseXYRelative());

    activeQuickAdd = new QuickAddPopup(
        *moduleDescs, screenPos, gx, gy,
        [this](const ModuleDescriptor* desc, int, int)
        {
            // Picking a module hands it to the pointer rather than placing it:
            // the click that follows chooses the spot, and the area (issue #36).
            if (desc != nullptr)
                beginAddModuleGhost(desc->index, desc->name);
        },
        [this]() { activeQuickAdd = nullptr; }
    );

    activeQuickAdd->grabFocusNow();
}

void PatchCanvas::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (patch == nullptr || moduleDescs == nullptr || !e.mods.isLeftButtonDown())
        return;

    // Only on empty canvas — double-clicking a module must not open Quick Add.
    // On a module it answers what that module costs the DSP, as the original
    // editor does; on one of its controls it is already a reset to default, so
    // leave that alone.
    auto pos = screenToCanvas(e.getPosition());

    // Two quick nudges are two nudges, not a request for the module's DSP cost.
    if (spinner.contains(pos.toFloat()))
        return;

    if (auto* comment = getCommentAt(pos))
    {
        selectComment(comment->id, false);
        showCommentEditor(comment->id);
        return;
    }

    HoverTarget target;
    if (findControlAt(pos, target))
    {
        if (target.componentId.isEmpty())
        {
            costBadgeModule = target.module;
            repaint();
        }
        return;
    }

    openQuickAddAtMouse();
}

void PatchCanvas::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    // A hint box left standing over a knob being turned would just be stale.
    clearHover();
    if (costBadgeModule.isValid()) { costBadgeModule.clear(); repaint(); }

    if (patch == nullptr || themeData == nullptr)
        return;

    auto pos = screenToCanvas(e.getPosition());

    // A block hanging off the pointer takes the click: the left button puts it
    // down here, the right one throws it away, and nothing underneath is
    // selected or dragged in the meantime.
    if (pendingDrop.active())
    {
        if (e.mods.isPopupMenu() || e.mods.isMiddleButtonDown())
            cancelPendingDrop();
        else
            dropPendingAt(pos);
        return;
    }

    // The nudge arrows showing under the hovered control take the click before
    // anything beneath them sees it — they overlap the very knob they step.
    if (!e.mods.isPopupMenu() && !e.mods.isMiddleButtonDown() && spinnerMouseDown(pos))
        return;

    // Middle-click: start canvas pan (drag to scroll viewport)
    if (e.mods.isMiddleButtonDown())
    {
        dragState = DragState();
        dragState.type = DragState::CanvasPan;
        dragState.startPos = e.getScreenPosition();  // absolute screen coords for pan
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    // Editor text notes, before modules: they own their grid rectangle outright.
    // Everything here mirrors what clicking a module does, because a note is
    // meant to feel like one.
    if (auto* comment = getCommentAt(pos))
    {
        const bool alreadySelected = isCommentSelected(comment->id);
        const bool addToSel = e.mods.isShiftDown();

        if (!alreadySelected || addToSel)
        {
            if (!addToSel)
                clearSelection();       // modules and other notes let go
            selectComment(comment->id, true);
        }

        if (e.mods.isPopupMenu())
        {
            showCommentContextMenu(comment->id);
            repaint();
            return;
        }

        auto bounds = getCommentBounds(*comment);
        dragState = DragState();
        dragCommentId = comment->id;
        dragCommentStartPos = { comment->x, comment->y };
        dragCommentStartRect = { comment->x, comment->y,
                                 comment->gridWidth(), comment->gridHeight() };

        // A grab on a bottom corner pulls the note bigger; anywhere else moves
        // it. Resizing is always about the one note under the pointer, whatever
        // else happens to be selected.
        const auto grip = commentGripAt(*comment, pos);
        if (grip != CommentGrip::None)
        {
            dragState.type = DragState::CommentResize;
            dragState.startPos = pos;
            dragCommentGrip = grip;
            repaint();
            return;
        }

        // With more than one thing selected the whole selection travels, the
        // way it does when the drag starts on a module.
        if (selection.size() + selectedCommentIds.size() > 1)
        {
            beginMultiMove(pos);
            repaint();
            return;
        }

        dragState.type = DragState::CommentMove;
        dragState.startPos = pos;
        dragCommentOffsetX = pos.x - bounds.getX();
        dragCommentOffsetY = pos.y - bounds.getY();
        repaint();
        return;
    }

    // Each canvas handles exactly one section; yOffset is always 0.
    ModuleContainer& activeContainer = (mySection == 1)
        ? patch->getPolyVoiceArea()
        : patch->getCommonArea();
    struct { ModuleContainer* container; int section; int yOffset; } areas[] = {
        { &activeContainer, mySection, 0 }
    };

    for (auto& area : areas)
    {
        for (auto& modulePtr : area.container->getModules())
        {
            auto& m = *modulePtr;
            auto rect = getModuleBounds(m, area.yOffset);

            if (!rect.contains(pos))
                continue;

            // Module was clicked, now test UI components
            auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
            if (theme == nullptr)
                continue;

            auto relPos = pos - rect.getPosition();

            // Test connectors FIRST (cable creation / deletion)
            for (auto& tc : theme->connectors)
            {
                juce::Rectangle<int> connRect(tc.x, tc.y, tc.size, tc.size);
                connRect = connRect.expanded(2);  // tolerance
                if (connRect.contains(relPos))
                {
                    auto* conn = findConnectorByComponentId(m, tc.componentId);
                    if (conn != nullptr)
                    {
                        if (e.mods.isRightButtonDown())
                        {
                            // Fire undo callbacks for each cable before removing
                            if (cableDeletedCallback && undoManager)
                            {
                                undoManager->beginNewTransaction("Delete Cables");
                                for (auto& cable : area.container->getConnections())
                                {
                                    if (cable.output == conn || cable.input == conn)
                                    {
                                        auto findOwner = [&](Connector* c) -> Module* {
                                            for (auto& mp : area.container->getModules())
                                                for (auto& mc : mp->getConnectors())
                                                    if (&mc == c) return mp.get();
                                            return nullptr;
                                        };
                                        auto* outMod = findOwner(cable.output);
                                        auto* inMod = findOwner(cable.input);
                                        if (outMod && inMod)
                                            cableDeletedCallback(area.section,
                                                outMod->getContainerIndex(), cable.output->getDescriptor()->index, cable.output->getDescriptor()->isOutput,
                                                inMod->getContainerIndex(), cable.input->getDescriptor()->index, cable.input->getDescriptor()->isOutput);
                                    }
                                }
                            }
                            area.container->removeConnectionsForConnector(conn);
                            repaint();
                            return;
                        }
                        // Ctrl/Cmd/Alt+drag on a connector that already has a
                        // cable lifts that cable off and carries its far end to
                        // wherever it is dropped, which is how the original
                        // editor re-routes a connection (#67). Three modifiers
                        // because the issue asked for whichever is easiest per
                        // platform and none of them was doing anything here.
                        //
                        // The unplug waits for the first mouse move (see
                        // mouseDrag): a modifier+click that never travels should
                        // leave the patch alone rather than unplug a cable and
                        // plug it straight back in.
                        const bool rerouteMods = e.mods.isCommandDown()
                                              || e.mods.isCtrlDown()
                                              || e.mods.isAltDown();
                        if (rerouteMods && connectorHasCable(*area.container, conn))
                        {
                            dragState.type = DragState::CableReroute;
                            dragState.module = &m;
                            dragState.sourceConnector = conn;
                            dragState.section = area.section;
                            cablePreviewEnd = pos;
                            return;
                        }

                        // Start cable creation
                        dragState.type = DragState::CableCreate;
                        dragState.module = &m;
                        dragState.sourceConnector = conn;
                        dragState.section = area.section;
                        cablePreviewEnd = pos;
                        showCablePreview = true;
                        return;
                    }
                }
            }

            // Test knobs
            for (auto& tk : theme->knobs)
            {
                juce::Rectangle<int> knobRect(tk.x, tk.y, tk.size, tk.size);
                if (knobRect.contains(relPos))
                {
                    auto* param = findParameter(m, tk.componentId);
                    if (param != nullptr)
                    {
                        if (e.mods.isRightButtonDown())
                        {
                            showParameterContextMenu(m, area.section, *param);
                            return;
                        }
                        if (e.mods.isCtrlDown())
                        {
                            // Ctrl+drag: adjust morph range
                            // Auto-assign to group 0 if not yet assigned
                            if (param->getMorphGroup() < 0)
                            {
                                param->setMorphGroup(0);
                                param->setMorphRange(0);
                                if (morphAssignCallback)
                                    morphAssignCallback(area.section, m.getContainerIndex(),
                                                        param->getDescriptor()->index, 0);
                            }
                            dragState.type = DragState::MorphRange;
                            dragState.module = &m;
                            dragState.parameter = param;
                            dragState.section = area.section;
                            dragState.startPos = pos;
                            dragState.startValue = param->getMorphRange();
                            return;
                        }
                        if (e.getNumberOfClicks() >= 2)
                        {
                            // Double-click: reset to default value
                            auto* pd = param->getDescriptor();
                            int oldValue = param->getValue();
                            int defVal = pd->defaultValue;
                            param->setValue(defVal);
                            if (parameterChangeCallback)
                                parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, defVal);
                            if (paramDragCompleteCallback && defVal != oldValue)
                                paramDragCompleteCallback(area.section, m.getContainerIndex(), pd->index, oldValue, defVal);
                            repaint();
                            return;
                        }
                        dragState.type = DragState::Knob;
                        dragState.module = &m;
                        dragState.parameter = param;
                        dragState.section = area.section;
                        dragState.startPos = pos;
                        dragState.startValue = param->getValue();
                        // Let the sweep run past the edge of the screen, and
                        // tell circular mode where the knob's centre is.
                        KnobDrag::begin(e, *this, relPos - knobRect.getCentre());
                        return;
                    }
                }
            }

            // Test sliders
            for (auto& ts : theme->sliders)
            {
                juce::Rectangle<int> sliderRect(ts.x, ts.y, ts.width, ts.height);
                if (sliderRect.contains(relPos))
                {
                    auto* param = findParameter(m, ts.componentId);
                    if (param != nullptr)
                    {
                        if (e.mods.isRightButtonDown())
                        {
                            showParameterContextMenu(m, area.section, *param);
                            return;
                        }
                        if (e.mods.isCtrlDown())
                        {
                            // Ctrl+drag: adjust morph range
                            if (param->getMorphGroup() < 0)
                            {
                                param->setMorphGroup(0);
                                param->setMorphRange(0);
                                if (morphAssignCallback)
                                    morphAssignCallback(area.section, m.getContainerIndex(),
                                                        param->getDescriptor()->index, 0);
                            }
                            dragState.type = DragState::MorphRange;
                            dragState.module = &m;
                            dragState.parameter = param;
                            dragState.section = area.section;
                            dragState.startPos = pos;
                            dragState.startValue = param->getMorphRange();
                            return;
                        }
                        if (e.getNumberOfClicks() >= 2)
                        {
                            // Double-click: reset to default value
                            auto* pd = param->getDescriptor();
                            int oldValue = param->getValue();
                            int defVal = pd->defaultValue;
                            param->setValue(defVal);
                            if (parameterChangeCallback)
                                parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, defVal);
                            if (paramDragCompleteCallback && defVal != oldValue)
                                paramDragCompleteCallback(area.section, m.getContainerIndex(), pd->index, oldValue, defVal);
                            repaint();
                            return;
                        }
                        dragState.type = DragState::Slider;
                        dragState.module = &m;
                        dragState.parameter = param;
                        dragState.section = area.section;
                        dragState.startPos = pos;
                        dragState.startValue = param->getValue();
                        // A slider runs out of desktop the same way a knob does.
                        KnobDrag::begin(e, *this, {});
                        return;
                    }
                }
            }

            // Test buttons
            for (auto& tb : theme->buttons)
            {
                juce::Rectangle<int> btnRect(tb.x, tb.y, tb.width, tb.height);
                if (btnRect.contains(relPos))
                {
                    // isCall buttons: trigger a method action rather than a parameter change
                    if (tb.isCall)
                    {
                        if (tb.callMethod == "rnd")
                        {
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (pd->maxValue - pd->minValue <= 1) continue; // skip binary params (bypass etc)
                                int rndVal = juce::Random::getSystemRandom().nextInt(pd->maxValue - pd->minValue + 1) + pd->minValue;
                                p.setValue(rndVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, rndVal);
                            }
                            repaint();
                        }
                        else if (tb.callMethod == "clear")
                        {
                            // Clears the steps, not the sequencer: it used to reset
                            // every parameter to its minimum, which took the step
                            // count down to 1 and the loop and transport settings
                            // with it. Emptying a pattern should leave the shape of
                            // the sequence alone (issue #34). A cleared step goes to
                            // its default, not its minimum: a CtrlSeq fader's rest
                            // position is centre (64), and zero is the floor, not
                            // "empty" (issue #53).
                            const auto stepIds = sequencerStepIds(*theme);
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (!isSequencerStepParam(*pd, stepIds)) continue;
                                int newVal = pd->defaultValue;
                                if (p.getValue() == newVal) continue;
                                p.setValue(newVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, newVal);
                            }
                            repaint();
                        }
                        else if (tb.callMethod == "randomize")
                        {
                            // Sequencer Rnd: randomize only per-step value controls,
                            // leaving Loop/Step-count/transport/UI custom params untouched.
                            const auto stepIds = sequencerStepIds(*theme);
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (!isSequencerStepParam(*pd, stepIds)) continue;
                                if (pd->maxValue - pd->minValue <= 0) continue;
                                int rndVal = juce::Random::getSystemRandom().nextInt(pd->maxValue - pd->minValue + 1) + pd->minValue;
                                p.setValue(rndVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, rndVal);
                            }
                            repaint();
                        }
                        else if (tb.callMethod == "zoomIn" || tb.callMethod == "zoomOut")
                        {
                            if (auto* zoomParam = findParameter(m, "p1"))
                            {
                                // NoteSeqB's zoom is a class="custom" parameter:
                                // display-only, and index 0 like the module's own
                                // "note 1", so sending it as a parameter change
                                // retuned the first step of the sequence.
                                auto* pd = zoomParam->getDescriptor();
                                int oldVal = zoomParam->getValue();
                                int delta = (tb.callMethod == "zoomIn") ? 1 : -1;
                                int newVal = juce::jlimit(pd->minValue, pd->maxValue, oldVal + delta);
                                if (newVal != oldVal)
                                {
                                    if (customParameterChangeCallback)
                                        customParameterChangeCallback(area.section, m.getContainerIndex(),
                                                                      pd->index, oldVal, newVal);
                                    else
                                        zoomParam->setValue(newVal);
                                }
                                repaint();
                            }
                        }
                        else if (tb.callMethod == "min" || tb.callMethod == "max")
                        {
                            bool doMax = (tb.callMethod == "max");
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (pd->maxValue - pd->minValue <= 1) continue; // skip binary params (bypass etc)
                                int newVal = doMax ? pd->maxValue : pd->minValue;
                                p.setValue(newVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, newVal);
                            }
                            repaint();
                        }
                        else if (tb.callMethod == "shift")
                        {
                            int shiftAmt = tb.callValue;
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (!pd->name.startsWith("band ")) continue;
                                int newVal;
                                if (shiftAmt == 0)
                                    newVal = pd->index + 1; // reset to identity (band[i] = i+1)
                                else
                                    newVal = juce::jlimit(0, pd->maxValue, p.getValue() + shiftAmt);
                                p.setValue(newVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, newVal);
                            }
                            repaint();
                        }
                        else if (tb.callMethod == "invert")
                        {
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (!pd->name.startsWith("band ")) continue;
                                int newVal = (p.getValue() > 0) ? (pd->maxValue + 1 - p.getValue()) : 0;
                                p.setValue(newVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, newVal);
                            }
                            repaint();
                        }
                        return;
                    }

                    auto* param = findParameter(m, tb.componentId);
                    if (param != nullptr)
                    {
                        if (e.mods.isRightButtonDown())
                        {
                            showParameterContextMenu(m, area.section, *param);
                            return;
                        }
                        if (e.getNumberOfClicks() >= 2 && !tb.isIncrement)
                        {
                            // Double-click: reset to default value (not for increment buttons)
                            auto* pd = param->getDescriptor();
                            int oldValue = param->getValue();
                            int defVal = pd->defaultValue;
                            param->setValue(defVal);
                            if (parameterChangeCallback)
                                parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, defVal);
                            if (paramDragCompleteCallback && defVal != oldValue)
                                paramDragCompleteCallback(area.section, m.getContainerIndex(), pd->index, oldValue, defVal);
                            repaint();
                            return;
                        }
                        dragState.type = DragState::Button;
                        dragState.module = &m;
                        dragState.parameter = param;
                        dragState.section = area.section;
                        dragState.startPos = pos;
                        dragState.startValue = param->getValue();

                        // Buttons toggle/cycle on click (no drag)
                        auto* pd = param->getDescriptor();
                        int newValue;
                        if (tb.isIncrement)
                        {
                            // A landscape pair is drawn as left and right arrows,
                            // so it has to be split on x. Splitting it on y like a
                            // stacked pair made both arrows step the same way,
                            // decided only by which half of the arrow was hit
                            // (issue #34). The four sequencers are the only
                            // modules with landscape arrows.
                            const bool goUp = tb.landscape
                                ? (relPos.x >= tb.x + tb.width / 2)
                                : (relPos.y <  tb.y + tb.height / 2);
                            newValue = juce::jlimit(pd->minValue, pd->maxValue,
                                                    param->getValue() + (goUp ? 1 : -1));
                        }
                        else
                        {
                            // Radio-selector: if the button has multiple discrete
                            // labels (cyclic=false), jump straight to the segment
                            // the user actually clicked rather than cycling through.
                            int numOptions = static_cast<int>(tb.labels.size());
                            if (!tb.cyclic && numOptions > 1)
                            {
                                int seg;
                                if (tb.landscape)
                                {
                                    int local = relPos.x - tb.x;
                                    seg = juce::jlimit(0, numOptions - 1,
                                                       local * numOptions / juce::jmax(1, tb.width));
                                }
                                else
                                {
                                    int local = relPos.y - tb.y;
                                    int renderIdx = juce::jlimit(0, numOptions - 1,
                                                                 local * numOptions / juce::jmax(1, tb.height));
                                    seg = tb.reversed ? (numOptions - 1 - renderIdx) : renderIdx;
                                }
                                newValue = juce::jlimit(pd->minValue, pd->maxValue, pd->minValue + seg);
                            }
                            else
                            {
                                newValue = param->getValue() + 1;
                                if (newValue > pd->maxValue)
                                    newValue = pd->minValue;
                            }
                        }

                        int oldButtonValue = dragState.parameter->getValue();
                        dragState.parameter->setValue(newValue);

                        // Button clicks complete immediately — fire drag complete
                        // for undo. The undoable action sends the value itself, so
                        // going through parameterChangeCallback as well put two
                        // identical messages on the wire for every press (issue #37).
                        if (paramDragCompleteCallback)
                            paramDragCompleteCallback(dragState.section, m.getContainerIndex(), pd->index, oldButtonValue, newValue);
                        else if (parameterChangeCallback)
                            parameterChangeCallback(dragState.section, m.getContainerIndex(), pd->index, newValue);

                        repaint();
                        return;
                    }
                }
            }

            // Test NoteSeqB piano-roll editor and its scrollbar.
            for (auto& cd : theme->customDisplays)
            {
                if (cd.type != "note-seq-editor" && cd.type != "scrollbar")
                    continue;

                juce::Rectangle<int> displayRect(cd.x, cd.y, cd.width, cd.height);
                if (!displayRect.contains(relPos))
                    continue;

                if (cd.type == "note-seq-editor")
                {
                    constexpr int kSteps = 16;
                    constexpr int kKeyWidth = 16;
                    int rollX = cd.x + kKeyWidth;
                    int rollW = juce::jmax(1, cd.width - kKeyWidth);
                    int localX = juce::jlimit(0, rollW - 1, relPos.x - rollX);
                    int step = juce::jlimit(0, kSteps - 1, localX * kSteps / rollW);
                    if (relPos.x < rollX)
                        step = 0;

                    if (cd.noteStepIds[step].isEmpty())
                        return;

                    auto* noteParam = findParameter(m, cd.noteStepIds[step]);
                    if (noteParam == nullptr)
                        return;

                    auto noteFromY = [&](int y)
                    {
                        int zoom = 3;
                        if (auto* p = findParameter(m, "p1"))
                            zoom = juce::jlimit(1, 6, p->getValue());

                        int centerNote = 60;
                        if (auto* p = findParameter(m, "p2"))
                        {
                            auto* pd = p->getDescriptor();
                            int v = p->getValue();
                            if (v >= pd->minValue && v <= pd->maxValue)
                                centerNote = v;
                        }
                        else if (auto* p = findParameter(m, cd.noteStepIds[step]))
                            centerNote = p->getValue();

                        int visibleNotes = juce::jlimit(12, 72, 72 - (zoom - 1) * 12);
                        int lowNote = juce::jlimit(0, 127 - visibleNotes, centerNote - visibleNotes / 2);
                        int highNote = lowNote + visibleNotes;
                        float normY = juce::jlimit(0.0f, 1.0f, static_cast<float>(y - cd.y) / static_cast<float>(cd.height));
                        return juce::jlimit(0, 127, static_cast<int>(std::round(static_cast<float>(highNote) - normY * static_cast<float>(visibleNotes))));
                    };

                    int oldValue = noteParam->getValue();
                    int newValue = noteFromY(relPos.y);
                    noteParam->setValue(newValue);
                    if (parameterChangeCallback)
                        parameterChangeCallback(area.section, m.getContainerIndex(), noteParam->getDescriptor()->index, newValue);

                    if (auto* stepParam = findParameter(m, "p20"))
                    {
                        int stepValue = step + 1;
                        if (stepParam->getValue() != stepValue)
                        {
                            stepParam->setValue(stepValue);
                            if (parameterChangeCallback)
                                parameterChangeCallback(area.section, m.getContainerIndex(), stepParam->getDescriptor()->index, stepValue);
                        }
                    }

                    dragState.type = DragState::NoteSeqEditor;
                    dragState.module = &m;
                    dragState.parameter = noteParam;
                    dragState.section = area.section;
                    dragState.startPos = pos;
                    dragState.startValue = oldValue;
                    dragState.customRect = displayRect;
                    for (int i = 0; i < kSteps; ++i)
                        dragState.customIds[i] = cd.noteStepIds[i];
                    repaint();
                    return;
                }

                if (cd.type == "scrollbar")
                {
                    auto* scrollParam = findParameter(m, "p2");
                    if (scrollParam == nullptr)
                        return;

                    auto* pd = scrollParam->getDescriptor();
                    float normY = juce::jlimit(0.0f, 1.0f, static_cast<float>(relPos.y - cd.y) / static_cast<float>(cd.height));
                    int newValue = juce::jlimit(pd->minValue, pd->maxValue,
                        static_cast<int>(std::round(static_cast<float>(pd->maxValue)
                            - normY * static_cast<float>(pd->maxValue - pd->minValue))));

                    int oldValue = scrollParam->getValue();
                    scrollParam->setValue(newValue);
                    if (parameterChangeCallback)
                        parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, newValue);

                    dragState.type = DragState::NoteSeqScrollbar;
                    dragState.module = &m;
                    dragState.parameter = scrollParam;
                    dragState.section = area.section;
                    dragState.startPos = pos;
                    dragState.startValue = oldValue;
                    dragState.customRect = displayRect;
                    repaint();
                    return;
                }
            }

            // Test DrumSynth preset spinner arrows
            if (m.getDescriptor()->index == 58)
            {
                int numP = static_cast<int>(drumPresets().size());
                // Preset display: right-click to save/manage
                juce::Rectangle<int> dispRect(120, 115, 57, 13);
                if (dispRect.contains(relPos) && e.mods.isRightButtonDown())
                {
                    showDrumPresetContextMenu(m, area.section);
                    return;
                }

                // Up/Down arrows
                juce::Rectangle<int> upRect(179, 115, 16, 6);
                juce::Rectangle<int> dnRect(179, 121, 16, 6);
                if (upRect.contains(relPos) || dnRect.contains(relPos))
                {
                    int key = m.getContainerIndex();
                    // "none" is -1, so either arrow steps onto the first preset.
                    int cur = resolvedDrumPreset(m);

                    if (upRect.contains(relPos))
                        cur = juce::jlimit(0, numP - 1, cur - 1);
                    else
                        cur = juce::jlimit(0, numP - 1, cur + 1);

                    drumPresetState[key] = cur;
                    applyDrumPreset(m, area.section, cur);
                    return;
                }
            }

            // Test partial arrow buttons on textDisplays
            for (auto& td : theme->textDisplays)
            {
                if (!td.partialArrows) continue;

                // Arrow row geometry (mirrors paintTextDisplays)
                float dh      = static_cast<float>(td.height);
                float renderH = juce::jmin(dh, 13.0f);
                float renderY = td.y + (dh - renderH) * 0.5f;
                float arrowY  = renderY + renderH + 1.0f;
                float arrowH  = 8.0f;

                juce::Rectangle<float> arrowRect(static_cast<float>(td.x), arrowY,
                                                 static_cast<float>(td.width), arrowH);
                if (!arrowRect.contains(relPos.toFloat())) continue;

                auto* param     = findParameter(m, td.componentId);   // p2
                auto* fineParam = findParameter(m, "p3");              // fine detune
                if (param == nullptr) continue;

                static const int kSnaps[]  = {4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124};
                static const int kNumSnaps = 11;
                int val    = param->getValue();
                int newVal = val;
                bool goUp  = (relPos.x > td.x + td.width / 2);

                if (goUp)
                {
                    for (int i = 0; i < kNumSnaps; ++i)
                        if (kSnaps[i] > val) { newVal = kSnaps[i]; break; }
                }
                else
                {
                    for (int i = kNumSnaps - 1; i >= 0; --i)
                        if (kSnaps[i] < val) { newVal = kSnaps[i]; break; }
                }

                if (newVal != val)
                {
                    int oldVal = val;
                    param->setValue(newVal);
                    if (parameterChangeCallback)
                        parameterChangeCallback(area.section, m.getContainerIndex(),
                                                param->getDescriptor()->index, newVal);
                    if (paramDragCompleteCallback)
                        paramDragCompleteCallback(area.section, m.getContainerIndex(),
                                                  param->getDescriptor()->index, oldVal, newVal);

                    // Reset fine (p3) to center/zero (value 64)
                    if (fineParam != nullptr && fineParam->getValue() != 64)
                    {
                        int oldFine = fineParam->getValue();
                        fineParam->setValue(64);
                        if (parameterChangeCallback)
                            parameterChangeCallback(area.section, m.getContainerIndex(),
                                                    fineParam->getDescriptor()->index, 64);
                        if (paramDragCompleteCallback)
                            paramDragCompleteCallback(area.section, m.getContainerIndex(),
                                                      fineParam->getDescriptor()->index, oldFine, 64);
                    }

                    repaint();
                    return;
                }
                return; // already at limit — consume click anyway
            }

            // Clicking a frequency display rotates the units it reads in, the
            // way the original does (issue #30). The setting is display-only:
            // it is stored in the patch and never sent to the synth, so it goes
            // through its own undoable action rather than a parameter change.
            for (auto& td : theme->textDisplays)
            {
                // Left button only: a right-click on a module belongs to its
                // context menu wherever it lands.
                if (!e.mods.isLeftButtonDown())
                    break;

                float dh      = static_cast<float>(td.height);
                float renderH = juce::jmin(dh, 13.0f);
                float renderY = td.y + (dh - renderH) * 0.5f;
                juce::Rectangle<float> boxRect(static_cast<float>(td.x), renderY,
                                               static_cast<float>(td.width), renderH);
                if (!boxRect.contains(relPos.toFloat()))
                    continue;

                auto* unitsParam = freqUnitsParamFor(m, td.componentId);
                if (unitsParam == nullptr || unitsParam->getDescriptor() == nullptr)
                    continue;

                const juce::String baseFormatter = td.formatterOverride.isNotEmpty()
                    ? td.formatterOverride
                    : (findParameter(m, td.componentId) != nullptr
                           ? findParameter(m, td.componentId)->getDescriptor()->formatter
                           : juce::String());
                const auto units = freqUnitsFor(m, td.componentId, baseFormatter);
                if (units.isEmpty())
                    continue;

                const int oldUnit = juce::jlimit(0, units.size() - 1, unitsParam->getValue());
                const int newUnit = (oldUnit + 1) % units.size();

                if (customParameterChangeCallback)
                    customParameterChangeCallback(area.section, m.getContainerIndex(),
                                                  unitsParam->getDescriptor()->index,
                                                  oldUnit, newUnit);
                else
                    unitsParam->setValue(newUnit);

                repaint();
                return;
            }

            // Module body fallback
            if (e.mods.isRightButtonDown())
            {
                // If right-clicking on a selected module and there are multiple selected → selection menu
                if (isSelected(&m) && selection.size() > 1)
                {
                    showSelectionContextMenu();
                    return;
                }

                // Single-module context menu
                Module* modPtr = &m;
                int sec = area.section;

                juce::PopupMenu menu;
                menu.addItem(1, "Rename Module...");
                menu.addSeparator();
                menu.addItem(2, "Duplicate");
                menu.addItem(3, "Duplicate with Cables");
                menu.addItem(4, "Copy");
                menu.addSeparator();
                menu.addItem(6, "Initialize Module");
                menu.addItem(7, "Exclude from Mutation", true, modPtr->isExcludedFromMutation());
                menu.addSeparator();
                menu.addItem(5, "Delete Module");

                // KeyQuantizer (m98): scale presets. Module exposes 12 binary
                // note toggles, but the param order is offset — params p3..p14
                // map to E,F,F#,G,G#,A,Bb,B,C,C#,D,D#. The lambda below maps
                // chromatic semitone (C=0..B=11) to the matching component-id.
                // Result IDs >= 100 are reserved for scale presets.
                const bool isKeyQuant = (modPtr->getDescriptor()->index == 98);
                if (isKeyQuant)
                {
                    juce::PopupMenu scales;
                    scales.addItem(100, "Chromatic");
                    scales.addSeparator();
                    scales.addItem(101, "Major (Ionian)");
                    scales.addItem(102, "Natural Minor (Aeolian)");
                    scales.addItem(103, "Harmonic Minor");
                    scales.addItem(104, "Melodic Minor");
                    scales.addSeparator();
                    scales.addItem(105, "Dorian");
                    scales.addItem(106, "Phrygian");
                    scales.addItem(107, "Lydian");
                    scales.addItem(108, "Mixolydian");
                    scales.addItem(109, "Locrian");
                    scales.addSeparator();
                    scales.addItem(110, "Pentatonic Major");
                    scales.addItem(111, "Pentatonic Minor");
                    scales.addItem(112, "Blues Major");
                    scales.addItem(113, "Blues Minor");
                    scales.addSeparator();
                    scales.addItem(114, "Whole Tone");
                    scales.addItem(115, "Diminished (W-H)");
                    menu.addSeparator();
                    menu.addSubMenu("Scales (root C)", scales);
                }

                // DrumSynth (m58): the preset library. It used to be reachable
                // only by right-clicking the small preset display box, and even
                // there it could delete but not recall. IDs >= 200 are its own.
                const bool isDrumSynth = (modPtr->getDescriptor()->index == 58);
                auto drumAction = std::make_shared<int>(0);
                if (isDrumSynth)
                {
                    menu.addSeparator();
                    menu.addSubMenu("Preset", buildDrumPresetMenu(*modPtr, drumAction));
                }

                menu.showMenuAsync(juce::PopupMenu::Options{},
                    [this, modPtr, sec, drumAction](int result)
                    {
                        if (result == 1)
                        {
                            // Rename: show text input dialog
                            auto* dialog = new juce::AlertWindow(
                                "Rename Module",
                                "Enter new name for \"" + modPtr->getTitle() + "\":",
                                juce::MessageBoxIconType::NoIcon);
                            dialog->addTextEditor("name", modPtr->getTitle(), "Module name:");
                            dialog->getTextEditor("name")->setInputRestrictions(16);
                            dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                            dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                            dialog->enterModalState(true, juce::ModalCallbackFunction::create(
                                [this, dialog, modPtr, sec](int r) {
                                    if (r == 1)
                                    {
                                        juce::String newName = dialog->getTextEditorContents("name").trim();
                                        juce::String oldName = modPtr->getTitle();
                                        if (newName.isNotEmpty() && newName != oldName)
                                        {
                                            // The undoable action applies setTitle; if no
                                            // callback is wired, fall back to a direct set.
                                            if (renameModuleCallback)
                                                renameModuleCallback(sec, modPtr, oldName, newName);
                                            else
                                                modPtr->setTitle(newName);
                                            repaint();
                                        }
                                    }
                                    delete dialog;
                                }), true);
                        }
                        else if (result == 2)
                        {
                            // Duplicate single module (no cables)
                            selectModule(modPtr, sec);
                            duplicateSelection(false);
                        }
                        else if (result == 3)
                        {
                            // Duplicate with cables
                            selectModule(modPtr, sec);
                            duplicateSelection(true);
                        }
                        else if (result == 4)
                        {
                            // Copy
                            selectModule(modPtr, sec);
                            copySelectionToClipboard();
                        }
                        else if (result == 5)
                        {
                            if (undoManager)
                                undoManager->beginNewTransaction("Delete Module");
                            // Deselect first: the delete repaints the inspector
                            // mid-flight, and it must not still be pointing at
                            // the module about to be freed (issue #61).
                            if (isSelected(modPtr))
                                clearSelection();
                            if (deleteModuleCallback)
                                deleteModuleCallback(sec, modPtr);
                            repaint();
                        }
                        else if (result == 6)
                        {
                            if (initModuleCallback)
                                initModuleCallback(sec, modPtr);
                        }
                        else if (result == 7)
                        {
                            modPtr->setExcludedFromMutation(!modPtr->isExcludedFromMutation());
                            repaint();
                        }
                        else if (result >= 100 && result < 200)
                        {
                            // KeyQuantizer scale presets. mask is a 12-bit
                            // chromatic bitmap (bit0=C..bit11=B); each bit set
                            // means "note enabled in scale".
                            // Bit n set = semitone n enabled (n=0..11, 0=C, 11=B).
                            // Masks mirror the VCV Fundamental Quantizer presets
                            // (https://github.com/VCVRack/Fundamental/tree/v2/presets/Quantizer).
                            int mask = 0;
                            switch (result)
                            {
                                case 100: mask = 0xFFF; break; // Chromatic
                                case 101: mask = 0xAB5; break; // Major (0,2,4,5,7,9,11)
                                case 102: mask = 0x5AD; break; // Natural Minor (0,2,3,5,7,8,10)
                                case 103: mask = 0x9AD; break; // Harmonic Minor (0,2,3,5,7,8,11)
                                case 104: mask = 0xAAD; break; // Melodic Minor asc (0,2,3,5,7,9,11)
                                case 105: mask = 0x6AD; break; // Dorian (0,2,3,5,7,9,10)
                                case 106: mask = 0x5AB; break; // Phrygian (0,1,3,5,7,8,10)
                                case 107: mask = 0xAD5; break; // Lydian (0,2,4,6,7,9,11)
                                case 108: mask = 0x6B5; break; // Mixolydian (0,2,4,5,7,9,10)
                                case 109: mask = 0x56B; break; // Locrian (0,1,3,5,6,8,10)
                                case 110: mask = 0x295; break; // Pentatonic Major (0,2,4,7,9)
                                case 111: mask = 0x4A9; break; // Pentatonic Minor (0,3,5,7,10)
                                case 112: mask = 0x29D; break; // Blues Major (0,2,3,4,7,9)
                                case 113: mask = 0x4E9; break; // Blues Minor (0,3,5,6,7,10)
                                case 114: mask = 0x555; break; // Whole Tone (0,2,4,6,8,10)
                                case 115: mask = 0xB6D; break; // Diminished W-H (0,2,3,5,6,8,9,11)
                                default:  return;
                            }
                            // Chromatic semitone → KeyQuant component-id
                            //   C(0)→p11, C#(1)→p12, D(2)→p13, D#(3)→p14,
                            //   E(4)→p3,  F(5)→p4,   F#(6)→p5, G(7)→p6,
                            //   G#(8)→p7, A(9)→p8,   Bb(10)→p9, B(11)→p10
                            static const char* const SEMI_TO_PID[12] = {
                                "p11","p12","p13","p14","p3","p4","p5","p6","p7","p8","p9","p10"
                            };
                            for (int s = 0; s < 12; ++s)
                            {
                                auto* p = findParameter(*modPtr, SEMI_TO_PID[s]);
                                if (p == nullptr) continue;
                                int newVal = (mask >> s) & 1;
                                if (p->getValue() == newVal) continue;
                                int oldVal = p->getValue();
                                p->setValue(newVal);
                                auto* pd = p->getDescriptor();
                                if (parameterChangeCallback)
                                    parameterChangeCallback(sec, modPtr->getContainerIndex(), pd->index, newVal);
                                if (paramDragCompleteCallback)
                                    paramDragCompleteCallback(sec, modPtr->getContainerIndex(), pd->index, oldVal, newVal);
                            }
                            repaint();
                        }
                        else if (result >= drumPresetSaveId)
                        {
                            handleDrumPresetMenuResult(result, *drumAction, *modPtr, sec);
                        }
                    });
                return;
            }

            // Left click → select module
            bool alreadySelected = isSelected(&m);
            bool addToSel = e.mods.isShiftDown();

            if (!alreadySelected || addToSel)
                selectModule(&m, area.section, addToSel);
            // If already selected and no shift → keep selection, just start move

            // Multi-move if more than one thing is selected, text notes included
            if (selection.size() + selectedCommentIds.size() > 1)
            {
                beginMultiMove(pos);
            }
            else
            {
                dragState.type = DragState::ModuleMove;
                dragState.module = &m;
                dragState.section = area.section;
                dragState.startPos = pos;
                dragState.startGridPos = m.getPosition();
                dragState.dragOffsetX = pos.x - rect.getX();
                dragState.dragOffsetY = pos.y - rect.getY();
            }
            repaint();
            return;
        }
    }

    // Clicked on empty area → start rubber band, clear selection
    if (!e.mods.isRightButtonDown())
    {
        clearSelection();
        dragState.type = DragState::RubberBand;
        dragState.startPos = pos;
        rubberBandRect = juce::Rectangle<int>(pos, pos);
        showRubberBand = true;
        repaint();
    }
    else
    {
        // Right-click on empty canvas → Add Module menu (by category) + Paste
        if (patch == nullptr || moduleDescs == nullptr) return;

        // Determine section + grid position for the new module
        // Each canvas is section-specific; yOffset is always 0.
        int clickSection = mySection;
        int clickGX = juce::jlimit(0, 39, pos.x / gridX);
        int clickGY = juce::jlimit(0, 127, pos.y / gridY);

        juce::PopupMenu menu;

        // "Add Module" submenu organised like the original NM menu, with
        // explicit separators. Morph is internal and never appears here.
        // IDs: 1000 + moduleIndex  (leaves plenty of room for other items)
        juce::PopupMenu addMenu;

        auto addModuleItem = [this](juce::PopupMenu& target, const char* moduleName, const char* label)
        {
            if (moduleDescs == nullptr)
                return;

            if (auto* desc = moduleDescs->getModuleByName(moduleName))
                if (desc->instantiable)
                {
                    // The original editor prints each module's DSP share right
                    // in this menu, which is how you pick a cheaper alternative
                    // before placing it (issue #31).
                    juce::String text = label != nullptr ? juce::String(label) : desc->fullname;
                    target.addItem(1000 + desc->index,
                                   text + " (" + formatDspCost(desc->cycles) + ")");
                }
        };

        auto addSubMenuIfNotEmpty = [&addMenu](const juce::String& title, juce::PopupMenu& subMenu)
        {
            if (subMenu.getNumItems() > 0)
                addMenu.addSubMenu(title, subMenu);
        };

        {
            juce::PopupMenu sub;
            addModuleItem(sub, "Keyboard", "Keyboard - voice");
            addModuleItem(sub, "KeyboardPatch", "Keyboard - Patch");
            addModuleItem(sub, "MIDIGlobal", "MIDI - global");
            sub.addSeparator();
            addModuleItem(sub, "AudioIn", "Audio In");
            addModuleItem(sub, "PolyAreaIn", "Poly Area In");
            sub.addSeparator();
            addModuleItem(sub, "1Output", "1 output");
            addModuleItem(sub, "2Output", "2 outputs");
            addModuleItem(sub, "4Output", "4 outputs");
            sub.addSeparator();
            addModuleItem(sub, "NoteDetect", "Note detector");
            addModuleItem(sub, "KeybSplit", "Keyboard Split");
            addSubMenuIfNotEmpty("IN/OUT", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "MasterOsc", "Master Oscillator");
            addModuleItem(sub, "OscA", "OSC A");
            addModuleItem(sub, "OscB", "OSC B");
            addModuleItem(sub, "OscC", "OSC C");
            addModuleItem(sub, "SpectralOsc", "Spectral Osc");
            addModuleItem(sub, "FormantOsc", "Formant Osc");
            sub.addSeparator();
            addModuleItem(sub, "OscSlvA", "OSC Slave A");
            addModuleItem(sub, "OscSlvB", "OSC Slave B");
            addModuleItem(sub, "OscSlvC", "OSC Slave C");
            addModuleItem(sub, "OscSlvD", "OSC Slave D");
            addModuleItem(sub, "OscSlvE", "OSC Slave E");
            addModuleItem(sub, "OscSineBank", "Osc Sine Bank");
            addModuleItem(sub, "OscSlvFM", "Osc Slave FM");
            sub.addSeparator();
            addModuleItem(sub, "Noise", "Noise generator");
            sub.addSeparator();
            addModuleItem(sub, "PercOsc", "Percussion OSC");
            addModuleItem(sub, "DrumSynth", "Drumsound synthesizer");
            addSubMenuIfNotEmpty("OSC", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "LFOA", "LFO A");
            addModuleItem(sub, "LFOB", "LFO B");
            addModuleItem(sub, "LFOC", "LFO C");
            sub.addSeparator();
            addModuleItem(sub, "LFOSlvA", "LFO Slave A");
            addModuleItem(sub, "LFOSlvB", "LFO Slave B");
            addModuleItem(sub, "LFOSlvC", "LFO Slave C");
            addModuleItem(sub, "LFOSlvD", "LFO Slave D");
            addModuleItem(sub, "LFOSlvE", "LFO Slave E");
            sub.addSeparator();
            addModuleItem(sub, "ClkGen", "Clock generator");
            sub.addSeparator();
            addModuleItem(sub, "ClkRndGen", "Clocked random step generator");
            addModuleItem(sub, "RndStepGen", "Random step generator");
            addModuleItem(sub, "RandomGen", "Random generator");
            addModuleItem(sub, "RndPulsGen", "Random puls generator");
            addModuleItem(sub, "PatternGen", "Clocked pattern generator");
            addSubMenuIfNotEmpty("LFO", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "ADSR", "ADSR envelope");
            addModuleItem(sub, "AD-Env", "Attack decay envelope");
            addModuleItem(sub, "Mod-Env", "ADSR env. with modulation");
            addModuleItem(sub, "AHD", "AHD env. with modulation");
            addModuleItem(sub, "Multi-Env", "Multistage Envelope");
            sub.addSeparator();
            addModuleItem(sub, "EnvFollower", "Envelope Follower");
            addSubMenuIfNotEmpty("ENV", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "FilterA", "Filter A (6dB LP)");
            addModuleItem(sub, "FilterB", "Filter B (6dB HP)");
            addModuleItem(sub, "FilterC", "Filter C (12dB Multimode)");
            sub.addSeparator();
            addModuleItem(sub, "FilterD", "Filter D (12dB Multimode)");
            addModuleItem(sub, "FilterE", "Filter E (24dB)");
            addModuleItem(sub, "FilterF", "Filter F (24dB classic LP)");
            sub.addSeparator();
            addModuleItem(sub, "VocalFilter", "Vocal filter");
            addModuleItem(sub, "Vocoder", "Vocoder");
            addModuleItem(sub, "FilterBank", "FilterBank");
            sub.addSeparator();
            addModuleItem(sub, "EqMid", "Parametric Eq");
            addModuleItem(sub, "EqShelving", "Hi and lo shelving eq");
            addSubMenuIfNotEmpty("FILTER", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "Mixer (3)", "3 inputs mixer");
            addModuleItem(sub, "Mixer (8)", "8 inputs mixer");
            sub.addSeparator();
            addModuleItem(sub, "GainControl", "Gain controller (multiply)");
            sub.addSeparator();
            addModuleItem(sub, "X-Fade", "X-fade with modulator");
            addModuleItem(sub, "Pan", "Pan");
            sub.addSeparator();
            addModuleItem(sub, "1to2Fade", "1 in to 2 out fader");
            addModuleItem(sub, "2to1Fade", "2 in to 1 out fader");
            addModuleItem(sub, "LevMult", "Adjustable gain control");
            addModuleItem(sub, "LevAdd", "Adjustable offset");
            sub.addSeparator();
            addModuleItem(sub, "OnOff", "On/off switch");
            sub.addSeparator();
            addModuleItem(sub, "4-1Switch", "4-1 Switch");
            addModuleItem(sub, "1-4Switch", "1-4 Switch");
            sub.addSeparator();
            addModuleItem(sub, "Amplifier", "Amplifier");
            addSubMenuIfNotEmpty("MIXER", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "Clip", "Clip");
            addModuleItem(sub, "Overdrive", "Overdrive");
            addModuleItem(sub, "WaveWrap", "Wave Wrapper");
            sub.addSeparator();
            addModuleItem(sub, "Quantizer", "Quantizer");
            addModuleItem(sub, "Delay", "Delay line");
            addModuleItem(sub, "Sample&Hold", "Sample and hold");
            addModuleItem(sub, "Diode", "Diode processing");
            addModuleItem(sub, "StereoChorus", "Stereo chorus");
            addModuleItem(sub, "Phaser", "Phaser");
            sub.addSeparator();
            addModuleItem(sub, "InvLevShift", "Level shifter / Inverter");
            sub.addSeparator();
            addModuleItem(sub, "Shaper", "Signal shaper");
            sub.addSeparator();
            addModuleItem(sub, "Compressor", "Compressor");
            addModuleItem(sub, "Expander", "Expander");
            addModuleItem(sub, "RingMod", "Ring and amplitude modulator");
            addModuleItem(sub, "Digitizer", "Digitizer");
            addSubMenuIfNotEmpty("AUDIO", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "Constant", "Constant");
            sub.addSeparator();
            addModuleItem(sub, "Smooth", "Smooth");
            addModuleItem(sub, "PortamentoA", "PortamentoA");
            addModuleItem(sub, "PortamentoB", "PortamentoB");
            sub.addSeparator();
            addModuleItem(sub, "NoteScaler", "Note scaler");
            addModuleItem(sub, "NoteQuant", "Note quantizer");
            addModuleItem(sub, "KeyQuant", "Key quantizer");
            addModuleItem(sub, "PartialGen", "Partial generator");
            sub.addSeparator();
            addModuleItem(sub, "ControlMixer", "Control signal mixer");
            addModuleItem(sub, "NoteVelScal", "Note and Vel Scaler");
            addSubMenuIfNotEmpty("CTRL", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "PosEdgeDelay", "Positive edge delay");
            addModuleItem(sub, "NegEdgeDelay", "Negative edge delay");
            addModuleItem(sub, "Pulse", "Pulse");
            addModuleItem(sub, "LogicDelay", "Logic delay");
            sub.addSeparator();
            addModuleItem(sub, "LogicInv", "Logic inverter");
            sub.addSeparator();
            addModuleItem(sub, "LogicProc", "Logic processor");
            sub.addSeparator();
            addModuleItem(sub, "CompareLev", "Compare to level");
            addModuleItem(sub, "CompareAB", "Compare");
            sub.addSeparator();
            addModuleItem(sub, "ClkDiv", "Clock divider");
            addModuleItem(sub, "ClkDivFix", "Clock divider, fixed");
            addSubMenuIfNotEmpty("LOGIC", sub);
        }
        {
            juce::PopupMenu sub;
            addModuleItem(sub, "NoteSeqA", "Note Sequencer A");
            addModuleItem(sub, "EventSeq", "Event Sequencer");
            addModuleItem(sub, "NoteSeqB", "Note Sequencer B");
            addModuleItem(sub, "CtrlSeq", "Control Sequencer");
            addSubMenuIfNotEmpty("SEQUENCER", sub);
        }

        menu.addSubMenu("Add Module", addMenu);

        if (!clipboard.empty())
        {
            menu.addSeparator();
            menu.addItem(1, "Paste");
        }

        menu.addSeparator();
        menu.addItem(4, "Add Comment");

        menu.addSeparator();
        menu.addItem(2, "Shake Cables");
        menu.addItem(3, "Reset Cables");

        menu.showMenuAsync(juce::PopupMenu::Options{},
            [this, clickSection, clickGX, clickGY, pos](int result)
            {
                juce::ignoreUnused(clickSection, clickGX, clickGY, pos);
                if (result == 1)
                {
                    beginPasteGhost();
                }
                else if (result == 4)
                {
                    if (commentAddCallback)
                        commentAddCallback(clickSection, clickGX, clickGY);
                    repaint();
                }
                else if (result == 2)
                {
                    shakeCables();
                }
                else if (result == 3)
                {
                    cableSagOffsets.clear();
                    repaint();
                }
                else if (result >= 1000)
                {
                    int typeIndex = result - 1000;
                    if (auto* desc = moduleDescs->getModuleByIndex(typeIndex))
                        beginAddModuleGhost(typeIndex, desc->name);
                }
            });
    }
}

void PatchCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (dragState.type == DragState::None)
        return;

    auto currentPos = screenToCanvas(e.getPosition());

    if (dragState.type == DragState::CanvasPan)
    {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            auto screenPos = e.getScreenPosition();  // absolute screen coords — stable
            int dx = screenPos.x - dragState.startPos.x;
            int dy = screenPos.y - dragState.startPos.y;
            auto vpos = vp->getViewPosition();
            vp->setViewPosition(vpos.x - dx, vpos.y - dy);
            dragState.startPos = screenPos;
        }
        return;
    }

    if (dragState.type == DragState::RubberBand)
    {
        rubberBandRect = juce::Rectangle<int>(dragState.startPos, currentPos);
        updateRubberBandSelection(rubberBandRect.toNearestInt());
        repaint();
        return;
    }

    if (dragState.type == DragState::MorphRange)
    {
        if (dragState.parameter == nullptr || dragState.module == nullptr) return;
        // Dragging up increases range, down decreases (same sensitivity as knob)
        int dy = dragState.startPos.y - currentPos.y;  // up = positive
        int newRange = juce::jlimit(-127, 127, dragState.startValue + dy);
        dragState.parameter->setMorphRange(newRange);

        // Rate-limited send to synth
        auto now = juce::Time::currentTimeMillis();
        if (now - dragState.lastSendTime >= paramSendIntervalMs)
        {
            dragState.lastSendTime = now;
            if (morphRangeChangeCallback)
            {
                int span = std::abs(newRange);
                int direction = (newRange >= 0) ? 0 : 1;
                morphRangeChangeCallback(dragState.section,
                                         dragState.module->getContainerIndex(),
                                         dragState.parameter->getDescriptor()->index,
                                         span, direction);
            }
        }
        repaint();
        return;
    }

    if (dragState.type == DragState::MultiModuleMove)
    {
        int dx = (currentPos.x - dragState.startPos.x + gridX / 2) / gridX;
        int dy = (currentPos.y - dragState.startPos.y + gridY / 2) / gridY;

        for (auto& ms : multiMoveState)
        {
            auto* module = resolve(ms.ref);
            if (module == nullptr)
                continue;   // deleted mid-drag; the rest of the block still moves

            // Both axes bounded: a drag that keeps going must stop at the
            // edges rather than carry the module off the canvas, where it
            // still exists but cannot be seen or grabbed again.
            const int h = module->getDescriptor()->height;
            int newX = juce::jlimit(0, 39, ms.startGridPos.x + dx);
            auto& container = patch->getContainer(ms.ref.section);
            int rawY = juce::jlimit(0, modulePlacementRows - h, ms.startGridPos.y + dy);
            int newY = findNearestFreeY(container, module, newX, rawY, h);
            module->setPosition({ newX, newY });
        }

        // Notes travel with them. The block keeps its shape rather than each
        // note hunting for a free row of its own: inside a moving selection the
        // members are as much in each other's way as anything else is, and the
        // modules above are moved the same way.
        for (auto& cs : commentMoveState)
        {
            if (auto* c = patch->getCommentById(cs.id))
            {
                c->x = juce::jlimit(0, 40 - c->gridWidth(), cs.startGridPos.x + dx);
                c->y = juce::jlimit(0, 128 - c->gridHeight(), cs.startGridPos.y + dy);
            }
        }
        repaint();
        return;
    }

    if (dragState.type == DragState::CommentMove)
    {
        if (patch != nullptr)
        {
            if (auto* c = patch->getCommentById(dragCommentId))
            {
                const int newX = juce::jlimit(0, 40 - c->gridWidth(),
                                              (currentPos.x - dragCommentOffsetX + gridX / 2) / gridX);
                const int rawY = juce::jlimit(0, 128 - c->gridHeight(),
                                              (currentPos.y - dragCommentOffsetY + gridY / 2) / gridY);

                // Same rule a module follows: it cannot be dropped on top of
                // anything, so it snaps to the nearest free spot in the columns
                // it covers.
                const int newY = findNearestFreeYForArea(newX, c->gridWidth(), rawY,
                                                         c->gridHeight(), nullptr, c->id);
                if (newX != c->x || newY != c->y)
                {
                    c->x = newX;
                    c->y = newY;
                    repaint();
                }
            }
        }
        return;
    }

    if (dragState.type == DragState::CommentResize)
    {
        if (patch != nullptr)
        {
            if (auto* c = patch->getCommentById(dragCommentId))
            {
                // The edge being pulled follows the pointer, snapped to the grid;
                // the opposite one stays where it was.
                const int bottom = juce::jlimit(dragCommentStartRect.getY() + 1, 128,
                                                (currentPos.y + gridY / 2) / gridY);
                c->height = bottom - dragCommentStartRect.getY();

                if (dragCommentGrip == CommentGrip::BottomRight)
                {
                    const int right = juce::jlimit(dragCommentStartRect.getX() + 1, 40,
                                                   (currentPos.x + gridX / 2) / gridX);
                    c->x = dragCommentStartRect.getX();
                    c->width = right - dragCommentStartRect.getX();
                }
                else
                {
                    const int right = dragCommentStartRect.getRight();
                    const int left = juce::jlimit(0, right - 1, (currentPos.x + gridX / 2) / gridX);
                    c->x = left;
                    c->width = right - left;
                }
                repaint();
            }
        }
        return;
    }

    if (dragState.type == DragState::ModuleMove)
    {
        // Both axes bounded, same as the multi-move: the canvas is 40 columns
        // by 128 rows and nothing may be dragged past any of its four edges.
        int moduleHeight = dragState.module->getDescriptor()->height;
        int newGridX = juce::jlimit(0, 39, (currentPos.x - dragState.dragOffsetX + gridX / 2) / gridX);
        int rawGridY = juce::jlimit(0, modulePlacementRows - moduleHeight,
                                    (currentPos.y - dragState.dragOffsetY + gridY / 2) / gridY);

        // Prevent overlap: snap to nearest free Y position in this column
        auto& container = patch->getContainer(dragState.section);
        int newGridY = findNearestFreeY(container, dragState.module, newGridX, rawGridY, moduleHeight);

        auto curPos = dragState.module->getPosition();
        if (newGridX != curPos.x || newGridY != curPos.y)
        {
            dragState.module->setPosition({ newGridX, newGridY });
            repaint();
        }
        return;
    }

    // First movement of a re-route: unplug the cable, and carry on as an
    // ordinary cable drag from the end that stayed put.
    if (dragState.type == DragState::CableReroute)
    {
        auto anchor = (patch != nullptr)
            ? noteCableToLift(patch->getContainer(dragState.section),
                              dragState.section, dragState.sourceConnector)
            : ConnectorHit{};

        if (anchor.connector == nullptr)
        {
            dragState = DragState();
            return;
        }

        dragState.type = DragState::CableCreate;
        dragState.module = anchor.module;
        dragState.sourceConnector = anchor.connector;
        dragState.rerouting = true;
        showCablePreview = true;
    }

    if (dragState.type == DragState::CableCreate)
    {
        cablePreviewEnd = currentPos;
        repaint();
        return;
    }

    if (dragState.type == DragState::NoteSeqEditor)
    {
        if (dragState.module == nullptr || dragState.parameter == nullptr)
            return;

        auto moduleRect = getModuleBounds(*dragState.module, 0);
        auto relPos = currentPos - moduleRect.getPosition();
        auto cd = dragState.customRect;

        int zoom = 3;
        if (auto* p = findParameter(*dragState.module, "p1"))
            zoom = juce::jlimit(1, 6, p->getValue());

        int centerNote = 60;
        if (auto* p = findParameter(*dragState.module, "p2"))
        {
            auto* spd = p->getDescriptor();
            int v = p->getValue();
            if (v >= spd->minValue && v <= spd->maxValue)
                centerNote = v;
        }
        else
        {
            centerNote = dragState.parameter->getValue();
        }

        int visibleNotes = juce::jlimit(12, 72, 72 - (zoom - 1) * 12);
        int lowNote = juce::jlimit(0, 127 - visibleNotes, centerNote - visibleNotes / 2);
        int highNote = lowNote + visibleNotes;
        float normY = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(relPos.y - cd.getY()) / static_cast<float>(cd.getHeight()));
        int newValue = juce::jlimit(0, 127,
            static_cast<int>(std::round(static_cast<float>(highNote) - normY * static_cast<float>(visibleNotes))));

        if (newValue != dragState.parameter->getValue())
        {
            auto* pd = dragState.parameter->getDescriptor();
            dragState.parameter->setValue(newValue);
            repaint();

            if (autoUploadOn && parameterChangeCallback && newValue != dragState.lastSentValue)
            {
                auto now = juce::Time::getMillisecondCounter();
                if (now - dragState.lastSendTime >= paramSendIntervalMs)
                {
                    parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, newValue);
                    dragState.lastSentValue = newValue;
                    dragState.lastSendTime = now;
                }
            }
        }
        return;
    }

    if (dragState.type == DragState::NoteSeqScrollbar)
    {
        if (dragState.module == nullptr || dragState.parameter == nullptr)
            return;

        auto moduleRect = getModuleBounds(*dragState.module, 0);
        auto relPos = currentPos - moduleRect.getPosition();
        auto cd = dragState.customRect;
        auto* pd = dragState.parameter->getDescriptor();

        float normY = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(relPos.y - cd.getY()) / static_cast<float>(cd.getHeight()));
        int newValue = juce::jlimit(pd->minValue, pd->maxValue,
            static_cast<int>(std::round(static_cast<float>(pd->maxValue)
                - normY * static_cast<float>(pd->maxValue - pd->minValue))));

        if (newValue != dragState.parameter->getValue())
        {
            dragState.parameter->setValue(newValue);
            repaint();

            if (autoUploadOn && parameterChangeCallback && newValue != dragState.lastSentValue)
            {
                auto now = juce::Time::getMillisecondCounter();
                if (now - dragState.lastSendTime >= paramSendIntervalMs)
                {
                    parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, newValue);
                    dragState.lastSentValue = newValue;
                    dragState.lastSendTime = now;
                }
            }
        }
        return;
    }

    if (dragState.parameter == nullptr)
        return;

    auto* pd = dragState.parameter->getDescriptor();
    int range = pd->maxValue - pd->minValue;
    if (range <= 0)
        return;

    int newValue = dragState.startValue;

    if (dragState.type == DragState::Knob || dragState.type == DragState::Slider)
    {
        // Movement since the knob was picked up, counted in screen pixels so
        // the desktop border cannot cut a sweep short. Zoom used to scale the
        // drag because it worked in canvas coordinates, so keep that: a knob
        // drawn twice as big takes twice the travel.
        auto travel = KnobDrag::travel(e);
        if (zoomLevel > 0.0f && zoomLevel != 1.0f)
            travel = (travel.toFloat() / zoomLevel).roundToInt();

        if (dragState.type == DragState::Knob)
        {
            newValue = KnobDrag::valueFor(travel, dragState.startValue,
                                          pd->minValue, pd->maxValue);
        }
        else
        {
            // Linear control: up increases, over 100px of travel for the whole
            // range. Sliders are drawn vertically throughout the module set.
            float normalized = static_cast<float>(-travel.y) / 100.0f;
            int valueDelta = static_cast<int>(normalized * range);
            newValue = juce::jlimit(pd->minValue, pd->maxValue, dragState.startValue + valueDelta);
        }
    }

    // Update parameter and repaint
    if (newValue != dragState.parameter->getValue())
    {
        dragState.parameter->setValue(newValue);
        repaint();

        // Send to synth in real-time (rate limited)
        if (parameterChangeCallback && dragState.module != nullptr && newValue != dragState.lastSentValue)
        {
            auto now = juce::Time::getMillisecondCounter();
            if (now - dragState.lastSendTime >= paramSendIntervalMs)
            {
                parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, newValue);
                dragState.lastSentValue = newValue;
                dragState.lastSendTime = now;
            }
        }
    }
}

void PatchCanvas::mouseUp(const juce::MouseEvent& e)
{
    // Unconditional, and before the early return: a knob drag must give the
    // pointer back on every path out, or it stays captured. No-op if no knob
    // or slider drag took it.
    KnobDrag::end(e, *this);

    // Letting go of a nudge arrow stops the repeat and closes the undo step.
    if (spinner.isHeld())
    {
        spinnerRelease();
        // The pointer may have wandered off the button while it was held; now
        // that it is free, work out what it is really over.
        updateSpinner(screenToCanvas(e.getPosition()));
        return;
    }

    if (dragState.type == DragState::None)
        return;

    if (dragState.type == DragState::CanvasPan)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::RubberBand)
    {
        showRubberBand = false;
        dragState = DragState();
        repaint();
        return;
    }

    if (dragState.type == DragState::CommentMove)
    {
        // The drag moved the note live; the action records the whole gesture as
        // one undo step, from where it started to where it was let go.
        if (patch != nullptr)
        {
            if (auto* c = patch->getCommentById(dragCommentId))
            {
                juce::Point<int> newPos { c->x, c->y };
                if (newPos != dragCommentStartPos && commentMoveCallback)
                {
                    c->x = dragCommentStartPos.x;
                    c->y = dragCommentStartPos.y;
                    if (undoManager)
                        undoManager->beginNewTransaction("Move Comment");
                    commentMoveCallback(dragCommentId, dragCommentStartPos, newPos);
                }
            }
        }
        dragCommentId = -1;
        dragState = DragState();
        repaint();
        return;
    }

    if (dragState.type == DragState::CommentResize)
    {
        // Same shape as the move above: the drag resized the note live, and the
        // action replays the whole gesture as one undo step.
        if (patch != nullptr)
        {
            if (auto* c = patch->getCommentById(dragCommentId))
            {
                const juce::Rectangle<int> newRect { c->x, c->y, c->gridWidth(), c->gridHeight() };
                if (newRect != dragCommentStartRect && commentResizeCallback)
                {
                    c->x      = dragCommentStartRect.getX();
                    c->y      = dragCommentStartRect.getY();
                    c->width  = dragCommentStartRect.getWidth();
                    c->height = dragCommentStartRect.getHeight();
                    commentResizeCallback(dragCommentId, dragCommentStartRect, newRect);
                }
            }
        }
        dragCommentId = -1;
        dragCommentGrip = CommentGrip::None;
        dragState = DragState();
        repaint();
        return;
    }

    if (dragState.type == DragState::MultiModuleMove)
    {
        // One transaction for the whole gesture, whatever it was carrying:
        // neither callback opens one of its own, so the modules and the notes
        // land in the same undo step.
        if (undoManager && (!multiMoveState.empty() || !commentMoveState.empty()))
            undoManager->beginNewTransaction("Move Selection");

        if (moduleMoveCallback)
        {
            for (auto& ms : multiMoveState)
            {
                auto* module = resolve(ms.ref);
                if (module == nullptr)
                    continue;
                auto newPos = module->getPosition();
                if (newPos != ms.startGridPos)
                    moduleMoveCallback(ms.ref.section, ms.ref.containerIndex,
                                       ms.startGridPos, newPos);
            }
        }

        if (commentMoveCallback && patch != nullptr)
        {
            for (auto& cs : commentMoveState)
            {
                if (auto* c = patch->getCommentById(cs.id))
                {
                    const juce::Point<int> newPos { c->x, c->y };
                    if (newPos != cs.startGridPos)
                    {
                        c->x = cs.startGridPos.x;
                        c->y = cs.startGridPos.y;
                        commentMoveCallback(cs.id, cs.startGridPos, newPos);
                    }
                }
            }
        }

        multiMoveState.clear();
        commentMoveState.clear();
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::ModuleMove)
    {
        if (moduleMoveCallback && undoManager && dragState.module)
        {
            auto newPos = dragState.module->getPosition();
            if (newPos != dragState.startGridPos)
            {
                undoManager->beginNewTransaction("Move Module");
                moduleMoveCallback(dragState.section, dragState.module->getContainerIndex(),
                                   dragState.startGridPos, newPos);
            }
        }
        dragState = DragState();
        return;
    }

    // A re-route that never moved: the cable was never unplugged, so there is
    // nothing to put back and nothing on the undo stack.
    if (dragState.type == DragState::CableReroute)
    {
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::CableCreate)
    {
        showCablePreview = false;
        auto hit = findConnectorAt(screenToCanvas(e.getPosition()));
        if (hit.connector != nullptr && hit.connector != dragState.sourceConnector
            && hit.section == dragState.section)
        {
            auto* src = dragState.sourceConnector;
            auto* dst = hit.connector;
            bool srcOut = src->getDescriptor()->isOutput;
            bool dstOut = dst->getDescriptor()->isOutput;
            auto& container = patch->getContainer(dragState.section);

            // Connect output→input (auto-swap if needed), or chain two inputs
            // like the original editor: the Connection "output" slot then holds
            // the drag-source input. Two outputs can never join.
            Connector* outConn = nullptr;
            Connector* inConn = nullptr;
            if (srcOut && !dstOut) { outConn = src; inConn = dst; }
            else if (!srcOut && dstOut) { outConn = dst; inConn = src; }
            else if (!srcOut && !dstOut) { outConn = src; inConn = dst; }

            // A net is driven by at most one output: refuse to join two nets
            // that already have distinct outputs. The cable a re-route is
            // carrying is still in the patch at this point, so the walk is told
            // to step over it: the question is what the nets look like once the
            // move has landed, not what they look like mid-gesture.
            if (outConn && inConn)
            {
                auto* drv1 = container.findNetOutput(outConn, liftedCable.out, liftedCable.in);
                auto* drv2 = container.findNetOutput(inConn,  liftedCable.out, liftedCable.in);
                if (drv1 != nullptr && drv2 != nullptr && drv1 != drv2)
                    outConn = inConn = nullptr;
            }

            // Find module owners: needed for the undo record, and for telling a
            // re-route that landed back on the connector it came from from one
            // that actually moved.
            auto findOwner = [&](Connector* c) -> Module* {
                for (auto& mp : container.getModules())
                    for (auto& mc : mp->getConnectors())
                        if (&mc == c) return mp.get();
                return nullptr;
            };
            auto* outMod = (outConn != nullptr) ? findOwner(outConn) : nullptr;
            auto* inMod  = (inConn  != nullptr) ? findOwner(inConn)  : nullptr;

            // Dropped straight back where it came from: nothing to move, and
            // nothing goes to the synth or onto the undo stack for it.
            if (dragState.rerouting && outMod != nullptr && inMod != nullptr
                && sameAsLiftedCable(outMod->getContainerIndex(), outConn,
                                     inMod->getContainerIndex(), inConn))
                outConn = inConn = nullptr;

            if (outConn && inConn)
            {
                if (undoManager)
                    undoManager->beginNewTransaction(dragState.rerouting ? "Move Cable"
                                                                         : "Add Cable");

                // A re-route takes the old cable off here, not when the drag
                // started: it is one edit, and until the drop lands there is
                // nothing to say.
                if (dragState.rerouting)
                    commitLiftedCableMove(container, outConn, inConn);
                else
                    container.addConnection(outConn, inConn);

                if (cableCreatedCallback && dragState.module && outMod && inMod)
                    cableCreatedCallback(dragState.section,
                        outMod->getContainerIndex(), outConn->getDescriptor()->index, outConn->getDescriptor()->isOutput,
                        inMod->getContainerIndex(), inConn->getDescriptor()->index, inConn->getDescriptor()->isOutput);
            }
        }

        // A re-route that found nowhere legal to land simply never happened:
        // the cable is still in the patch, and dropping the note puts it back on
        // screen. Nothing was sent, so there is nothing to take back.
        liftedCable = {};

        repaint();
        dragState = DragState();
        return;
    }

    if (dragState.parameter == nullptr)
    {
        dragState = DragState();
        return;
    }

    // MorphRange: send final morph range on release
    if (dragState.type == DragState::MorphRange && dragState.module != nullptr && morphRangeChangeCallback)
    {
        int finalRange = dragState.parameter->getMorphRange();
        int span = std::abs(finalRange);
        int direction = (finalRange >= 0) ? 0 : 1;
        morphRangeChangeCallback(dragState.section, dragState.module->getContainerIndex(),
                                 dragState.parameter->getDescriptor()->index,
                                 span, direction);
        dragState = DragState();
        return;
    }

    // Send final value to synth (only for knobs/sliders, buttons already sent on mouseDown)
    // Skip if the value was already sent during drag
    if (dragState.type != DragState::Button && dragState.module != nullptr)
    {
        int finalValue = dragState.parameter->getValue();
        if (finalValue != dragState.lastSentValue && parameterChangeCallback)
        {
            auto* pd = dragState.parameter->getDescriptor();
            parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, finalValue);
        }

        // Fire drag complete for undo (knobs/sliders only — buttons fire on mouseDown)
        if (paramDragCompleteCallback && finalValue != dragState.startValue)
        {
            auto* pd = dragState.parameter->getDescriptor();
            paramDragCompleteCallback(dragState.section, dragState.module->getContainerIndex(),
                                      pd->index, dragState.startValue, finalValue);
        }
    }

    // Clear drag state
    dragState = DragState();
}

bool PatchCanvas::keyPressed(const juce::KeyPress& key)
{
    // Escape drops whatever is hanging off the pointer before it means
    // anything else
    if (key == juce::KeyPress::escapeKey && pendingDrop.active())
    {
        cancelPendingDrop();
        return true;
    }

    // Delete / Backspace → delete selection
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        // Modules and a selected text note alike: deleteSelection takes both.
        if (hasSelection())
        {
            deleteSelection();
            return true;
        }
    }

    // Escape → clear selection
    if (key == juce::KeyPress::escapeKey)
    {
        if (!selection.empty()) { clearSelection(); repaint(); return true; }
        if (!selectedCommentIds.empty()) { selectedCommentIds.clear(); repaint(); return true; }
    }

    // Ctrl+A → select all modules in this section
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        if (patch != nullptr)
        {
            clearSelection();
            ModuleContainer& container = (mySection == 1) ? patch->getPolyVoiceArea()
                                                          : patch->getCommonArea();
            for (auto& modulePtr : container.getModules())
                selection.push_back(refTo(*modulePtr));
            // Text notes are part of a selection everywhere else, so "select
            // all" has to mean all of it: they were the one thing this left
            // behind.
            for (const auto& c : patch->getComments())
                if (c.section == mySection)
                    selectedCommentIds.push_back(c.id);
            repaint();
            return true;
        }
    }

    // Arrows → nudge selection one grid cell (with undo)
    if (!selection.empty() && !key.getModifiers().isAnyModifierKeyDown())
    {
        int dx = 0, dy = 0;
        if      (key == juce::KeyPress::leftKey)  dx = -1;
        else if (key == juce::KeyPress::rightKey) dx = 1;
        else if (key == juce::KeyPress::upKey)    dy = -1;
        else if (key == juce::KeyPress::downKey)  dy = 1;

        if (dx != 0 || dy != 0)
        {
            if (moduleMoveCallback && undoManager)
            {
                // The selection travels as one block, so it stops when its
                // leading edge reaches the border rather than each module
                // stopping on its own: clamping them individually piled them
                // all onto the same row at the edge, on top of each other.
                bool againstEdge = false;
                for (auto& sel : selection)
                {
                    const auto* module = resolve(sel);
                    if (module == nullptr)
                        continue;
                    // The bottom bound subtracts the module's own height, or a
                    // tall module ends up hanging below the canvas.
                    const int h = module->getDescriptor()->height;
                    const auto pos = module->getPosition();
                    if (pos.x + dx < 0 || pos.x + dx > 39
                        || pos.y + dy < 0 || pos.y + dy > modulePlacementRows - h)
                    {
                        againstEdge = true;
                        break;
                    }
                }

                if (!againstEdge)
                {
                    undoManager->beginNewTransaction("Move Modules");
                    for (auto& sel : selection)
                    {
                        const auto* module = resolve(sel);
                        if (module == nullptr)
                            continue;
                        const auto oldPos = module->getPosition();
                        const juce::Point<int> newPos(oldPos.x + dx, oldPos.y + dy);
                        moduleMoveCallback(sel.section, sel.containerIndex, oldPos, newPos);
                    }
                }
            }
            repaint();
            return true;
        }
    }

    // Ctrl+C → copy. A selected text note copies like a module.
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        if (hasSelection()) { copySelectionToClipboard(); return true; }
    }

    // Ctrl+X → cut (copy + delete)
    if (key == juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0))
    {
        if (hasSelection())
        {
            copySelectionToClipboard();
            deleteSelection();
            return true;
        }
    }

    // Ctrl+V → hang the copied modules off the pointer, to be dropped by the
    // click that follows
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        if (canPaste())
        {
            beginPasteGhost();
            return true;
        }
    }

    // Ctrl+D → duplicate with cables
    if (key == juce::KeyPress('d', juce::ModifierKeys::commandModifier, 0))
    {
        if (hasSelection()) { duplicateSelection(true); return true; }
    }

    // Enter → put a pending block down where the pointer is, or, with nothing
    // pending, open the Quick Add popup there (only one at a time). Adding a
    // module from the keyboard is meant to be quick, so the whole thing is
    // Enter, a few letters, Enter, Enter — without ever reaching for the mouse.
    if (key == juce::KeyPress::returnKey && patch != nullptr && moduleDescs != nullptr)
    {
        if (pendingDrop.active())
        {
            // With the pointer parked off every canvas there is nowhere to read
            // a position from, so the block goes to this canvas, which is the
            // one the keyboard is in.
            if (!dropPendingAtPointer())
                dropPendingAt(screenToCanvas(getMouseXYRelative()));
            return true;
        }

        openQuickAddAtMouse();
        return true;
    }

    // Ctrl+R → randomize (simple), Ctrl+Shift+R → randomize (gaussian)
    if (key == juce::KeyPress('r', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (fileCommandCallback) { fileCommandCallback("randomizeGaussian"); return true; }
    }
    if (key == juce::KeyPress('r', juce::ModifierKeys::commandModifier, 0))
    {
        if (fileCommandCallback) { fileCommandCallback("randomize"); return true; }
    }

    // Ctrl+Z → undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (undoCallback) { undoCallback(); return true; }
    }

    // Ctrl+Shift+Z → redo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (redoCallback) { redoCallback(); return true; }
    }

    // Ctrl+Y → redo (alternative)
    if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0))
    {
        if (redoCallback) { redoCallback(); return true; }
    }

    // File commands and slot switching
    if (fileCommandCallback)
    {
        if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
            { fileCommandCallback("saveAs"); return true; }
        if (key == juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("new"); return true; }
        if (key == juce::KeyPress('o', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("open"); return true; }
        if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("save"); return true; }
        if (key == juce::KeyPress('p', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("patchSettings"); return true; }
        if (key == juce::KeyPress('g', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("synthSettings"); return true; }
        if (key == juce::KeyPress('b', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("presetBrowser"); return true; }

        // Ctrl+1..4 → slot A..D; Ctrl+5..8 → floaters (Knob/Keyboard/Notes/Mutator)
        if (key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown())
        {
            int code = key.getKeyCode();
            if (code == 127) code = '8';  // X11 legacy: Ctrl+8 arrives as DEL (0x7F)
            if (code >= '1' && code <= '4')
            {
                fileCommandCallback("slot" + juce::String(code - '1'));
                return true;
            }
            if (code >= '5' && code <= '8')
            {
                fileCommandCallback("floater" + juce::String(code - '5'));
                return true;
            }
        }
    }

    // F1 → show help popup for the selected/hovered module
    if (key == juce::KeyPress::F1Key)
    {
        // Prefer the module under the mouse cursor, fall back to last selected
        Module* target = nullptr;
        auto mousePos = screenToCanvas(getMouseXYRelative());
        if (patch != nullptr)
        {
            for (auto& modPtr : patch->getPolyVoiceArea().getModules())
            {
                auto pos = modPtr->getPosition();
                int pw = 255;
                int ph = (modPtr->getDescriptor() ? modPtr->getDescriptor()->height * 15 : 60);
                juce::Rectangle<int> bounds(pos.x * gridX, pos.y * gridY, pw, ph);
                if (bounds.contains(mousePos)) { target = modPtr.get(); break; }
            }
            if (!target)
                for (auto& modPtr : patch->getCommonArea().getModules())
                {
                    auto pos = modPtr->getPosition();
                    int ph = (modPtr->getDescriptor() ? modPtr->getDescriptor()->height * 15 : 60);
                    juce::Rectangle<int> bounds(pos.x * gridX, pos.y * gridY, 255, ph);
                    if (bounds.contains(mousePos)) { target = modPtr.get(); break; }
                }
        }
        if (!target)
            target = resolve(selectedRef);

        if (target && target->getDescriptor())
        {
            // Pass both fullname and short name separated by '|' so findModuleHelp
            // can try each — e.g. "12/18/24dB Classic Low Pass Filter|FilterF"
            juce::String helpQuery = target->getDescriptor()->fullname
                                   + "|" + target->getDescriptor()->name;
            ModuleHelpPopup::show(helpQuery, this);
        }

        return true;
    }

    // F5 / F7 -> morph overlay display
    if (auto* parent = findParentComponentOfClass<PatchCanvasComponent>())
    {
        if (handleOverlayKey(key, *parent))
            return true;
    }
    else if (handleOverlayKey(key, *this))
    {
        return true;
    }

    // Ctrl++ → Zoom In, Ctrl+- → Zoom Out
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == '+' || key.getKeyCode() == '=' || key.getKeyCode() == juce::KeyPress::numberPadAdd)
        {
            setZoomLevel(zoomLevel + zoomStep);
            return true;
        }
        if (key.getKeyCode() == '-' || key.getKeyCode() == juce::KeyPress::numberPadSubtract)
        {
            setZoomLevel(zoomLevel - zoomStep);
            return true;
        }
    }

    // + / - → step the control the pointer is over (issue #66). The same target
    // the nudge arrows are pointing at, so what steps is whatever has the arrows
    // under it, and a run of presses closes into one undo step on key up.
    // Shift is allowed through: on most layouts `+` is Shift and `=`.
    if (!key.getModifiers().isCommandDown() && !key.getModifiers().isAltDown())
    {
        const int code = key.getKeyCode();
        int delta = 0;
        if (code == '+' || code == '=' || code == juce::KeyPress::numberPadAdd)
            delta = 1;
        else if (code == '-' || code == '_' || code == juce::KeyPress::numberPadSubtract)
            delta = -1;

        if (delta != 0 && spinnerTarget.module.isValid())
        {
            beginKeyStep();
            spinnerStep(delta);
            return true;
        }
    }

    // Shift+Z → always reset zoom to 100%
    if (key == juce::KeyPress('z', juce::ModifierKeys::shiftModifier, 0))
    {
        resetZoom();
        return true;
    }

    // Z → zoom-to-selection (if any) or reset to 100%
    if (key.getTextCharacter() == 'z' && !key.getModifiers().isCommandDown())
    {
        if (hasSelection())
            zoomToSelection();
        else
            resetZoom();
        return true;
    }

    // S → shake cables (matches the View menu hint)
    if (key.getTextCharacter() == 's' && !key.getModifiers().isAnyModifierKeyDown())
    {
        shakeCables();
        return true;
    }

    return false;
}

bool PatchCanvas::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    // Accept module drags from ModuleBrowserPanel
    auto description = dragSourceDetails.description;
    if (!description.isObject())
        return false;

    auto* obj = description.getDynamicObject();
    if (obj == nullptr)
        return false;

    auto type = obj->getProperty("type").toString();
    return type == "module" || type == "snippetFile" || type == "comment";
}

void PatchCanvas::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    if (!patch || !moduleDescs)
        return;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto type = obj->getProperty("type").toString();
    if (type == "module")
    {
        dropPreviewTypeId = obj->getProperty("typeId");
        showModuleDropPreview = true;
    }
    else if (type == "comment")
    {
        dropPreviewTypeId = PendingDrop::commentGhost;
        showModuleDropPreview = true;
    }
    repaint();
}

void PatchCanvas::itemDragMove(const SourceDetails& dragSourceDetails)
{
    if (!patch || !moduleDescs)
        return;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto type = obj->getProperty("type").toString();
    if (type != "module" && type != "comment")
        return;

    if (!showModuleDropPreview)
        return;

    auto mousePos = screenToCanvas(dragSourceDetails.localPosition.toInt());
    dropPreviewSection = mySection;
    dropPreviewGridX = juce::jlimit(0, 39, mousePos.x / gridX);
    dropPreviewGridY = juce::jlimit(0, 127, mousePos.y / gridY);

    repaint();
}

void PatchCanvas::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
{
    showModuleDropPreview = false;
    repaint();
}

void PatchCanvas::itemDropped(const SourceDetails& dragSourceDetails)
{
    showModuleDropPreview = false;

    if (!patch || !moduleDescs)
        return;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    auto mousePos = screenToCanvas(dragSourceDetails.localPosition.toInt());
    int section = mySection;
    int dropX = juce::jlimit(0, 39, mousePos.x / PatchCanvas::gridX);
    int dropY = juce::jlimit(0, 127, mousePos.y / PatchCanvas::gridY);

    auto type = obj->getProperty("type").toString();
    if (type == "snippetFile")
    {
        auto file = juce::File(obj->getProperty("path").toString());
        if (snippetDropCallback_ && file.existsAsFile())
            snippetDropCallback_(file, section, dropX, dropY);
        repaint();
        return;
    }

    if (type == "comment")
    {
        if (commentAddCallback)
            commentAddCallback(section, dropX, dropY);
        repaint();
        return;
    }

    int typeId = obj->getProperty("typeId");
    juce::String moduleName = obj->getProperty("name").toString();

    // Trigger callback if set
    if (moduleDropCallback)
    {
        if (undoManager) undoManager->beginNewTransaction("Add Module");
        moduleDropCallback(typeId, section, dropX, dropY, moduleName);
    }

    repaint();
}

// --- Parameter context menu ---

void PatchCanvas::showParameterContextMenu(Module& m, int section, Parameter& param)
{
    auto* pd = param.getDescriptor();
    if (pd == nullptr) return;

    bool atDefault = (param.getValue() == pd->defaultValue);
    int currentMorphGroup = param.getMorphGroup();  // -1=none, 0-3=assigned

    juce::PopupMenu menu;
    menu.addSectionHeader(pd->name);

    // 1. Default Value
    menu.addItem(1, "Default Value", !atDefault);

    // 2a. Lock Parameter (toggle)
    menu.addItem(3, param.isLocked() ? "Unlock Parameter" : "Lock Parameter");

    // 2. Zero Morph — sends MorphRangeChange with span=0 to synth
    bool hasMorphAssigned = (currentMorphGroup >= 0);
    menu.addItem(2, "Zero Morph", hasMorphAssigned);

    // 3. Knob assignment submenu
    // Find current knob assignment for this param
    int currentKnob = -1;
    if (patch != nullptr)
    {
        for (int k = 0; k < 23; ++k)
        {
            const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == section
                && ka.module == m.getContainerIndex() && ka.param == pd->index)
            { currentKnob = k; break; }
        }
    }
    {
        juce::PopupMenu knobSubMenu;
        // Knob 1-6
        for (int k = 0; k < 6; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Knob 7-12
        for (int k = 6; k < 12; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Knob 13-15
        for (int k = 12; k < 15; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Knob 16-18
        for (int k = 15; k < 18; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Pedal=19, After touch=20, On/Off switch=22 (18 and 21 unused in the wire format)
        for (int k : { 19, 20, 22 })
        {
            juce::String label = KnobAssignmentMessage::getKnobName(k);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        knobSubMenu.addItem(99, "Disable", currentKnob >= 0);
        menu.addSubMenu("Knob", knobSubMenu);
    }

    // 4. Morph Group submenu
    juce::PopupMenu morphSubMenu;
    for (int g = 1; g <= 4; ++g)
    {
        bool isCurrent = (currentMorphGroup == g - 1);
        morphSubMenu.addItem(10 + g, "Group " + juce::String(g),
                             true, isCurrent);
    }
    morphSubMenu.addSeparator();
    morphSubMenu.addItem(10, "Disable", currentMorphGroup >= 0);
    menu.addSubMenu("Morph", morphSubMenu);

    // 5. MIDI Controller submenu
    int currentMidiCtrl = -1;
    if (patch != nullptr)
    {
        for (const auto& ca : patch->ctrlAssignments)
        {
            if (ca.section == section && ca.module == m.getContainerIndex() && ca.param == pd->index)
            { currentMidiCtrl = ca.control; break; }
        }
    }
    {
        juce::PopupMenu midiSubMenu;
        for (int cc = 0; cc < 120; ++cc)
        {
            bool isCurrent = (cc == currentMidiCtrl);
            midiSubMenu.addItem(200 + cc, "CC " + juce::String(cc), true, isCurrent);
        }
        midiSubMenu.addSeparator();
        midiSubMenu.addItem(199, "Disable", currentMidiCtrl >= 0);
        menu.addSubMenu("MIDI Controller", midiSubMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this, &m, section, &param, currentKnob, currentMidiCtrl](int result)
        {
            auto* pd2 = param.getDescriptor();
            if (pd2 == nullptr) return;

            if (result == 3)
            {
                // Toggle lock
                param.setLocked(!param.isLocked());
                repaint();
            }
            else if (result == 1)
            {
                // Set to default value
                int oldVal = param.getValue();
                param.setValue(pd2->defaultValue);
                if (parameterChangeCallback)
                    parameterChangeCallback(section, m.getContainerIndex(),
                                           pd2->index, pd2->defaultValue);
                if (paramDragCompleteCallback && oldVal != pd2->defaultValue)
                    paramDragCompleteCallback(section, m.getContainerIndex(),
                                              pd2->index, oldVal, pd2->defaultValue);
                repaint();
            }
            else if (result == 2)
            {
                // Zero Morph: remove morph assignment entirely (group + range)
                param.setMorphGroup(-1);
                param.setMorphRange(0);
                if (morphAssignCallback)
                    morphAssignCallback(section, m.getContainerIndex(),
                                       pd2->index, -1);
                repaint();
            }
            else if (result == 10)
            {
                // Disable morph assignment
                param.setMorphGroup(-1);
                param.setMorphRange(0);
                if (morphAssignCallback)
                    morphAssignCallback(section, m.getContainerIndex(),
                                       pd2->index, -1);
                repaint();
            }
            else if (result >= 11 && result <= 14)
            {
                // Assign to morph group 0-3, start at range=0
                int group = result - 11;
                param.setMorphGroup(group);
                param.setMorphRange(0);
                if (morphAssignCallback)
                    morphAssignCallback(section, m.getContainerIndex(),
                                       pd2->index, group);
                // Explicitly tell the synth range=0 so it matches our model
                if (morphRangeChangeCallback)
                    morphRangeChangeCallback(section, m.getContainerIndex(),
                                            pd2->index, 0, 0);
                repaint();
            }
            // Knob assignment (99=disable, 100-122=assign knob 0-22)
            else if (result == 99)
            {
                if (knobAssignCallback && currentKnob >= 0)
                    knobAssignCallback(section, m.getContainerIndex(), pd2->index, -1);
            }
            else if (result >= 100 && result < 123)
            {
                int knob = result - 100;
                if (knobAssignCallback)
                    knobAssignCallback(section, m.getContainerIndex(), pd2->index, knob);
            }
            // MIDI Controller assignment (199=disable, 200-319=assign CC 0-119)
            else if (result == 199)
            {
                if (midiCtrlAssignCallback && currentMidiCtrl >= 0)
                    midiCtrlAssignCallback(section, m.getContainerIndex(), pd2->index, -1);
            }
            else if (result >= 200 && result < 320)
            {
                int cc = result - 200;
                if (midiCtrlAssignCallback)
                    midiCtrlAssignCallback(section, m.getContainerIndex(), pd2->index, cc);
            }
        });
}

// --- Selection helpers ---
