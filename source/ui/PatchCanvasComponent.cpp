#include "PatchCanvasComponent.h"
#include "../model/ModulePlacement.h"
#include "../format/ValueFormatters.h"
#include <cmath>
#include <set>
#include <unordered_map>

void PatchCanvas::setLightMeterData(const int lights[128], const int meters[128])
{
    bool lightChanged[128], meterChanged[128];
    bool any = false;
    for (int i = 0; i < 128; ++i)
    {
        lightChanged[i] = lights[i] != globalLightValues[i];
        meterChanged[i] = meters[i] != globalMeterValues[i];
        any = any || lightChanged[i] || meterChanged[i];
    }

    // The synth streams these many times per second, mostly re-sending the
    // same values; only the modules whose slots changed need a repaint.
    if (!any)
        return;

    std::copy(lights, lights + 128, globalLightValues);
    std::copy(meters, meters + 128, globalMeterValues);

    if (patch == nullptr)
        return;

    // Walk the live modules and look their slots up, rather than holding on to
    // the pointers the table was built from: this runs on every frame the synth
    // sends, and a module deleted between two frames would be read here.
    const auto& table = lightRangeTable();
    const auto& container = patch->getContainer(mySection);

    for (const auto& modulePtr : container.getModules())
    {
        const auto* slots = table.find(mySection, modulePtr->getContainerIndex());
        if (slots == nullptr)
            continue;

        const auto& r = *slots;

        bool dirty = false;
        for (int i = 0; !dirty && i < r.lightCount && r.lightBase + i < 128; ++i)
            dirty = lightChanged[r.lightBase + i];
        for (int i = 0; !dirty && i < r.meterCount && r.meterBase + i < 128; ++i)
            dirty = meterChanged[r.meterBase + i];

        if (dirty)
        {
            auto rf = getModuleBounds(*modulePtr, 0).toFloat();
            rf *= zoomLevel;
            repaint(rf.getSmallestIntegerContainer().expanded(2));
        }
    }
}

const LightMeterLayout::Table& PatchCanvas::lightRangeTable() const
{
    const auto fingerprint = LightMeterLayout::fingerprint(patch, themeData);
    if (!lightRangeCacheValid_ || lightRangeCache_.fingerprint != fingerprint)
    {
        lightRangeCache_ = LightMeterLayout::build(patch, themeData);
        lightRangeCacheValid_ = true;
    }
    return lightRangeCache_;
}


PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, sectionHeight);
    setWantsKeyboardFocus(true);
    liveCanvases.push_back(this);

    // The arrows are placed in canvas coordinates, which the zoom scales.
    spinner.repaintArea = [this](juce::Rectangle<float> area)
    {
        repaintCanvasArea(area.getSmallestIntegerContainer());
    };
}

bool PatchCanvas::handleOverlayKey(const juce::KeyPress& key, juce::Component& repaintTarget)
{
    // F5, F7, F8 and F9 read out the parameter values and the three kinds of
    // assignment, as the original
    // editor's function keys do. Each toggles: pressing the same key again
    // closes the readout rather than needing the key held down. The View >
    // Overlays menu drives the same toggles, so both go through one place.
    auto toggle = [&repaintTarget](OverlayMode mode)
    {
        toggleOverlayMode(mode);
        // The canvases repaint themselves; the component the key arrived at is
        // the scroll container around one of them and is not on that list.
        repaintTarget.repaint();
        return true;
    };

    if (key == juce::KeyPress::F5Key) return toggle(OverlayMode::Values);

    if (key == juce::KeyPress::F7Key) return toggle(OverlayMode::MorphGroups);
    if (key == juce::KeyPress::F8Key) return toggle(OverlayMode::Knobs);
    if (key == juce::KeyPress::F9Key) return toggle(OverlayMode::MidiCtrls);
    // Not one of the original's keys: it has no whole-patch cost readout, only
    // the per-module answer on a double-click. Worth having when you are hunting
    // for what to cut in a patch that is over budget. It moved from F10, which
    // Windows and some Linux desktops reserve for the menu bar; F10 stays as a
    // quiet alias.
    if (key == juce::KeyPress::F3Key || key == juce::KeyPress::F10Key)
        return toggle(OverlayMode::ModuleCosts);

    return false;
}

void PatchCanvas::updateSizeForZoom()
{
    int w = juce::roundToInt(canvasWidth * zoomLevel);
    int h = juce::roundToInt(sectionHeight * zoomLevel);
    setSize(w, h);
}

void PatchCanvas::setZoomLevel(float z, juce::Point<int> /*anchor*/)
{
    z = juce::jlimit(zoomMin, zoomMax, z);
    if (std::abs(z - zoomLevel) < 0.001f)
        return;
    zoomLevel = z;
    updateSizeForZoom();
    repaint();
}

void PatchCanvas::resetZoom()
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        auto vpCenter = vp->getViewPosition() + juce::Point<int>(vp->getWidth() / 2, vp->getHeight() / 2);
        auto canvasCenter = screenToCanvas(vpCenter);
        zoomLevel = 1.0f;
        updateSizeForZoom();
        vp->setViewPosition(canvasCenter.x - vp->getWidth() / 2,
                            canvasCenter.y - vp->getHeight() / 2);
    }
    else
    {
        zoomLevel = 1.0f;
        updateSizeForZoom();
    }
    repaint();
}

void PatchCanvas::zoomToSelection()
{
    // Selected text notes count as selected: Z zooms to whatever is picked out
    // on the canvas, and a note is one of the things you can pick out.
    std::vector<const PatchComment*> selectedComments;
    if (patch != nullptr)
        for (const auto& c : patch->getComments())
            if (c.section == mySection && isCommentSelected(c.id))
                selectedComments.push_back(&c);

    if (selection.empty() && selectedComments.empty())
        return;

    int minX = 999999, minY = 999999, maxX = 0, maxY = 0;
    for (auto& sel : selection)
    {
        const auto* m = resolve(sel);
        if (m == nullptr)
            continue;
        auto gpos = m->getPosition();
        int px = gpos.x * gridX;
        int py = gpos.y * gridY;
        int ph = m->getDescriptor() ? m->getDescriptor()->height * gridY : 60;
        minX = juce::jmin(minX, px);
        minY = juce::jmin(minY, py);
        maxX = juce::jmax(maxX, px + gridX);
        maxY = juce::jmax(maxY, py + ph);
    }

    for (const auto* c : selectedComments)
    {
        const auto r = getCommentBounds(*c);
        minX = juce::jmin(minX, r.getX());
        minY = juce::jmin(minY, r.getY());
        maxX = juce::jmax(maxX, r.getRight());
        maxY = juce::jmax(maxY, r.getBottom());
    }

    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        int bw = maxX - minX;
        int bh = maxY - minY;
        if (bw > 0 && bh > 0)
        {
            float zx = static_cast<float>(vp->getWidth()) / static_cast<float>(bw);
            float zy = static_cast<float>(vp->getHeight()) / static_cast<float>(bh);
            float newZoom = juce::jlimit(zoomMin, zoomMax, juce::jmin(zx, zy) * 0.9f);
            zoomLevel = newZoom;
            updateSizeForZoom();

            int cx = juce::roundToInt((minX + bw / 2.0f) * newZoom);
            int cy = juce::roundToInt((minY + bh / 2.0f) * newZoom);
            vp->setViewPosition(cx - vp->getWidth() / 2, cy - vp->getHeight() / 2);
        }
    }
    repaint();
}

void PatchCanvas::setSection(int s)
{
    mySection = s;
    updateSizeForZoom();
}

PatchCanvas::~PatchCanvas()
{
    liveCanvases.erase(std::remove(liveCanvases.begin(), liveCanvases.end(), this),
                       liveCanvases.end());

    // A pending drop outlives one canvas — closing a slot window while modules
    // hang off the pointer must not leave it pointing at this one.
    if (pendingHost == this)
        pendingHost = nullptr;
    if (liveCanvases.empty())
        pendingDrop = {};

    // If the popup is still open when we're destroyed, clear its callbacks
    // first so it can't fire onDismiss with a dangling 'this' pointer.
    if (activeQuickAdd != nullptr)
    {
        activeQuickAdd->clearCallbacks();
        delete activeQuickAdd;
        activeQuickAdd = nullptr;
    }
}

void PatchCanvas::setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td)
{
    // A new patch reuses container indices, so every reference into the old one
    // has to go: left behind, they would name whatever sits at the same index
    // in the incoming patch.
    dragState = DragState();
    // Names modules in the outgoing patch, and a re-route can only ever be
    // resolved against the patch it was lifted from.
    liftedCable = {};
    selection.clear();
    selectedRef.clear();
    // Comment ids belong to the outgoing patch; the incoming one reuses them.
    selectedCommentIds.clear();
    commentMoveState.clear();
    dragCommentId = -1;
    drumPresetState.clear();
    clearHover();
    costBadgeModule.clear();
    cableSagOffsets.clear();
    activeQuickAdd = nullptr;
    showCablePreview = false;
    showModuleDropPreview = false;
    showRubberBand = false;

    patch = p;
    moduleDescs = md;
    themeData = td;

    // Apply morph assignments from patch model to individual parameters
    if (p != nullptr)
    {
        for (const auto& ma : p->morphAssignments)
        {
            auto& container = p->getContainer(ma.section);
            auto* mod = container.getModuleByIndex(ma.module);
            if (mod == nullptr) continue;
            auto* param = mod->getParameter(ma.param);
            if (param == nullptr) continue;
            param->setMorphGroup(ma.morph);
            param->setMorphRange(ma.range);
        }
    }

    repaint();
}

juce::Rectangle<int> PatchCanvas::getModuleBounds(const Module& m, int yOffset) const
{
    auto pos = m.getPosition();
    int x = pos.x * gridX;
    int y = yOffset + pos.y * gridY;
    int h = m.getDescriptor()->height * gridY;
    return { x, y, gridX, h };
}

// --- Editor text notes ---------------------------------------------------

Parameter* PatchCanvas::findParameter(Module& m, const juce::String& componentId)
{
    return const_cast<Parameter*>(
        static_cast<const PatchCanvas*>(this)->findParameter(static_cast<const Module&>(m), componentId));
}

const Parameter* PatchCanvas::findParameter(const Module& m, const juce::String& componentId) const
{
    if (componentId.isEmpty())
        return nullptr;

    for (auto& p : m.getParameters())
    {
        if (p.getDescriptor()->componentId == componentId)
            return &p;
    }

    // Log miss only once per module type + componentId (avoid flooding)
    static std::set<juce::String> logged;
    auto key = m.getDescriptor()->componentId + ":" + componentId;
    if (logged.find(key) == logged.end())
    {
        logged.insert(key);
        DBG("findParameter MISS: module=" + m.getDescriptor()->componentId
            + " (" + m.getTitle() + ") param=" + componentId
            + " [has " + juce::String(m.getParameters().size()) + " params: "
            + [&]() {
                juce::String ids;
                for (auto& p : m.getParameters())
                    ids += p.getDescriptor()->componentId + " ";
                return ids;
            }() + "]");
    }
    return nullptr;
}

const ThemeConnector* PatchCanvas::findThemeConnector(const ModuleTheme& theme, const juce::String& componentId) const
{
    for (auto& tc : theme.connectors)
    {
        if (tc.componentId == componentId)
            return &tc;
    }
    return nullptr;
}

juce::Point<int> PatchCanvas::getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const
{
    auto bounds = getModuleBounds(m, yOffset);

    // Try theme-based position first
    if (themeData != nullptr)
    {
        auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
        if (theme != nullptr)
        {
            auto* tc = findThemeConnector(*theme, conn.getDescriptor()->componentId);
            if (tc != nullptr)
                return { bounds.getX() + tc->x + tc->size / 2,
                         bounds.getY() + tc->y + tc->size / 2 };
        }
    }

    // Fallback: geometric distribution
    auto* desc = conn.getDescriptor();

    int outputIdx = 0, inputIdx = 0;
    int thisOutputIdx = -1, thisInputIdx = -1;
    for (auto& c : m.getConnectors())
    {
        if (c.getDescriptor()->isOutput)
        {
            if (&c == &conn) thisOutputIdx = outputIdx;
            outputIdx++;
        }
        else
        {
            if (&c == &conn) thisInputIdx = inputIdx;
            inputIdx++;
        }
    }

    int moduleH = bounds.getHeight();
    int headerH = 14;

    if (desc->isOutput)
    {
        int spacing = (outputIdx > 0) ? (moduleH - headerH) / outputIdx : moduleH;
        int connY = bounds.getY() + headerH + thisOutputIdx * spacing + spacing / 2;
        return { bounds.getRight() - 4, connY };
    }
    else
    {
        int spacing = (inputIdx > 0) ? (moduleH - headerH) / inputIdx : moduleH;
        int connY = bounds.getY() + headerH + thisInputIdx * spacing + spacing / 2;
        return { bounds.getX() + 4, connY };
    }
}

// A seamless, faint grain overlay tiled over the canvas for themes that want
// texture (Nord Classic). It's an overlay so it works on top of any base colour
// without re-tinting. A very light, fine grain — values dialled in by ear.
void PatchCanvas::repaintCanvasArea(juce::Rectangle<int> canvasArea)
{
    // Canvas coordinates are the zoomed-out ones the painting code works in;
    // repaint() wants component pixels. A couple of pixels of margin covers
    // the rounding and any border drawn just outside the rectangle.
    auto scaled = canvasArea.toFloat() * zoomLevel;
    repaint(scaled.getSmallestIntegerContainer().expanded(2));
}

// ── Frequency display units (issue #30) ─────────────────────────────────────
//
// modules.xml gives 24 modules a "freq display units" custom parameter, and the
// original editor rotates through its settings when the frequency box is
// clicked: an absolute frequency reads as Hz or as a note, and a slave's detune
// reads as a partial ratio, as semitones, or as the frequency it lands on. The
// setting belongs to the display only — it never reaches the synth, and the
// value it is applied to does not change.
namespace
{
    const juce::String kFreqUnitsParamName { "freq display units" };
    // Sentinel: a slave oscillator's absolute frequency, which exists only
    // relative to whatever master drives it.
    const juce::String kSlaveHzFormatter { "@slaveHz" };

    // The units apply to the module's own frequency, which in every one of
    // these modules is its first ordinary parameter.
    bool isFrequencyDisplay(const Module& m, const juce::String& displayComponentId)
    {
        for (const auto& p : m.getParameters())
        {
            const auto* pd = p.getDescriptor();
            if (pd == nullptr || pd->componentId != displayComponentId)
                continue;
            return pd->paramClass == "parameter" && pd->index == 0;
        }
        return false;
    }

    // The master driving a slave oscillator, found through its master-slave
    // input, or nullptr when nothing is patched into it.
    const Module* findMasterFor(ModuleContainer& container, const Module& slave)
    {
        Connector* masterIn = nullptr;
        auto* mutableSlave = container.getModuleByIndex(slave.getContainerIndex());
        if (mutableSlave == nullptr)
            return nullptr;

        for (auto& c : mutableSlave->getConnectors())
        {
            const auto* cd = c.getDescriptor();
            if (cd != nullptr && !cd->isOutput && cd->signalType == SignalType::MasterSlave)
            {
                masterIn = &c;
                break;
            }
        }
        if (masterIn == nullptr)
            return nullptr;

        auto* driver = container.findNetOutput(masterIn);
        if (driver == nullptr)
            return nullptr;

        for (const auto& modulePtr : container.getModules())
            for (const auto& c : modulePtr->getConnectors())
                if (&c == driver)
                    return modulePtr.get();
        return nullptr;
    }
}

const Parameter* PatchCanvas::freqUnitsParamFor(const Module& m, const juce::String& displayComponentId) const
{
    if (displayComponentId.isEmpty() || !isFrequencyDisplay(m, displayComponentId))
        return nullptr;

    for (const auto& p : m.getParameters())
    {
        const auto* pd = p.getDescriptor();
        if (pd != nullptr && pd->paramClass == "custom" && pd->name == kFreqUnitsParamName)
            return &p;
    }
    return nullptr;
}

Parameter* PatchCanvas::freqUnitsParamFor(Module& m, const juce::String& displayComponentId)
{
    return const_cast<Parameter*>(
        static_cast<const PatchCanvas*>(this)->freqUnitsParamFor(static_cast<const Module&>(m),
                                                                 displayComponentId));
}

juce::Array<PatchCanvas::FreqUnit> PatchCanvas::freqUnitsFor(const Module& m,
                                                            const juce::String& displayComponentId,
                                                            const juce::String& baseFormatter) const
{
    juce::Array<FreqUnit> units;

    const auto* unitsParam = freqUnitsParamFor(m, displayComponentId);
    if (unitsParam == nullptr || unitsParam->getDescriptor() == nullptr)
        return units;

    // A ratio display belongs to a slave, whose value is an interval; anything
    // else shows an absolute pitch. The first entry is always what the display
    // reads today, so a patch that never touched the setting looks unchanged.
    if (baseFormatter == "fmtPartials")
    {
        units.add({ "fmtPartials",  "Ratio" });
        units.add({ "fmtSemitones", "Semitones" });
        units.add({ kSlaveHzFormatter, "Frequency" });
    }
    else
    {
        units.add({ baseFormatter, "Frequency" });
        units.add({ "fmtNote",     "Semitones" });
    }

    // maxValue is what modules.xml allows the stored setting to reach: 1 for the
    // two-unit displays, 2 for the slave oscillators.
    const int allowed = juce::jlimit(1, units.size(), unitsParam->getDescriptor()->maxValue + 1);
    units.removeRange(allowed, units.size() - allowed);
    return units;
}

juce::String PatchCanvas::formatInFreqUnit(const Module& m, const Parameter& valueParam,
                                           const FreqUnit& unit) const
{
    if (unit.formatter != kSlaveHzFormatter)
        return ValueFormatters::format(unit.formatter, valueParam.getValue());

    // A slave's frequency is its master's, shifted by the detune it carries.
    // fmtOscHz is exponential with twelve steps to the octave, and the detune is
    // centred on 64, so the two add before formatting.
    if (patch == nullptr)
        return "--";

    auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();
    const auto* master = findMasterFor(container, m);
    if (master == nullptr)
        return "--";   // nothing driving it: it has no frequency of its own

    const Parameter* masterPitch = nullptr;
    for (const auto& p : master->getParameters())
    {
        const auto* pd = p.getDescriptor();
        if (pd != nullptr && pd->paramClass == "parameter" && pd->index == 0)
        {
            masterPitch = &p;
            break;
        }
    }
    if (masterPitch == nullptr)
        return "--";

    return ValueFormatters::format("fmtOscHz",
                                   masterPitch->getValue() + valueParam.getValue() - 64);
}

juce::String PatchCanvas::getParameterValueText(const Parameter& param) const
{
    auto* pd = param.getDescriptor();
    if (pd == nullptr)
        return juce::String(param.getValue());

    const auto fmt = [pd](int v) { return ValueFormatters::format(pd->formatter, v); };
    const auto start = fmt(param.getValue());

    const int group = param.getMorphGroup();
    if (group < 0 || group >= 4 || param.getMorphRange() == 0)
        return start;

    const int end = juce::jlimit(pd->minValue, pd->maxValue,
                                 param.getValue() + param.getMorphRange());
    return start + "-" + fmt(end);
}

bool PatchCanvas::isAreaFree(int gx, int gw, int gy, int gh,
                             const Module* excludeModule, int excludeCommentId) const
{
    if (patch == nullptr)
        return true;

    gw = juce::jmax(1, gw);
    gh = juce::jmax(1, gh);

    const auto& container = patch->getContainer(mySection);

    for (auto& m : container.getModules())
    {
        if (m.get() == excludeModule || m == nullptr)
            continue;
        const auto pos = m->getPosition();
        if (pos.x < gx || pos.x >= gx + gw)   // a module is always one column wide
            continue;
        const int mh = m->getDescriptor() != nullptr ? m->getDescriptor()->height : 1;
        if (gy < pos.y + mh && pos.y < gy + gh)
            return false;
    }

    // A text note holds its rectangle of the grid as firmly as a module does, in
    // both directions: it gets out of the way of a module being dragged, and a
    // module is in the way of it.
    for (const auto& c : patch->getComments())
    {
        if (c.section != mySection || c.id == excludeCommentId)
            continue;
        if (c.x + c.gridWidth() <= gx || c.x >= gx + gw)
            continue;
        if (gy < c.y + c.gridHeight() && c.y < gy + gh)
            return false;
    }

    return true;
}

int PatchCanvas::findNearestFreeYForArea(int gx, int gw, int targetY, int gh,
                                         const Module* excludeModule, int excludeCommentId) const
{
    // Everything below row 128 is "free" in the occupancy sense and invisible
    // in every other sense, so the search never offers a row the block would
    // hang out of. A drag that kept going used to drop modules off the bottom
    // of the canvas, where they still exist but cannot be seen or clicked.
    const int maxY = modulePlacementRows - juce::jmax(1, gh);
    targetY = juce::jlimit(0, maxY, targetY);

    if (isAreaFree(gx, gw, targetY, gh, excludeModule, excludeCommentId))
        return targetY;

    for (int offset = 1; offset < 256; ++offset)
    {
        const int above = targetY - offset;
        if (above >= 0 && isAreaFree(gx, gw, above, gh, excludeModule, excludeCommentId))
            return above;
        const int below = targetY + offset;
        if (below <= maxY && isAreaFree(gx, gw, below, gh, excludeModule, excludeCommentId))
            return below;
    }
    return targetY;
}

bool PatchCanvas::isPositionFree(const ModuleContainer& /*container*/, const Module* exclude, int gx, int gy, int height) const
{
    return isAreaFree(gx, 1, gy, height, exclude, -1);
}

int PatchCanvas::findNearestFreeY(const ModuleContainer& container, const Module* exclude, int gx, int targetY, int height) const
{
    // Same bottom rule as findNearestFreeYForArea: no candidate may leave the
    // module hanging past row 128.
    const int maxY = modulePlacementRows - juce::jmax(1, height);
    targetY = juce::jlimit(0, maxY, targetY);

    if (isPositionFree(container, exclude, gx, targetY, height))
        return targetY;

    // Search above and below alternately, return closest free slot
    for (int offset = 1; offset < 256; offset++)
    {
        int above = targetY - offset;
        if (above >= 0 && isPositionFree(container, exclude, gx, above, height))
            return above;
        int below = targetY + offset;
        if (below <= maxY && isPositionFree(container, exclude, gx, below, height))
            return below;
    }
    return targetY; // fallback — a completely full column keeps the target row
}

Connector* PatchCanvas::findConnectorByComponentId(Module& m, const juce::String& componentId)
{
    for (auto& c : m.getConnectors())
    {
        if (c.getDescriptor()->componentId == componentId)
            return &c;
    }
    return nullptr;
}

PatchCanvas::ConnectorHit PatchCanvas::findConnectorAt(juce::Point<int> pos)
{
    if (patch == nullptr || themeData == nullptr)
        return {};

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

            auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
            if (theme == nullptr)
                continue;

            auto relPos = pos - rect.getPosition();
            for (auto& tc : theme->connectors)
            {
                juce::Rectangle<int> connRect(tc.x, tc.y, tc.size, tc.size);
                connRect = connRect.expanded(2);
                if (connRect.contains(relPos))
                {
                    auto* conn = findConnectorByComponentId(m, tc.componentId);
                    if (conn != nullptr)
                        return { &m, conn, area.section };
                }
            }
        }
    }
    return {};
}

bool PatchCanvas::isDragging(int section, int moduleId, int parameterId) const
{
    if (dragState.type == DragState::None || dragState.module == nullptr || dragState.parameter == nullptr)
        return false;

    return dragState.section == section
        && dragState.module->getContainerIndex() == moduleId
        && dragState.parameter->getDescriptor()->index == parameterId;
}

// --- DragAndDropTarget implementation ---

bool PatchCanvas::isSelected(const Module* m) const
{
    if (m == nullptr)
        return false;
    const auto ref = refTo(*m);
    for (auto& s : selection)
        if (s == ref) return true;
    return false;
}

bool PatchCanvas::connectorHasCable(const ModuleContainer& container, const Connector* conn)
{
    for (const auto& c : container.getConnections())
        if (c.output == conn || c.input == conn)
            return true;
    return false;
}

PatchCanvas::ConnectorHit PatchCanvas::noteCableToLift(ModuleContainer& container,
                                                      int section, Connector* conn)
{
    // The last cable in the list is the one drawn on top, which is the one the
    // pointer looks like it is grabbing. Repeating the gesture walks down the
    // stack, so several cables come off a connector one at a time.
    const Connection* found = nullptr;
    for (const auto& c : container.getConnections())
        if (c.output == conn || c.input == conn)
            found = &c;

    if (found == nullptr)
        return {};

    Connector* out = found->output;
    Connector* in  = found->input;

    auto findOwner = [&container](const Connector* c) -> Module*
    {
        for (auto& mp : container.getModules())
            for (auto& mc : mp->getConnectors())
                if (&mc == c) return mp.get();
        return nullptr;
    };

    auto* outMod = findOwner(out);
    auto* inMod  = findOwner(in);
    if (outMod == nullptr || inMod == nullptr)
        return {};

    // Noted, not removed. The canvas stops drawing it so it looks lifted, and
    // the patch stays exactly as it was until the drop lands.
    liftedCable = { section, out, in,
                    outMod->getContainerIndex(), out->getDescriptor()->index, out->getDescriptor()->isOutput,
                    inMod->getContainerIndex(),  in->getDescriptor()->index,  in->getDescriptor()->isOutput };

    // The end that stays put is the one the drag now carries.
    return (out == conn) ? ConnectorHit{ inMod, in, section }
                         : ConnectorHit{ outMod, out, section };
}

void PatchCanvas::commitLiftedCableMove(ModuleContainer& container,
                                        Connector* outConn, Connector* inConn)
{
    if (!liftedCable.isValid())
        return;

    // Off first, then on, which is the order every other cable edit in the
    // editor uses and the order the synth is given them in. Both model calls
    // notify the synchronizer; the two undo actions are told the model work is
    // already done, exactly as the right-click delete and the plain cable drag
    // already do.
    container.removeConnection(liftedCable.out, liftedCable.in);

    if (cableDeletedCallback)
        cableDeletedCallback(liftedCable.section,
                             liftedCable.outModIndex, liftedCable.outConnIndex, liftedCable.outIsOutput,
                             liftedCable.inModIndex,  liftedCable.inConnIndex,  liftedCable.inIsOutput);

    liftedCable = {};

    container.addConnection(outConn, inConn);
}

bool PatchCanvas::sameAsLiftedCable(int outModIndex, const Connector* out,
                                    int inModIndex,  const Connector* in) const
{
    if (!liftedCable.isValid() || out == nullptr || in == nullptr)
        return false;

    return outModIndex == liftedCable.outModIndex
        && out->getDescriptor()->index    == liftedCable.outConnIndex
        && out->getDescriptor()->isOutput == liftedCable.outIsOutput
        && inModIndex  == liftedCable.inModIndex
        && in->getDescriptor()->index     == liftedCable.inConnIndex
        && in->getDescriptor()->isOutput  == liftedCable.inIsOutput;
}

void PatchCanvas::dropDragIfModuleGone()
{
    // Selection, hover, the spinner, the cost badge and the multi-move all name
    // their modules by reference, and a reference to a deleted module resolves
    // to nothing: there is nothing for them to forget.
    //
    // A drag is the exception, because it holds a Parameter* and a Connector*
    // as well as its module. Those live inside the module rather than beside
    // it, so an index cannot name them. A drag runs from one mouse-down to the
    // matching mouse-up, which an undo or the MCP bridge can outlive, so it is
    // checked before anything reads it. Costs nothing when no drag is running.
    if (dragState.module == nullptr || patch == nullptr)
        return;

    if (!patch->getContainer(dragState.section).contains(dragState.module))
    {
        // A re-route in flight has changed nothing, so dropping it is free: the
        // cable it was carrying is still in the patch and comes straight back
        // into view.
        liftedCable = {};
        dragState = DragState();
    }
}

void PatchCanvas::clearSelection()
{
    selection.clear();
    // Notes are part of the selection, so letting go of it lets go of them too.
    selectedCommentIds.clear();
    selectedRef.clear();
    if (moduleSelectedCallback) moduleSelectedCallback(nullptr, -1);
}

void PatchCanvas::beginMultiMove(juce::Point<int> pos)
{
    dragState = DragState();
    dragState.type = DragState::MultiModuleMove;
    dragState.startPos = pos;

    multiMoveState.clear();
    for (auto& sel : selection)
        if (const auto* m = resolve(sel))
            multiMoveState.push_back({ sel, m->getPosition() });

    commentMoveState.clear();
    if (patch != nullptr)
        for (int id : selectedCommentIds)
            if (auto* c = patch->getCommentById(id))
                commentMoveState.push_back({ id, { c->x, c->y } });
}

void PatchCanvas::selectModule(Module* m, int section, bool addToSelection)
{
    if (!addToSelection) clearSelection();
    if (m == nullptr)
        return;

    const ModuleRef ref { section, m->getContainerIndex() };
    if (!isSelected(m))
        selection.push_back(ref);
    selectedRef = ref;
    // Notify inspector — report the most recently selected module
    if (moduleSelectedCallback) moduleSelectedCallback(m, section);
}

void PatchCanvas::updateRubberBandSelection(juce::Rectangle<int> rect)
{
    selection.clear();
    selectedCommentIds.clear();
    if (patch == nullptr) return;

    ModuleContainer& container = (mySection == 1)
        ? patch->getPolyVoiceArea()
        : patch->getCommonArea();

    for (auto& modulePtr : container.getModules())
    {
        auto bounds = getModuleBounds(*modulePtr, 0);
        if (rect.intersects(bounds))
            selection.push_back(refTo(*modulePtr));
    }

    // The band catches text notes too: dragging a box round a corner of the
    // patch should take everything in it, labels included.
    for (const auto& c : patch->getComments())
        if (c.section == mySection && rect.intersects(getCommentBounds(c)))
            selectedCommentIds.push_back(c.id);
}

bool PatchCanvasComponent::keyPressed(const juce::KeyPress& key)
{
    // Escape and Enter have to reach a pending drop from here too: the command
    // can be given from the Edit menu, which leaves the focus off the canvases.
    if (key == juce::KeyPress::escapeKey && PatchCanvas::isDropPending())
    {
        PatchCanvas::cancelPendingDrop();
        return true;
    }
    if (key == juce::KeyPress::returnKey && PatchCanvas::dropPendingAtPointer())
        return true;

    return PatchCanvas::handleOverlayKey(key, *this);
}

void PatchCanvasComponent::resized()
{
    auto area = getLocalBounds();
    juce::Component* comps[] = { &polyViewport, &resizerBar, &commonViewport };
    layout.layOutComponents(comps, 3,
                            area.getX(), area.getY(),
                            area.getWidth(), area.getHeight(),
                            true,   // vertical layout
                            true);
}

void PatchCanvasComponent::setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td)
{
    polyCanvas.setPatch(p, md, td);
    commonCanvas.setPatch(p, md, td);

    if (p != nullptr)
    {
        // Scroll each panel to show the topmost module in that section
        auto scrollTo = [](juce::Viewport& vp, const ModuleContainer& container) {
            int minY = -1;
            for (auto& m : container.getModules())
            {
                int y = m->getPosition().y * PatchCanvas::gridY;
                minY = (minY < 0) ? y : juce::jmin(minY, y);
            }
            vp.setViewPosition(0, (minY > 0) ? juce::jmax(0, minY - 20) : 0);
        };
        scrollTo(polyViewport,   p->getPolyVoiceArea());
        scrollTo(commonViewport, p->getCommonArea());
    }
}

// ============================================================
// DrumSynth (m58) — Preset section overlay
// Draws the Preset display + up/down spinner arrows at the
// bottom-right of the module (local preset, CtrlIndex=-1).
// Preset index is stored in drumSynthPresetIndex (module-local
// state via component-id lookup in drumPresetState map).
// ============================================================
