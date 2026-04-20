#include "PatchCanvasComponent.h"
#include "QuickAddPopup.h"
#include "../format/ValueFormatters.h"
#include "BinaryData.h"
#include <cmath>
#include <set>
#include <unordered_map>

// Decoration bitmap cache: iconName ("decoration-N") → loaded juce::Image.
// PNGs are embedded via juce_add_binary_data in CMakeLists.txt.
static juce::Image loadDecorationImage(const juce::String& iconName)
{
    static std::unordered_map<juce::String, juce::Image> cache;
    auto it = cache.find(iconName);
    if (it != cache.end())
        return it->second;

    juce::Image img;
    // JUCE BinaryData strips non-alphanumeric: "decoration-7.png" → "decoration7_png"
    auto resourceName = iconName.replaceCharacter('-', ' ').removeCharacters(" ") + "_png";
    int dataSize = 0;
    if (const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize))
        img = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));
    cache[iconName] = img;
    return img;
}

// Returns a grayscale-recoloured copy of the decoration PNG using luminance as
// alpha so dark pixels become opaque strokes in the target tint. Cached by
// (iconName, tint) so the tinting loop only runs once per (icon, colour-scheme).
static juce::Image getTintedDecoration(const juce::String& iconName, juce::Colour tint)
{
    struct Key { juce::String n; juce::uint32 c; bool operator==(const Key& o) const { return n == o.n && c == o.c; } };
    struct KH { size_t operator()(const Key& k) const { return std::hash<std::string>{}(k.n.toStdString()) ^ k.c; } };
    static std::unordered_map<Key, juce::Image, KH> cache;

    Key key { iconName, tint.getARGB() };
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    auto src = loadDecorationImage(iconName);
    if (! src.isValid()) { cache[key] = juce::Image(); return {}; }

    juce::Image tinted(juce::Image::ARGB, src.getWidth(), src.getHeight(), true);
    for (int py = 0; py < src.getHeight(); ++py)
        for (int px = 0; px < src.getWidth(); ++px)
        {
            auto c = src.getPixelAt(px, py);
            float lum = c.getBrightness();
            float a   = (1.0f - lum) * (c.getAlpha() / 255.0f);
            if (a > 0.01f)
                tinted.setPixelAt(px, py, tint.withAlpha(a));
        }
    cache[key] = tinted;
    return tinted;
}


// --- PatchCanvas (inner scrollable surface) ---

void PatchCanvas::setLightMeterData(const int lights[128], const int meters[128])
{
    std::copy(lights, lights + 128, globalLightValues);
    std::copy(meters, meters + 128, globalMeterValues);
    repaint();
}

int PatchCanvas::computeModuleLightIndex(const Module& targetModule, int targetSection, bool forMeters) const
{
    if (patch == nullptr || themeData == nullptr) return 0;

    // Build sorted list: poly (section=1) first, then common (section=0), each sorted by containerIndex
    struct ModuleRef { const Module* mod; int section; };
    std::vector<ModuleRef> ordered;

    auto addSection = [&](const ModuleContainer& container, int sec)
    {
        size_t start = ordered.size();
        for (auto& m : container.getModules())
            ordered.push_back({ m.get(), sec });
        std::sort(ordered.begin() + static_cast<std::ptrdiff_t>(start), ordered.end(),
                  [](const ModuleRef& a, const ModuleRef& b) {
                      return a.mod->getContainerIndex() < b.mod->getContainerIndex();
                  });
    };

    addSection(patch->getPolyVoiceArea(), 1);
    addSection(patch->getCommonArea(), 0);

    int baseIndex = 0;
    for (auto& ref : ordered)
    {
        if (ref.mod == &targetModule && ref.section == targetSection)
            return baseIndex;

        // Count lights/meters for this module from its theme
        auto compId = ref.mod->getDescriptor() ? ref.mod->getDescriptor()->componentId : juce::String();
        if (const ModuleTheme* theme = themeData->getModuleTheme(compId))
        {
            bool hasMeterOrLedArray = false;
            int meterSlots = 0;
            for (auto& light : theme->lights)
            {
                if (forMeters && (light.type == "meter" || light.type == "led-array"))
                {
                    hasMeterOrLedArray = true;
                    if (light.type == "meter")
                        ++meterSlots;
                }
                if (!forMeters && light.type == "led")  ++baseIndex;
            }

            // NOMAD registers meters and sequencer led-arrays as MeterMessage
            // pairs. A single led-array/single meter still consumes two slots.
            if (forMeters && hasMeterOrLedArray)
                baseIndex += juce::jmax(2, meterSlots);
        }
    }
    return baseIndex;
}

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, sectionHeight);
    setWantsKeyboardFocus(true);
    initDrumPresets();
    loadDrumPresetsFromFile();
}

bool PatchCanvas::handleMorphOverlayKey(const juce::KeyPress& key, juce::Component& repaintTarget)
{
    if (key == juce::KeyPress::F5Key)
    {
        morphOverlayMode = (morphOverlayMode == MorphOverlayMode::Values)
            ? MorphOverlayMode::Off
            : MorphOverlayMode::Values;
        repaintTarget.repaint();
        return true;
    }

    if (key == juce::KeyPress::F7Key)
    {
        morphOverlayMode = (morphOverlayMode == MorphOverlayMode::Groups)
            ? MorphOverlayMode::Off
            : MorphOverlayMode::Groups;
        repaintTarget.repaint();
        return true;
    }

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
    if (selection.empty())
        return;

    int minX = 999999, minY = 999999, maxX = 0, maxY = 0;
    for (auto& sel : selection)
    {
        auto gpos = sel.module->getPosition();
        int px = gpos.x * gridX;
        int py = gpos.y * gridY;
        int ph = sel.module->getDescriptor() ? sel.module->getDescriptor()->height * gridY : 60;
        minX = juce::jmin(minX, px);
        minY = juce::jmin(minY, py);
        maxX = juce::jmax(maxX, px + gridX);
        maxY = juce::jmax(maxY, py + ph);
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

void PatchCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        // Zoom towards mouse cursor
        float oldZoom = zoomLevel;
        float newZoom = juce::jlimit(zoomMin, zoomMax, oldZoom + wheel.deltaY * zoomStep * 3.0f);
        if (std::abs(newZoom - oldZoom) < 0.001f)
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
        return;
    }

    // Default: let viewport handle normal scrolling
    juce::Component::mouseWheelMove(e, wheel);
}

void PatchCanvas::setSection(int s)
{
    mySection = s;
    updateSizeForZoom();
}

PatchCanvas::~PatchCanvas()
{
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
    // Reset all raw pointers into the old patch to avoid dangling refs
    dragState = DragState();
    selection.clear();
    selectedModule = nullptr;
    selectedSection = -1;
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

void PatchCanvas::paint(juce::Graphics& g)
{
    g.fillAll(activeScheme_.gridBackground);

    // Apply zoom transform — all subsequent drawing is in canvas (logical) coordinates
    g.addTransform(juce::AffineTransform::scale(zoomLevel));

    // Draw grid lines at column/row boundaries
    g.setColour(activeScheme_.gridLines);
    auto clip = g.getClipBounds();

    int startX = (clip.getX() / gridX) * gridX;
    int startY = (clip.getY() / gridY) * gridY;

    for (int x = startX; x < clip.getRight(); x += gridX)
        g.drawVerticalLine(x, static_cast<float>(clip.getY()), static_cast<float>(clip.getBottom()));

    for (int y = startY; y < clip.getBottom(); y += gridY)
        g.drawHorizontalLine(y, static_cast<float>(clip.getX()), static_cast<float>(clip.getRight()));

    if (patch == nullptr)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", clip, juce::Justification::centred, false);
        return;
    }

    // Each canvas instance is section-specific (mySection 0=common, 1=poly).
    // yOffset is always 0 since each canvas starts from the top.
    auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();

    if (container.getModules().empty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", clip, juce::Justification::centred, false);
    }

    paintModules(g, container, 0);
    paintCables(g, container, 0);
    paintMorphOverlays(g, container, 0);

    // Cable creation preview (rubber-band cable)
    if (showCablePreview && dragState.sourceConnector != nullptr && dragState.module != nullptr)
    {
        auto srcPos = getConnectorPosition(*dragState.module, *dragState.sourceConnector, 0);

        // Color from source connector signal type
        auto* srcDesc = dragState.sourceConnector->getDescriptor();
        juce::Colour cableColor = juce::Colours::white;
        if (srcDesc)
        {
            switch (srcDesc->signalType)
            {
                case SignalType::Audio:       cableColor = activeScheme_.cableAudio;       break;
                case SignalType::Control:     cableColor = activeScheme_.cableControl;     break;
                case SignalType::Logic:       cableColor = activeScheme_.cableLogic;       break;
                case SignalType::MasterSlave: cableColor = activeScheme_.cableMasterSlave; break;
                case SignalType::User1:       cableColor = activeScheme_.cableUser1;       break;
                case SignalType::User2:       cableColor = activeScheme_.cableUser2;       break;
                default: break;
            }
        }

        // Check if cursor is hovering a valid destination connector
        auto hit = findConnectorAt(cablePreviewEnd);
        bool validTarget = hit.connector != nullptr
                        && hit.connector != dragState.sourceConnector
                        && hit.section == dragState.section
                        && srcDesc != nullptr
                        && srcDesc->isOutput != hit.connector->getDescriptor()->isOutput;

        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());
        float endX = static_cast<float>(cablePreviewEnd.x);
        float endY = static_cast<float>(cablePreviewEnd.y);
        float midY = (srcPos.y + cablePreviewEnd.y) * 0.5f;
        float sag = std::abs(float(srcPos.x - cablePreviewEnd.x)) * 0.15f + 15.0f;
        path.cubicTo(static_cast<float>(srcPos.x), midY + sag,
                     endX, midY + sag,
                     endX, endY);

        // Dark outline
        g.setColour(activeScheme_.gridBackground.withAlpha(0.6f));
        g.strokePath(path, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Colored cable
        g.setColour(cableColor.withAlpha(validTarget ? 0.95f : 0.55f));
        g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Highlight valid target connector with a bright ring
        if (validTarget && hit.module != nullptr)
        {
            auto tPos = getConnectorPosition(*hit.module, *hit.connector, 0);
            float r = 10.0f;
            g.setColour(cableColor.brighter(0.5f));
            g.drawEllipse(tPos.x - r, tPos.y - r, r * 2, r * 2, 2.0f);
        }
    }

    // Rubber band selection rectangle
    if (showRubberBand)
    {
        auto rb = rubberBandRect.toFloat();
        g.setColour(activeScheme_.selectionFill);
        g.fillRect(rb);
        g.setColour(activeScheme_.snapHighlight.withAlpha(0.8f));
        g.drawRect(rb, 1.5f);
    }

    // Module drop preview (ghost outline)
    if (showModuleDropPreview && moduleDescs != nullptr)
    {
        auto* descriptor = moduleDescs->getModuleByIndex(dropPreviewTypeId);
        if (descriptor != nullptr)
        {
            int x = dropPreviewGridX * gridX;
            int y = dropPreviewGridY * gridY;
            int width = gridX;
            int height = descriptor->height * gridY;

            juce::Rectangle<int> previewBounds(x, y, width, height);

            // Draw semi-transparent module outline
            g.setColour(juce::Colours::cyan.withAlpha(0.3f));
            g.fillRoundedRectangle(previewBounds.toFloat(), 3.0f);

            g.setColour(juce::Colours::cyan.withAlpha(0.8f));
            g.drawRoundedRectangle(previewBounds.toFloat(), 3.0f, 2.0f);

            // Module name
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(descriptor->fullname, previewBounds.reduced(4, 4), juce::Justification::centred, true);
        }
    }
}

void PatchCanvas::paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    for (auto& modulePtr : container.getModules())
    {
        auto& m = *modulePtr;
        auto rect = getModuleBounds(m, yOffset);

        // Check if module is visible in clip region
        if (!g.getClipBounds().intersects(rect))
            continue;

        const ModuleTheme* theme = nullptr;
        if (themeData != nullptr)
            theme = themeData->getModuleTheme(m.getDescriptor()->componentId);

        if (theme != nullptr)
            paintModuleThemed(g, m, mySection, rect, *theme, container);
        else
            paintModuleFallback(g, m, rect);

    }
}

void PatchCanvas::paintMorphOverlays(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    if (morphOverlayMode == MorphOverlayMode::Off || themeData == nullptr)
        return;

    for (auto& modulePtr : container.getModules())
    {
        auto& m = *modulePtr;
        auto rect = getModuleBounds(m, yOffset);
        if (!g.getClipBounds().intersects(rect))
            continue;

        if (const auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId))
            paintMorphOverlay(g, m, rect, *theme);
    }
}

void PatchCanvas::paintModuleThemed(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container)
{
    paintModuleBackground(g, m, bounds, theme);
    paintCustomDisplays(g, m, bounds, theme);
    paintLabels(g, m, bounds, theme);
    paintTextDisplays(g, m, bounds, theme);
    paintSliders(g, m, bounds, theme);
    // Decorations (signal-flow lines/symbols) sit beneath knobs and buttons so
    // overlapping controls (e.g. GainControl's shift button crossing dec-2) stay
    // visible on top.
    paintStaticIcons(g, bounds, theme);
    paintKnobs(g, m, bounds, theme);
    auto bgForButtons = activeScheme_.moduleBg.isOpaque()
        ? activeScheme_.moduleBg
        : m.getDescriptor()->background;
    paintButtons(g, m, bounds, theme, bgForButtons);
    paintResetButtons(g, m, bounds, theme);
    paintConnectors(g, m, bounds, theme, container);
    paintLights(g, m, section, bounds, theme);
    if (m.getDescriptor()->index == 58)
        paintDrumSynthExtras(g, m, bounds);
}

void PatchCanvas::paintMorphOverlay(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    if (morphOverlayMode == MorphOverlayMode::Off)
        return;

    juce::StringArray seen;

    auto paintControl = [&](const juce::String& componentId, juce::Rectangle<float> controlBounds)
    {
        if (componentId.isEmpty() || seen.contains(componentId))
            return;

        seen.add(componentId);
        if (auto* param = findParameter(m, componentId))
            if (param->getMorphGroup() >= 0 && param->getMorphGroup() < 4)
                paintMorphOverlayBadge(g, controlBounds, bounds, *param);
    };

    for (const auto& tk : theme.knobs)
        paintControl(tk.componentId,
                     { static_cast<float>(bounds.getX() + tk.x),
                       static_cast<float>(bounds.getY() + tk.y),
                       static_cast<float>(tk.size),
                       static_cast<float>(tk.size) });

    for (const auto& ts : theme.sliders)
        paintControl(ts.componentId,
                     { static_cast<float>(bounds.getX() + ts.x),
                       static_cast<float>(bounds.getY() + ts.y),
                       static_cast<float>(ts.width),
                       static_cast<float>(ts.height) });

    for (const auto& tb : theme.buttons)
        paintControl(tb.componentId,
                     { static_cast<float>(bounds.getX() + tb.x),
                       static_cast<float>(bounds.getY() + tb.y),
                       static_cast<float>(tb.width),
                       static_cast<float>(tb.height) });

    for (const auto& td : theme.textDisplays)
        paintControl(td.componentId,
                     { static_cast<float>(bounds.getX() + td.x),
                       static_cast<float>(bounds.getY() + td.y),
                       static_cast<float>(td.width),
                       static_cast<float>(td.height) });
}

void PatchCanvas::paintMorphOverlayBadge(juce::Graphics& g, juce::Rectangle<float> controlBounds,
                                         juce::Rectangle<int> moduleBounds, const Parameter& param)
{
    auto text = getMorphOverlayText(param);
    if (text.isEmpty())
        return;

    g.setFont(juce::FontOptions("Fira Sans", 10.0f, juce::Font::bold));
    const float badgeH = 14.0f;
    const float badgeW = juce::jlimit(18.0f, 70.0f, static_cast<float>(text.length()) * 6.0f + 8.0f);
    float bx = controlBounds.getCentreX() - badgeW * 0.5f;
    float by = controlBounds.getY() - badgeH - 2.0f;

    const auto module = moduleBounds.toFloat().reduced(3.0f, 2.0f);
    bx = juce::jlimit(module.getX(), module.getRight() - badgeW, bx);
    if (by < module.getY())
        by = controlBounds.getBottom() + 2.0f;
    by = juce::jlimit(module.getY(), module.getBottom() - badgeH, by);

    juce::Rectangle<float> badge(bx, by, badgeW, badgeH);
    const int group = param.getMorphGroup();
    const juce::Colour fillColor = (group >= 0 && group < 4)
        ? activeScheme_.morphColor[group].withAlpha(0.92f)
        : juce::Colours::white;
    const juce::Colour textColor = fillColor.getBrightness() > 0.55f
        ? juce::Colours::black.withAlpha(0.85f)
        : juce::Colours::white;
    g.setColour(fillColor);
    g.fillRoundedRectangle(badge, 3.0f);
    g.setColour(fillColor.darker(0.25f));
    g.drawRoundedRectangle(badge, 3.0f, 1.4f);
    g.setColour(textColor);
    g.drawText(text, badge.toNearestInt(), juce::Justification::centred, false);
}

juce::String PatchCanvas::getMorphOverlayText(const Parameter& param) const
{
    const int group = param.getMorphGroup();
    if (group < 0 || group >= 4)
        return {};

    if (morphOverlayMode == MorphOverlayMode::Groups)
        return "M" + juce::String(group + 1);

    if (morphOverlayMode == MorphOverlayMode::Values)
    {
        const int range = param.getMorphRange();
        return (range >= 0 ? "+" : "") + juce::String(range);
    }

    return {};
}

void PatchCanvas::paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    auto bgColour = activeScheme_.moduleBg.isOpaque()
        ? activeScheme_.moduleBg
        : m.getDescriptor()->background;

    // Module body (flat background, no title band)
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);

    // GroupBoxes — rounded rect section borders (e.g. PWidth in OscA)
    for (auto& gb : theme.groupBoxes)
    {
        auto gbRect = juce::Rectangle<float>(
            static_cast<float>(bounds.getX() + gb.x),
            static_cast<float>(bounds.getY() + gb.y),
            static_cast<float>(gb.width),
            static_cast<float>(gb.height));
        g.setColour(bgColour.darker(0.25f));
        g.fillRoundedRectangle(gbRect, 3.0f);
        g.setColour(bgColour.darker(0.5f));
        g.drawRoundedRectangle(gbRect, 3.0f, 1.0f);
    }

    // Module name (no band, text directly on background)
    auto titleBar = juce::Rectangle<int>(bounds.getX(), bounds.getY() + 2, bounds.getWidth(), 12);
    g.setColour(activeScheme_.moduleText);
    g.setFont(juce::FontOptions("Fira Sans", 12.5f, juce::Font::bold));
    g.drawText(m.getTitle(), titleBar.reduced(4, 0), juce::Justification::centredLeft, true);

    // Subtle edge lines on all four sides
    g.setColour(activeScheme_.moduleBorder);
    float x1 = static_cast<float>(bounds.getX());
    float y1 = static_cast<float>(bounds.getY());
    float x2 = static_cast<float>(bounds.getRight());
    float y2 = static_cast<float>(bounds.getBottom());
    g.drawLine(x1, y1, x2, y1, 1.0f); // top
    g.drawLine(x1, y2, x2, y2, 1.0f); // bottom
    g.drawLine(x1, y1, x1, y2, 1.0f); // left
    g.drawLine(x2, y1, x2, y2, 1.0f); // right

    // Selection: border-only highlight drawn in paintModules — no background fill here
    if (isSelected(&m))
    {
        g.setColour(activeScheme_.selectionRect);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.5f), 2.5f, 1.0f);
    }
}

bool PatchCanvas::hasHiddenCable(const Connector& conn, const ModuleContainer& container) const
{
    if (patch == nullptr) return false;

    // If all cables are fully transparent, treat every connected cable as "hidden"
    if (cableOpacity < 0.01f)
    {
        for (auto& connection : container.getConnections())
            if (connection.output == &conn || connection.input == &conn)
                return true;
        return false;
    }

    const auto& hdr = patch->getHeader();

    for (auto& connection : container.getConnections())
    {
        if (connection.output == &conn || connection.input == &conn)
        {
            if (connection.output == nullptr || connection.output->getDescriptor() == nullptr)
                continue;

            bool visible = true;
            switch (connection.output->getDescriptor()->signalType)
            {
                case SignalType::Audio:       visible = hdr.cableVisRed;    break;
                case SignalType::Control:     visible = hdr.cableVisBlue;   break;
                case SignalType::Logic:       visible = hdr.cableVisYellow; break;
                case SignalType::MasterSlave: visible = hdr.cableVisGray;   break;
                case SignalType::User1:       visible = hdr.cableVisGreen;  break;
                case SignalType::User2:       visible = hdr.cableVisPurple; break;
                case SignalType::None:        visible = hdr.cableVisWhite;  break;
            }

            if (!visible) return true;
        }
    }
    return false;
}

void PatchCanvas::paintConnectors(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container)
{
    for (auto& tc : theme.connectors)
    {
        float cx = static_cast<float>(bounds.getX() + tc.x);
        float cy = static_cast<float>(bounds.getY() + tc.y);
        float sz = static_cast<float>(tc.size);

        // Determine color from CSS class
        juce::Colour connColour = juce::Colours::white;
        if      (tc.cssClass == "cAUDIO")   connColour = activeScheme_.cableAudio;
        else if (tc.cssClass == "cCONTROL") connColour = activeScheme_.cableControl;
        else if (tc.cssClass == "cLOGIC")   connColour = activeScheme_.cableLogic;
        else if (tc.cssClass == "cSLAVE")   connColour = activeScheme_.cableMasterSlave;
        else if (tc.cssClass == "cUSER1")   connColour = activeScheme_.cableUser1;
        else if (tc.cssClass == "cUSER2")   connColour = activeScheme_.cableUser2;

        // Find the actual connector object and check if output
        bool isOutput = false;
        const Connector* actualConnector = nullptr;
        for (auto& conn : m.getConnectors())
        {
            if (conn.getDescriptor() && conn.getDescriptor()->componentId == tc.componentId)
            {
                isOutput = conn.getDescriptor()->isOutput;
                actualConnector = &conn;
                break;
            }
        }

        // Check if this connector has a hidden (filtered) cable — show "capped" visual
        bool capped = (actualConnector != nullptr) && hasHiddenCable(*actualConnector, container);

        const float innerRatio = 0.38f;
        const float innerSz = sz * innerRatio;
        const float innerOffset = (sz - innerSz) * 0.5f;
        const juce::Colour darkHole = activeScheme_.connHole;
        const juce::Colour outline  = activeScheme_.connOutline;

        if (isOutput)
        {
            // Output: rounded rectangle (square shape)
            const float cornerRadius = sz * 0.25f;
            g.setColour(connColour);
            g.fillRoundedRectangle(cx, cy, sz, sz, cornerRadius);

            g.setColour(outline);
            g.drawRoundedRectangle(cx, cy, sz, sz, cornerRadius, 1.0f);

            if (capped)
            {
                const float sqSz = innerSz * 1.1f;
                const float sqOff = (sz - sqSz) * 0.5f;
                g.setColour(connColour.darker(0.4f));
                g.fillRoundedRectangle(cx + sqOff, cy + sqOff, sqSz, sqSz, cornerRadius * 0.4f);
            }
            else
            {
                // Dark inner square (plug hole)
                const float sqSz = innerSz * 1.1f;
                const float sqOff = (sz - sqSz) * 0.5f;
                g.setColour(darkHole);
                g.fillRoundedRectangle(cx + sqOff, cy + sqOff, sqSz, sqSz, cornerRadius * 0.4f);
            }
        }
        else
        {
            // Input: filled circle
            g.setColour(connColour);
            g.fillEllipse(cx, cy, sz, sz);

            g.setColour(outline);
            g.drawEllipse(cx, cy, sz, sz, 1.0f);

            if (capped)
            {
                g.setColour(connColour.darker(0.4f));
                g.fillEllipse(cx + innerOffset, cy + innerOffset, innerSz, innerSz);
            }
            else
            {
                // Dark inner circle (socket hole)
                g.setColour(darkHole);
                g.fillEllipse(cx + innerOffset, cy + innerOffset, innerSz, innerSz);
            }
        }
    }

    // For each input connector, if there's a knob at similar Y and nearby X, draw a connecting line
    // Skip for m58 (DrumSynth): Vel/Pitch connectors falsely match OSC knobs by proximity
    if (m.getDescriptor()->index == 58) return;

    const int yTolerance = 12;
    g.setColour(activeScheme_.connectorLine);
    for (auto& tc : theme.connectors)
    {
        if (tc.cssClass == "cSLAVE") continue;
        bool isOut = false;
        for (auto& conn : m.getConnectors())
            if (conn.getDescriptor() && conn.getDescriptor()->componentId == tc.componentId)
                { isOut = conn.getDescriptor()->isOutput; break; }
        if (isOut) continue;

        float connCx = static_cast<float>(bounds.getX() + tc.x) + tc.size * 0.5f;
        float connCy = static_cast<float>(bounds.getY() + tc.y) + tc.size * 0.5f;

        float bestDist = 999.0f;
        float knobCx = -1.0f;
        float bestKnobSz = 21.0f;
        for (auto& tk : theme.knobs)
        {
            float kx = static_cast<float>(bounds.getX() + tk.x) + tk.size * 0.5f;
            float ky = static_cast<float>(bounds.getY() + tk.y) + tk.size * 0.5f;
            float dy = std::abs(ky - connCy);
            float dx = kx - connCx;
            if (dy <= static_cast<float>(yTolerance) && dx > 0.0f && dx < bestDist)
            {
                bestDist = dx;
                knobCx = kx;
                bestKnobSz = static_cast<float>(tk.size);
            }
        }

        if (knobCx > 0.0f && bestDist < 40.0f)
        {
            float renderScale = (bestKnobSz >= 26.0f) ? 0.82f : 0.78f;
            float knobRadius  = bestKnobSz * renderScale * 0.5f;
            float lineY  = connCy;
            float lineX0 = connCx + tc.size * 0.5f;
            float lineX1 = knobCx - knobRadius - 1.0f;
            if (lineX1 > lineX0)
                g.drawLine(lineX0, lineY, lineX1, lineY, 1.0f);
        }
    }
}

void PatchCanvas::paintLabels(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    g.setColour(activeScheme_.moduleText);
    g.setFont(juce::FontOptions(9.0f));

    static const juce::StringArray boldLabels { "OSC", "Noise Filter", "Bend" };

    for (auto& label : theme.labels)
    {
        bool isBold = m.getDescriptor()->index == 58 && boldLabels.contains(label.text);
        g.setFont(juce::FontOptions("Fira Sans", 9.0f, isBold ? juce::Font::bold : juce::Font::plain));

        if (label.text.containsChar('\n'))
        {
            // Multiline label: use y as top of first line (XML positions are intentional)
            auto lines = juce::StringArray::fromLines(label.text);
            const int lineH = 10;
            const int startY = bounds.getY() + label.y;
            for (int i = 0; i < lines.size(); ++i)
            {
                g.drawText(lines[i],
                           bounds.getX() + label.x, startY + i * lineH,
                           60, lineH,
                           juce::Justification::centredLeft, true);
            }
        }
        else if (m.getDescriptor()->index == 58 && label.text == "Bend")
        {
            // DrumSynth: "Bend" label rendered vertically (rotated -90°), bold
            juce::Graphics::ScopedSaveState ss(g);
            g.setFont(juce::FontOptions("Fira Sans", 9.0f, juce::Font::bold));
            int cx = bounds.getX() + label.x + 5;
            int cy = bounds.getY() + label.y + 20;
            g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                           static_cast<float>(cx),
                                                           static_cast<float>(cy)));
            g.drawText("Bend", cx - 20, cy - 5, 40, 10, juce::Justification::centred, true);
        }
        else
        {
            g.drawText(label.text,
                       bounds.getX() + label.x, bounds.getY() + label.y,
                       120, 12,
                       juce::Justification::centredLeft, true);
        }
    }
}

void PatchCanvas::paintKnobs(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& tk : theme.knobs)
    {
        float cx = static_cast<float>(bounds.getX() + tk.x);
        float cy = static_cast<float>(bounds.getY() + tk.y);
        float sz = static_cast<float>(tk.size);

        auto* param = findParameter(m, tk.componentId);
        int morphGroup = (param != nullptr) ? param->getMorphGroup() : -1;
        bool hasMorph = (morphGroup >= 0 && morphGroup < 4);
        juce::Colour baseColor = hasMorph ? activeScheme_.morphColor[morphGroup] : activeScheme_.knobBase;

        // Compute knob geometry first — needed for both wedge and grip
        float normalized = 0.5f;
        if (param != nullptr)
        {
            auto* pd = param->getDescriptor();
            int paramRange = pd->maxValue - pd->minValue;
            if (paramRange > 0)
                normalized = static_cast<float>(param->getValue() - pd->minValue) / static_cast<float>(paramRange);
        }

        // Knob center — always at the XML-defined centre regardless of render scale
        float centerX = cx + sz * 0.5f;
        float centerY = cy + sz * 0.5f;

        // Render the knob slightly smaller than the XML size so it looks
        // proportional on screen (original Nomad knobs are compact).
        float renderScale = (sz >= 26.0f) ? 0.82f : 0.78f;
        float rSz    = sz * renderScale;
        float radius  = rSz * 0.5f;
        float rcx    = centerX - radius;
        float rcy    = centerY - radius;

        // Knob angle: 270° range from 7 o'clock (-135°) to 5 o'clock (+135°)
        const float kRangedeg = 270.0f;
        const float kStartDeg = -135.0f;
        float knobAngleDeg = kStartDeg + normalized * kRangedeg;
        float knobAngle    = knobAngleDeg * juce::MathConstants<float>::pi / 180.0f;

        // Background circle — morph group color if assigned, grey otherwise
        g.setColour(baseColor);
        g.fillEllipse(rcx, rcy, rSz, rSz);

        // Morph wedge: starts at the knob's current position, sweeps by morphRange
        if (hasMorph && param != nullptr)
        {
            int morphRangeVal = param->getMorphRange();  // -127..127
            float sweepRad = (static_cast<float>(morphRangeVal) / 127.0f)
                             * kRangedeg * juce::MathConstants<float>::pi / 180.0f;

            if (std::abs(sweepRad) > 0.005f)
            {
                float fromAngle = (sweepRad >= 0.0f) ? knobAngle : knobAngle + sweepRad;
                float toAngle   = (sweepRad >= 0.0f) ? knobAngle + sweepRad : knobAngle;

                float r = radius * 0.82f;
                juce::Path wedge;
                wedge.addPieSegment(centerX - r, centerY - r, r * 2.0f, r * 2.0f,
                                    fromAngle, toAngle, 0.0f);
                g.setColour(baseColor.darker(0.55f));
                g.fillPath(wedge);
            }
        }

        // Outline
        g.setColour(hasMorph ? baseColor.darker(0.4f) : activeScheme_.knobBorder);
        g.drawEllipse(rcx, rcy, rSz, rSz, 1.0f);

        // Travel-limit tick marks at -135° (7 o'clock) and +135° (5 o'clock)
        // Drawn OUTSIDE the knob circle, on the module background
        {
            const float pi = juce::MathConstants<float>::pi;
            const float tickInner = radius * 1.08f;
            const float tickOuter = radius * 1.45f;
            const float limitAngles[2] = { -135.0f * pi / 180.0f, 135.0f * pi / 180.0f };
            g.setColour(activeScheme_.knobTickMark);
            for (float a : limitAngles)
            {
                float sa = std::sin(a), ca = std::cos(a);
                g.drawLine(centerX + sa * tickInner, centerY - ca * tickInner,
                           centerX + sa * tickOuter, centerY - ca * tickOuter, 1.5f);
            }
        }

        // Grip line — drawn on top of everything so it's always visible
        float innerR = radius * 0.3f;
        float outerR = radius * 0.85f;
        float sinA   = std::sin(knobAngle);
        float cosA   = std::cos(knobAngle);

        g.setColour(activeScheme_.knobGrip);
        g.drawLine(centerX + sinA * innerR, centerY - cosA * innerR,
                   centerX + sinA * outerR, centerY - cosA * outerR, 1.5f);

        // Lock indicator — small padlock icon at bottom-right of knob
        if (param != nullptr && param->isLocked())
        {
            float lockSize = juce::jmax(7.0f, rSz * 0.35f);
            float lx = rcx + rSz - lockSize + 1.0f;
            float ly = rcy + rSz - lockSize + 1.0f;
            float bodyH = lockSize * 0.55f;
            float bodyW = lockSize * 0.85f;
            float bodyX = lx + (lockSize - bodyW) * 0.5f;
            float bodyY = ly + lockSize - bodyH;
            // Lock body
            g.setColour(activeScheme_.lockBody);
            g.fillRoundedRectangle(bodyX, bodyY, bodyW, bodyH, 1.0f);
            // Shackle arc
            float shackleW = bodyW * 0.55f;
            float shackleH = lockSize - bodyH;
            float shackleX = bodyX + (bodyW - shackleW) * 0.5f;
            g.setColour(activeScheme_.lockShackle);
            juce::Path shackle;
            shackle.addArc(shackleX, ly, shackleW, shackleH * 2.0f,
                           -juce::MathConstants<float>::pi, 0.0f, true);
            g.strokePath(shackle, juce::PathStrokeType(1.5f));
        }
    }
}

// Draw a waveform/icon shape by name into the given rectangle
// Paths are drawn in a centred sub-rect (75% wide, 55% tall) so they look
// compact and proportional at any button size.
static void drawButtonIcon(juce::Graphics& g, const juce::String& iconName,
                           float ix, float iy, float iw, float ih, juce::Colour colour)
{
    g.setColour(colour);
    float mx = ix + iw * 0.5f,  my = iy + ih * 0.5f;
    float pw = iw * 0.75f,       ph = ih * 0.55f;
    float x0 = mx - pw * 0.5f,  x1 = mx + pw * 0.5f;
    float y0 = my - ph * 0.5f,  y1 = my + ph * 0.5f;

    juce::Path p;

    if (iconName == "wf_sine")
    {
        p.startNewSubPath(x0, my);
        for (int i = 0; i <= 32; ++i)
        {
            float t = static_cast<float>(i) / 32.0f;
            p.lineTo(x0 + pw * t,
                     my - ph * 0.5f * std::sin(t * juce::MathConstants<float>::twoPi));
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_tri")
    {
        p.startNewSubPath(x0, my);
        p.lineTo(x0 + pw * 0.25f, y0);
        p.lineTo(x0 + pw * 0.75f, y1);
        p.lineTo(x1, my);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_saw")
    {
        p.startNewSubPath(x0, y1);
        p.lineTo(x1, y0);
        p.lineTo(x1, y1);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_saw_inv")
    {
        p.startNewSubPath(x1, y1);
        p.lineTo(x0, y0);
        p.lineTo(x0, y1);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_square")
    {
        p.startNewSubPath(x0, my);
        p.lineTo(x0, y0);
        p.lineTo(mx, y0);
        p.lineTo(mx, y1);
        p.lineTo(x1, y1);
        p.lineTo(x1, my);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_noise")
    {
        // Irregular noise zigzag
        p.startNewSubPath(x0,              my + ph * 0.1f);
        p.lineTo(x0 + pw * 0.12f,  y0 + ph * 0.1f);
        p.lineTo(x0 + pw * 0.25f,  y1 - ph * 0.05f);
        p.lineTo(x0 + pw * 0.38f,  y0);
        p.lineTo(x0 + pw * 0.52f,  y1);
        p.lineTo(x0 + pw * 0.65f,  my - ph * 0.2f);
        p.lineTo(x0 + pw * 0.78f,  y0 + ph * 0.3f);
        p.lineTo(x1,               my);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_percosc")
    {
        // Exponentially decaying oscillation (percussion hit)
        p.startNewSubPath(x0,              my);
        p.lineTo(x0 + pw * 0.08f,  y0);
        p.lineTo(x0 + pw * 0.18f,  my);
        p.lineTo(x0 + pw * 0.28f,  y1 + ph * 0.1f);
        p.lineTo(x0 + pw * 0.38f,  my);
        p.lineTo(x0 + pw * 0.46f,  y0 + ph * 0.35f);
        p.lineTo(x0 + pw * 0.54f,  my);
        p.lineTo(x0 + pw * 0.61f,  y1 + ph * 0.35f);
        p.lineTo(x0 + pw * 0.68f,  my);
        p.lineTo(x1,               my);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_formant")
    {
        // Complex formant wave: 2 harmonics with envelope
        for (int i = 0; i <= 32; ++i)
        {
            float t   = static_cast<float>(i) / 32.0f;
            float env = 0.55f + 0.45f * std::sin(t * juce::MathConstants<float>::pi);
            float val = (std::sin(t * juce::MathConstants<float>::twoPi * 2.0f) * 0.6f
                       + std::sin(t * juce::MathConstants<float>::twoPi * 5.0f) * 0.4f) * env;
            float px  = x0 + pw * t;
            float py  = my - val * ph * 0.5f;
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_spectral")
    {
        // Spectral partial bars: 4 bars of decreasing height (1/n)
        constexpr int nBars = 4;
        float spacing = pw / static_cast<float>(nBars);
        float barW    = spacing * 0.55f;
        for (int i = 0; i < nBars; ++i)
        {
            float h   = ph * (1.0f / static_cast<float>(i + 1));
            float bx2 = x0 + static_cast<float>(i) * spacing + (spacing - barW) * 0.5f;
            p.addRectangle(bx2, y1 - h, barW, h);
        }
        g.fillPath(p);
    }
    else if (iconName == "decorator_rndgen")
    {
        // Smooth S&H random wave (curved staircase)
        p.startNewSubPath(x0,               my - ph * 0.15f);
        p.lineTo         (x0 + pw * 0.22f,  my - ph * 0.15f);
        p.quadraticTo    (x0 + pw * 0.26f,  my + ph * 0.35f,  x0 + pw * 0.30f, my + ph * 0.35f);
        p.lineTo         (x0 + pw * 0.52f,  my + ph * 0.35f);
        p.quadraticTo    (x0 + pw * 0.56f,  y0,               x0 + pw * 0.60f, y0);
        p.lineTo         (x0 + pw * 0.80f,  y0);
        p.quadraticTo    (x0 + pw * 0.84f,  my - ph * 0.1f,   x1,              my - ph * 0.1f);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "decorator_rndgen_diskret")
    {
        // Discrete stepped random (staircase / sample-hold)
        p.startNewSubPath(x0,               my);
        p.lineTo         (x0 + pw * 0.25f,  my);
        p.lineTo         (x0 + pw * 0.25f,  y0 + ph * 0.2f);
        p.lineTo         (x0 + pw * 0.50f,  y0 + ph * 0.2f);
        p.lineTo         (x0 + pw * 0.50f,  y1);
        p.lineTo         (x0 + pw * 0.75f,  y1);
        p.lineTo         (x0 + pw * 0.75f,  my - ph * 0.2f);
        p.lineTo         (x1,               my - ph * 0.2f);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "decorator.rndgen.logic")
    {
        // Random pulse train (gate pulses of varying height)
        float baseline = y1;
        p.addRectangle(x0,               baseline - ph * 0.5f,  pw * 0.18f, ph * 0.5f);
        p.addRectangle(x0 + pw * 0.28f,  baseline - ph * 0.85f, pw * 0.18f, ph * 0.85f);
        p.addRectangle(x0 + pw * 0.56f,  baseline - ph * 0.3f,  pw * 0.18f, ph * 0.3f);
        p.addRectangle(x0 + pw * 0.80f,  baseline - ph * 0.65f, pw * 0.18f, ph * 0.65f);
        g.fillPath(p);
    }
    else if (iconName == "icon_drum")
    {
        // Drum body (ellipse) + drumstick
        float dw = iw * 0.62f, dh = ih * 0.42f;
        float dx = ix + (iw - dw) * 0.5f - iw * 0.06f;
        float dy = iy + ih * 0.42f;
        g.drawEllipse(dx, dy, dw, dh, 1.0f);
        juce::Path stick;
        float sx0 = dx + dw * 0.70f, sy0 = iy + ih * 0.05f;
        float sx1 = dx + dw * 1.00f, sy1 = iy + ih * 0.44f;
        stick.startNewSubPath(sx0, sy0);
        stick.lineTo(sx1, sy1);
        g.strokePath(stick, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse(sx0 - 1.5f, sy0 - 1.5f, 3.0f, 3.0f);
    }
    else if (iconName == "lev_shift_up" || iconName == "lev_shift_down"
          || iconName == "lev_shift_noshift")
    {
        // Tiny "~" mark with an underscore beneath — GainControl shift button.
        // "up": tilde above the line, "down": tilde below, "noshift": flat line.
        float lineY = iy + ih * 0.72f;
        g.drawLine(ix + iw * 0.20f, lineY, ix + iw * 0.80f, lineY, 1.0f);
        if (iconName != "lev_shift_noshift")
        {
            juce::Path tilde;
            float baseY = iy + ih * (iconName == "lev_shift_up" ? 0.38f : 0.50f);
            float amp   = ih * (iconName == "lev_shift_up" ? 0.20f : -0.20f);
            tilde.startNewSubPath(ix + iw * 0.20f, baseY);
            tilde.quadraticTo(ix + iw * 0.35f, baseY - amp,
                              ix + iw * 0.50f, baseY);
            tilde.quadraticTo(ix + iw * 0.65f, baseY + amp,
                              ix + iw * 0.80f, baseY);
            g.strokePath(tilde, juce::PathStrokeType(1.0f));
        }
    }
    else if (iconName == "env_log")
    {
        // ADSR attack shape — logarithmic: fast rise then flatten (concave up)
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 24; ++i)
        {
            float t = static_cast<float>(i) / 24.0f;
            float e = 1.0f - std::pow(1.0f - t, 2.5f);   // log-like (fast start, slow end)
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "env_lin")
    {
        // ADSR attack shape — linear: straight diagonal ramp
        p.startNewSubPath(x0, y1);
        p.lineTo(x1, y0);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "env_exp")
    {
        // ADSR attack shape — exponential: slow start then fast rise (concave down)
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 24; ++i)
        {
            float t = static_cast<float>(i) / 24.0f;
            float e = std::pow(t, 2.5f);   // exp-like (slow start, fast end)
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "decoration-15")
    {
        // VCA triangle symbol (▷): audio in → amplifier triangle → audio out
        float midY = iy + ih * 0.5f;
        float leftX = ix + iw * 0.15f;
        float tipX  = ix + iw * 0.85f;
        float topY  = iy + ih * 0.1f;
        float botY  = iy + ih * 0.9f;
        juce::Path tri;
        tri.startNewSubPath(leftX, topY);
        tri.lineTo(leftX, botY);
        tri.lineTo(tipX, midY);
        tri.closeSubPath();
        g.strokePath(tri, juce::PathStrokeType(1.0f));
        // Line extends left to reach the connector circle
        g.drawLine(ix - iw * 0.5f, midY, leftX, midY, 1.0f);
    }
    else if (iconName == "decoration-6")
    {
        // Box/processor symbol (□) with entry/exit lines reaching connectors
        float midY = iy + ih * 0.5f;
        float bx = ix + iw * 0.1f;
        float by = iy + ih * 0.15f;
        float bw = iw * 0.8f;
        float bh = ih * 0.7f;
        // Left entry line extending to reach the left connector circle
        g.drawLine(ix - iw * 0.5f, midY, bx, midY, 1.0f);
        // Box
        g.drawRect(bx, by, bw, bh, 1.0f);
        // Right exit line to reach the right connector circle
        g.drawLine(bx + bw, midY, ix + iw + iw * 0.3f, midY, 1.0f);
    }
    else if (iconName == "decoration-7")
    {
        // Fade curves: two parentheses "( )" — audio fade/routing pair
        float midY = iy + ih * 0.5f;
        juce::Path left, right;
        float r  = ih * 0.45f;
        float lx = ix + iw * 0.25f;
        float rx = ix + iw * 0.75f;
        left.startNewSubPath (lx + r * 0.4f, midY - r);
        left.quadraticTo     (lx - r * 0.5f, midY,  lx + r * 0.4f, midY + r);
        right.startNewSubPath(rx - r * 0.4f, midY - r);
        right.quadraticTo    (rx + r * 0.5f, midY,  rx - r * 0.4f, midY + r);
        g.strokePath(left,  juce::PathStrokeType(1.0f));
        g.strokePath(right, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "decoration-8")
    {
        // 1to2Fade: long input line → small triangle → short split
        float midY = iy + ih * 0.5f;
        float triX = ix + iw * 0.72f;
        float triH = ih * 0.55f;
        g.drawLine(ix, midY, triX, midY, 1.0f);
        juce::Path tri;
        tri.startNewSubPath(triX, midY - triH * 0.5f);
        tri.lineTo(triX, midY + triH * 0.5f);
        tri.lineTo(triX + iw * 0.12f, midY);
        tri.closeSubPath();
        g.strokePath(tri, juce::PathStrokeType(1.0f));
        g.drawLine(triX + iw * 0.12f, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "decoration-9")
    {
        // 2to1Fade: short input → small triangle → long output line
        float midY = iy + ih * 0.5f;
        float triX = ix + iw * 0.18f;
        float triH = ih * 0.55f;
        g.drawLine(ix, midY, triX, midY, 1.0f);
        juce::Path tri;
        tri.startNewSubPath(triX, midY - triH * 0.5f);
        tri.lineTo(triX, midY + triH * 0.5f);
        tri.lineTo(triX + iw * 0.10f, midY);
        tri.closeSubPath();
        g.strokePath(tri, juce::PathStrokeType(1.0f));
        g.drawLine(triX + iw * 0.10f, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "decoration-11")
    {
        // LevAdd: line → sum circle (⊕) → line
        float midY = iy + ih * 0.5f;
        float r    = juce::jmin(ih * 0.40f, iw * 0.10f);
        float cx   = ix + iw * 0.60f;
        g.drawLine(ix, midY, cx - r, midY, 1.0f);
        g.drawEllipse(cx - r, midY - r, r * 2.0f, r * 2.0f, 1.0f);
        // plus sign inside
        g.drawLine(cx - r * 0.55f, midY, cx + r * 0.55f, midY, 1.0f);
        g.drawLine(cx, midY - r * 0.55f, cx, midY + r * 0.55f, 1.0f);
        g.drawLine(cx + r, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "decoration-12")
    {
        // LevMult / Amplifier: line → VCA triangle ▷ → short exit
        float midY = iy + ih * 0.5f;
        float triL = ix + iw * 0.60f;
        float triR = ix + iw * 0.82f;
        float triH = ih * 0.70f;
        g.drawLine(ix, midY, triL, midY, 1.0f);
        juce::Path tri;
        tri.startNewSubPath(triL, midY - triH * 0.5f);
        tri.lineTo(triL, midY + triH * 0.5f);
        tri.lineTo(triR, midY);
        tri.closeSubPath();
        g.strokePath(tri, juce::PathStrokeType(1.0f));
        g.drawLine(triR, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "decoration-13")
    {
        // X-Fade: horizontal line with vertical crossbar at end
        float midY = iy + ih * 0.5f;
        g.drawLine(ix, midY, ix + iw * 0.80f, midY, 1.0f);
        g.drawLine(ix + iw * 0.80f, iy + ih * 0.15f,
                   ix + iw * 0.80f, iy + ih * 0.85f, 1.0f);
    }
    else if (iconName == "decoration-14")
    {
        // OnOff: horizontal line that steps down, break, then step up again
        float midY = iy + ih * 0.5f;
        float dipY = iy + ih * 0.85f;
        float x1v = ix + iw * 0.72f;
        float x2v = ix + iw * 0.82f;
        g.drawLine(ix, midY, x1v, midY, 1.0f);
        g.drawLine(x1v, midY, x1v, dipY, 1.0f);
        g.drawLine(x2v, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "decoration-17")
    {
        // Pan curve bump (small arch)
        juce::Path arch;
        float y1v = iy + ih * 0.85f;
        arch.startNewSubPath(ix + iw * 0.1f, y1v);
        arch.quadraticTo(ix + iw * 0.5f, iy + ih * 0.05f,
                         ix + iw * 0.9f, y1v);
        g.strokePath(arch, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "decoration-18")
    {
        // Pan: horizontal line with vertical bars at both ends (stereo routing)
        float midY = iy + ih * 0.5f;
        float x0v = ix + iw * 0.15f;
        float x1v = ix + iw * 0.85f;
        g.drawLine(x0v, midY, x1v, midY, 1.0f);
        g.drawLine(x0v, iy + ih * 0.15f, x0v, iy + ih * 0.85f, 1.0f);
        g.drawLine(x1v, iy + ih * 0.15f, x1v, iy + ih * 0.85f, 1.0f);
    }
    else if (iconName == "decoration-2")
    {
        // GainControl: long input → small step box → line → VCA triangle
        float midY = iy + ih * 0.5f;
        float bx = ix + iw * 0.30f;
        float by = iy + ih * 0.30f;
        float bw = iw * 0.10f;
        float bh = ih * 0.40f;
        g.drawLine(ix, midY, bx, midY, 1.0f);
        g.drawRect(bx, by, bw, bh, 1.0f);
        float triL = ix + iw * 0.70f;
        float triR = ix + iw * 0.90f;
        float triH = ih * 0.70f;
        g.drawLine(bx + bw, midY, triL, midY, 1.0f);
        juce::Path tri;
        tri.startNewSubPath(triL, midY - triH * 0.5f);
        tri.lineTo(triL, midY + triH * 0.5f);
        tri.lineTo(triR, midY);
        tri.closeSubPath();
        g.strokePath(tri, juce::PathStrokeType(1.0f));
        g.drawLine(triR, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "env_multi_bipolar")
    {
        // Bipolar envelope: ramp up above midline, ramp down below, return to mid
        // Represents +/- (both positive and negative excursions)
        float mid = my;
        float amp  = ph * 0.42f;
        p.startNewSubPath(x0, mid);
        p.lineTo(x0 + pw * 0.25f, mid - amp);   // rise to positive peak
        p.lineTo(x0 + pw * 0.50f, mid);          // back to zero
        p.lineTo(x0 + pw * 0.75f, mid + amp);   // fall to negative peak
        p.lineTo(x1,              mid);           // return to zero
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "env_multi_uni-exp")
    {
        // Unipolar exponential: attack ramp with exponential curve (convex up)
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 24; ++i)
        {
            float t = static_cast<float>(i) / 24.0f;
            float e = 1.0f - std::exp(-4.0f * t);   // concave exponential rise
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "env_multi_uni-lin")
    {
        // Unipolar linear: straight diagonal ramp from bottom-left to top-right
        p.startNewSubPath(x0, y1);
        p.lineTo(x1, y0);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "ds-2-1")
    {
        // Positive Edge Delay (low -> high with up arrow)
        p.startNewSubPath(x0, y1);
        p.lineTo(x0 + pw * 0.5f, y1);
        p.lineTo(x0 + pw * 0.5f, y0);
        p.lineTo(x1, y0);
        p.startNewSubPath(x0 + pw * 0.5f - 2.5f, my + 1.5f);
        p.lineTo(x0 + pw * 0.5f, my - 2.5f);
        p.lineTo(x0 + pw * 0.5f + 2.5f, my + 1.5f);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "ds-2-3")
    {
        // Negative Edge Delay (high -> low with down arrow)
        p.startNewSubPath(x0, y0);
        p.lineTo(x0 + pw * 0.5f, y0);
        p.lineTo(x0 + pw * 0.5f, y1);
        p.lineTo(x1, y1);
        p.startNewSubPath(x0 + pw * 0.5f - 2.5f, my - 1.5f);
        p.lineTo(x0 + pw * 0.5f, my + 2.5f);
        p.lineTo(x0 + pw * 0.5f + 2.5f, my - 1.5f);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "ds-2-2")
    {
        // Logic Delay (wide positive pulse)
        p.startNewSubPath(x0, y1);
        p.lineTo(x0 + pw * 0.2f, y1);
        p.lineTo(x0 + pw * 0.2f, y0);
        p.lineTo(x0 + pw * 0.8f, y0);
        p.lineTo(x0 + pw * 0.8f, y1);
        p.lineTo(x1, y1);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "ds-2-4")
    {
        // Pulse (narrow positive pulse)
        p.startNewSubPath(x0, y1);
        p.lineTo(x0 + pw * 0.35f, y1);
        p.lineTo(x0 + pw * 0.35f, y0);
        p.lineTo(x0 + pw * 0.65f, y0);
        p.lineTo(x0 + pw * 0.65f, y1);
        p.lineTo(x1, y1);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "ds-2-6")
    {
        // LogicInv input (positive pulse)
        p.startNewSubPath(x0, y1);
        p.lineTo(x0 + pw * 0.25f, y1);
        p.lineTo(x0 + pw * 0.25f, y0);
        p.lineTo(x0 + pw * 0.75f, y0);
        p.lineTo(x0 + pw * 0.75f, y1);
        p.lineTo(x1, y1);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "ds-2-5")
    {
        // LogicInv output (negative pulse)
        p.startNewSubPath(x0, y0);
        p.lineTo(x0 + pw * 0.25f, y0);
        p.lineTo(x0 + pw * 0.25f, y1);
        p.lineTo(x0 + pw * 0.75f, y1);
        p.lineTo(x0 + pw * 0.75f, y0);
        p.lineTo(x1, y0);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "diode_half")
    {
        // Half-wave rectifier: positive half of sine, negative half at zero
        // Flat line at bottom, then single positive arch
        float base = y1 - ph * 0.1f;
        g.drawLine(x0, base, x0 + pw * 0.2f, base, 1.0f);
        juce::Path arch;
        arch.startNewSubPath(x0 + pw * 0.2f, base);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float s = std::sin(t * juce::MathConstants<float>::pi);
            arch.lineTo(x0 + pw * (0.2f + t * 0.6f), base - ph * 0.75f * s);
        }
        arch.lineTo(x1, base);
        g.strokePath(arch, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "diode_full")
    {
        // Full-wave rectifier: both halves of sine folded positive (two arches)
        float base = y1 - ph * 0.1f;
        juce::Path arches;
        arches.startNewSubPath(x0, base);
        for (int i = 1; i <= 30; ++i)
        {
            float t = static_cast<float>(i) / 30.0f;
            float s = std::abs(std::sin(t * 2.0f * juce::MathConstants<float>::pi));
            arches.lineTo(x0 + pw * t, base - ph * 0.75f * s);
        }
        g.strokePath(arches, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "shaper_lin")
    {
        // Linear transfer: straight diagonal from bottom-left to top-right
        p.startNewSubPath(x0, y1);
        p.lineTo(x1, y0);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "shaper_log1")
    {
        // Logarithmic shaper (gentle): starts steep, then flattens
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float e = std::sqrt(t);
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "shaper_log2")
    {
        // Logarithmic shaper (steep): more aggressive log curve
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float e = std::pow(t, 0.35f);
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "shaper_exp1")
    {
        // Exponential shaper (gentle): starts flat, then rises quickly
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float e = t * t;
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "shaper_exp2")
    {
        // Exponential shaper (steep): even more aggressive exponential curve
        p.startNewSubPath(x0, y1);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float e = std::pow(t, 3.0f);
            p.lineTo(x0 + pw * t, y1 - ph * e);
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "decoration-1")
    {
        // Sample-and-hold circuit symbol (68×16):
        // Step signal (waveform going up then flat) + capacitor/hold box
        float midY = iy + ih * 0.55f;
        float stepY = iy + ih * 0.15f;
        float boxX = ix + iw * 0.72f;
        float boxW = iw * 0.22f;
        float boxH = ih * 0.65f;
        // Waveform: flat → step up → flat
        g.drawLine(ix,                 midY, ix + iw * 0.28f, midY,  1.0f);
        g.drawLine(ix + iw * 0.28f,    midY, ix + iw * 0.28f, stepY, 1.0f);
        g.drawLine(ix + iw * 0.28f,    stepY, ix + iw * 0.56f, stepY, 1.0f);
        // Arrow to box
        g.drawLine(ix + iw * 0.56f, stepY, boxX - 1.0f, stepY, 1.0f);
        // Hold box (capacitor symbol)
        g.drawRect(boxX, iy + (ih - boxH) * 0.5f, boxW, boxH, 1.0f);
    }
    else if (iconName == "decoration-3")
    {
        // Ring modulator symbol (25×22):
        // Circle with × inside, arrow from top, line to right
        float cx = ix + iw * 0.50f;
        float cy = iy + ih * 0.75f;
        float r  = ih * 0.22f;
        // Circle
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        // × inside circle
        float d = r * 0.55f;
        g.drawLine(cx - d, cy - d, cx + d, cy + d, 1.0f);
        g.drawLine(cx - d, cy + d, cx + d, cy - d, 1.0f);
        // Arrow from top (modulation input): vertical line + arrowhead pointing down
        float arrowTop = iy;
        float arrowBot = cy - r;
        g.drawLine(cx, arrowTop + 2.0f, cx, arrowBot, 1.0f);
        juce::Path ah;
        ah.startNewSubPath(cx - 2.5f, cy - r - 3.5f);
        ah.lineTo(cx, cy - r);
        ah.lineTo(cx + 2.5f, cy - r - 3.5f);
        g.strokePath(ah, juce::PathStrokeType(1.0f));
        // Arrow from left (audio input): arrow pointing right
        float arrowL = ix;
        float arrowR = cx - r;
        juce::Path la;
        la.startNewSubPath(arrowL, cy);
        la.lineTo(arrowR, cy);
        la.lineTo(arrowR - 3.0f, cy - 2.5f);
        la.startNewSubPath(arrowR, cy);
        la.lineTo(arrowR - 3.0f, cy + 2.5f);
        g.strokePath(la, juce::PathStrokeType(1.0f));
        // Output line to right
        g.drawLine(cx + r, cy, ix + iw, cy, 1.0f);
    }
    else if (iconName == "decoration-5")
    {
        // Delay buffer symbol (25×11): small square box with line in/out
        float midY = iy + ih * 0.5f;
        float bx = ix + iw * 0.25f;
        float bw = iw * 0.50f;
        float bh = ih * 0.70f;
        g.drawLine(ix, midY, bx, midY, 1.0f);
        g.drawRect(bx, iy + (ih - bh) * 0.5f, bw, bh, 1.0f);
        g.drawLine(bx + bw, midY, ix + iw, midY, 1.0f);
    }
    else if (iconName == "ds-2-8")
    {
        // 6dB low-pass filter curve: flat passband, then rolls off
        // Matches FilterA (6dB LPF): flat left, gentle curve down to right
        p.startNewSubPath(x0, my - ph * 0.45f);
        p.lineTo(x0 + pw * 0.40f, my - ph * 0.45f);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float fval = -ph * 0.45f + ph * 0.9f * (1.0f - std::exp(-3.5f * t));
            p.lineTo(x0 + pw * (0.40f + 0.60f * t), my + fval);
        }
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "ds-2-7")
    {
        // 6dB high-pass filter curve: rolls off at low freqs, flat passband
        // Matches FilterB (6dB HPF): curve up from bottom-left, flat on right
        p.startNewSubPath(x0, my + ph * 0.45f);
        for (int i = 1; i <= 20; ++i)
        {
            float t = static_cast<float>(i) / 20.0f;
            float fval = ph * 0.45f - ph * 0.9f * (1.0f - std::exp(-3.5f * t));
            p.lineTo(x0 + pw * (0.60f * t), my + fval);
        }
        p.lineTo(x1, my - ph * 0.45f);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "vocoder_emp")
    {
        // High-freq emphasis filter curve: flat at low end, rises at high freqs (shelving boost)
        p.startNewSubPath(x0, my + ph * 0.15f);
        p.lineTo(x0 + pw * 0.45f, my + ph * 0.15f);
        for (int i = 1; i <= 16; ++i)
        {
            float t  = static_cast<float>(i) / 16.0f;
            float yv = ph * 0.15f - ph * 0.6f * (1.0f - std::exp(-4.0f * t));
            p.lineTo(x0 + pw * (0.45f + 0.55f * t), my + yv);
        }
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
    else if (iconName == "loop_off" || iconName == "loop_on")
    {
        // Two-arrow recycle/loop: top arc going right, bottom arc going left,
        // each ending in a small arrowhead.
        bool on = (iconName == "loop_on");
        juce::Colour c = on ? juce::Colour(0xff7adf7a) : colour.withMultipliedAlpha(0.7f);
        g.setColour(c);

        float r  = juce::jmin(pw, ph) * 0.42f;
        float th = juce::jmax(1.0f, r * 0.25f);
        float gap = juce::degreesToRadians(35.0f);

        // Top arc: from left-top going clockwise to right-top, leaving a gap on the right.
        juce::Path topArc;
        topArc.addCentredArc(mx, my, r, r, 0.0f,
                             -juce::MathConstants<float>::halfPi - juce::MathConstants<float>::halfPi + gap,
                             juce::MathConstants<float>::halfPi - gap,
                             true);
        g.strokePath(topArc, juce::PathStrokeType(th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Bottom arc: from right-bottom going clockwise (i.e. leftward) to left-bottom.
        juce::Path botArc;
        botArc.addCentredArc(mx, my, r, r, 0.0f,
                             juce::MathConstants<float>::halfPi + gap,
                             juce::MathConstants<float>::pi + juce::MathConstants<float>::halfPi - gap,
                             true);
        g.strokePath(botArc, juce::PathStrokeType(th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Top arrowhead: at the gap on the right side, pointing right.
        float ang = juce::MathConstants<float>::halfPi - gap;
        float ax = mx + std::sin(ang) * r;
        float ay = my - std::cos(ang) * r;
        float headSz = r * 0.55f;
        juce::Path topHead;
        topHead.addTriangle(ax + headSz * 0.9f,  ay,
                            ax - headSz * 0.2f,  ay - headSz * 0.7f,
                            ax - headSz * 0.2f,  ay + headSz * 0.3f);
        g.fillPath(topHead);

        // Bottom arrowhead: at the gap on the left side, pointing left.
        ang = juce::MathConstants<float>::pi + juce::MathConstants<float>::halfPi - gap;
        float bx2 = mx + std::sin(ang) * r;
        float by2 = my - std::cos(ang) * r;
        juce::Path botHead;
        botHead.addTriangle(bx2 - headSz * 0.9f, by2,
                            bx2 + headSz * 0.2f, by2 + headSz * 0.7f,
                            bx2 + headSz * 0.2f, by2 - headSz * 0.3f);
        g.fillPath(botHead);
    }
    else if (iconName == "rec_off" || iconName == "rec_on")
    {
        // Record dot
        bool on = (iconName == "rec_on");
        juce::Colour c = on ? juce::Colour(0xffd64545) : colour.withMultipliedAlpha(0.6f);
        g.setColour(c);
        float r = juce::jmin(pw, ph) * 0.55f;
        g.fillEllipse(mx - r * 0.5f, my - r * 0.5f, r, r);
        if (!on)
        {
            g.setColour(colour);
            g.drawEllipse(mx - r * 0.5f, my - r * 0.5f, r, r, 0.8f);
        }
    }
    else if (iconName == "start")
    {
        // Play triangle pointing right
        juce::Path tri;
        float w = pw * 0.55f;
        float h = ph * 0.7f;
        tri.addTriangle(mx - w * 0.4f, my - h * 0.5f,
                        mx - w * 0.4f, my + h * 0.5f,
                        mx + w * 0.6f, my);
        g.fillPath(tri);
    }
    else if (iconName == "stop")
    {
        // Stop square
        float s = juce::jmin(pw, ph) * 0.55f;
        g.fillRect(mx - s * 0.5f, my - s * 0.5f, s, s);
    }
    else
    {
        g.drawEllipse(ix + iw * 0.1f, iy + ih * 0.1f, iw * 0.8f, ih * 0.8f, 1.0f);
    }
}

void PatchCanvas::paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, juce::Colour moduleBg)
{
    for (auto& tb : theme.buttons)
    {
        float bx = static_cast<float>(bounds.getX() + tb.x);
        float by = static_cast<float>(bounds.getY() + tb.y);
        float bw = static_cast<float>(tb.width);
        float bh = static_cast<float>(tb.height);

        auto* param = findParameter(m, tb.componentId);
        int val = (param != nullptr) ? param->getValue() : 0;

        // --- Increment buttons: draw arrow pairs ---
        if (tb.isIncrement)
        {
            g.setColour(activeScheme_.incrementBg);
            g.fillRect(bx, by, bw, bh);
            g.setColour(activeScheme_.incrementBorder);
            g.drawRect(bx, by, bw, bh, 1.0f);

            g.setColour(activeScheme_.incrementFg);
            float cx = bx + bw * 0.5f;
            float cy = by + bh * 0.5f;

            if (tb.landscape)
            {
                float half = bw * 0.5f;
                // Left arrow
                juce::Path leftArr;
                leftArr.addTriangle(bx + half * 0.25f, cy,
                                    bx + half * 0.75f, cy - bh * 0.3f,
                                    bx + half * 0.75f, cy + bh * 0.3f);
                g.fillPath(leftArr);
                // Right arrow
                juce::Path rightArr;
                rightArr.addTriangle(bx + half + half * 0.75f, cy,
                                     bx + half + half * 0.25f, cy - bh * 0.3f,
                                     bx + half + half * 0.25f, cy + bh * 0.3f);
                g.fillPath(rightArr);
            }
            else
            {
                // Divide into 3 bands: top arrow / value / bottom arrow
                float arrowH = bh * 0.32f;
                float valH   = bh * 0.36f;
                float topMid = by + arrowH * 0.5f;
                float botMid = by + bh - arrowH * 0.5f;

                // Up arrow (top third)
                juce::Path upArr;
                upArr.addTriangle(cx, by + arrowH * 0.15f,
                                  cx - bw * 0.3f, by + arrowH * 0.85f,
                                  cx + bw * 0.3f, by + arrowH * 0.85f);
                g.fillPath(upArr);
                juce::ignoreUnused(topMid, valH);

                // Current value (middle band). Prefer labels[val] if provided
                // (e.g. Multi-Env sustain: "--", "L1"..."L4"); else fall back to numeric.
                if (param != nullptr)
                {
                    juce::String valStr;
                    if (val >= 0 && val < static_cast<int>(tb.labels.size())
                        && tb.labels[static_cast<size_t>(val)].isNotEmpty())
                        valStr = tb.labels[static_cast<size_t>(val)];
                    else
                        valStr = juce::String(val);

                    g.setColour(activeScheme_.moduleText);
                    g.setFont(juce::Font(juce::FontOptions().withHeight(7.5f)));
                    g.drawText(valStr,
                               static_cast<int>(bx), static_cast<int>(by + arrowH),
                               static_cast<int>(bw), static_cast<int>(valH),
                               juce::Justification::centred, false);
                    g.setColour(activeScheme_.incrementFg);
                }

                // Down arrow (bottom third)
                juce::Path downArr;
                downArr.addTriangle(cx, by + bh - arrowH * 0.15f,
                                    cx - bw * 0.3f, by + bh - arrowH * 0.85f,
                                    cx + bw * 0.3f, by + bh - arrowH * 0.85f);
                g.fillPath(downArr);
                juce::ignoreUnused(botMid);
            }
            continue;
        }

        int numOptions = static_cast<int>(tb.labels.size());

        // Helper: draw a single bevel-effect button segment
        // pressed=true → sunken look; false → raised look
        auto drawBevelSegment = [&](float sx, float sy, float sw, float sh,
                                    bool pressed, juce::Colour baseFill,
                                    const juce::String& label, juce::Colour labelColour,
                                    const juce::String& iconRef = juce::String())
        {
            // Fill
            g.setColour(baseFill);
            g.fillRect(sx, sy, sw, sh);

            // Bevel edges: raised = light top/left, dark bottom/right; pressed = inverted
            juce::Colour hiEdge  = baseFill.brighter(0.55f);
            juce::Colour loEdge  = baseFill.darker(0.55f);
            juce::Colour topLeft  = pressed ? loEdge : hiEdge;
            juce::Colour botRight = pressed ? hiEdge : loEdge;

            g.setColour(topLeft);
            g.drawLine(sx, sy, sx + sw, sy, 1.0f);
            g.drawLine(sx, sy, sx, sy + sh, 1.0f);
            g.setColour(botRight);
            g.drawLine(sx, sy + sh - 1.0f, sx + sw, sy + sh - 1.0f, 1.0f);
            g.drawLine(sx + sw - 1.0f, sy, sx + sw - 1.0f, sy + sh, 1.0f);

            float ox = pressed ? 1.0f : 0.0f;
            float oy = pressed ? 1.0f : 0.0f;

            if (iconRef.isNotEmpty())
            {
                drawButtonIcon(g, iconRef,
                               sx + ox + 1.0f, sy + oy + 1.0f,
                               sw - 2.0f, sh - 2.0f, labelColour);
            }
            else if (label.isNotEmpty())
            {
                float fontSize = juce::jmin(8.0f, juce::jmin(sw * 0.85f, sh - 2.0f));
                if (fontSize < 4.0f) fontSize = 4.0f;
                g.setColour(labelColour);
                g.setFont(juce::FontOptions("Fira Sans", fontSize, juce::Font::bold));
                g.drawText(label,
                           static_cast<int>(sx + ox), static_cast<int>(sy + oy),
                           static_cast<int>(sw), static_cast<int>(sh),
                           juce::Justification::centred, true);
            }
        };

        // --- Radio-selector buttons (cyclic=false, multiple options) ---
        if (!tb.cyclic && numOptions > 1)
        {
            for (int i = 0; i < numOptions; i++)
            {
                float segX, segY, segW, segH;

                if (tb.landscape)
                {
                    segW = bw / static_cast<float>(numOptions);
                    segH = bh;
                    segX = bx + static_cast<float>(i) * segW;
                    segY = by;
                }
                else
                {
                    // Vertical: index 0 = top by default; reversed buttons use bottom-to-top
                    segW = bw;
                    segH = bh / static_cast<float>(numOptions);
                    segX = bx;
                    int renderIdx = tb.reversed ? (numOptions - 1 - i) : i;
                    segY = by + static_cast<float>(renderIdx) * segH;
                }

                bool selected = (i == val);

                juce::String segLabel;
                if (i < static_cast<int>(tb.labels.size()))
                    segLabel = tb.labels[static_cast<size_t>(i)];

                juce::String segIcon;
                if (i < static_cast<int>(tb.imageRefs.size()))
                    segIcon = tb.imageRefs[static_cast<size_t>(i)];

                // Only show numeric fallback if no label AND no icon
                if (segLabel.isEmpty() && segIcon.isEmpty())
                    segLabel = juce::String(i);

                juce::Colour base  = selected ? moduleBg.brighter(0.25f).withSaturation(0.5f) : moduleBg.darker(0.15f);
                juce::Colour label = selected ? activeScheme_.buttonTextActive : activeScheme_.buttonText;
                drawBevelSegment(segX, segY, segW, segH, selected, base, segLabel, label, segIcon);
            }

            // Outer border
            g.setColour(activeScheme_.buttonBorder);
            g.drawRect(bx, by, bw, bh, 1.0f);
            continue;
        }

        // --- Toggle buttons (cyclic=true) or single-option ---
        bool isOn = (val > 0);

        // Pick an icon for the current state if the theme provides one
        juce::String iconRef;
        if (!tb.imageRefs.empty())
        {
            int idx = juce::jlimit(0, static_cast<int>(tb.imageRefs.size()) - 1, val);
            iconRef = tb.imageRefs[static_cast<size_t>(idx)];
        }

        // Determine label text for current state. If neither labels nor icons
        // are defined, leave blank — flat toggle buttons (e.g. EventSeq steps).
        juce::String labelText;
        if (!tb.labels.empty())
        {
            if (val >= 0 && val < numOptions)
                labelText = tb.labels[static_cast<size_t>(val)];
            else
                labelText = tb.labels[0];
        }

        // --- Mute / compact toggle: small button (<=20x20) rendered as connector-sized square ---
        if (tb.cyclic && bw <= 20.0f && bh <= 20.0f)
        {
            const float sq = 13.0f;
            float sx = bx + (bw - sq) * 0.5f;
            float sy = by + (bh - sq) * 0.5f;

            juce::Colour muteBase = isOn ? activeScheme_.muteActive : moduleBg.darker(0.2f);
            juce::Colour muteText = isOn ? juce::Colours::white : activeScheme_.buttonText;
            drawBevelSegment(sx, sy, sq, sq, isOn, muteBase, labelText, muteText, iconRef);

            g.setColour(activeScheme_.buttonBorder);
            g.drawRect(sx, sy, sq, sq, 1.0f);
            continue;
        }

        juce::Colour base      = isOn ? moduleBg.brighter(0.2f).withSaturation(0.4f) : moduleBg.darker(0.15f);
        juce::Colour labelCol  = isOn ? activeScheme_.buttonTextActive : activeScheme_.buttonText;
        drawBevelSegment(bx, by, bw, bh, isOn, base, labelText, labelCol, iconRef);

        // Outer border
        g.setColour(activeScheme_.buttonBorder);
        g.drawRect(bx, by, bw, bh, 1.0f);
    }
}

void PatchCanvas::paintSliders(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& ts : theme.sliders)
    {
        float sx = static_cast<float>(bounds.getX() + ts.x);
        float sy = static_cast<float>(bounds.getY() + ts.y);
        float sw = static_cast<float>(ts.width);
        float sh = static_cast<float>(ts.height);

        // Track background
        g.setColour(activeScheme_.resetBg);
        g.fillRect(sx, sy, sw, sh);

        // Track border
        g.setColour(activeScheme_.resetBorder);
        g.drawRect(sx, sy, sw, sh, 1.0f);

        // Get parameter value for grip position
        float normalized = 0.5f;
        auto* param = findParameter(m, ts.componentId);
        if (param != nullptr)
        {
            auto* pd = param->getDescriptor();
            int range = pd->maxValue - pd->minValue;
            if (range > 0)
                normalized = static_cast<float>(param->getValue() - pd->minValue) / static_cast<float>(range);
        }

        // Draw grip
        g.setColour(activeScheme_.resetText);
        bool vertical = (ts.orientation != "horizontal");
        if (vertical)
        {
            // Grip moves from bottom (0) to top (1)
            float gripH = 4.0f;
            float gripY = sy + sh - gripH - (sh - gripH) * normalized;
            g.fillRect(sx + 1.0f, gripY, sw - 2.0f, gripH);
        }
        else
        {
            float gripW = 4.0f;
            float gripX = sx + (sw - gripW) * normalized;
            g.fillRect(gripX, sy + 1.0f, gripW, sh - 2.0f);
        }

        // Lock indicator — small yellow dot at bottom-right corner
        if (param != nullptr && param->isLocked())
        {
            g.setColour(activeScheme_.lockBody);
            g.fillEllipse(sx + sw - 5.0f, sy + sh - 5.0f, 4.0f, 4.0f);
        }
    }
}

void PatchCanvas::paintTextDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& td : theme.textDisplays)
    {
        float dx = static_cast<float>(bounds.getX() + td.x);
        float dy = static_cast<float>(bounds.getY() + td.y);
        float dw = static_cast<float>(td.width);
        float dh = static_cast<float>(td.height);

        // Dark blue background — clip height to max 13px for compact look
        float renderH = juce::jmin(dh, 13.0f);
        float renderY = dy + (dh - renderH) * 0.5f;

        g.setColour(activeScheme_.displayBg);
        g.fillRect(dx, renderY, dw, renderH);

        // Inner-bevel border: darker on top/left, slightly lighter on bottom/right
        g.setColour(activeScheme_.displayBorder);
        g.drawLine(dx, renderY,           dx + dw, renderY,           1.0f);
        g.drawLine(dx, renderY,           dx,       renderY + renderH, 1.0f);
        g.setColour(activeScheme_.displayText);
        g.drawLine(dx,       renderY + renderH, dx + dw, renderY + renderH, 1.0f);
        g.drawLine(dx + dw,  renderY,           dx + dw, renderY + renderH, 1.0f);

        // Value text
        auto* param = findParameter(m, td.componentId);
        if (param != nullptr)
        {
            int val = param->getValue();
            juce::String displayStr;

            // Formatter: theme override (e.g. slave-osc partial ratio) wins over
            // the descriptor's formatter from modules.xml. ValueFormatters is the
            // C++ port of nmformat.js — single source of truth for value display.
            const juce::String& fmtName = td.formatterOverride.isNotEmpty()
                ? td.formatterOverride
                : param->getDescriptor()->formatter;
            displayStr = ValueFormatters::format(fmtName, val);

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(8.5f));
            g.drawText(displayStr,
                       static_cast<int>(dx), static_cast<int>(renderY),
                       static_cast<int>(dw), static_cast<int>(renderH),
                       juce::Justification::centred, true);
        }

        // Partial format: draw ◄ ► arrow buttons below the display box
        if (td.partialFormat)
        {
            float arrowY = renderY + renderH + 1.0f;
            float arrowH = 8.0f;
            float midX   = dx + dw * 0.5f;

            // Button backgrounds
            g.setColour(activeScheme_.resetBg);
            g.fillRect(dx, arrowY, dw, arrowH);
            g.setColour(activeScheme_.resetBorder);
            g.drawRect(dx, arrowY, dw, arrowH, 1.0f);
            g.drawLine(midX, arrowY, midX, arrowY + arrowH, 1.0f);

            // ◄ left arrow (decrement partial)
            float lCx = dx + dw * 0.25f, cy = arrowY + arrowH * 0.5f;
            juce::Path la;
            la.addTriangle(lCx - 3.0f, cy, lCx + 2.5f, cy - 2.5f, lCx + 2.5f, cy + 2.5f);
            g.setColour(activeScheme_.resetText);
            g.fillPath(la);

            // ► right arrow (increment partial)
            float rCx = dx + dw * 0.75f;
            juce::Path ra;
            ra.addTriangle(rCx + 3.0f, cy, rCx - 2.5f, cy - 2.5f, rCx - 2.5f, cy + 2.5f);
            g.fillPath(ra);
        }
    }
}

void PatchCanvas::paintResetButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& rb : theme.resetButtons)
    {
        auto* param = findParameter(m, rb.componentId);
        int val = (param != nullptr) ? param->getValue() : -1;
        bool atDefault = (val == rb.defaultValue);

        float rx = static_cast<float>(bounds.getX() + rb.x);
        float ry = static_cast<float>(bounds.getY() + rb.y);
        float rw = static_cast<float>(rb.width);
        float rh = static_cast<float>(rb.height);

        juce::Path tri;
        tri.addTriangle(rx, ry, rx + rw, ry, rx + rw * 0.5f, ry + rh);
        g.setColour(atDefault ? activeScheme_.resetDotOn : activeScheme_.resetDotOff);
        g.fillPath(tri);
        g.setColour(activeScheme_.connHole);
        g.strokePath(tri, juce::PathStrokeType(0.5f));
    }
}

void PatchCanvas::paintStaticIcons(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& si : theme.staticIcons)
    {
        // Decoration and filter-curve icons: drawn at exact XML position/size, no box, no scale
        if (si.iconName.startsWith("decoration-")
            || si.iconName.startsWith("ds-2-"))
        {
            float ix = static_cast<float>(bounds.getX() + si.x);
            float iy = static_cast<float>(bounds.getY() + si.y);
            float iw = static_cast<float>(si.width);
            float ih = static_cast<float>(si.height);

            // Prefer the embedded PNG for pixel-perfect fidelity; fall back to
            // the procedural drawer for icons we don't have a bitmap for.
            if (si.iconName.startsWith("decoration-"))
            {
                auto img = getTintedDecoration(si.iconName, activeScheme_.moduleText);
                if (img.isValid())
                {
                    g.drawImage(img, juce::Rectangle<float>(ix, iy, iw, ih),
                                juce::RectanglePlacement::stretchToFit);
                    continue;
                }
            }

            g.setColour(activeScheme_.moduleText);
            drawButtonIcon(g, si.iconName, ix, iy, iw, ih, activeScheme_.moduleText);
            continue;
        }

        const float scale  = 1.14f;
        const float pad    = 4.0f;
        const float margin = 2.0f;   // min gap from module edge
        float iw = static_cast<float>(si.width)  * scale;
        float ih = static_cast<float>(si.height) * scale;

        // Clamp box so there's always `margin` px gap from every module edge
        float bw = iw + pad * 2.0f;
        float bh = ih + pad * 2.0f;
        float bx = static_cast<float>(bounds.getX() + si.x) - pad - 3.0f;
        float by = static_cast<float>(bounds.getY() + si.y) - pad + 5.0f;
        bx = juce::jlimit(static_cast<float>(bounds.getX())  + margin,
                          static_cast<float>(bounds.getRight())  - bw - margin, bx);
        by = juce::jlimit(static_cast<float>(bounds.getY())  + margin,
                          static_cast<float>(bounds.getBottom()) - bh - margin, by);

        // Icon follows the (possibly shifted) box
        float ix = bx + pad;
        float iy = by + pad;

        // Semi-transparent black rounded box
        g.setColour(juce::Colour(0x66000000));
        g.fillRoundedRectangle(bx, by, bw, bh, 3.0f);
        g.setColour(juce::Colour(0x66ffffff));
        g.drawRoundedRectangle(bx, by, bw, bh, 3.0f, 1.0f);

        // Waveform icon in white
        drawButtonIcon(g, si.iconName, ix, iy, iw, ih, juce::Colours::white);
    }
}

void PatchCanvas::paintLights(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    // Compute base indices for this module's LEDs and meters in the global arrays
    int ledBase   = computeModuleLightIndex(m, section, false);
    int meterBase = computeModuleLightIndex(m, section, true);

    // Build map of meter vertical centers (for LED alignment) and meter index per component-id
    std::map<juce::String, float> meterCenterY;
    std::map<juce::String, int>   meterGlobalIdx;
    int meterCount = 0;
    for (auto& tl : theme.lights)
    {
        if (tl.type == "meter")
        {
            float cy = static_cast<float>(bounds.getY() + tl.y) + tl.height * 0.5f;
            // Only store first occurrence for center (multiple meters per channel use same id)
            if (meterCenterY.find(tl.componentId) == meterCenterY.end())
                meterCenterY[tl.componentId] = cy;
            meterGlobalIdx[tl.componentId] = meterBase + meterCount;
            ++meterCount;
        }
    }

    // Track LED index per component-id (LEDs and led-arrays share the slot space)
    std::map<juce::String, int> ledGlobalIdx;
    int ledCount = 0;
    for (auto& tl : theme.lights)
    {
        if (tl.type == "led" || tl.type == "led-array")
        {
            ledGlobalIdx[tl.componentId] = ledBase + ledCount;
            ++ledCount;
        }
    }

    for (auto& tl : theme.lights)
    {
        float lx = static_cast<float>(bounds.getX() + tl.x) + (tl.type == "led" ? 2.0f : 0.0f);
        float lw = static_cast<float>(tl.width);
        float lh = static_cast<float>(tl.height);

        if (tl.type == "led-array")
        {
            // Row of 16 small LEDs distributed across `width`. The active step
            // is read only from synth meter data, matching NOMAD's LightProcessor.
            // No local timer is used: sequencers advance only from Clk pulses.
            const int kSteps = 16;
            int rawStep = (meterBase < 128) ? globalMeterValues[meterBase] : 16;
            int activeStep = -1;
            if (rawStep >= 0 && rawStep < kSteps)
                activeStep = rawStep;

            float ly = static_cast<float>(bounds.getY() + tl.y);
            float dotSize = juce::jmin(lh, lw / static_cast<float>(kSteps) - 1.0f);
            if (dotSize < 3.0f) dotSize = 3.0f;
            float spacing = lw / static_cast<float>(kSteps);

            for (int i = 0; i < kSteps; ++i)
            {
                float dx = lx + spacing * static_cast<float>(i) + (spacing - dotSize) * 0.5f;
                float dy = ly + (lh - dotSize) * 0.5f;
                bool on = (i == activeStep);
                if (on)
                {
                    g.setColour(activeScheme_.ledOn);
                    g.fillEllipse(dx, dy, dotSize, dotSize);
                    g.setColour(activeScheme_.ledAudioOn);
                    g.drawEllipse(dx, dy, dotSize, dotSize, 0.5f);
                }
                else
                {
                    g.setColour(activeScheme_.ledOff);
                    g.fillEllipse(dx, dy, dotSize, dotSize);
                    g.setColour(activeScheme_.meterTrack);
                    g.drawEllipse(dx, dy, dotSize, dotSize, 0.5f);
                }
            }
            continue;
        }

        if (tl.type == "led")
        {
            // Vertically centre LED on its paired meter
            float ly;
            auto it = meterCenterY.find(tl.componentId);
            if (it != meterCenterY.end())
                ly = it->second - lh * 0.5f;
            else
                ly = static_cast<float>(bounds.getY() + tl.y);

            // Determine if LED is on:
            // - ledOnValue >= 0: driven by paired meter reaching that threshold
            // - otherwise: driven by globalLightValues
            bool ledOn = false;
            if (tl.ledOnValue >= 0)
            {
                int mIdx = meterGlobalIdx.count(tl.componentId) ? meterGlobalIdx[tl.componentId] : meterBase;
                int mVal = (mIdx < 128) ? globalMeterValues[mIdx] : 0;
                ledOn = (mVal >= tl.ledOnValue);
            }
            else
            {
                int ledIdx = ledGlobalIdx.count(tl.componentId) ? ledGlobalIdx[tl.componentId] : ledBase;
                ledOn = (ledIdx < 128) && (globalLightValues[ledIdx] > 0);
            }

            if (ledOn)
            {
                // On: bright red (clipping indicator)
                g.setColour(activeScheme_.ledOn);
                g.fillEllipse(lx, ly, lw, lh);
                g.setColour(activeScheme_.ledAudioOn);
                g.drawEllipse(lx, ly, lw, lh, 0.5f);
            }
            else
            {
                // Off: dark circle
                g.setColour(activeScheme_.ledOff);
                g.fillEllipse(lx, ly, lw, lh);
                g.setColour(activeScheme_.meterTrack);
                g.drawEllipse(lx, ly, lw, lh, 0.5f);
            }
        }
        else  // meter
        {
            float ly = static_cast<float>(bounds.getY() + tl.y);

            // Background
            g.setColour(activeScheme_.meterBg);
            g.fillRect(lx, ly, lw, lh);

            // Get meter value (0-127)
            int mIdx = meterGlobalIdx.count(tl.componentId) ? meterGlobalIdx[tl.componentId] : meterBase;
            int mVal = (mIdx < 128) ? globalMeterValues[mIdx] : 0;

            if (mVal > 0)
            {
                float fill = static_cast<float>(mVal) / 127.0f;
                float barW = lw * fill;

                // Colour: green → yellow → red based on level
                juce::Colour barColour;
                if (fill < 0.6f)       barColour = activeScheme_.meterLow;
                else if (fill < 0.85f) barColour = activeScheme_.meterMid;
                else                   barColour = activeScheme_.meterHigh;

                g.setColour(barColour);
                g.fillRect(lx, ly, barW, lh);
            }

            // dB scale below meter bar — only between meters (not after the last one)
            if (lw >= 60.0f && ly + lh + 12.0f < static_cast<float>(bounds.getBottom()))
            {
                const int dbMarks[] = { 0, -6, -12, -18, -24, -30 };
                g.setColour(activeScheme_.moduleText);
                g.setFont(juce::FontOptions(6.0f));
                for (int db : dbMarks)
                {
                    float t = 1.0f + db / 30.0f;   // 0 dB → t=1.0, -30 dB → t=0.0
                    float tx = lx + lw * t;
                    g.drawLine(tx, ly + lh, tx, ly + lh + 2.0f, 1.0f);
                    juce::String label = (db == 0) ? "0" : juce::String(db);
                    g.drawText(label,
                               static_cast<int>(tx) - 8, static_cast<int>(ly + lh + 2),
                               16, 8,
                               juce::Justification::centred, false);
                }
            }
        }
    }
}

void PatchCanvas::paintCustomDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& cd : theme.customDisplays)
    {
        float dx = static_cast<float>(bounds.getX() + cd.x);
        float dy = static_cast<float>(bounds.getY() + cd.y);
        float dw = static_cast<float>(cd.width);
        float dh = static_cast<float>(cd.height);

        auto type = cd.type;

        // Routing brackets are transparent overlays — no background box
        if (type == "multimode-routing")
        {
            // handled below
        }
        else
        {
            // Dark background
            g.setColour(activeScheme_.displayBgCustom);
            g.fillRect(dx, dy, dw, dh);

            // Subtle border
            g.setColour(activeScheme_.displayBorderCustom);
            g.drawRect(dx, dy, dw, dh, 0.5f);
        }

        // --- Multi-Env display ---
        if (type == "multi-env-display")
        {
            // Read levels L1-L4 (p1-p4) and times T1-T5 (p5-p9)
            // Java MultiEnvelope: level is inverted (127-val), so val=0→top, val=127→bottom
            // → normalized display level = 1.0 - val/127
            float levels[5] = { 0.0f, 0.5f, 0.5f, 0.5f, 0.5f };  // L0=start, L1-L4
            float times[5]  = { 64.0f, 64.0f, 64.0f, 64.0f, 64.0f };  // T1-T5

            for (int i = 0; i < 4; i++)
            {
                if (cd.levelIds[i].isNotEmpty())
                {
                    auto* p = findParameter(m, cd.levelIds[i]);
                    if (p) levels[i + 1] = 1.0f - static_cast<float>(p->getValue()) / 127.0f;
                }
            }
            for (int i = 0; i < 5; i++)
            {
                if (cd.timeIds[i].isNotEmpty())
                {
                    auto* p = findParameter(m, cd.timeIds[i]);
                    if (p) times[i] = static_cast<float>(p->getValue());
                }
            }

            // Sustain segment: 0=none, 1-4=hold at that level
            int sustainSeg = 0;
            if (cd.sustainComponentId.isNotEmpty())
            {
                auto* p = findParameter(m, cd.sustainComponentId);
                if (p) sustainSeg = p->getValue();  // 0=-- (no sustain), 1-4
            }

            // Curve type: 0=all-lin (bipolar), 1=rising-LOG/falling-EXP, 2=rising-LOG/falling-LIN
            int curveType = 0;
            if (cd.curveComponentId.isNotEmpty())
            {
                auto* p = findParameter(m, cd.curveComponentId);
                if (p) curveType = p->getValue();
            }

            // Start level: 0.5 for bipolar (curveType=0), 0 for unipolar
            levels[0] = (curveType == 0) ? 0.5f : 0.0f;

            float margin = 2.0f;
            float plotW  = dw - margin * 2.0f;
            float plotH  = dh - margin * 2.0f;
            float origX  = dx + margin;
            float origY  = dy + dh - margin;
            auto sX = [&](float px) { return origX + px; };
            auto sY = [&](float ny) { return origY - ny * plotH; };

            // Draw reference line for bipolar mode (at 45% height like Java)
            if (curveType == 0)
            {
                g.setColour(activeScheme_.displayCurveGreen.withAlpha(0.35f));
                float refY = dy + dh * 0.45f;
                g.drawLine(origX, refY, origX + plotW, refY, 0.5f);
            }

            // Distribute time segments proportionally
            float totalT = 0.0f;
            for (int i = 0; i < 5; i++) totalT += times[i];
            if (totalT < 1.0f) totalT = 1.0f;

            // If sustain: reserve 20% of width for hold, rest distributed among T1-T5
            float holdW = (sustainSeg > 0) ? plotW * 0.18f : 0.0f;
            float segW  = plotW - holdW;

            juce::Path env;
            float curX = 0.0f;
            env.startNewSubPath(sX(curX), sY(levels[0]));

            for (int i = 0; i < 5; i++)  // segments 1-5 (T1-T5)
            {
                float w = times[i] / totalT * segW;
                float y0 = levels[i];      // start level
                float y1 = (i < 4) ? levels[i + 1] : levels[0];  // end level (T5→back to start)
                float x0 = curX;
                float x1 = curX + w;

                // Rising = LOG, Falling = EXP or LIN based on curveType
                // curveType=0: all LIN
                if (curveType == 0 || juce::approximatelyEqual(y0, y1))
                {
                    env.lineTo(sX(x1), sY(y1));
                }
                else if (y1 > y0)
                {
                    // Rising → LOG: ctrl1=(x0, (y0+y1)/2+small), ctrl2=((x0+x1)/2, y1)
                    // Java log: ctrl1=(srcX, (srcY-destY)/2+destY), ctrl2=((destX-srcX)/2+srcX, destY)
                    // In our space (y goes up when ny increases): same formula
                    env.cubicTo(sX(x0),           sY((y0 - y1) * 0.5f + y1),
                                sX((x1 - x0) * 0.5f + x0), sY(y1),
                                sX(x1),            sY(y1));
                }
                else
                {
                    // Falling → EXP or LIN
                    if (curveType == 1)
                    {
                        // EXP: ctrl1=((x1-x0)/2+x0, y0), ctrl2=(x1, (y1-y0)/2+y0)
                        // Java exp: ctrl1=((destX-srcX)/2+srcX, srcY), ctrl2=(destX, (destY-srcY)/2+srcY)
                        env.cubicTo(sX((x1 - x0) * 0.5f + x0), sY(y0),
                                    sX(x1),                     sY((y1 - y0) * 0.5f + y0),
                                    sX(x1),                     sY(y1));
                    }
                    else
                    {
                        env.lineTo(sX(x1), sY(y1));
                    }
                }

                curX = x1;

                // Insert sustain hold after segment sustainSeg
                if (sustainSeg > 0 && (i + 1) == sustainSeg)
                {
                    curX += holdW;
                    env.lineTo(sX(curX), sY(y1));
                }
            }

            g.setColour(activeScheme_.displayCurveGreen);
            g.strokePath(env, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- Envelope displays (ADSR, AD, AHD) ---
        if (type == "adsr-envelope" || type == "adsr-mod-envelope"
            || type == "ad-envelope" || type == "ahd-envelope")
        {
            // Normalized envelope values [0..1] — matches JTEnvelopeDisplay.java
            float va = 0.3f, vd = 0.3f, vh = 0.3f, vs = 0.7f, vr = 0.3f;

            auto paramNorm = [&](const juce::String& compId) -> float {
                auto* p = findParameter(m, compId);
                if (!p) return -1.0f;
                auto* pd = p->getDescriptor();
                int range = pd->maxValue - pd->minValue;
                return (range > 0) ? static_cast<float>(p->getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;
            };

            // Explicit component IDs from theme XML
            if (cd.attackComponentId.isNotEmpty())  { float v = paramNorm(cd.attackComponentId);  if (v >= 0) va = v; }
            if (cd.decayComponentId.isNotEmpty())   { float v = paramNorm(cd.decayComponentId);   if (v >= 0) vd = v; }
            if (cd.holdComponentId.isNotEmpty())    { float v = paramNorm(cd.holdComponentId);    if (v >= 0) vh = v; }
            if (cd.sustainComponentId.isNotEmpty()) { float v = paramNorm(cd.sustainComponentId); if (v >= 0) vs = v; }
            if (cd.releaseComponentId.isNotEmpty()) { float v = paramNorm(cd.releaseComponentId); if (v >= 0) vr = v; }

            // Fallback: name heuristic (skip morph parameters)
            if (cd.attackComponentId.isEmpty() || cd.decayComponentId.isEmpty())
            {
                for (auto& p : m.getParameters())
                {
                    auto* pd = p.getDescriptor();
                    if (pd->paramClass == "morph") continue;
                    auto name = pd->name.toLowerCase();
                    int range = pd->maxValue - pd->minValue;
                    float norm = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;

                    if (cd.attackComponentId.isEmpty()  && name.contains("attack"))   va = norm;
                    if (cd.decayComponentId.isEmpty()   && name.contains("decay"))    vd = norm;
                    if (cd.holdComponentId.isEmpty()    && name.contains("hold"))     vh = norm;
                    if (cd.sustainComponentId.isEmpty() && name.contains("sustain"))  vs = norm;
                    if (cd.releaseComponentId.isEmpty() && name.contains("release"))  vr = norm;
                }
            }

            // INV button (Mod-Env p9): flip envelope vertically
            bool inverse = false;
            if (cd.inverseComponentId.isNotEmpty())
            {
                auto* p = findParameter(m, cd.inverseComponentId);
                if (p && p->getValue() != 0) inverse = true;
            }

            bool isAD  = (type == "ad-envelope");
            bool isAHD = (type == "ahd-envelope");
            bool hEnabled  = isAHD;
            bool srEnabled = !isAD && !isAHD;  // ADSR and Mod-Env have sustain+release

            // Java: segments = 2 + (hEnabled?1:0) + (srEnabled?2:0)
            float segs = 2.0f + (hEnabled ? 1.0f : 0.0f) + (srEnabled ? 2.0f : 0.0f);

            // Pixel helpers — port of JTEnvelopeDisplay AffineTransform
            float margin = 2.0f;
            float plotW  = dw - margin * 2.0f;
            float plotH  = dh - margin * 2.0f;
            float origX  = dx + margin;
            float origY  = dy + dh - margin;  // y=0 baseline

            auto sX = [&](float nx) { return origX + nx * plotW / segs; };
            // inverse flips the graph: peak becomes trough and vice versa
            auto sY = [&](float ny) {
                return inverse ? (origY - (1.0f - ny) * plotH) : (origY - ny * plotH);
            };

            juce::Path env;
            env.startNewSubPath(sX(0.0f), sY(0.0f));
            float left = 0.0f;

            // ----- Attack -----
            {
                float prevLeft = left;
                left += va;
                if (srEnabled)
                {
                    // LOG attack (configureADSR / Mod-Env)
                    // Java: curveTo((0, 0.25), (0, 1), (va, 1))
                    env.cubicTo(sX(prevLeft), sY(0.25f),
                                sX(prevLeft), sY(1.0f),
                                sX(left),     sY(1.0f));
                }
                else
                {
                    // LIN attack (configureAD / configureAHD)
                    env.lineTo(sX(left), sY(1.0f));
                }
            }

            // ----- Hold (AHD only) -----
            if (hEnabled)
            {
                left += vh;
                env.lineTo(sX(left), sY(1.0f));
            }

            // ----- Decay -----
            {
                float decayW = srEnabled ? (vd * (1.0f - vs)) : vd;
                float decayY = srEnabled ? vs : 0.0f;
                float l = left;
                left += decayW;
                // Java: curveTo((l, (1-dy)*0.5+dy), ((left-l)*0.5+l, dy), (left, dy))
                env.cubicTo(sX(l),                      sY((1.0f - decayY) * 0.5f + decayY),
                            sX((left - l) * 0.5f + l),  sY(decayY),
                            sX(left),                   sY(decayY));
            }

            // ----- Sustain hold + Release (ADSR / Mod-Env) -----
            if (srEnabled)
            {
                // Sustain filler fills remaining space to keep total = segs
                // Java: sx = 1 + (1-vd*(1-vs)) + (1-vr*vs) + (1-va) + (hEnabled?(1-vh):0)
                float sx = 1.0f + (1.0f - vd * (1.0f - vs))
                                + (1.0f - vr * vs)
                                + (1.0f - va)
                                + (hEnabled ? (1.0f - vh) : 0.0f);
                left += sx;
                env.lineTo(sX(left), sY(vs));

                // Release
                float rx = vr * vs;
                float l  = left;
                left += rx;
                // Java: curveTo((l, rx), (l, 0), (left, 0))
                env.cubicTo(sX(l),    sY(rx),
                            sX(l),    sY(0.0f),
                            sX(left), sY(0.0f));
            }

            g.setColour(activeScheme_.displayCurveGreen);
            g.strokePath(env, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- LFO Display ---
        if (type == "LFODisplay")
        {
            // Waveform shape: prefer explicit shapeComponentId, else search by name
            int waveform = (cd.fixedWaveform >= 0) ? cd.fixedWaveform : 0;
            if (cd.fixedWaveform < 0)
            {
                if (cd.shapeComponentId.isNotEmpty())
                {
                    auto* sp = findParameter(m, cd.shapeComponentId);
                    if (sp) waveform = sp->getValue();
                }
                else
                {
                    for (auto& p : m.getParameters())
                    {
                        auto name = p.getDescriptor()->name.toLowerCase();
                        if (name.contains("waveform") || name.contains("wave") || name.contains("shape"))
                        { waveform = p.getValue(); break; }
                    }
                }
            }

            // Phase offset: fmtPhase → degrees, convert to [0,1] cycle fraction
            float phaseOffset = 0.0f;
            if (cd.phaseComponentId.isNotEmpty())
            {
                auto* pp = findParameter(m, cd.phaseComponentId);
                if (pp)
                {
                    float degrees = static_cast<float>(pp->getValue()) * 2.8125f - 180.0f;
                    phaseOffset = degrees / 360.0f;
                }
            }

            // LFOB / LFOC square waveform: width param p8 drives pulse duty cycle
            float pulseWidth = 0.5f;
            if (waveform == 4)
            {
                for (auto& par : m.getParameters())
                {
                    auto desc = par.getDescriptor();
                    if (desc == nullptr) continue;
                    auto nameLc = desc->name.toLowerCase();
                    if (nameLc == "pw" || nameLc.contains("pulse") || nameLc.contains("width"))
                    {
                        int range = desc->maxValue - desc->minValue;
                        if (range > 0)
                            pulseWidth = juce::jlimit(0.05f, 0.95f,
                                0.05f + 0.90f * static_cast<float>(par.getValue() - desc->minValue)
                                              / static_cast<float>(range));
                        break;
                    }
                }
            }

            // Rate cycles: scale how many cycles are shown (LFOSlvA rate knob)
            float cycles = 1.0f;
            if (cd.rateComponentId.isNotEmpty())
            {
                auto* rp = findParameter(m, cd.rateComponentId);
                if (rp)
                {
                    auto* rpd = rp->getDescriptor();
                    int range = rpd->maxValue - rpd->minValue;
                    float norm = (range > 0) ? static_cast<float>(rp->getValue() - rpd->minValue) / static_cast<float>(range) : 0.5f;
                    // Exponential scaling: 0.25 cycles (very slow) to 8 cycles (very fast)
                    cycles = 0.25f * std::pow(32.0f, norm);
                }
            }

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;
            float midY = dy + dh * 0.5f;
            float amp = plotH * 0.45f;

            // Build wave path first, draw center line behind it
            // Center reference line behind the wave
            g.setColour(activeScheme_.displayGrid.withAlpha(0.5f));
            g.drawHorizontalLine(static_cast<int>(midY), dx + margin, dx + dw - margin);

            juce::Path wave;
            int steps = static_cast<int>(plotW);
            float prevWval = 0.0f;
            bool firstPoint = true;
            // Discontinuous waveforms (sawtooth, inv-saw) need gap detection so
            // the wrap-around doesn't draw a slanted line back to the start.
            // Square is intentionally NOT in this set: we want the vertical edge
            // between high/low to render as a visible steep line connecting the
            // two horizontal segments.
            bool isDiscontinuousWave = (waveform == 2 || waveform == 3);
            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float tp = std::fmod(t * cycles + phaseOffset + 10.0f, 1.0f);
                float px = dx + margin + t * plotW;
                float wval = 0.0f;

                switch (waveform)
                {
                    case 0: // Sine
                        wval = std::sin(tp * juce::MathConstants<float>::twoPi);
                        break;
                    case 1: // Triangle
                        wval = (tp < 0.25f) ? tp * 4.0f : (tp < 0.75f) ? 2.0f - tp * 4.0f : tp * 4.0f - 4.0f;
                        break;
                    case 2: // Sawtooth (ramp up: -1 to +1)
                        wval = 2.0f * tp - 1.0f;
                        break;
                    case 3: // Inv Sawtooth (ramp down: +1 to -1)
                        wval = 1.0f - 2.0f * tp;
                        break;
                    case 4: // Square / Pulse (duty driven by pulseWidth)
                    default:
                        wval = (tp < pulseWidth) ? 1.0f : -1.0f;
                        break;
                }

                float py = midY - wval * amp;
                // Only break the path for waveforms with real discontinuities (jump ~2.0)
                // Smooth waveforms (sine/triangle) always connect — no threshold check needed
                bool isDiscontinuity = isDiscontinuousWave && !firstPoint
                                       && std::abs(wval - prevWval) > 1.5f;
                if (firstPoint || isDiscontinuity)
                    wave.startNewSubPath(px, py);
                else
                    wave.lineTo(px, py);
                prevWval = wval;
                firstPoint = false;
            }

            g.setColour(activeScheme_.displayCurveBlue);
            g.strokePath(wave, juce::PathStrokeType(1.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            continue;
        }

        // --- NoteSeqB piano-roll display ---
        if (type == "note-seq-editor")
        {
            constexpr int kSteps = 16;

            int notes[kSteps];
            int validNotes = 0;
            int noteSum = 0;
            for (int i = 0; i < kSteps; ++i)
            {
                notes[i] = 60;
                if (cd.noteStepIds[i].isNotEmpty())
                {
                    if (auto* p = findParameter(m, cd.noteStepIds[i]))
                    {
                        notes[i] = juce::jlimit(0, 127, p->getValue());
                        noteSum += notes[i];
                        ++validNotes;
                    }
                }
            }

            int currentStep = 0;
            if (auto* p = findParameter(m, "p20"))
            {
                int rawStep = p->getValue();
                currentStep = juce::jlimit(0, kSteps - 1, rawStep > 0 ? rawStep - 1 : 0);
            }

            int stepCount = kSteps;
            if (auto* p = findParameter(m, "p19"))
                stepCount = juce::jlimit(1, kSteps, p->getValue() + 1);

            int zoom = 3;
            if (auto* p = findParameter(m, "p1"))
                zoom = juce::jlimit(1, 6, p->getValue());

            int centerNote = validNotes > 0 ? noteSum / validNotes : 60;
            if (auto* p = findParameter(m, "p2"))
            {
                auto* pd = p->getDescriptor();
                int v = p->getValue();
                if (v >= pd->minValue && v <= pd->maxValue)
                    centerNote = v;
            }

            int visibleNotes = juce::jlimit(12, 72, 72 - (zoom - 1) * 12);
            int lowNote = juce::jlimit(0, 127 - visibleNotes, centerNote - visibleNotes / 2);
            int highNote = lowNote + visibleNotes;

            auto isBlackKey = [](int midiNote)
            {
                switch (midiNote % 12)
                {
                    case 1: case 3: case 6: case 8: case 10: return true;
                    default: return false;
                }
            };

            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(static_cast<int>(dx), static_cast<int>(dy),
                                                    static_cast<int>(dw), static_cast<int>(dh)));

            const float keyW = 16.0f;
            const float rollX = dx + keyW;
            const float rollW = juce::jmax(1.0f, dw - keyW);
            const float stepW = rollW / static_cast<float>(kSteps);
            const float rowH = dh / static_cast<float>(visibleNotes + 1);

            // Piano-key strip and pitch lanes.
            g.setColour(activeScheme_.displayBg.darker(0.25f));
            g.fillRect(dx, dy, keyW, dh);
            for (int note = lowNote; note <= highNote; ++note)
            {
                float y = dy + (static_cast<float>(highNote - note) / static_cast<float>(visibleNotes)) * dh;
                bool black = isBlackKey(note);
                if (black)
                {
                    g.setColour(juce::Colours::black.withAlpha(0.28f));
                    g.fillRect(rollX, y - rowH * 0.5f, rollW, juce::jmax(1.0f, rowH));
                    g.fillRect(dx + 2.0f, y - rowH * 0.45f, keyW - 4.0f, juce::jmax(1.0f, rowH * 0.9f));
                }

                g.setColour((note % 12 == 0) ? activeScheme_.displayGrid.withAlpha(0.75f)
                                              : activeScheme_.displayGrid.withAlpha(0.25f));
                g.drawHorizontalLine(static_cast<int>(std::round(y)), rollX, dx + dw);
            }

            // Step grid, with current and disabled/out-of-range steps visible.
            for (int i = 0; i <= kSteps; ++i)
            {
                float x = rollX + static_cast<float>(i) * stepW;
                g.setColour((i % 4 == 0) ? activeScheme_.displayGrid.withAlpha(0.85f)
                                         : activeScheme_.displayGrid.withAlpha(0.35f));
                g.drawVerticalLine(static_cast<int>(std::round(x)), dy, dy + dh);
            }

            if (currentStep < stepCount)
            {
                g.setColour(activeScheme_.ledOn.withAlpha(0.16f));
                g.fillRect(rollX + stepW * static_cast<float>(currentStep), dy, stepW, dh);
            }

            if (stepCount < kSteps)
            {
                g.setColour(activeScheme_.displayBg.withAlpha(0.45f));
                g.fillRect(rollX + stepW * static_cast<float>(stepCount), dy,
                           stepW * static_cast<float>(kSteps - stepCount), dh);
            }

            // Note blocks.
            for (int i = 0; i < kSteps; ++i)
            {
                int note = notes[i];
                bool clipped = (note < lowNote || note > highNote);
                int visibleNote = juce::jlimit(lowNote, highNote, note);
                float nx = rollX + stepW * static_cast<float>(i) + 1.5f;
                float ny = dy + (static_cast<float>(highNote - visibleNote) / static_cast<float>(visibleNotes)) * dh;
                float nh = juce::jmax(3.0f, rowH * 0.72f);
                float nw = juce::jmax(4.0f, stepW - 3.0f);
                bool active = (i == currentStep && i < stepCount);

                auto noteColour = active ? activeScheme_.displayCurveYellow : activeScheme_.displayCurveGreen;
                if (i >= stepCount)
                    noteColour = noteColour.withMultipliedAlpha(0.35f);
                if (clipped)
                    noteColour = activeScheme_.displayCurveRed.withAlpha(0.75f);

                g.setColour(noteColour.withAlpha(0.80f));
                g.fillRoundedRectangle(nx, ny - nh * 0.5f, nw, nh, 1.5f);
                g.setColour(noteColour.brighter(0.35f));
                g.drawRoundedRectangle(nx, ny - nh * 0.5f, nw, nh, 1.5f, 0.7f);
            }

            g.restoreState();
            continue;
        }

        // --- Scrollbar used by note-seq-editor ---
        if (type == "scrollbar")
        {
            int zoom = 3;
            if (auto* p = findParameter(m, "p1"))
                zoom = juce::jlimit(1, 6, p->getValue());

            int sliderPos = 60;
            if (auto* p = findParameter(m, "p2"))
            {
                auto* pd = p->getDescriptor();
                int v = p->getValue();
                sliderPos = (v >= pd->minValue && v <= pd->maxValue) ? v : sliderPos;
            }

            float visibleFrac = juce::jlimit(0.15f, 0.85f, 1.0f - static_cast<float>(zoom - 1) * 0.12f);
            float thumbH = juce::jmax(10.0f, dh * visibleFrac);
            float norm = juce::jlimit(0.0f, 1.0f, (117.0f - static_cast<float>(sliderPos)) / (117.0f - 6.0f));
            float thumbY = dy + 2.0f + norm * (dh - thumbH - 4.0f);

            g.setColour(activeScheme_.displayBgCustom.darker(0.35f));
            g.fillRoundedRectangle(dx + 2.0f, dy + 2.0f, dw - 4.0f, dh - 4.0f, 2.0f);
            g.setColour(activeScheme_.displayGrid.withAlpha(0.7f));
            g.drawRoundedRectangle(dx + 2.0f, dy + 2.0f, dw - 4.0f, dh - 4.0f, 2.0f, 0.7f);
            g.setColour(activeScheme_.displayCurveBlue.withAlpha(0.85f));
            g.fillRoundedRectangle(dx + 3.0f, thumbY, dw - 6.0f, thumbH, 2.0f);
            continue;
        }

        // --- Overdrive / Clip / WaveWrap displays ---
        if (type == "overdrive-display" || type == "clip-display" || type == "wavewrap-display")
        {
            auto normP = [](const Parameter* p, float def) -> float {
                if (!p) return def;
                auto* pd = p->getDescriptor();
                int range = pd->maxValue - pd->minValue;
                return range > 0 ? (float)(p->getValue() - pd->minValue) / (float)range : def;
            };

            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(dx, dy, dw, dh));

            if (type == "overdrive-display")
            {
                // Port of Overdrive.java (angle-based bezier) + JTOverdriveDisplay.java
                float od = normP(findParameter(m, cd.overdriveComponentId), 0.5f);

                auto mpx = [&](float n) { return (float)dx + n * (float)dw; };
                auto mpy = [&](float n) { return (float)dy + n * (float)dh; };

                g.setColour(activeScheme_.displayGrid);
                g.drawLine(mpx(0.0f), mpy(1.0f), mpx(1.0f), mpy(0.0f), 0.5f);

                // Angle-based bezier control points from Overdrive.java setBezier()
                float srcX = 0.45f * od, srcY = 1.0f;
                float dstX = 1.0f - 0.45f * od, dstY = 0.0f;
                float a1 = -45.0f * (1.0f - od) * juce::MathConstants<float>::pi / 180.0f;
                float a2 = (180.0f - 45.0f * (1.0f - od)) * juce::MathConstants<float>::pi / 180.0f;
                float c1x = srcX + 0.25f * std::cos(a1);
                float c1y = srcY + 0.25f * std::sin(a1);
                float c2x = dstX + 0.25f * std::cos(a2);
                float c2y = dstY + 0.25f * std::sin(a2);

                juce::Path curve;
                curve.startNewSubPath(mpx(-0.1f), mpy(1.0f));
                curve.lineTo(mpx(srcX), mpy(srcY));
                curve.cubicTo(mpx(c1x), mpy(c1y), mpx(c2x), mpy(c2y), mpx(dstX), mpy(dstY));
                curve.lineTo(mpx(1.1f), mpy(0.0f));

                g.setColour(activeScheme_.displayCurveWarm);
                g.strokePath(curve, juce::PathStrokeType(1.2f));
            }
            else if (type == "clip-display")
            {
                // Port of ClipDisp.java
                // vclip = 1 - norm: vclip=1 → full pass-through, vclip=0 → max clipping
                float vclip = 1.0f - normP(findParameter(m, cd.clipComponentId), 0.0f);
                auto* pSym  = findParameter(m, cd.symmetryComponentId);
                bool  sym   = pSym ? (pSym->getValue() == pSym->getDescriptor()->maxValue) : true;

                float cxf  = (float)dx + dw * 0.5f;
                float cyf  = (float)dy + dh * 0.5f;
                float len  = (float)(std::min(dw, dh) - 1);
                float half = len * 0.5f;
                float delta = std::ceil(len * (1.0f - vclip) / 2.0f);

                g.setColour(activeScheme_.displayGrid);
                g.drawLine(cxf, cyf - half, cxf, cyf + half, 0.5f);
                g.drawLine(cxf - half, cyf, cxf + half, cyf, 0.5f);

                g.setColour(activeScheme_.displayCurveWarm);
                g.drawLine(cxf,         cyf,         cxf + delta, cyf - delta, 1.2f);
                g.drawLine(cxf + delta, cyf - delta, cxf + half,  cyf - delta, 1.2f);
                if (sym)
                {
                    g.drawLine(cxf,         cyf,         cxf - delta, cyf + delta, 1.2f);
                    g.drawLine(cxf - delta, cyf + delta, cxf - half,  cyf + delta, 1.2f);
                }
                else
                {
                    g.drawLine(cxf, cyf, cxf - half, cyf + half, 1.2f);
                }
            }
            else // wavewrap-display
            {
                // Port of WaveWrapDisp.java
                // Zigzag wave: div=16*vwrap+1, 9 peaks, n=-9..8
                // x(n) = (n+0.5)*len/div + cx, y(n) = fww(n)*(len/2)+cy
                // fww(n) = sin((n*2-1)*π/2) = +1 for odd n, -1 for even n
                float vwrap = normP(findParameter(m, cd.wavewrapComponentId), 0.0f);

                float cxf  = (float)dx + dw * 0.5f;
                float cyf  = (float)dy + dh * 0.5f;
                float len  = (float)(std::min(dw, dh) - 1);
                float half = len * 0.5f;
                float div  = 16.0f * vwrap + 1.0f;

                g.setColour(activeScheme_.displayGrid);
                g.drawLine(cxf, cyf - half, cxf, cyf + half, 0.5f);
                g.drawLine(cxf - half, cyf, cxf + half, cyf, 0.5f);

                const int nPeaks = 9;
                juce::Path wave;
                for (int n = -nPeaks; n < nPeaks; ++n)
                {
                    float px = (n + 0.5f) * len / div + cxf;
                    float fy = ((n & 1) == 0) ? -1.0f : 1.0f;
                    float py = fy * half + cyf;
                    if (n == -nPeaks) wave.startNewSubPath(px, py);
                    else              wave.lineTo(px, py);
                }

                g.setColour(activeScheme_.displayCurveWarm);
                g.strokePath(wave, juce::PathStrokeType(1.0f));
            }

            g.restoreState();
            continue;
        }

        // --- Filter displays ---
        if (type == "filter-e-display" || type == "filter-f-display")
        {
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(dx, dy, dw, dh));

            // Map normalised 0..1 → pixel (coords outside 0..1 are clipped)
            auto mpx = [&](float n) { return dx + n * dw; };
            auto mpy = [&](float n) { return dy + n * dh; };

            // Angle-based Bezier control points (Java FilterE/F Point.setBezier)
            // Returns {ctrl1x, ctrl1y, ctrl2x, ctrl2y}
            auto sb = [](float sx, float sy, float ex, float ey,
                         float d1, float d2, float a1_deg, float a2_deg)
                -> std::array<float,4>
            {
                const float r = juce::MathConstants<float>::pi / 180.0f;
                return { sx + d1*std::cos(a1_deg*r), sy + d1*std::sin(a1_deg*r),
                         ex + d2*std::cos(a2_deg*r), ey + d2*std::sin(a2_deg*r) };
            };

            auto normParam = [](const Parameter* p, float def) -> float {
                if (p == nullptr) return def;
                auto* pd = p->getDescriptor();
                int range = pd->maxValue - pd->minValue;
                return (range > 0) ? static_cast<float>(p->getValue() - pd->minValue)
                                     / static_cast<float>(range) : def;
            };

            const float resAmp = 0.45f;
            g.setColour(activeScheme_.displayGrid);
            g.drawHorizontalLine(static_cast<int>(mpy(resAmp)), dx, dx + dw);

            juce::Path filt;
            filt.startNewSubPath(mpx(-0.1f), mpy(1.1f)); // MOVETO (outside, will be clipped)

            if (type == "filter-f-display")
            {
                // === FilterF.java: LP-only, slopes 12/18/24 dB ===
                float cutNorm = normParam(findParameter(m, cd.cutoffComponentId), 0.5f);
                float resoNorm = normParam(findParameter(m, cd.resonanceComponentId), 0.0f);
                int   slopeVal = 0;
                if (auto* pS = findParameter(m, cd.slopeComponentId)) slopeVal = pS->getValue();

                float co        = cutNorm * 0.8f + 0.1f;          // Java: cutOff/127*0.8+0.1
                float resonance = (1.0f - resoNorm) * resAmp;     // Java: (1-reso)*resAmplitude
                float slope     = (2.0f - slopeVal) * 0.1f;       // Java: (2-slope)*0.1

                filt.lineTo(mpx(-0.1f), mpy(resAmp));
                float p2x = -0.2f + co;
                filt.lineTo(mpx(p2x), mpy(resAmp));

                float p3x = co, p3y = resonance;
                float ang3 = 110.0f + 70.0f * resonance / resAmp;
                auto  c3 = sb(p2x, resAmp, p3x, p3y, 0.1f, 0.1f, 0.0f, ang3);
                filt.cubicTo(mpx(c3[0]), mpy(c3[1]), mpx(c3[2]), mpy(c3[3]), mpx(p3x), mpy(p3y));

                float p4x = 0.3f + co + slope, p4y = 1.1f;
                float ang4 = 70.0f - 70.0f * resonance / resAmp;
                auto  c4 = sb(p3x, p3y, p4x, p4y, 0.1f, 0.1f, ang4, -110.0f);
                filt.cubicTo(mpx(c4[0]), mpy(c4[1]), mpx(c4[2]), mpy(c4[3]), mpx(p4x), mpy(p4y));

                filt.lineTo(mpx(-0.1f), mpy(1.1f));
            }
            else // filter-e-display
            {
                // === FilterE.java: LP/BP/HP/BR, slopes 12/24 dB ===
                float cutNorm  = normParam(findParameter(m, cd.cutoffComponentId), 0.5f);
                float resoNorm = normParam(findParameter(m, cd.resonanceComponentId), 0.0f);
                int   typeVal  = 0;
                if (auto* pT = findParameter(m, cd.typeComponentId))       typeVal  = pT->getValue();
                int   slopeVal = 1;
                if (auto* pS = findParameter(m, cd.slopeComponentId))      slopeVal = pS->getValue();
                int   gcVal    = 1;
                if (auto* pG = findParameter(m, cd.gainControlComponentId)) gcVal   = pG->getValue();

                float co   = cutNorm * 0.8f + 0.1f;  // Java: cutOff*0.8+0.1
                float reso = resoNorm;                // 0..1
                int   sl   = slopeVal;                // 0=12 dB, 1=24 dB
                int   gc   = gcVal;                   // 0=full, 1=half
                float gainOffset = (gc == 0) ? 1.0f : 0.5f;

                switch (typeVal)
                {
                case 2: // HP
                {
                    float p1x = co - 0.5f + sl*0.25f;
                    float p1y = 1.1f + resAmp*0.5f*reso*gc;
                    filt.lineTo(mpx(p1x), mpy(p1y));

                    float p2x = co, p2y = resAmp - reso*resAmp*gainOffset;
                    auto  c2  = sb(p1x, p1y, p2x, p2y,
                                   0.05f, 0.2f+0.1f*reso,
                                   -70.0f+40.0f*sl, 180.0f-80.0f*reso);
                    filt.cubicTo(mpx(c2[0]), mpy(c2[1]), mpx(c2[2]), mpy(c2[3]), mpx(p2x), mpy(p2y));

                    float p3x = co + 0.1f + sl*0.15f, p3y = resAmp + resAmp*0.5f*reso*gc;
                    auto  c3  = sb(p2x, p2y, p3x, p3y,
                                   0.05f*reso, 0.05f+0.05f*reso,
                                   80.0f*reso, 180.0f);
                    filt.cubicTo(mpx(c3[0]), mpy(c3[1]), mpx(c3[2]), mpy(c3[3]), mpx(p3x), mpy(p3y));

                    filt.lineTo(mpx(1.5f),  mpy(p3y));
                    filt.lineTo(mpx(1.1f),  mpy(1.1f));
                    filt.lineTo(mpx(-0.1f), mpy(1.1f));
                    break;
                }
                case 0: // LP
                {
                    float py1 = resAmp + resAmp*0.5f*reso*gc;
                    filt.lineTo(mpx(-0.3f), mpy(py1));

                    float p2x = -0.1f + co - sl*0.15f;
                    filt.lineTo(mpx(p2x), mpy(py1));

                    float p3x = co, p3y = resAmp - reso*resAmp*gainOffset;
                    auto  c3  = sb(p2x, py1, p3x, p3y,
                                   0.05f+0.05f*reso, 0.05f*reso,
                                   0.0f, 180.0f-80.0f*reso);
                    filt.cubicTo(mpx(c3[0]), mpy(c3[1]), mpx(c3[2]), mpy(c3[3]), mpx(p3x), mpy(p3y));

                    float p4x = 0.5f + co - sl*0.25f, p4y = 1.1f + resAmp*0.5f*reso*gc;
                    auto  c4  = sb(p3x, p3y, p4x, p4y,
                                   0.2f+0.1f*reso, 0.05f,
                                   (85.0f-15.0f*sl)*reso, -150.0f+40.0f*sl);
                    filt.cubicTo(mpx(c4[0]), mpy(c4[1]), mpx(c4[2]), mpy(c4[3]), mpx(p4x), mpy(p4y));

                    filt.lineTo(mpx(1.1f),  mpy(1.1f));
                    filt.lineTo(mpx(-0.1f), mpy(1.1f));
                    break;
                }
                case 1: // BP
                {
                    float leftEnd  = (sl == 0) ? -0.65f + co + 0.12f*reso : -0.45f + co + 0.06f*reso;
                    float rightEnd = (sl == 0) ?  0.65f + co - 0.12f*reso :  0.45f + co - 0.06f*reso;
                    float py1  = 1.1f + resAmp*0.5f*reso*gc;
                    filt.lineTo(mpx(-0.1f), mpy(py1));
                    filt.lineTo(mpx(leftEnd), mpy(py1));

                    float p3x = co, p3y = resAmp - reso*resAmp*gainOffset;
                    float lSO = 0.25f, lSE = (sl == 1) ? 0.25f : 0.25f+0.15f*reso;
                    float a3a = -50.0f - 40.0f*reso - 20.0f*sl;
                    float a3b = 180.0f - 80.0f*reso;
                    auto  c3  = sb(leftEnd, py1, p3x, p3y, lSO, lSE, a3a, a3b);
                    filt.cubicTo(mpx(c3[0]), mpy(c3[1]), mpx(c3[2]), mpy(c3[3]), mpx(p3x), mpy(p3y));

                    auto  c4  = sb(p3x, p3y, rightEnd, py1, lSE, lSO, 180.0f-a3b, 180.0f-a3a);
                    filt.cubicTo(mpx(c4[0]), mpy(c4[1]), mpx(c4[2]), mpy(c4[3]), mpx(rightEnd), mpy(py1));

                    filt.lineTo(mpx(1.1f),  mpy(py1));
                    filt.lineTo(mpx(1.1f),  mpy(1.1f));
                    filt.lineTo(mpx(-0.1f), mpy(1.1f));
                    break;
                }
                case 3: // BR (notch)
                {
                    float gOff3 = (gc == 0) ? 0.35f : 0.0f;
                    float py1   = resAmp + resAmp*gOff3*reso;
                    filt.lineTo(mpx(-0.5f), mpy(py1));

                    float p2x = -0.45f + (0.15f-0.1f*sl)*reso + co - sl*0.05f;
                    filt.lineTo(mpx(p2x), mpy(py1));

                    float lSO, lSE, res3;
                    if (sl == 0) { lSO = 0.3f; lSE = 0.2f; res3 = 1.0f; }
                    else         { lSO = 0.2f; lSE = 0.4f; res3 = 1.0f + resAmp*0.8f*(1.0f-reso); }

                    float p3x = co, p3y = res3;
                    auto  c3  = sb(p2x, py1, p3x, p3y, lSO, lSE, 0.0f, -90.0f);
                    filt.cubicTo(mpx(c3[0]), mpy(c3[1]), mpx(c3[2]), mpy(c3[3]), mpx(p3x), mpy(p3y));

                    float p4x = 0.45f - (0.15f-0.1f*sl)*reso + co + sl*0.05f;
                    auto  c4  = sb(p3x, p3y, p4x, py1, lSE, lSO, -90.0f, 180.0f);
                    filt.cubicTo(mpx(c4[0]), mpy(c4[1]), mpx(c4[2]), mpy(c4[3]), mpx(p4x), mpy(py1));

                    filt.lineTo(mpx(1.1f),  mpy(py1));
                    filt.lineTo(mpx(1.1f),  mpy(1.1f));
                    filt.lineTo(mpx(-0.1f), mpy(1.1f));
                    break;
                }
                default:
                    filt.lineTo(mpx(1.1f),  mpy(1.1f));
                    filt.lineTo(mpx(-0.1f), mpy(1.1f));
                    break;
                }
            }

            g.setColour(activeScheme_.displayCurveBlue);
            g.strokePath(filt, juce::PathStrokeType(1.2f));
            g.restoreState();
            continue;
        }

        // --- EQ displays ---
        if (type == "eq-mid-display" || type == "eq-shelving-display")
        {
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(dx, dy, dw, dh));

            auto mpx = [&](float n) { return dx + n * dw; };
            auto mpy = [&](float n) { return dy + n * dh; };

            auto normParam = [](const Parameter* p, float def) -> float {
                if (p == nullptr) return def;
                auto* pd = p->getDescriptor();
                int range = pd->maxValue - pd->minValue;
                return (range > 0) ? static_cast<float>(p->getValue() - pd->minValue)
                                     / static_cast<float>(range) : def;
            };

            float freq = normParam(findParameter(m, cd.freqComponentId), 0.5f);
            float gain = normParam(findParameter(m, cd.gainComponentId), 0.5f);
            float bw   = normParam(findParameter(m, cd.bwComponentId),   0.5f);

            const float baseline = 0.5f;
            g.setColour(activeScheme_.displayGrid);
            g.drawHorizontalLine(static_cast<int>(mpy(baseline)), dx, dx + dw);

            juce::Path eq;

            if (type == "eq-shelving-display")
            {
                // === EqualizerShelve.java: lo/hi shelf ===
                // typeComponentId = p3 (mode: 0=Lo, 1=Hi), added to XML
                int mode = 0;
                if (auto* pM = findParameter(m, cd.typeComponentId)) mode = pM->getValue();

                float shelfY = 1.0f - gain; // Java: setGain → Y = 1-gain
                float tx1 = freq * 0.7f;
                float tx2 = (freq + 0.2f) * 0.7f;

                eq.startNewSubPath(mpx(-0.01f), mpy(1.01f));
                if (mode == 0) // Lo-shelf: left portion at shelfY, right at baseline
                {
                    eq.lineTo(mpx(-0.01f), mpy(shelfY));
                    eq.lineTo(mpx(tx1),    mpy(shelfY));
                    eq.lineTo(mpx(tx2),    mpy(baseline));
                    eq.lineTo(mpx(1.10f),  mpy(baseline));
                    eq.lineTo(mpx(1.01f),  mpy(1.01f));
                }
                else // Hi-shelf: mirrored — left at baseline, right at shelfY
                {
                    float htx1 = 1.0f - tx2;
                    float htx2 = 1.0f - tx1;
                    eq.lineTo(mpx(-0.01f), mpy(baseline));
                    eq.lineTo(mpx(htx1),   mpy(baseline));
                    eq.lineTo(mpx(htx2),   mpy(shelfY));
                    eq.lineTo(mpx(1.01f),  mpy(shelfY));
                    eq.lineTo(mpx(1.01f),  mpy(1.01f));
                }
                eq.lineTo(mpx(-0.01f), mpy(1.10f));
            }
            else // eq-mid-display
            {
                // === EqualizerMid.java: parametric peak/cut ===
                // EXP bezier rise from baseline to peak, LOG bezier fall back to baseline.
                // Y: 0=top (boost), 0.5=flat (baseline), 1=bottom (cut)
                float p2x = freq - bw/2.0f - 0.05f;
                float p4x = freq + bw/2.0f + 0.05f;

                // EXP: ctrl1=((ex-sx)/2+sx, sy), ctrl2=(ex, (ey-sy)/2+sy)
                float eC1x = (freq-p2x)/2.0f + p2x, eC1y = baseline;
                float eC2x = freq,                    eC2y = (gain-baseline)/2.0f + baseline;
                // LOG: ctrl1=(sx, (sy-ey)/2+ey), ctrl2=((ex-sx)/2+sx, ey)
                float lC1x = freq,                    lC1y = (gain-baseline)/2.0f + baseline;
                float lC2x = (p4x-freq)/2.0f + freq, lC2y = baseline;

                eq.startNewSubPath(mpx(-0.1f),  mpy(1.1f));
                eq.lineTo(mpx(-0.1f),  mpy(baseline));
                eq.lineTo(mpx(p2x),    mpy(baseline));
                eq.cubicTo(mpx(eC1x), mpy(eC1y), mpx(eC2x), mpy(eC2y), mpx(freq), mpy(gain));
                eq.cubicTo(mpx(lC1x), mpy(lC1y), mpx(lC2x), mpy(lC2y), mpx(p4x),  mpy(baseline));
                eq.lineTo(mpx(1.1f),   mpy(baseline));
                eq.lineTo(mpx(1.1f),   mpy(1.1f));
                eq.lineTo(mpx(-0.1f),  mpy(1.1f));
            }

            g.setColour(activeScheme_.displayCurveYellow);
            g.strokePath(eq, juce::PathStrokeType(1.2f));
            g.restoreState();
            continue;
        }

        // --- Compressor / Expander displays ---
        if (type == "compressor-display" || type == "expander-display")
        {
            // Ratio lookup table shared by Compressor.java and Expander.java
            static const float kRatioTable[] = {
                1.0f,1.1f,1.2f,1.3f,1.4f,1.5f,1.6f,1.7f,1.8f,1.9f,2.0f,2.2f,2.4f,2.6f,2.8f,
                3.0f,3.2f,3.4f,3.6f,3.8f,4.0f,4.2f,4.4f,4.6f,4.8f,5.0f,5.5f,6.0f,6.5f,7.0f,
                7.5f,8.0f,8.5f,9.0f,9.5f,10.0f,11.0f,12.0f,13.0f,14.0f,15.0f,16.0f,17.0f,
                18.0f,19.0f,20.0f,22.0f,24.0f,26.0f,28.0f,30.0f,32.0f,34.0f,36.0f,38.0f,
                40.0f,42.0f,44.0f,46.0f,48.0f,50.0f,55.0f,60.0f,65.0f,70.0f,75.0f,80.0f
            };
            constexpr int kRatioTableSize = (int)(sizeof(kRatioTable) / sizeof(kRatioTable[0]));

            auto normP = [](const Parameter* p, float def) -> float {
                if (!p) return def;
                auto* pd = p->getDescriptor();
                int range = pd->maxValue - pd->minValue;
                return range > 0 ? (float)(p->getValue() - pd->minValue) / (float)range : def;
            };

            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(dx, dy, dw, dh));

            auto mpx = [&](float n) { return (float)dx + n * (float)dw; };
            auto mpy = [&](float n) { return (float)dy + n * (float)dh; };

            // Unity diagonal
            g.setColour(activeScheme_.displayGrid);
            g.drawLine(mpx(0.0f), mpy(1.0f), mpx(1.0f), mpy(0.0f), 0.5f);

            if (type == "compressor-display")
            {
                // Port of Compressor.java / JTCompressorDisplay.java
                // threshold → normalized, ratio → raw int, refLevel → normalized, limiter → raw int
                float threshold = normP(findParameter(m, cd.thresholdComponentId), 0.5f);
                float refLevel  = normP(findParameter(m, cd.refLevelComponentId),  0.5f);
                auto* pRatio    = findParameter(m, cd.ratioComponentId);
                auto* pLimiter  = findParameter(m, cd.limiterComponentId);
                int   ratioIdx  = pRatio   ? pRatio->getValue()   : 0;
                int   limiterInt= pLimiter ? pLimiter->getValue() : 12;

                float ratio   = 1.0f / kRatioTable[juce::jlimit(0, kRatioTableSize - 1, ratioIdx)];
                float limiter = (24.0f - (float)limiterInt) / 42.0f;

                // RefLevel guide lines
                g.drawLine(mpx(-0.1f), mpy(1.0f - refLevel), mpx(1.1f), mpy(1.0f - refLevel), 0.5f);
                g.drawLine(mpx(refLevel), mpy(-0.1f), mpx(refLevel), mpy(1.1f), 0.5f);

                juce::Path curve;
                if (threshold <= refLevel)
                {
                    float b        = (1.0f - refLevel) + ratio * refLevel;
                    float xLimiter = (limiter - b) / (-ratio);
                    float yThresh  = -ratio * threshold + b;

                    if (yThresh >= limiter)
                    {
                        curve.startNewSubPath(mpx(-1.0f), mpy(1.0f + yThresh + threshold));
                        curve.lineTo(mpx(threshold), mpy(yThresh));
                        curve.lineTo(mpx(xLimiter),  mpy(limiter));
                    }
                    else if (threshold <= 1.0f - limiter)
                    {
                        curve.startNewSubPath(mpx(-1.0f), mpy(1.0f + limiter + threshold));
                        curve.lineTo(mpx(threshold), mpy(limiter));
                        curve.lineTo(mpx(threshold), mpy(limiter));
                    }
                    else
                    {
                        curve.startNewSubPath(mpx(0.0f), mpy(1.0f));
                        curve.lineTo(mpx(1.0f - limiter), mpy(limiter));
                        curve.lineTo(mpx(1.0f - limiter), mpy(limiter));
                    }
                    curve.lineTo(mpx(2.0f), mpy(limiter));
                }
                else
                {
                    float tempThresh = std::min(threshold, 1.0f - limiter);
                    float b          = (1.0f - tempThresh) + ratio * tempThresh;
                    float xLimiter   = (limiter - b) / (-ratio);

                    curve.startNewSubPath(mpx(0.0f), mpy(1.0f));
                    curve.lineTo(mpx(tempThresh), mpy(1.0f - tempThresh));
                    curve.lineTo(mpx(xLimiter),   mpy(limiter));
                    curve.lineTo(mpx(2.0f),        mpy(limiter));
                }

                g.setColour(activeScheme_.displayCurveRed);
                g.strokePath(curve, juce::PathStrokeType(1.2f));
            }
            else // expander-display
            {
                // Port of Expander.java / JTExpanderDisplay.java
                // threshold → normalized, ratio → raw int (NOT inverted), gate → normalized
                float threshold = normP(findParameter(m, cd.thresholdComponentId), 0.5f);
                float gate      = normP(findParameter(m, cd.gateComponentId),      0.0f);
                auto* pRatio    = findParameter(m, cd.ratioComponentId);
                int   ratioIdx  = pRatio ? pRatio->getValue() : 0;
                float ratio     = kRatioTable[juce::jlimit(0, kRatioTableSize - 1, ratioIdx)];

                juce::Path curve;
                if (gate <= threshold)
                {
                    float intersectGate = ratio * (-gate + threshold) - threshold + 1.0f;
                    curve.startNewSubPath(mpx(gate), mpy(1.2f));
                    curve.lineTo(mpx(gate),      mpy(intersectGate));
                    curve.lineTo(mpx(threshold), mpy(1.0f - threshold));
                }
                else
                {
                    curve.startNewSubPath(mpx(gate), mpy(1.1f));
                    curve.lineTo(mpx(gate), mpy(1.0f - gate));
                    curve.lineTo(mpx(gate), mpy(1.0f - gate));
                }
                curve.lineTo(mpx(1.1f), mpy(-0.1f));

                g.setColour(activeScheme_.displayCurveRed);
                g.strokePath(curve, juce::PathStrokeType(1.2f));
            }

            g.restoreState();
            continue;
        }

        // --- NoteVelScale graph ---
        if (type == "NoteVelScaleDisplay")
        {
            float lGain = 0.5f, bp = 0.5f, rGain = 0.5f;

            auto* pL = findParameter(m, "p2");
            if (pL)
            {
                int range = pL->getDescriptor()->maxValue - pL->getDescriptor()->minValue;
                if (range > 0) lGain = static_cast<float>(pL->getValue() - pL->getDescriptor()->minValue) / static_cast<float>(range);
            }

            auto* pB = findParameter(m, "p3");
            if (pB)
            {
                int range = pB->getDescriptor()->maxValue - pB->getDescriptor()->minValue;
                if (range > 0) bp = static_cast<float>(pB->getValue() - pB->getDescriptor()->minValue) / static_cast<float>(range);
            }

            auto* pR = findParameter(m, "p4");
            if (pR)
            {
                int range = pR->getDescriptor()->maxValue - pR->getDescriptor()->minValue;
                if (range > 0) rGain = static_cast<float>(pR->getValue() - pR->getDescriptor()->minValue) / static_cast<float>(range);
            }

            // Background & Border
            g.setColour(activeScheme_.displayBg);
            g.fillRect(dx, dy, dw, dh);
            g.setColour(activeScheme_.displayBorder);
            g.drawRect(dx, dy, dw, dh, 1.0f);

            float margin = 2.0f;
            float plotW = dw - margin * 2.0f;
            float plotH = dh - margin * 2.0f;
            float midY  = dy + margin + plotH * 0.5f;

            // Draw horizontal guide (0dB)
            g.setColour(activeScheme_.displayGrid);
            g.drawLine(dx + margin, midY, dx + dw - margin, midY, 0.5f);

            // Draw vertical guide (breakpoint)
            float ox = dx + margin + bp * plotW;
            g.drawLine(ox, dy + margin, ox, dy + dh - margin, 0.5f);

            // Origin is (ox, midY)
            // Java original slopes:
            // flg = ((1 - vlGain) * 2 - 1) * 24 / 25
            // frg = (vrGain * 2 - 1) * 24 / 25
            // Line 1: (ox, cy) to (ox+len, cy + flg*len) -> Right side (Java used vlGain for Right!)
            // Line 2: (ox, cy) to (ox-len, cy - frg*len) -> Left side  (Java used vrGain for Left!)
            
            float flg = ((1.0f - lGain) * 2.0f - 1.0f) * 24.0f / 25.0f;
            float frg = (rGain * 2.0f - 1.0f) * 24.0f / 25.0f;
            
            float len = std::sqrt(plotW * plotW + plotH * plotH);

            juce::Path curve;
            // Left segment (Java line 2)
            curve.startNewSubPath(ox, midY);
            curve.lineTo(ox - len, midY - frg * len);
            
            // Right segment (Java line 1)
            curve.startNewSubPath(ox, midY);
            curve.lineTo(ox + len, midY + flg * len);

            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(static_cast<int>(dx + margin),
                               static_cast<int>(dy + margin),
                               static_cast<int>(plotW),
                               static_cast<int>(plotH));

            g.setColour(activeScheme_.displayCurveYellow);
            g.strokePath(curve, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- Vocoder routing display ---
        if (type == "vocoder-display")
        {
            // Black background with border
            g.setColour(activeScheme_.displayBg);
            g.fillRect(dx, dy, dw, dh);
            g.setColour(activeScheme_.displayBorder);
            g.drawRect(dx, dy, dw, dh, 1.0f);

            // Draw routing lines: band[i] = output band index (1-based, 0=off)
            // Line: top row at column (band[i]-1) → bottom row at column i
            constexpr int kBands = 16;
            float space   = dw / static_cast<float>(kBands);
            float loffset = dx + space * 0.5f;
            float top     = dy + 1.0f;
            float bot     = dy + dh - 1.0f;

            g.setColour(activeScheme_.vocoderRouting);  // green routing lines (matches original)
            for (int i = 0; i < kBands; ++i)
            {
                if (cd.bandIds[i].isEmpty()) continue;
                auto* param = findParameter(m, cd.bandIds[i]);
                if (param == nullptr) continue;
                int bandVal = param->getValue();
                if (bandVal <= 0) continue;  // 0 = off, no line

                float x0 = loffset + static_cast<float>(bandVal - 1) * space;  // output column
                float x1 = loffset + static_cast<float>(i) * space;            // input column
                g.drawLine(x0, top, x1, bot, 1.0f);
            }
            continue;
        }

        // --- Phaser display ---
        if (type == "phaser-display")
        {
            // Port of Phaser.java / JTPhaserDisplay.java
            // feedback→raw int(norm*127), peaks→1+int(norm*5), spread→raw int(norm*127)
            // Curve: alternating EXP/LOG cubics using CurvePathIterator formulas
            auto normP = [](const Parameter* p, float def) -> float {
                if (!p) return def;
                auto* pd = p->getDescriptor();
                int range = pd->maxValue - pd->minValue;
                return range > 0 ? (float)(p->getValue() - pd->minValue) / (float)range : def;
            };

            float normFb     = normP(findParameter(m, cd.feedbackComponentId), 0.496f);
            float normPeaks  = normP(findParameter(m, cd.peaksComponentId),    0.4f);
            float normSpread = normP(findParameter(m, cd.spreadComponentId),   0.496f);
            // Center freq drives the horizontal position of the first peak.
            // Default 64/127 ≈ 0.5 keeps the existing centred layout.
            float normFreq   = normP(findParameter(m, cd.freqComponentId),     0.5f);

            int feedBack  = (int)(normFb     * 127.0f);
            int nbPeaks   = juce::jlimit(1, 6, 1 + (int)(normPeaks * 5.0f));
            int spreadAbs = (int)(normSpread * 127.0f);

            float feedbackOdd, feedbackEven;
            if (feedBack < 77)
            {
                feedbackOdd  = (float)feedBack / 77.0f;
                feedbackEven = 0.5f;
            }
            else
            {
                feedbackOdd  = 1.0f;
                feedbackEven = 0.5f - 0.3f * (float)(feedBack - 77) / 50.0f;
            }

            float spreadMax = 0.25f + 0.75f * (float)(nbPeaks - 1) / 5.0f;
            float spread    = spreadMax * (float)spreadAbs / 127.0f;
            // L: left edge of the peak cluster. centre-freq slides the cluster
            // across the available range (0..1-spread), so freq=0 anchors left,
            // freq=1 anchors right; default 0.5 reproduces the prior centred look.
            float L         = juce::jlimit(0.0f, 1.0f - spread, normFreq * (1.0f - spread));
            float R         = L + spread;

            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(dx, dy, dw, dh));

            auto mpx = [&](float n) { return (float)dx + n * (float)dw; };
            auto mpy = [&](float n) { return (float)dy + n * (float)dh; };

            // Midline guide
            g.setColour(activeScheme_.displayGrid);
            g.drawLine(mpx(-0.1f), mpy(0.5f), mpx(1.1f), mpy(0.5f), 0.5f);

            // CurvePathIterator EXP/LOG bezier formulas (src→dst):
            // EXP: ctrl1=((dx-sx)/2+sx, sy),  ctrl2=(dx, (dy-sy)/2+sy)
            // LOG: ctrl1=(sx, (sy-dy)/2+dy),  ctrl2=((dx-sx)/2+sx, dy)
            auto cubicEXP = [&](juce::Path& path, float sx, float sy, float ex, float ey) {
                path.cubicTo(mpx((ex-sx)*0.5f+sx), mpy(sy),
                             mpx(ex),               mpy((ey-sy)*0.5f+sy),
                             mpx(ex),               mpy(ey));
            };
            auto cubicLOG = [&](juce::Path& path, float sx, float sy, float ex, float ey) {
                path.cubicTo(mpx(sx),               mpy((sy-ey)*0.5f+ey),
                             mpx((ex-sx)*0.5f+sx),  mpy(ey),
                             mpx(ex),               mpy(ey));
            };

            // Build point sequence from Phaser.java update_peaks()
            // pairs [xe=Even,LOG→first=EXP], [xo=Odd,EXP] per peak, then [R,Even,LOG], [1.1,0.5,LOG]
            struct PhPt { float x, y; bool isEXP; };
            PhPt pts[16];
            int  nPts = 0;
            for (int i = 0; i < nbPeaks; ++i)
            {
                float xe = L + spread * (float)(i * 2)     / (float)(nbPeaks * 2);
                float xo = L + spread * (float)(i * 2 + 1) / (float)(nbPeaks * 2);
                pts[nPts++] = { xe, feedbackEven, (i == 0) }; // first overridden to EXP
                pts[nPts++] = { xo, feedbackOdd,  true };
            }
            pts[nPts++] = { R,    feedbackEven, false };
            pts[nPts++] = { 1.1f, 0.5f,         false };

            // Draw fill (matching Java g.fill(phaser))
            juce::Path fill;
            fill.startNewSubPath(mpx(-0.1f), mpy(1.1f));
            fill.lineTo(mpx(-0.1f), mpy(0.5f));
            float prevX = -0.1f, prevY = 0.5f;
            for (int i = 0; i < nPts; ++i)
            {
                if (pts[i].isEXP) cubicEXP(fill, prevX, prevY, pts[i].x, pts[i].y);
                else              cubicLOG(fill, prevX, prevY, pts[i].x, pts[i].y);
                prevX = pts[i].x; prevY = pts[i].y;
            }
            fill.lineTo(mpx(1.1f), mpy(1.1f));
            fill.closeSubPath();

            g.setColour(activeScheme_.displayCurvePurple.withAlpha(0.25f));
            g.fillPath(fill);

            // Draw curve outline
            juce::Path curvePh;
            curvePh.startNewSubPath(mpx(-0.1f), mpy(0.5f));
            prevX = -0.1f; prevY = 0.5f;
            for (int i = 0; i < nPts; ++i)
            {
                if (pts[i].isEXP) cubicEXP(curvePh, prevX, prevY, pts[i].x, pts[i].y);
                else              cubicLOG(curvePh, prevX, prevY, pts[i].x, pts[i].y);
                prevX = pts[i].x; prevY = pts[i].y;
            }
            g.setColour(activeScheme_.displayCurvePurple);
            g.strokePath(curvePh, juce::PathStrokeType(1.0f));

            g.restoreState();
            continue;
        }

        // --- Multimode routing bracket (FilterC / FilterD) ---
        if (type == "multimode-routing")
        {
            float bx0 = static_cast<float>(bounds.getX());
            float by0 = static_cast<float>(bounds.getY());

            float inX  = bx0 + static_cast<float>(cd.mmInX);
            float outX = bx0 + static_cast<float>(cd.mmOutX);
            float hpY  = by0 + static_cast<float>(cd.mmHpY);
            float bpY  = by0 + static_cast<float>(cd.mmBpY);
            float lpY  = by0 + static_cast<float>(cd.mmLpY);

            // Vertical bar: fixed distance from outputs, placed before the HP/BP/LP labels
            float barX   = outX - 28.0f;  // bar well to the left of the labels (~x=209)
            float lineEnd = outX - 18.0f; // lines stop just before the label text (~x=219)

            g.setColour(activeScheme_.bracketRouting); // grey

            // Horizontal line from in connector to bar, at BP level (centre of bracket)
            g.drawLine(inX + 7.0f, bpY, barX, bpY, 1.0f);

            // Vertical bar connecting HP to LP
            g.drawLine(barX, hpY, barX, lpY, 1.0f);

            // Short horizontal lines from bar to just before the labels
            g.drawLine(barX, hpY, lineEnd, hpY, 1.0f);
            g.drawLine(barX, bpY, lineEnd, bpY, 1.0f);
            g.drawLine(barX, lpY, lineEnd, lpY, 1.0f);

            continue;
        }

        // --- Fallback: label with type name ---
        auto label = cd.type;
        label = label.replace("-display", "").replace("-envelope", " env")
                     .replace("-editor", "").replace("Display", "");

        g.setColour(activeScheme_.displayBorderCustom);
        g.setFont(juce::FontOptions(juce::jmin(7.0f, dh - 2.0f)));
        g.drawText(label,
                   static_cast<int>(dx + 1), static_cast<int>(dy + 1),
                   static_cast<int>(dw - 2), static_cast<int>(dh - 2),
                   juce::Justification::centred, true);
    }
}

void PatchCanvas::paintModuleFallback(juce::Graphics& g, const Module& m, juce::Rectangle<int> rect)
{
    // Original simple rendering for modules without theme data
    auto bgColour = m.getDescriptor()->background;
    g.setColour(bgColour);
    g.fillRoundedRectangle(rect.toFloat(), 3.0f);

    // Border
    g.setColour(bgColour.brighter(0.3f));
    g.drawRoundedRectangle(rect.toFloat().reduced(0.5f), 3.0f, 1.0f);

    // Title text
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(m.getTitle(), rect.reduced(4, 1).removeFromTop(13),
               juce::Justification::centredLeft, true);

    // Draw connector dots at geometric positions
    for (auto& conn : m.getConnectors())
    {
        // Geometric fallback position
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

        int moduleH = rect.getHeight();
        int headerH = 14;
        int px, py;

        if (desc->isOutput)
        {
            int spacing = (outputIdx > 0) ? (moduleH - headerH) / outputIdx : moduleH;
            py = rect.getY() + headerH + thisOutputIdx * spacing + spacing / 2;
            px = rect.getRight() - 4;
        }
        else
        {
            int spacing = (inputIdx > 0) ? (moduleH - headerH) / inputIdx : moduleH;
            py = rect.getY() + headerH + thisInputIdx * spacing + spacing / 2;
            px = rect.getX() + 4;
        }

        g.setColour(getSignalColour(desc->signalType));
        g.fillEllipse(static_cast<float>(px - 3), static_cast<float>(py - 3), 6.0f, 6.0f);
    }
}

void PatchCanvas::shakeCables()
{
    cableSagOffsets.clear();
    std::mt19937 rng(static_cast<unsigned>(juce::Time::currentTimeMillis()));
    std::uniform_real_distribution<float> dist(-0.6f, 0.6f);

    auto addOffsets = [&](const ModuleContainer& container)
    {
        for (auto& conn : container.getConnections())
        {
            if (conn.output && conn.input)
            {
                auto key = std::make_pair(conn.output, conn.input);
                cableSagOffsets[key] = dist(rng);
            }
        }
    };

    if (patch != nullptr)
    {
        addOffsets(patch->getPolyVoiceArea());
        addOffsets(patch->getCommonArea());
    }
    repaint();
}

void PatchCanvas::paintCables(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    for (auto& conn : container.getConnections())
    {
        if (conn.output == nullptr || conn.input == nullptr)
            continue;

        // Check cable visibility by signal type
        if (patch != nullptr)
        {
            const auto& hdr = patch->getHeader();
            switch (conn.output->getDescriptor()->signalType)
            {
                case SignalType::Audio:       if (!hdr.cableVisRed)    continue; break;
                case SignalType::Control:     if (!hdr.cableVisBlue)   continue; break;
                case SignalType::Logic:       if (!hdr.cableVisYellow) continue; break;
                case SignalType::MasterSlave: if (!hdr.cableVisGray)   continue; break;
                case SignalType::User1:       if (!hdr.cableVisGreen)  continue; break;
                case SignalType::User2:       if (!hdr.cableVisPurple) continue; break;
                case SignalType::None:        if (!hdr.cableVisWhite)  continue; break;
            }
        }

        // Find the modules that own these connectors
        const Module* srcModule = nullptr;
        const Module* dstModule = nullptr;

        for (auto& modulePtr : container.getModules())
        {
            for (auto& c : modulePtr->getConnectors())
            {
                if (&c == conn.output)
                    srcModule = modulePtr.get();
                if (&c == conn.input)
                    dstModule = modulePtr.get();
            }
        }

        if (srcModule == nullptr || dstModule == nullptr)
            continue;

        auto srcPos = getConnectorPosition(*srcModule, *conn.output, yOffset);
        auto dstPos = getConnectorPosition(*dstModule, *conn.input, yOffset);

        juce::Colour cableCol = activeScheme_.cableAudio;
        switch (conn.output->getDescriptor()->signalType)
        {
            case SignalType::Audio:       cableCol = activeScheme_.cableAudio;       break;
            case SignalType::Control:     cableCol = activeScheme_.cableControl;     break;
            case SignalType::Logic:       cableCol = activeScheme_.cableLogic;       break;
            case SignalType::MasterSlave: cableCol = activeScheme_.cableMasterSlave; break;
            case SignalType::User1:       cableCol = activeScheme_.cableUser1;       break;
            case SignalType::User2:       cableCol = activeScheme_.cableUser2;       break;
            default: cableCol = getSignalColour(conn.output->getDescriptor()->signalType); break;
        }

        if (cableOpacity < 0.01f)
            continue;

        // Build path — curved or straight depending on style
        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());

        const bool isCurved   = (cableStyleIdx == 0 || cableStyleIdx == 2);
        const bool isThick    = (cableStyleIdx == 0 || cableStyleIdx == 1);
        const float strokeW   = isThick ? 2.5f : 1.4f;
        const float outlineW  = isThick ? 4.0f : 2.2f;

        if (isCurved)
        {
            float midY = (srcPos.y + dstPos.y) * 0.5f;
            float baseSag = std::abs(static_cast<float>(srcPos.x - dstPos.x)) * 0.15f + 15.0f;

            float sagMultiplier = 1.0f;
            auto key = std::make_pair(conn.output, conn.input);
            auto it = cableSagOffsets.find(key);
            if (it != cableSagOffsets.end())
                sagMultiplier += it->second;

            path.cubicTo(static_cast<float>(srcPos.x), midY + baseSag * sagMultiplier,
                         static_cast<float>(dstPos.x), midY + baseSag * sagMultiplier,
                         static_cast<float>(dstPos.x), static_cast<float>(dstPos.y));
        }
        else
        {
            path.lineTo(static_cast<float>(dstPos.x), static_cast<float>(dstPos.y));
        }

        // Dark outline for contrast, then colored cable on top
        g.setColour(juce::Colour(0xaa000000).withMultipliedAlpha(cableOpacity));
        g.strokePath(path, juce::PathStrokeType(outlineW, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        g.setColour(cableCol.withAlpha(0.80f * cableOpacity));
        g.strokePath(path, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
}

// --- Mouse Event Handlers ---

void PatchCanvas::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (patch == nullptr || themeData == nullptr)
        return;

    auto pos = screenToCanvas(e.getPosition());

    // Middle-click: start canvas pan (drag to scroll viewport)
    if (e.mods.isMiddleButtonDown())
    {
        dragState = DragState();
        dragState.type = DragState::CanvasPan;
        dragState.startPos = e.getScreenPosition();  // absolute screen coords for pan
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
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
                            // Reset all parameters to their minimum, including binary
                            // step toggles (EventSeq, etc). Loop/play/rec also reset
                            // to 0 — equivalent to a fresh sequencer.
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                int newVal = pd->minValue;
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
                            //   NoteSeqA → vertical sliders
                            //   NoteSeqB → noteStepIds inside the piano-roll editor
                            //   EventSeq → step toggles (binary), identified by name
                            //              pattern "seq N, step M" so transport
                            //              toggles (active/gate1/gate2) stay put.
                            std::set<juce::String> stepIds;
                            for (auto& ts : theme->sliders)
                                stepIds.insert(ts.componentId);
                            for (auto& cd : theme->customDisplays)
                                if (cd.type == "note-seq-editor")
                                    for (auto& id : cd.noteStepIds)
                                        if (id.isNotEmpty())
                                            stepIds.insert(id);
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                bool isStep = stepIds.count(pd->componentId) != 0
                                              || (pd->name.startsWithIgnoreCase("seq ")
                                                  && pd->name.containsIgnoreCase("step "));
                                if (!isStep) continue;
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
                                auto* pd = zoomParam->getDescriptor();
                                int oldVal = zoomParam->getValue();
                                int delta = (tb.callMethod == "zoomIn") ? 1 : -1;
                                int newVal = juce::jlimit(pd->minValue, pd->maxValue, oldVal + delta);
                                zoomParam->setValue(newVal);
                                if (parameterChangeCallback)
                                    parameterChangeCallback(area.section, m.getContainerIndex(), pd->index, newVal);
                                if (paramDragCompleteCallback && newVal != oldVal)
                                    paramDragCompleteCallback(area.section, m.getContainerIndex(), pd->index, oldVal, newVal);
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
                            // Top half → +1, bottom half → -1
                            bool goUp = relPos.y < tb.y + tb.height / 2;
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

                        if (parameterChangeCallback)
                            parameterChangeCallback(dragState.section, m.getContainerIndex(), pd->index, newValue);
                        // Button clicks complete immediately — fire drag complete for undo
                        if (paramDragCompleteCallback)
                            paramDragCompleteCallback(dragState.section, m.getContainerIndex(), pd->index, oldButtonValue, newValue);

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
                int numP = static_cast<int>(drumPresets.size());
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
                    int cur = 0;
                    auto it = drumPresetState.find(key);
                    if (it != drumPresetState.end()) cur = it->second;

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
                if (!td.partialFormat) continue;

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

                menu.showMenuAsync(juce::PopupMenu::Options{},
                    [this, modPtr, sec](int result)
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
                                        if (newName.isNotEmpty())
                                        {
                                            modPtr->setTitle(newName);
                                            if (renameModuleCallback)
                                                renameModuleCallback(sec, modPtr, newName);
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
                            if (deleteModuleCallback)
                                deleteModuleCallback(sec, modPtr);
                            if (selectedModule == modPtr)
                                clearSelection();
                            repaint();
                        }
                        else if (result == 6)
                        {
                            if (initModuleCallback)
                                initModuleCallback(sec, modPtr);
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
                    });
                return;
            }

            // Left click → select module
            bool alreadySelected = isSelected(&m);
            bool addToSel = e.mods.isShiftDown();

            if (!alreadySelected || addToSel)
                selectModule(&m, area.section, addToSel);
            // If already selected and no shift → keep selection, just start move

            // Multi-move if more than one module selected
            if (selection.size() > 1)
            {
                dragState.type = DragState::MultiModuleMove;
                dragState.startPos = pos;
                multiMoveState.clear();
                for (auto& sel : selection)
                    multiMoveState.push_back({ sel.module, sel.section, sel.module->getPosition() });
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
                    target.addItem(1000 + desc->index, label != nullptr ? juce::String(label) : desc->fullname);
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
        menu.addItem(2, "Shake Cables");
        menu.addItem(3, "Reset Cables");

        menu.showMenuAsync(juce::PopupMenu::Options{},
            [this, clickSection, clickGX, clickGY, pos](int result)
            {
                if (result == 1)
                {
                    pasteFromClipboard(pos);
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
                    auto* desc = moduleDescs->getModuleByIndex(typeIndex);
                    if (desc && moduleDropCallback)
                    {
                        if (undoManager) undoManager->beginNewTransaction("Add Module");
                        moduleDropCallback(typeIndex, clickSection, clickGX, clickGY, desc->name);
                    }
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
            int newX = juce::jmax(0, ms.startGridPos.x + dx);
            auto& container = patch->getContainer(ms.section);
            int rawY = juce::jmax(0, ms.startGridPos.y + dy);
            int newY = findNearestFreeY(container, ms.module, newX, rawY, ms.module->getDescriptor()->height);
            ms.module->setPosition({ newX, newY });
        }
        repaint();
        return;
    }

    if (dragState.type == DragState::ModuleMove)
    {
        int newGridX = juce::jmax(0, (currentPos.x - dragState.dragOffsetX + gridX / 2) / gridX);
        int rawGridY = juce::jmax(0, (currentPos.y - dragState.dragOffsetY + gridY / 2) / gridY);

        // Prevent overlap: snap to nearest free Y position in this column
        auto& container = patch->getContainer(dragState.section);
        int moduleHeight = dragState.module->getDescriptor()->height;
        int newGridY = findNearestFreeY(container, dragState.module, newGridX, rawGridY, moduleHeight);

        auto curPos = dragState.module->getPosition();
        if (newGridX != curPos.x || newGridY != curPos.y)
        {
            dragState.module->setPosition({ newGridX, newGridY });
            repaint();
        }
        return;
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

    if (dragState.type == DragState::Knob)
    {
        int rawDelta = 0;
        if (knobControlIdx == 1)
        {
            // Circular: clockwise from top = increase.
            // atan2(dx, -dy): 0=up, +π/2=right, -π/2=left.
            // 90° arc = full parameter range. Clamp at ±153° to avoid bottom discontinuity.
            auto d = currentPos - dragState.startPos;
            const float len = std::sqrt(static_cast<float>(d.x * d.x + d.y * d.y));
            if (len >= 5.0f)
            {
                constexpr float kHalfPi = juce::MathConstants<float>::pi / 2.0f;
                constexpr float kClamp  = juce::MathConstants<float>::pi * 0.85f;
                float angle = std::atan2(static_cast<float>(d.x), static_cast<float>(-d.y));
                angle = juce::jlimit(-kClamp, kClamp, angle);
                const int range = pd->maxValue - pd->minValue;
                rawDelta = static_cast<int>(angle / kHalfPi * range);
            }
        }
        else if (knobControlIdx == 2)
        {
            // Vertical: up = increase
            rawDelta = dragState.startPos.y - currentPos.y;
        }
        else
        {
            // Horizontal (default): right = increase
            rawDelta = currentPos.x - dragState.startPos.x;
        }
        int valueDelta = static_cast<int>(rawDelta * 0.5f);
        newValue = juce::jlimit(pd->minValue, pd->maxValue, dragState.startValue + valueDelta);
    }
    else if (dragState.type == DragState::Slider)
    {
        // Linear control: drag along slider axis
        // Determine if vertical or horizontal
        bool isVertical = true;  // Default, could check theme orientation

        if (isVertical)
        {
            // Vertical slider: drag up = increase
            int deltaY = dragState.startPos.y - currentPos.y;
            float normalized = static_cast<float>(deltaY) / 100.0f;  // 100px = full range
            int valueDelta = static_cast<int>(normalized * range);
            newValue = juce::jlimit(pd->minValue, pd->maxValue, dragState.startValue + valueDelta);
        }
        else
        {
            // Horizontal slider: drag right = increase
            int deltaX = currentPos.x - dragState.startPos.x;
            float normalized = static_cast<float>(deltaX) / 100.0f;
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

    if (dragState.type == DragState::MultiModuleMove)
    {
        if (moduleMoveCallback && undoManager)
        {
            undoManager->beginNewTransaction("Move Modules");
            for (auto& ms : multiMoveState)
            {
                auto newPos = ms.module->getPosition();
                if (newPos != ms.startGridPos)
                    moduleMoveCallback(ms.section, ms.module->getContainerIndex(),
                                       ms.startGridPos, newPos);
            }
        }
        multiMoveState.clear();
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

            // Connect output→input, auto-swap if needed
            Connector* outConn = nullptr;
            Connector* inConn = nullptr;
            if (srcOut && !dstOut) { outConn = src; inConn = dst; }
            else if (!srcOut && dstOut) { outConn = dst; inConn = src; }

            if (outConn && inConn)
            {
                container.addConnection(outConn, inConn);

                if (cableCreatedCallback && dragState.module)
                {
                    if (undoManager)
                        undoManager->beginNewTransaction("Add Cable");
                    // Find module owners for undo info
                    auto findOwner = [&](Connector* c) -> Module* {
                        for (auto& mp : container.getModules())
                            for (auto& mc : mp->getConnectors())
                                if (&mc == c) return mp.get();
                        return nullptr;
                    };
                    auto* outMod = findOwner(outConn);
                    auto* inMod = findOwner(inConn);
                    if (outMod && inMod)
                        cableCreatedCallback(dragState.section,
                            outMod->getContainerIndex(), outConn->getDescriptor()->index, true,
                            inMod->getContainerIndex(), inConn->getDescriptor()->index, false);
                }
            }
        }
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
    // Delete / Backspace → delete selection
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!selection.empty())
        {
            deleteSelection();
            return true;
        }
    }

    // Ctrl+C → copy
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        if (!selection.empty()) { copySelectionToClipboard(); return true; }
    }

    // Ctrl+V → paste at centre of viewport
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        if (!clipboard.empty())
        {
            pasteFromClipboard(screenToCanvas({ getWidth() / 2, getHeight() / 4 }));
            return true;
        }
    }

    // Ctrl+D → duplicate with cables
    if (key == juce::KeyPress('d', juce::ModifierKeys::commandModifier, 0))
    {
        if (!selection.empty()) { duplicateSelection(true); return true; }
    }

    // Enter → Quick Add popup at mouse position (only one at a time)
    if (key == juce::KeyPress::returnKey && patch != nullptr && moduleDescs != nullptr)
    {
        if (activeQuickAdd != nullptr)
            return true;  // already open

        auto mousePos = screenToCanvas(getMouseXYRelative());

        int section = mySection;
        int gx = juce::jlimit(0, 39, mousePos.x / gridX);
        int gy = juce::jlimit(0, 127, mousePos.y / gridY);

        auto screenPos = localPointToGlobal(getMouseXYRelative());

        activeQuickAdd = new QuickAddPopup(
            *moduleDescs, screenPos, gx, gy,
            [this, section](const ModuleDescriptor* desc, int pgx, int pgy)
            {
                if (moduleDropCallback && desc)
                {
                    if (undoManager) undoManager->beginNewTransaction("Add Module");
                    moduleDropCallback(desc->index, section, pgx, pgy, desc->name);
                }
            },
            [this]() { activeQuickAdd = nullptr; }
        );

        activeQuickAdd->grabFocusNow();
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

    // Ctrl+N → new patch, Ctrl+O → open, Ctrl+S → save, Ctrl+W → close
    if (fileCommandCallback)
    {
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
        if (!target && selectedModule != nullptr)
            target = selectedModule;

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
        if (handleMorphOverlayKey(key, *parent))
            return true;
    }
    else if (handleMorphOverlayKey(key, *this))
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

    // Shift+Z → always reset zoom to 100%
    if (key == juce::KeyPress('z', juce::ModifierKeys::shiftModifier, 0))
    {
        resetZoom();
        return true;
    }

    // Z → zoom-to-selection (if any) or reset to 100%
    if (key.getTextCharacter() == 'z' && !key.getModifiers().isCommandDown())
    {
        if (!selection.empty())
            zoomToSelection();
        else
            resetZoom();
        return true;
    }

    return false;
}

bool PatchCanvas::isPositionFree(const ModuleContainer& container, const Module* exclude, int gx, int gy, int height) const
{
    for (auto& m : container.getModules())
    {
        if (m.get() == exclude)
            continue;
        auto pos = m->getPosition();
        if (pos.x != gx)
            continue;
        int mh = m->getDescriptor()->height;
        // Y ranges overlap if gy < pos.y + mh AND pos.y < gy + height
        if (gy < pos.y + mh && pos.y < gy + height)
            return false;
    }
    return true;
}

int PatchCanvas::findNearestFreeY(const ModuleContainer& container, const Module* exclude, int gx, int targetY, int height) const
{
    if (isPositionFree(container, exclude, gx, targetY, height))
        return targetY;

    // Search above and below alternately, return closest free slot
    for (int offset = 1; offset < 256; offset++)
    {
        int above = targetY - offset;
        if (above >= 0 && isPositionFree(container, exclude, gx, above, height))
            return above;
        int below = targetY + offset;
        if (isPositionFree(container, exclude, gx, below, height))
            return below;
    }
    return targetY; // fallback — should not happen
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

bool PatchCanvas::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    // Accept module drags from ModuleBrowserPanel
    auto description = dragSourceDetails.description;
    if (!description.isObject())
        return false;

    auto* obj = description.getDynamicObject();
    if (obj == nullptr)
        return false;

    return obj->getProperty("type").toString() == "module";
}

void PatchCanvas::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    if (!patch || !moduleDescs)
        return;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    dropPreviewTypeId = obj->getProperty("typeId");
    showModuleDropPreview = true;
    repaint();
}

void PatchCanvas::itemDragMove(const SourceDetails& dragSourceDetails)
{
    if (!showModuleDropPreview || !patch || !moduleDescs)
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

    int typeId = obj->getProperty("typeId");
    juce::String moduleName = obj->getProperty("name").toString();

    auto mousePos = screenToCanvas(dragSourceDetails.localPosition.toInt());
    int section = mySection;
    int dropX = juce::jlimit(0, 39, mousePos.x / PatchCanvas::gridX);
    int dropY = juce::jlimit(0, 127, mousePos.y / PatchCanvas::gridY);

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
        // Pedal, After touch, On/Off switch
        const char* specialNames[] = { "Pedal", "After touch", "On/Off switch" };
        for (int k = 18; k < 21; ++k)
        {
            juce::String label = specialNames[k - 18];
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

bool PatchCanvas::isSelected(const Module* m) const
{
    for (auto& s : selection)
        if (s.module == m) return true;
    return false;
}

void PatchCanvas::clearSelection()
{
    selection.clear();
    selectedModule = nullptr;
    selectedSection = -1;
    if (moduleSelectedCallback) moduleSelectedCallback(nullptr, -1);
}

void PatchCanvas::selectModule(Module* m, int section, bool addToSelection)
{
    if (!addToSelection) clearSelection();
    if (!isSelected(m))
        selection.push_back({ m, section });
    selectedModule = m;
    selectedSection = section;
    // Notify inspector — report the most recently selected module
    if (moduleSelectedCallback) moduleSelectedCallback(m, section);
}

void PatchCanvas::updateRubberBandSelection(juce::Rectangle<int> rect)
{
    selection.clear();
    if (patch == nullptr) return;

    ModuleContainer& container = (mySection == 1)
        ? patch->getPolyVoiceArea()
        : patch->getCommonArea();

    for (auto& modulePtr : container.getModules())
    {
        auto bounds = getModuleBounds(*modulePtr, 0);
        if (rect.intersects(bounds))
            selection.push_back({ modulePtr.get(), mySection });
    }
}

void PatchCanvas::deleteSelection()
{
    if (selection.empty() || patch == nullptr) return;

    if (undoManager)
        undoManager->beginNewTransaction("Delete Selection");

    for (auto& sel : selection)
    {
        if (deleteModuleCallback)
            deleteModuleCallback(sel.section, sel.module);
        else
        {
            auto& container = patch->getContainer(sel.section);
            container.removeModule(sel.module);
        }
    }
    clearSelection();
    repaint();
}

void PatchCanvas::duplicateSelection(bool withCables)
{
    if (selection.empty() || patch == nullptr || moduleDescs == nullptr) return;

    if (undoManager)
        undoManager->beginNewTransaction("Duplicate");

    // Offset for duplicates: 1 column to the right, 0 rows down
    const int offsetX = 1, offsetY = 2;

    // Map old Module* → {new Module*, section} — section stored at creation to
    // avoid a redundant O(N) search when building the new selection below.
    std::map<Module*, std::pair<Module*, int>> oldToNew;

    for (auto& sel : selection)
    {
        auto* desc = sel.module->getDescriptor();
        if (!desc) continue;

        auto& container = patch->getContainer(sel.section);
        auto pos = sel.module->getPosition();
        int newX = pos.x + offsetX;
        int newY = findNearestFreeY(container, nullptr, newX, pos.y + offsetY, desc->height);

        auto* newMod = patch->createModule(sel.section, desc->index, newX, newY,
                                           sel.module->getTitle(), *moduleDescs);
        if (!newMod) continue;

        // Copy parameter values
        auto& srcParams = sel.module->getParameters();
        auto& dstParams = newMod->getParameters();
        for (size_t i = 0; i < srcParams.size() && i < dstParams.size(); ++i)
            dstParams[i].setValue(srcParams[i].getValue());

        oldToNew[sel.module] = { newMod, sel.section };
    }

    // Recreate internal cables if requested
    if (withCables)
    {
        // Gather all selected module pointers
        std::set<Module*> selSet;
        for (auto& s : selection) selSet.insert(s.module);

        for (auto& sel : selection)
        {
            auto& container = patch->getContainer(sel.section);
            for (auto& cable : container.getConnections())
            {
                // Find which modules own output and input
                Module* srcMod = nullptr;
                Module* dstMod = nullptr;
                Connector* srcConn = cable.output;
                Connector* dstConn = cable.input;

                for (auto& m : container.getModules())
                {
                    for (auto& c : m->getConnectors())
                    {
                        if (&c == srcConn) srcMod = m.get();
                        if (&c == dstConn) dstMod = m.get();
                    }
                }

                // Only duplicate cable if BOTH endpoints are in the selection
                if (srcMod && dstMod && selSet.count(srcMod) && selSet.count(dstMod))
                {
                    auto* newSrcMod = oldToNew[srcMod].first;
                    auto* newDstMod = oldToNew[dstMod].first;
                    if (!newSrcMod || !newDstMod) continue;

                    // Find matching connectors by descriptor index
                    auto* srcDesc = srcConn->getDescriptor();
                    auto* dstDesc = dstConn->getDescriptor();
                    if (!srcDesc || !dstDesc) continue;

                    Connector* newSrc = newSrcMod->getConnector(srcDesc->index, srcDesc->isOutput);
                    Connector* newDst = newDstMod->getConnector(dstDesc->index, dstDesc->isOutput);
                    if (newSrc && newDst)
                        container.addConnection(newSrc, newDst);
                }
            }
        }
    }

    // Select the new modules — section is already known from creation time
    clearSelection();
    for (auto& [old, newModAndSection] : oldToNew)
    {
        auto* newMod = newModAndSection.first;
        int section  = newModAndSection.second;
        selection.push_back({ newMod, section });
    }

    repaint();
}

void PatchCanvas::copySelectionToClipboard()
{
    if (selection.empty() || patch == nullptr) return;

    clipboard.clear();
    clipboardCables.clear();

    std::map<Module*, int> modToClipIdx;

    for (int i = 0; i < (int)selection.size(); ++i)
    {
        auto& sel = selection[static_cast<size_t>(i)];
        ClipboardEntry entry;
        entry.typeIndex = sel.module->getDescriptor() ? sel.module->getDescriptor()->index : 0;
        entry.name = sel.module->getTitle();
        entry.section = sel.section;
        entry.gridPos = sel.module->getPosition();
        for (auto& p : sel.module->getParameters())
            entry.paramValues.push_back(p.getValue());
        clipboard.push_back(entry);
        modToClipIdx[sel.module] = i;
    }

    // Store internal cables — scan each unique section once (NOT per selected module,
    // which would duplicate every cable N times for N selected modules in that section).
    std::set<Module*> selSet;
    std::set<int> sectionsToScan;
    for (auto& s : selection) { selSet.insert(s.module); sectionsToScan.insert(s.section); }

    for (int section : sectionsToScan)
    {
        auto& container = patch->getContainer(section);
        for (auto& cable : container.getConnections())
        {
            Module* srcMod = nullptr; Module* dstMod = nullptr;
            for (auto& m : container.getModules())
            {
                for (auto& c : m->getConnectors())
                {
                    if (&c == cable.output) srcMod = m.get();
                    if (&c == cable.input)  dstMod = m.get();
                }
            }
            if (srcMod && dstMod && selSet.count(srcMod) && selSet.count(dstMod))
            {
                auto* srcDesc = cable.output->getDescriptor();
                auto* dstDesc = cable.input->getDescriptor();
                if (srcDesc && dstDesc)
                    clipboardCables.push_back({ modToClipIdx[srcMod], srcDesc->index, srcDesc->isOutput,
                                                modToClipIdx[dstMod], dstDesc->index, dstDesc->isOutput });
            }
        }
    }
}

void PatchCanvas::pasteFromClipboard(juce::Point<int> mousePos)
{
    if (clipboard.empty() || patch == nullptr || moduleDescs == nullptr) return;

    if (undoManager)
        undoManager->beginNewTransaction("Paste");

    // Find bounding box of clipboard to offset paste position
    int minX = clipboard[0].gridPos.x, minY = clipboard[0].gridPos.y;
    for (auto& e : clipboard)
    {
        minX = std::min(minX, e.gridPos.x);
        minY = std::min(minY, e.gridPos.y);
    }

    // Determine target section and position from mouse
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0) separatorY = canvasHeight / 2;
    int pasteSection = (mousePos.y < separatorY) ? 1 : 0;
    int pasteGridX = mousePos.x / gridX;
    int pasteGridY = (pasteSection == 1) ? mousePos.y / gridY
                                         : (mousePos.y - separatorY) / gridY;

    std::vector<Module*> pasted;
    for (auto& entry : clipboard)
    {
        int dx = entry.gridPos.x - minX;
        int dy = entry.gridPos.y - minY;
        auto& container = patch->getContainer(pasteSection);
        int newY = findNearestFreeY(container, nullptr, pasteGridX + dx,
                                    pasteGridY + dy,
                                    moduleDescs->getModuleByIndex(entry.typeIndex)
                                        ? moduleDescs->getModuleByIndex(entry.typeIndex)->height : 1);
        auto* newMod = patch->createModule(pasteSection, entry.typeIndex,
                                           pasteGridX + dx, newY,
                                           entry.name, *moduleDescs);
        if (!newMod) { pasted.push_back(nullptr); continue; }

        auto& params = newMod->getParameters();
        for (size_t i = 0; i < entry.paramValues.size() && i < params.size(); ++i)
            params[i].setValue(entry.paramValues[i]);
        pasted.push_back(newMod);
    }

    // Recreate cables
    auto& container = patch->getContainer(pasteSection);
    for (auto& cb : clipboardCables)
    {
        if (cb.srcModuleClipIdx >= (int)pasted.size()) continue;
        if (cb.dstModuleClipIdx >= (int)pasted.size()) continue;
        auto* s = pasted[static_cast<size_t>(cb.srcModuleClipIdx)];
        auto* d = pasted[static_cast<size_t>(cb.dstModuleClipIdx)];
        if (!s || !d) continue;
        auto* sc = s->getConnector(cb.srcConnectorIdx, cb.srcIsOutput);
        auto* dc = d->getConnector(cb.dstConnectorIdx, cb.dstIsOutput);
        if (sc && dc) container.addConnection(sc, dc);
    }

    // Select pasted modules
    clearSelection();
    for (auto* m : pasted)
        if (m) selection.push_back({ m, pasteSection });

    repaint();
}

void PatchCanvas::showSelectionContextMenu()
{
    juce::PopupMenu menu;
    juce::String label = selection.size() == 1
        ? "1 module selected"
        : juce::String((int)selection.size()) + " modules selected";
    menu.addSectionHeader(label);
    menu.addItem(1, "Duplicate");
    menu.addItem(2, "Duplicate with Cables");
    menu.addItem(3, "Copy");
    menu.addItem(6, "Save as Snippet...");
    menu.addSeparator();
    menu.addItem(5, "Initialize");
    menu.addSeparator();
    menu.addItem(4, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options{}, [this](int result) {
        if (result == 1) duplicateSelection(false);
        else if (result == 2) duplicateSelection(true);
        else if (result == 3) copySelectionToClipboard();
        else if (result == 4) deleteSelection();
        else if (result == 5) {
            if (initModuleCallback) {
                if (undoManager)
                    undoManager->beginNewTransaction("Initialize Selection");
                for (auto& sel : selection)
                    if (sel.module)
                        initModuleCallback(sel.section, sel.module);
            }
        }
        else if (result == 6) saveSelectionAsSnippet();
    });
}

void PatchCanvas::saveSelectionAsSnippet()
{
    if (selection.empty() || !snippetSaveCallback_) return;

    copySelectionToClipboard();
    if (clipboard.empty()) return;

    SnipData snip;
    snip.name = "snippet";

    for (auto& e : clipboard)
    {
        SnipEntry se;
        se.typeIndex   = e.typeIndex;
        se.name        = e.name;
        se.section     = e.section;
        se.gridPos     = e.gridPos;
        se.paramValues = e.paramValues;
        snip.entries.push_back(std::move(se));
    }

    std::map<int, int> clipToSnip;
    std::vector<SnipEntry> filteredEntries;
    for (int i = 0; i < static_cast<int>(snip.entries.size()); ++i)
    {
        auto& entry = snip.entries[static_cast<size_t>(i)];
        if (isSnippetExcludedModuleType(entry.typeIndex))
            continue;

        clipToSnip[i] = static_cast<int>(filteredEntries.size());
        filteredEntries.push_back(std::move(entry));
    }
    snip.entries = std::move(filteredEntries);

    for (auto& cb : clipboardCables)
    {
        auto srcIt = clipToSnip.find(cb.srcModuleClipIdx);
        auto dstIt = clipToSnip.find(cb.dstModuleClipIdx);
        if (srcIt == clipToSnip.end() || dstIt == clipToSnip.end())
            continue;

        SnipCable sc;
        sc.srcIdx = srcIt->second;
        sc.srcConn = cb.srcConnectorIdx;
        sc.srcIsOutput = cb.srcIsOutput;
        sc.dstIdx = dstIt->second;
        sc.dstConn = cb.dstConnectorIdx;
        sc.dstIsOutput = cb.dstIsOutput;
        snip.cables.push_back(sc);
    }

    if (!snip.entries.empty())
        snippetSaveCallback_(std::move(snip));
}

// --- PatchCanvasComponent (two-panel split viewport) ---

PatchCanvasComponent::PatchCanvasComponent()
{
    setWantsKeyboardFocus(true);

    // Set sections before anything else
    polyCanvas.setSection(1);    // Poly (top)
    commonCanvas.setSection(0);  // Common (bottom)

    // Setup viewports
    polyViewport.setViewedComponent(&polyCanvas, false);
    polyViewport.setScrollBarsShown(true, true);

    commonViewport.setViewedComponent(&commonCanvas, false);
    commonViewport.setScrollBarsShown(true, true);

    addAndMakeVisible(polyViewport);
    addAndMakeVisible(resizerBar);
    addAndMakeVisible(commonViewport);

    // StretchableLayout: [polyViewport | resizerBar | commonViewport]
    // Initial 50/50 split
    layout.setItemLayout(0, 60, -1.0, -0.9);   // poly  (min 60px, preferred 90%)
    layout.setItemLayout(1, resizerThick, resizerThick, resizerThick);  // resizer
    layout.setItemLayout(2, 60, -1.0, -0.1);   // common (min 60px, preferred 10%)
}

bool PatchCanvasComponent::keyPressed(const juce::KeyPress& key)
{
    return PatchCanvas::handleMorphOverlayKey(key, *this);
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

    // Preset name text
    int presetIdx = 0;
    auto it = drumPresetState.find(m.getContainerIndex());
    if (it != drumPresetState.end())
        presetIdx = it->second;
    presetIdx = juce::jlimit(0, static_cast<int>(drumPresets.size()) - 1, presetIdx);

    juce::String presetName = drumPresets.empty() ? "none"
                              : drumPresets[static_cast<size_t>(presetIdx)].name;

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
// DrumSynth preset system
// p1=MTune p2=STune p3=MDecay p4=SDecay p5=MLevel p6=SLevel
// p7=Ffreq p8=Fres p9=Fswp p10=Fdcy p11=Fmode p12=Amt p13=Dcy p14=Click p15=Noise
// ============================================================

void PatchCanvas::initDrumPresets()
{
    drumPresets.clear();
    // Built-in 1: Basic Kick
    drumPresets.push_back({ "Basic Kick",
        { 60, 0, 80, 40, 100, 50,   // MTune=113Hz, STune=1:1, MDecay=long, SDecay=med, Mlevel=high, SLevel=mid
          30, 20, 40, 50, 0,         // Ffreq=low, Fres=low, Fswp=mid, Fdcy=mid, Fmode=HP
          50, 60, 60, 10 } });       // Amt=mid, BendDcy=mid, Click=high, Noise=low

    // Built-in 2: Basic Snare
    drumPresets.push_back({ "Basic Snare",
        { 48, 48, 40, 50, 80, 70,   // MTune=80Hz, STune=2:1, MDecay=med, SDecay=med, levels
          70, 40, 30, 40, 0,         // Ffreq=mid-high, Fres=mid, Fswp=low, Fdcy=mid, Fmode=HP
          30, 40, 80, 90 } });       // Amt=low, Dcy=med, Click=high, Noise=high
}

static juce::File getDrumPresetsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Nomad2026")
               .getChildFile("drum_presets.txt");
}

void PatchCanvas::saveDrumPresetsToFile()
{
    auto f = getDrumPresetsFile();
    f.getParentDirectory().createDirectory();
    juce::FileOutputStream out(f);
    if (!out.openedOk()) return;
    out.setPosition(0);
    out.truncate();

    // Skip built-in presets (first 2), save only user presets
    for (size_t i = 2; i < drumPresets.size(); ++i)
    {
        auto& p = drumPresets[i];
        out.writeText(p.name + "|", false, false, nullptr);
        for (int j = 0; j < 15; ++j)
            out.writeText(juce::String(p.params[static_cast<size_t>(j)]) + (j < 14 ? "," : ""), false, false, nullptr);
        out.writeText("\n", false, false, nullptr);
    }
}

void PatchCanvas::loadDrumPresetsFromFile()
{
    auto f = getDrumPresetsFile();
    if (!f.existsAsFile()) return;
    juce::StringArray lines;
    f.readLines(lines);
    for (auto& line : lines)
    {
        if (line.trim().isEmpty()) continue;
        auto parts = juce::StringArray::fromTokens(line, "|", "");
        if (parts.size() < 2) continue;
        DrumPreset dp;
        dp.name = parts[0].trim();
        auto vals = juce::StringArray::fromTokens(parts[1], ",", "");
        for (int i = 0; i < 15 && i < vals.size(); ++i)
            dp.params[static_cast<size_t>(i)] = vals[i].getIntValue();
        drumPresets.push_back(dp);
    }
}

void PatchCanvas::applyDrumPreset(Module& m, int section, int presetIdx)
{
    if (presetIdx < 0 || presetIdx >= static_cast<int>(drumPresets.size())) return;
    auto& preset = drumPresets[static_cast<size_t>(presetIdx)];

    // p1..p15 → parameterDescriptor index 0..14
    for (int i = 0; i < 15; ++i)
    {
        auto* param = findParameter(m, "p" + juce::String(i + 1));
        if (param == nullptr) continue;
        int newVal = juce::jlimit(0, 127, preset.params[static_cast<size_t>(i)]);
        int oldVal = param->getValue();
        param->setValue(newVal);
        if (parameterChangeCallback)
            parameterChangeCallback(section, m.getContainerIndex(), param->getDescriptor()->index, newVal);
        if (paramDragCompleteCallback)
            paramDragCompleteCallback(section, m.getContainerIndex(), param->getDescriptor()->index, oldVal, newVal);
    }
    repaint();
}

void PatchCanvas::showDrumPresetContextMenu(Module& m, int section)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save preset...");
    if (drumPresets.size() > 2)
    {
        menu.addSeparator();
        int id = 100;
        for (size_t i = 2; i < drumPresets.size(); ++i)
            menu.addItem(id++, "Delete \"" + drumPresets[i].name + "\"");
    }
    menu.showMenuAsync(juce::PopupMenu::Options{}, [this, &m, section](int result)
    {
        if (result == 1)
        {
            // Save current params as new preset
            auto* dialog = new juce::AlertWindow("Save Drum Preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
            dialog->addTextEditor("name", "My Preset", "");
            dialog->addButton("Save", 1);
            dialog->addButton("Cancel", 0);
            dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog, &m, section](int r)
            {
                if (r == 1)
                {
                    DrumPreset dp;
                    dp.name = dialog->getTextEditorContents("name").trim();
                    if (dp.name.isEmpty()) dp.name = "Preset " + juce::String(drumPresets.size() - 1);
                    for (int i = 0; i < 15; ++i)
                    {
                        auto* param = findParameter(const_cast<Module&>(m), "p" + juce::String(i + 1));
                        dp.params[static_cast<size_t>(i)] = (param != nullptr) ? param->getValue() : 64;
                    }
                    drumPresets.push_back(dp);
                    saveDrumPresetsToFile();
                    repaint();
                }
                delete dialog;
            }), true);
        }
        else if (result >= 100)
        {
            size_t idx = static_cast<size_t>(result - 100) + 2;
            if (idx < drumPresets.size())
            {
                drumPresets.erase(drumPresets.begin() + static_cast<int>(idx));
                saveDrumPresetsToFile();
                // Clamp any active preset index
                for (auto& kv : drumPresetState)
                    if (kv.second >= static_cast<int>(drumPresets.size()))
                        kv.second = static_cast<int>(drumPresets.size()) - 1;
                repaint();
            }
        }
    });
}
