#include "PatchCanvasComponent.h"
#include "../format/ValueFormatters.h"
#include "../protocol/KnobAssignmentMessage.h"
#include "BinaryData.h"
#include <cmath>
#include <unordered_map>

// PatchCanvas: everything that draws. Split out of PatchCanvasComponent.cpp,
// which had grown past nine thousand lines; this is the same code, moved.

static juce::Colour contrastingInk(juce::Colour background)
{
    return background.getPerceivedBrightness() > 0.5f
        ? juce::Colours::black : juce::Colours::white;
}

// A sequencer's per-step value controls, as opposed to its step count, its loop
// setting and its transport buttons. Both Rnd and Clr act on exactly these, so
// the rule lives in one place: they used to disagree, and Clr flattening the
// step count to 1 was the visible half of that (issue #34).
//   NoteSeqA/CtrlSeq → the vertical sliders
//   NoteSeqB         → the note ids inside the piano-roll editor
//   EventSeq         → binary step toggles, named "seq N, step M", which is what
//                      keeps the active/gate transport toggles out of it
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

static const juce::Image& canvasGrainTexture()
{
    static const juce::Image tex = []
    {
        const int N = 256;
        juce::Image img(juce::Image::ARGB, N, N, true);
        juce::Image::BitmapData bmp(img, juce::Image::BitmapData::writeOnly);
        juce::Random rng(0x9E3779B9);
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
            {
                float v = rng.nextFloat() * 2.0f - 1.0f;    // fine grain, no blur
                auto  a = (juce::uint8) juce::jlimit(0, 255, (int) (std::abs(v) * 8.0f));
                auto  c = (v < 0.0f ? juce::Colours::black : juce::Colours::white).withAlpha(a);
                bmp.setPixelColour(x, y, c);
            }
        return img;
    }();
    return tex;
}

void PatchCanvas::paint(juce::Graphics& g)
{
    // The cable preview below reads the drag's module and connector, and a
    // drag is the one thing here still held by pointer. Everything else names
    // its module by reference and cannot go stale (issue #61).
    dropDragIfModuleGone();

    g.fillAll(activeScheme_.gridBackground);

    if (activeScheme_.canvasTexture)
    {
        g.setTiledImageFill(canvasGrainTexture(), 0, 0, 1.0f);
        g.fillRect(g.getClipBounds());
    }

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

    // Where to centre the "empty canvas" hint. Deliberately NOT g.getClipBounds():
    // a partial repaint clips to the dirty rectangle, so centring there draws one
    // copy of the text per region that happens to be invalidated, and they pile
    // up on screen. The sub-windows made that obvious — they repaint in pieces
    // far more often than a single full-width canvas did.
    auto placeholderArea = [this]
    {
        auto visible = getLocalBounds();
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            visible = vp->getViewArea();
        return visible.toFloat() / juce::jmax(0.01f, zoomLevel);
    };

    // The hint is painted on the canvas background, so its ink has to be taken
    // from that background and not from moduleText: the themes with light module
    // faces (Nomad, Nord Classic) set moduleText to black, and their canvas is
    // nearly black too, which left the hint unreadable (#70). contrasting() picks
    // the readable end for whatever background the theme brings.
    auto drawPlaceholder = [this, &g, &placeholderArea]
    {
        // Not juce::Colour::contrasting(), which flips from dark ink to light at
        // exactly 0.5 perceived brightness: Nord Classic's canvas is #80808b,
        // sitting right on that boundary, and it came back a light grey on a mid
        // grey. Anything but a genuinely dark canvas takes the dark ink. The two
        // alphas differ because dark ink on a light ground fades sooner than
        // light ink does on a dark one; both land above the 3:1 that large text
        // wants while still reading as a hint rather than a heading.
        const bool darkCanvas = activeScheme_.gridBackground.getPerceivedBrightness() < 0.42f;
        g.setColour(darkCanvas ? juce::Colours::white.withAlpha(0.45f)
                               : juce::Colours::black.withAlpha(0.60f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", placeholderArea(), juce::Justification::centred, false);
    };

    if (patch == nullptr)
    {
        drawPlaceholder();
        return;
    }

    // Each canvas instance is section-specific (mySection 0=common, 1=poly).
    // yOffset is always 0 since each canvas starts from the top.
    auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();

    if (container.getModules().empty())
        drawPlaceholder();

    paintComments(g);
    paintModules(g, container, 0);
    paintCables(g, container, 0);
    paintOverlays(g, container, 0);
    spinner.paint(g, { activeScheme_.resetBg, activeScheme_.resetBorder, activeScheme_.resetText });
    paintHoverBadge(g);
    paintDragValueBadge(g);

    // The answer to a double-click on a module, kept up until the next click.
    if (const auto* costModule = resolve(costBadgeModule))
        if (auto* desc = costModule->getDescriptor())
            paintModuleCostBadge(g, getModuleBounds(*costModule, 0),
                                 desc->fullname + "  " + formatDspCost(desc->cycles));

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

        // Check if cursor is hovering a valid destination connector.
        // output→input and input→input (chained) are valid; output→output is not.
        auto hit = findConnectorAt(cablePreviewEnd);
        bool validTarget = hit.connector != nullptr
                        && hit.connector != dragState.sourceConnector
                        && hit.section == dragState.section
                        && srcDesc != nullptr
                        && !(srcDesc->isOutput && hit.connector->getDescriptor()->isOutput);

        // Joining two nets that already have distinct driving outputs is invalid
        if (validTarget && patch != nullptr)
        {
            auto& cont = patch->getContainer(dragState.section);
            auto* drv1 = cont.findNetOutput(dragState.sourceConnector);
            auto* drv2 = cont.findNetOutput(hit.connector);
            if (drv1 != nullptr && drv2 != nullptr && drv1 != drv2)
                validTarget = false;
        }

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

    // Module drop preview, while dragging one in from the module browser
    if (showModuleDropPreview)
        paintGhostOutline(g, dropPreviewTypeId, dropPreviewGridX, dropPreviewGridY);

    // Modules hanging off the pointer after Paste or Add Module, waiting for
    // the click that puts them down
    if (pendingDrop.active() && pendingHost == this)
        for (auto& ghost : pendingDrop.ghosts)
            paintGhostOutline(g, ghost.typeIndex,
                              pendingGrid.x + ghost.dx, pendingGrid.y + ghost.dy,
                              ghost.w, ghost.h);
}

void PatchCanvas::paintGhostOutline(juce::Graphics& g, int typeIndex, int gx, int gy,
                                    int gw, int gh) const
{
    // The editor's own text note has no descriptor, so it carries its size here.
    if (typeIndex == PendingDrop::commentGhost)
    {
        juce::Rectangle<int> bounds(gx * gridX, gy * gridY,
                                    juce::jmax(1, gw) * gridX,
                                    juce::jmax(1, gh) * gridY);

        g.setColour(juce::Colours::cyan.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
        g.setColour(juce::Colours::cyan.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.toFloat(), 3.0f, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Comment", bounds.reduced(4, 4), juce::Justification::centred, true);
        return;
    }

    if (moduleDescs == nullptr)
        return;

    auto* descriptor = moduleDescs->getModuleByIndex(typeIndex);
    if (descriptor == nullptr)
        return;

    juce::Rectangle<int> bounds(gx * gridX, gy * gridY, gridX, descriptor->height * gridY);

    g.setColour(juce::Colours::cyan.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);

    g.setColour(juce::Colours::cyan.withAlpha(0.8f));
    g.drawRoundedRectangle(bounds.toFloat(), 3.0f, 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(descriptor->fullname, bounds.reduced(4, 4), juce::Justification::centred, true);
}

void PatchCanvas::paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    // Validated once for the whole pass. Nothing can change the patch while a
    // paint is running, and checking per module made the check itself the cost.
    const auto& lightSlots = lightRangeTable();

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
            paintModuleThemed(g, m, mySection, rect, *theme, container,
                              lightSlots.find(mySection, m.getContainerIndex()));
        else
            paintModuleFallback(g, m, rect);

    }
}

void PatchCanvas::paintOverlays(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    if (overlayMode == OverlayMode::Off || themeData == nullptr)
        return;

    for (auto& modulePtr : container.getModules())
    {
        auto& m = *modulePtr;
        auto rect = getModuleBounds(m, yOffset);
        if (!g.getClipBounds().intersects(rect))
            continue;

        // The cost readout is per module, not per control, so it does not go
        // through the theme walk the other modes use.
        if (overlayMode == OverlayMode::ModuleCosts)
        {
            if (auto* desc = m.getDescriptor())
                paintModuleCostBadge(g, rect, formatDspCost(desc->cycles));
            continue;
        }

        if (const auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId))
            paintOverlay(g, m, rect, *theme);
    }
}

void PatchCanvas::paintModuleThemed(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container,
                                    const LightMeterLayout::ModuleSlots* lightSlots)
{
    paintModuleBackground(g, m, bounds, theme);
    paintCustomDisplays(g, m, bounds, theme);
    paintLabels(g, m, bounds, theme);
    paintTextDisplays(g, m, bounds, theme);
    paintSliders(g, m, bounds, theme);
    // Decorations (signal-flow lines/symbols) sit beneath knobs and buttons so
    // overlapping controls (e.g. GainControl's shift button crossing dec-2) stay
    // visible on top.
    paintStaticIcons(g, m, bounds, theme);
    paintKnobs(g, m, bounds, theme);
    auto bgForButtons = activeScheme_.moduleBg.isOpaque()
        ? activeScheme_.moduleBg
        : m.getDescriptor()->background;
    paintButtons(g, m, bounds, theme, bgForButtons);
    paintResetButtons(g, m, bounds, theme);
    paintConnectors(g, m, bounds, theme, container);
    paintLights(g, m, section, bounds, theme, lightSlots);
    if (m.getDescriptor()->index == 58)
        paintDrumSynthExtras(g, m, bounds);
}

void PatchCanvas::paintOverlay(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    if (overlayMode == OverlayMode::Off)
        return;

    juce::StringArray seen;

    auto paintControl = [&](const juce::String& componentId, juce::Rectangle<float> controlBounds)
    {
        if (componentId.isEmpty() || seen.contains(componentId))
            return;

        seen.add(componentId);
        auto* param = findParameter(m, componentId);
        if (param == nullptr)
            return;

        // F7 is about morph assignments, so it only has something to say about
        // assigned parameters. F5 reads out the whole patch, as the original
        // editor's does, so it draws every control (issue #32).
        const bool assigned = param->getMorphGroup() >= 0 && param->getMorphGroup() < 4;
        if (overlayMode == OverlayMode::MorphGroups && !assigned)
            return;

        paintOverlayBadge(g, controlBounds, bounds, m, *param);
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

// Which control the cursor is over, in the same order and over the same control
// kinds the F5 readout draws, so hovering and the readout can never disagree
// about what is a parameter.
void PatchCanvas::paintDragValueBadge(juce::Graphics& g)
{
    if (overlayMode == OverlayMode::Values)
        return;   // the whole patch is already reading out, this one included

    const bool draggingParam = dragState.type == DragState::Knob
                            || dragState.type == DragState::Slider
                            || dragState.type == DragState::Button
                            || dragState.type == DragState::MorphRange;
    if (!draggingParam || dragState.module == nullptr || dragState.parameter == nullptr)
        return;

    auto* pd = dragState.parameter->getDescriptor();
    if (pd == nullptr)
        return;

    juce::Rectangle<float> control;
    juce::Rectangle<int> mod;
    if (!controlBoundsFor(*dragState.module, pd->componentId, control, mod))
        return;

    paintOverlayBadge(g, control, mod, *dragState.module, *dragState.parameter,
                      getParameterValueText(*dragState.parameter));
}

void PatchCanvas::paintHoverBadge(juce::Graphics& g)
{
    // The F5 readout already covers every control, so a hover box on top of it
    // would just be a second copy of the same number.
    if (!hoverBadgeVisible || overlayMode == OverlayMode::Values)
        return;

    // Hovering reads out controls only. The module's own cost is asked for by
    // double-clicking it, the way the original editor does: crossing modules is
    // something the cursor does constantly, and a cost box following it around
    // would be noise rather than an answer.
    if (hoverTarget.componentId.isEmpty())
        return;

    const auto* hoverModule = resolve(hoverTarget.module);
    if (hoverModule == nullptr)
        return;

    auto* param = findParameter(*hoverModule, hoverTarget.componentId);
    if (param == nullptr)
        return;

    juce::String text = getParameterValueText(*param);

    // On a display that rotates its units, the readout answers what the value
    // is in the units the box is NOT showing, which is what the original puts
    // in its tooltip (issue #30).
    if (const auto* unitsParam = freqUnitsParamFor(*hoverModule, hoverTarget.componentId))
    {
        const auto& m = *hoverModule;
        juce::String baseFormatter = param->getDescriptor()->formatter;
        if (const auto* theme = themeData != nullptr
                ? themeData->getModuleTheme(m.getDescriptor()->componentId) : nullptr)
            for (const auto& td : theme->textDisplays)
                if (td.componentId == hoverTarget.componentId && td.formatterOverride.isNotEmpty())
                    baseFormatter = td.formatterOverride;

        const auto units = freqUnitsFor(m, hoverTarget.componentId, baseFormatter);
        if (units.size() > 1)
        {
            const int shown = juce::jlimit(0, units.size() - 1, unitsParam->getValue());
            juce::StringArray others;
            for (int i = 0; i < units.size(); ++i)
                if (i != shown)
                    others.add(formatInFreqUnit(m, *param, units.getReference(i)));
            text = others.joinIntoString("  ");
        }
    }

    paintOverlayBadge(g, hoverTarget.controlBounds, hoverTarget.moduleBounds,
                      *hoverModule, *param, text);
}

// The module-level twin of paintOverlayBadge: same box, but anchored to the
// module's top edge rather than to a control inside it.
void PatchCanvas::paintModuleCostBadge(juce::Graphics& g, juce::Rectangle<int> moduleBounds,
                                       const juce::String& text)
{
    g.setFont(juce::FontOptions("Fira Sans", 10.0f, juce::Font::bold));
    const float badgeH = 14.0f;
    const float badgeW = juce::jlimit(18.0f, 110.0f,
                                      g.getCurrentFont().getStringWidthFloat(text) + 10.0f);

    juce::Rectangle<float> badge(moduleBounds.toFloat().getRight() - badgeW - 4.0f,
                                 moduleBounds.toFloat().getY() + 3.0f, badgeW, badgeH);

    g.setColour(juce::Colour(0xff1c2229).withAlpha(0.92f));
    g.fillRoundedRectangle(badge, 3.0f);
    g.setColour(juce::Colour(0xff1c2229).darker(0.25f));
    g.drawRoundedRectangle(badge, 3.0f, 1.4f);
    g.setColour(juce::Colours::white);
    g.drawText(text, badge.toNearestInt(), juce::Justification::centred, false);
}

void PatchCanvas::paintOverlayBadge(juce::Graphics& g, juce::Rectangle<float> controlBounds,
                                         juce::Rectangle<int> moduleBounds, const Module& m,
                                         const Parameter& param,
                                         const juce::String& textOverride)
{
    auto text = textOverride.isNotEmpty() ? textOverride : getOverlayText(m, param);
    if (text.isEmpty())
        return;

    g.setFont(juce::FontOptions("Fira Sans", 10.0f, juce::Font::bold));
    const float badgeH = 14.0f;
    // Measured rather than estimated from the character count: a formatted value
    // ("2.30kHz", "-63.5 dB") is far wider than the "+34" this used to show.
    const float badgeW = juce::jlimit(18.0f, 82.0f,
                                      g.getCurrentFont().getStringWidthFloat(text) + 10.0f);
    float bx = controlBounds.getCentreX() - badgeW * 0.5f;
    float by = controlBounds.getY() - badgeH - 2.0f;

    const auto module = moduleBounds.toFloat().reduced(3.0f, 2.0f);
    bx = juce::jlimit(module.getX(), module.getRight() - badgeW, bx);
    if (by < module.getY())
        by = controlBounds.getBottom() + 2.0f;
    by = juce::jlimit(module.getY(), module.getBottom() - badgeH, by);

    juce::Rectangle<float> badge(bx, by, badgeW, badgeH);
    // A morphed parameter keeps its group's colour, which is information worth
    // carrying. Everything else gets a neutral box: with the whole patch read
    // out at once, white badges everywhere drowned the modules underneath.
    const int group = param.getMorphGroup();
    const juce::Colour fillColor = (group >= 0 && group < 4)
        ? activeScheme_.morphColor[group].withAlpha(0.92f)
        : juce::Colour(0xff1c2229).withAlpha(0.92f);
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

juce::String PatchCanvas::getOverlayText(const Module& m, const Parameter& param) const
{
    const int group = param.getMorphGroup();

    if (overlayMode == OverlayMode::MorphGroups)
        return (group >= 0 && group < 4) ? "M" + juce::String(group + 1) : juce::String();

    if (overlayMode == OverlayMode::Values)
        return getParameterValueText(param);

    // Assignment readouts. Both are reverse lookups: the patch stores them by
    // knob and by controller, and here we have the parameter and want to know
    // what points at it.
    auto* pd = param.getDescriptor();
    if (pd == nullptr || patch == nullptr)
        return {};
    const int moduleIdx = m.getContainerIndex();

    if (overlayMode == OverlayMode::Knobs)
    {
        for (size_t k = 0; k < patch->knobAssignments.size(); ++k)
        {
            const auto& ka = patch->knobAssignments[k];
            if (ka.assigned && ka.section == mySection
                && ka.module == moduleIdx && ka.param == pd->index)
                return KnobAssignmentMessage::getKnobName(static_cast<int>(k));
        }
        return {};
    }

    if (overlayMode == OverlayMode::MidiCtrls)
    {
        for (const auto& ca : patch->ctrlAssignments)
            if (ca.section == mySection && ca.module == moduleIdx && ca.param == pd->index)
                return "CC " + juce::String(ca.control);
        return {};
    }

    return {};
}

// Values are read out in the parameter's own units, the same way its display box
// would show them, so a cutoff reads "440 Hz" rather than "64". A morphed
// parameter reads out the span the morph sweeps it across, from where it sits to
// where the morph takes it, which is what the original editor shows
// ("46Hz-2.30kHz"). Shared by the F5 readout and the hover hint box so the two
// can never word the same parameter differently.
juce::Colour PatchCanvas::wireframeInk(const Module& m) const
{
    // Opaque-moduleBg themes (Dark, Nord, …) already use a light text colour that
    // reads on the canvas. Classic-style themes leave moduleBg transparent and the
    // text is dark (meant for the light module fill we no longer draw), so fall back
    // to the module's own XML colour — brightened so thin strokes/labels stay legible.
    if (activeScheme_.moduleBg.isOpaque())
        return activeScheme_.moduleText;
    return m.getDescriptor()->background.brighter(0.35f);
}

void PatchCanvas::paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    auto bgColour = activeScheme_.moduleBg.isOpaque()
        ? activeScheme_.moduleBg
        : m.getDescriptor()->background;

    const bool wire = activeScheme_.wireframe;

    // Module body (flat background, no title band). Wireframe: no fill — the
    // canvas grid shows through and the edge lines below form the outline.
    if (!wire)
    {
        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
    }

    // GroupBoxes — rounded rect section borders (e.g. PWidth in OscA)
    for (auto& gb : theme.groupBoxes)
    {
        auto gbRect = juce::Rectangle<float>(
            static_cast<float>(bounds.getX() + gb.x),
            static_cast<float>(bounds.getY() + gb.y),
            static_cast<float>(gb.width),
            static_cast<float>(gb.height));
        if (!wire)
        {
            g.setColour(bgColour.darker(0.25f));
            g.fillRoundedRectangle(gbRect, 3.0f);
        }
        g.setColour(wire ? activeScheme_.groupBoxBorder : bgColour.darker(0.5f));
        g.drawRoundedRectangle(gbRect, 3.0f, 1.0f);
    }

    // Module name (no band, text directly on background)
    auto titleBar = juce::Rectangle<int>(bounds.getX(), bounds.getY() + 2, bounds.getWidth(), 12);
    g.setColour(wire ? wireframeInk(m) : activeScheme_.moduleText);
    g.setFont(juce::FontOptions("Fira Sans", 12.5f, juce::Font::bold));
    g.drawText(m.getTitle(), titleBar.reduced(4, 0), juce::Justification::centredLeft, true);

    // Patch Mutator: red frame marks modules excluded from mutation (G2 behavior)
    if (mutatorModeOn && m.isExcludedFromMutation())
    {
        g.setColour(juce::Colour(0xffcc3333));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 3.0f, 2.0f);
    }

    if (wire)
    {
        // Without a body fill the 1px edge lines vanish, so draw a crisp rounded
        // frame in the (near-white) module text colour — brighter than the inner
        // group-box outlines, so module bounds read clearly.
        g.setColour(wireframeInk(m).withAlpha(0.7f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 3.0f, 1.2f);
    }
    else
    {
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
    }

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

        // Color from the descriptor's signal type, so the jack always matches
        // the cables plugged into it (the theme CSS class disagrees with the
        // descriptor on 43 connectors — audit 2026-06-11). CSS class is kept
        // as a fallback for theme connectors without a descriptor match.
        juce::Colour connColour = juce::Colours::white;
        if (actualConnector != nullptr)
            connColour = getSignalColour(actualConnector->getDescriptor()->signalType);
        else if (tc.cssClass == "cAUDIO")   connColour = activeScheme_.cableAudio;
        else if (tc.cssClass == "cCONTROL") connColour = activeScheme_.cableControl;
        else if (tc.cssClass == "cLOGIC")   connColour = activeScheme_.cableLogic;
        else if (tc.cssClass == "cSLAVE")   connColour = activeScheme_.cableMasterSlave;
        else if (tc.cssClass == "cUSER1")   connColour = activeScheme_.cableUser1;
        else if (tc.cssClass == "cUSER2")   connColour = activeScheme_.cableUser2;

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
        const ConnectorDescriptor* cd = nullptr;
        for (auto& conn : m.getConnectors())
            if (conn.getDescriptor() && conn.getDescriptor()->componentId == tc.componentId)
                { cd = conn.getDescriptor(); break; }
        if (cd == nullptr || cd->isOutput || cd->signalType == SignalType::MasterSlave)
            continue;

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
    g.setColour(activeScheme_.wireframe ? wireframeInk(m) : activeScheme_.moduleText);
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
                       label.width, label.height,
                       label.centred ? juce::Justification::centred
                                     : juce::Justification::centredLeft, true);
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

        // Background circle — morph group color if assigned, grey otherwise.
        // Wireframe: no fill — the ring (drawn below in baseColor) carries the
        // morph-group color so assignments stay readable.
        if (!activeScheme_.wireframe)
        {
            g.setColour(baseColor);
            g.fillEllipse(rcx, rcy, rSz, rSz);
        }

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

        // Outline — in wireframe a morph knob keeps its group hue, but a plain
        // knob uses the dark module ink (knobBase would vanish on a light-grey
        // canvas like Nord Classic); non-wireframe keeps the normal knob border.
        g.setColour(activeScheme_.wireframe ? (hasMorph ? baseColor : wireframeInk(m))
                                            : (hasMorph ? baseColor.darker(0.4f) : activeScheme_.knobBorder));
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

        // Grip indicator: knobGrip is a dark fill detail that disappears on an
        // unfilled wireframe knob, so use the bright module text colour there.
        g.setColour(activeScheme_.wireframe ? wireframeInk(m)
                                            : hasMorph ? contrastingInk(baseColor)
                                                       : activeScheme_.knobGrip);
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

        // Morph-assigned buttons get the group color, like knobs do (issue #16):
        // selected segment filled with it and a thicker outer border around the
        // control so the assignment reads at a glance.
        int morphGroup = (param != nullptr) ? param->getMorphGroup() : -1;
        bool hasMorph = (morphGroup >= 0 && morphGroup < 4);
        juce::Colour morphCol = hasMorph ? activeScheme_.morphColor[morphGroup]
                                         : activeScheme_.buttonBorder;

        // --- Increment buttons: draw arrow pairs ---
        if (tb.isIncrement)
        {
            g.setColour(activeScheme_.incrementBg);
            g.fillRect(bx, by, bw, bh);
            g.setColour(hasMorph ? morphCol : activeScheme_.incrementBorder);
            g.drawRect(bx, by, bw, bh, hasMorph ? 2.0f : 1.0f);

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

                juce::Colour base  = selected ? (hasMorph ? morphCol
                                                          : moduleBg.brighter(0.25f).withSaturation(0.5f))
                                              : moduleBg.darker(0.15f);
                // The four group colors span light and dark, so pick the label
                // shade from the fill instead of the theme (yellow needs dark text)
                juce::Colour label = selected ? (hasMorph ? morphCol.contrasting(0.8f)
                                                          : activeScheme_.buttonTextActive)
                                              : activeScheme_.buttonText;
                drawBevelSegment(segX, segY, segW, segH, selected, base, segLabel, label, segIcon);
            }

            // Outer border
            g.setColour(morphCol);
            g.drawRect(bx, by, bw, bh, hasMorph ? 2.0f : 1.0f);
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

            g.setColour(morphCol);
            g.drawRect(sx, sy, sq, sq, hasMorph ? 2.0f : 1.0f);
            continue;
        }

        juce::Colour base      = isOn ? (hasMorph ? morphCol
                                                  : moduleBg.brighter(0.2f).withSaturation(0.4f))
                                      : moduleBg.darker(0.15f);
        juce::Colour labelCol  = isOn ? (hasMorph ? morphCol.contrasting(0.8f)
                                                  : activeScheme_.buttonTextActive)
                                      : activeScheme_.buttonText;
        drawBevelSegment(bx, by, bw, bh, isOn, base, labelText, labelCol, iconRef);

        // Outer border
        g.setColour(morphCol);
        g.drawRect(bx, by, bw, bh, hasMorph ? 2.0f : 1.0f);
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

        // Draw grip — morph-assigned sliders show the group color, like knobs
        int morphGroup = (param != nullptr) ? param->getMorphGroup() : -1;
        bool hasMorph = (morphGroup >= 0 && morphGroup < 4);
        g.setColour(hasMorph ? activeScheme_.morphColor[morphGroup] : activeScheme_.resetText);
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

        if (!activeScheme_.wireframe)
        {
            g.setColour(activeScheme_.displayBg);
            g.fillRect(dx, renderY, dw, renderH);
        }

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

            // Displays with a units setting read the same value through whichever
            // unit the patch has stored for them (issue #30).
            const auto units = freqUnitsFor(m, td.componentId, fmtName);
            if (!units.isEmpty())
            {
                const auto* unitsParam = freqUnitsParamFor(m, td.componentId);
                const int chosen = juce::jlimit(0, units.size() - 1, unitsParam->getValue());
                displayStr = formatInFreqUnit(m, *param, units.getReference(chosen));
            }
            else
            {
                displayStr = ValueFormatters::format(fmtName, val);
            }

            // White reads on the dark display fill; with no fill (wireframe) use
            // the module text colour so it stays legible on any canvas.
            g.setColour(activeScheme_.wireframe ? wireframeInk(m) : juce::Colours::white);
            g.setFont(juce::FontOptions(8.5f));
            g.drawText(displayStr,
                       static_cast<int>(dx), static_cast<int>(renderY),
                       static_cast<int>(dw), static_cast<int>(renderH),
                       juce::Justification::centred, true);
        }

        // Partial format: draw ◄ ► arrow buttons below the display box
        if (td.partialArrows)
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

void PatchCanvas::paintStaticIcons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    const juce::Colour iconInk = activeScheme_.wireframe ? wireframeInk(m) : activeScheme_.moduleText;
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
                auto img = getTintedDecoration(si.iconName, iconInk);
                if (img.isValid())
                {
                    g.drawImage(img, juce::Rectangle<float>(ix, iy, iw, ih),
                                juce::RectanglePlacement::stretchToFit);
                    continue;
                }
            }

            g.setColour(iconInk);
            drawButtonIcon(g, si.iconName, ix, iy, iw, ih, iconInk);
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

        // Semi-transparent black rounded box (skipped in wireframe — outline only)
        if (!activeScheme_.wireframe)
        {
            g.setColour(juce::Colour(0x66000000));
            g.fillRoundedRectangle(bx, by, bw, bh, 3.0f);
        }
        g.setColour(activeScheme_.wireframe ? iconInk.withAlpha(0.6f)
                                            : juce::Colour(0x66ffffff));
        g.drawRoundedRectangle(bx, by, bw, bh, 3.0f, 1.0f);

        // Waveform icon — white on the dark box; wireframe ink (per-module) otherwise.
        drawButtonIcon(g, si.iconName, ix, iy, iw, ih,
                       activeScheme_.wireframe ? iconInk : juce::Colours::white);
    }
}

void PatchCanvas::paintLights(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme,
                              const LightMeterLayout::ModuleSlots* slots)
{
    // Where this module's LEDs and meters sit in the global arrays. The slots
    // are handed in: looking them up here meant validating the whole table once
    // per module per paint, which is the same quadratic cost the cache was
    // added to remove, only cheaper per unit.
    const int ledBase   = slots != nullptr ? slots->lightBase : 0;
    const int meterBase = slots != nullptr ? slots->meterBase : 0;

    // Build map of meter vertical centers (for LED alignment) and meter index
    // per component-id. NOMAD's LightProcessor gives every meter/led-array
    // module a pair of global slots in wire order: even = channel B, odd =
    // channel A. A stereo module's first light (left) reads A and its second
    // (right) reads B; a module with a single meter light gets its value on B.
    std::map<juce::String, float> meterCenterY;
    std::map<juce::String, int>   meterGlobalIdx;
    juce::StringArray meterIds;
    for (auto& tl : theme.lights)
    {
        if (tl.type == "meter")
        {
            float cy = static_cast<float>(bounds.getY() + tl.y) + tl.height * 0.5f;
            // Only store first occurrence for center (multiple meters per channel use same id)
            if (meterCenterY.find(tl.componentId) == meterCenterY.end())
                meterCenterY[tl.componentId] = cy;
            meterIds.addIfNotAlreadyThere(tl.componentId);
        }
    }
    if (meterIds.size() >= 2)
    {
        meterGlobalIdx[meterIds[0]] = meterBase + 1;  // left  -> channel A (odd slot)
        meterGlobalIdx[meterIds[1]] = meterBase;      // right -> channel B (even slot)
    }
    else if (meterIds.size() == 1)
    {
        meterGlobalIdx[meterIds[0]] = meterBase;      // single light -> channel B
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
                // Some classic-theme modules (AudioIn) ship their own printed dB
                // scale as numeric labels between the meters — drawing the
                // synthetic scale on top produces overlapping digits.
                bool themeHasPrintedScale = false;
                for (auto& lbl : theme.labels)
                {
                    if (lbl.text.isNotEmpty()
                        && lbl.text.containsOnly("0123456789")
                        && lbl.x >= tl.x - 8 && lbl.x <= tl.x + tl.width + 8
                        && std::abs(lbl.y - tl.y) <= 16)
                    {
                        themeHasPrintedScale = true;
                        break;
                    }
                }

                if (!themeHasPrintedScale)
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
    if (cableOpacity < 0.01f)
        return;

    // Connector-to-module lookup, rebuilt per paint. It is a sorted vector
    // rather than a std::map because a map node is a heap allocation and a
    // large patch has a couple of thousand connectors: that was a couple of
    // thousand allocations on every repaint, including the small per-module
    // ones the LEDs trigger many times a second. The buffer is a member, so
    // after the first paint it stops allocating altogether.
    //
    // Deliberately NOT cached across paints: the keys are raw pointers into
    // modules that a delete can destroy, and a freed connector buffer whose
    // memory gets reused would turn a stale entry into a wrong answer
    // (issue #61's family). Rebuilt inside one paint, it cannot go stale.
    auto& owners = cableOwnerScratch_;
    owners.clear();
    for (auto& modulePtr : container.getModules())
        for (auto& c : modulePtr->getConnectors())
            owners.push_back({ &c, modulePtr.get() });
    std::sort(owners.begin(), owners.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    auto findOwner = [&owners](const Connector* conn) -> const Module*
    {
        const auto it = std::lower_bound(owners.begin(), owners.end(), conn,
                                         [](const auto& entry, const Connector* key)
                                         { return entry.first < key; });
        return (it != owners.end() && it->first == conn) ? it->second : nullptr;
    };

    const auto clip = g.getClipBounds();

    for (auto& conn : container.getConnections())
    {
        if (conn.output == nullptr || conn.input == nullptr)
            continue;

        // The cable a re-route is carrying is still in the patch, and stays
        // there until the drop lands. Hiding it here is the whole illusion of
        // it having been unplugged, and it costs the patch nothing if the drag
        // comes to nothing. Gated on the drag as well as on the note, so a drag
        // that ends any way other than a drop cannot leave a cable invisible.
        if (dragState.rerouting && liftedCable.isValid()
            && conn.output == liftedCable.out && conn.input == liftedCable.in)
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
        const Module* srcModule = findOwner(conn.output);
        const Module* dstModule = findOwner(conn.input);
        if (srcModule == nullptr || dstModule == nullptr)
            continue;

        auto srcPos = getConnectorPosition(*srcModule, *conn.output, yOffset);
        auto dstPos = getConnectorPosition(*dstModule, *conn.input, yOffset);

        // Skip cables fully outside the clip region — the common case when a
        // light/meter update repaints a single module's rectangle. The sag
        // margin covers the deepest curve (shakeCables multiplier ≤ 1.6).
        {
            float baseSag = std::abs(static_cast<float>(srcPos.x - dstPos.x)) * 0.15f + 15.0f;
            int sagMargin = juce::roundToInt(baseSag * 1.6f) + 8;
            auto cableBounds = juce::Rectangle<int>::leftTopRightBottom(
                    juce::jmin(srcPos.x, dstPos.x), juce::jmin(srcPos.y, dstPos.y),
                    juce::jmax(srcPos.x, dstPos.x), juce::jmax(srcPos.y, dstPos.y))
                .expanded(8, sagMargin);
            if (!clip.intersects(cableBounds))
                continue;
        }

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
