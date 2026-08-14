#include "PatchCanvasComponent.h"
#include "QuickAddPopup.h"
#include "KnobDrag.h"
#include "../format/ValueFormatters.h"
#include "../protocol/KnobAssignmentMessage.h"
#include "BinaryData.h"
#include <cmath>
#include <set>
#include <unordered_map>

PatchCanvas::PendingDrop PatchCanvas::pendingDrop;

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

    for (const auto& r : computeModuleLightRanges())
    {
        if (r.section != mySection)
            continue;

        bool dirty = false;
        for (int i = 0; !dirty && i < r.lightCount && r.lightBase + i < 128; ++i)
            dirty = lightChanged[r.lightBase + i];
        for (int i = 0; !dirty && i < r.meterCount && r.meterBase + i < 128; ++i)
            dirty = meterChanged[r.meterBase + i];

        if (dirty)
        {
            auto rf = getModuleBounds(*r.mod, 0).toFloat();
            rf *= zoomLevel;
            repaint(rf.getSmallestIntegerContainer().expanded(2));
        }
    }
}

std::vector<PatchCanvas::ModuleLightRange> PatchCanvas::computeModuleLightRanges() const
{
    std::vector<ModuleLightRange> ranges;
    if (patch == nullptr || themeData == nullptr) return ranges;

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

    int lightBase = 0;
    int meterBase = 0;
    for (auto& ref : ordered)
    {
        int lightCount = 0;
        int meterCount = 0;

        // Count lights/meters for this module from its theme
        auto compId = ref.mod->getDescriptor() ? ref.mod->getDescriptor()->componentId : juce::String();
        if (const ModuleTheme* theme = themeData->getModuleTheme(compId))
        {
            bool hasMeterOrLedArray = false;
            int meterSlots = 0;
            for (auto& light : theme->lights)
            {
                if (light.type == "meter" || light.type == "led-array")
                {
                    hasMeterOrLedArray = true;
                    if (light.type == "meter")
                        ++meterSlots;
                }
                if (light.type == "led")
                    ++lightCount;
            }

            // NOMAD registers meters and sequencer led-arrays as MeterMessage
            // pairs. A single led-array/single meter still consumes two slots.
            if (hasMeterOrLedArray)
                meterCount = juce::jmax(2, meterSlots);
        }

        ranges.push_back({ ref.mod, ref.section, lightBase, lightCount, meterBase, meterCount });
        lightBase += lightCount;
        meterBase += meterCount;
    }
    return ranges;
}

int PatchCanvas::computeModuleLightIndex(const Module& targetModule, int targetSection, bool forMeters) const
{
    for (const auto& r : computeModuleLightRanges())
        if (r.mod == &targetModule && r.section == targetSection)
            return forMeters ? r.meterBase : r.lightBase;
    return 0;
}

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, sectionHeight);
    setWantsKeyboardFocus(true);
    liveCanvases.push_back(this);
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
    // for what to cut in a patch that is over budget.
    if (key == juce::KeyPress::F10Key) return toggle(OverlayMode::ModuleCosts);

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
        auto gpos = sel.module->getPosition();
        int px = gpos.x * gridX;
        int py = gpos.y * gridY;
        int ph = sel.module->getDescriptor() ? sel.module->getDescriptor()->height * gridY : 60;
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
    // Reset all raw pointers into the old patch to avoid dangling refs
    dragState = DragState();
    selection.clear();
    selectedModule = nullptr;
    selectedSection = -1;
    // Comment ids belong to the outgoing patch; the incoming one reuses them.
    selectedCommentIds.clear();
    commentMoveState.clear();
    dragCommentId = -1;
    // Keyed by container index, so leaving it would name modules in the
    // incoming patch after whatever sat at the same index in the outgoing one.
    drumPresetState.clear();
    clearHover();   // hoverTarget holds a Module* into the outgoing patch
    costBadgeModule = nullptr;
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

juce::Rectangle<int> PatchCanvas::getCommentBounds(const PatchComment& c) const
{
    return { c.x * gridX, c.y * gridY, c.gridWidth() * gridX, c.gridHeight() * gridY };
}

PatchComment* PatchCanvas::getCommentAt(juce::Point<int> canvasPos)
{
    if (patch == nullptr)
        return nullptr;

    for (auto& c : patch->getComments())
        if (c.section == mySection && getCommentBounds(c).contains(canvasPos))
            return &c;
    return nullptr;
}

PatchCanvas::CommentGrip PatchCanvas::commentGripAt(const PatchComment& c,
                                                    juce::Point<int> canvasPos) const
{
    auto bounds = getCommentBounds(c);
    const int s = commentGripSize;

    if (juce::Rectangle<int>(bounds.getRight() - s, bounds.getBottom() - s, s, s)
            .contains(canvasPos))
        return CommentGrip::BottomRight;

    if (juce::Rectangle<int>(bounds.getX(), bounds.getBottom() - s, s, s)
            .contains(canvasPos))
        return CommentGrip::BottomLeft;

    return CommentGrip::None;
}

// The text fills the panel: bold, centred, and at whatever size the box can
// carry, so widening a note makes its words bigger rather than just adding
// empty paper around them.
void PatchCanvas::drawCommentText(juce::Graphics& g, const juce::String& text,
                                  juce::Rectangle<int> bounds, juce::Colour colour) const
{
    if (text.isEmpty())
        return;

    auto area = bounds.reduced(8, 6);
    if (area.getWidth() < 8 || area.getHeight() < 8)
        return;

    // How many lines the note is written on decides how tall each one may be;
    // the width then caps it, so a long line shrinks instead of being clipped.
    juce::StringArray lines;
    lines.addLines(text);
    const int lineCount = juce::jmax(1, lines.size());

    float size = static_cast<float>(area.getHeight()) / (lineCount * 1.25f);

    int longest = 0;
    for (const auto& line : lines)
        longest = juce::jmax(longest, line.length());
    if (longest > 0)
    {
        // Bold Fira Sans runs a little over half its point size per character,
        // which is close enough to start from and is then measured properly.
        size = juce::jmin(size, static_cast<float>(area.getWidth()) / (longest * 0.55f));
    }

    size = juce::jlimit(9.0f, 44.0f, size);

    juce::Font font(juce::FontOptions("Fira Sans", size, juce::Font::bold));
    // Measured rather than trusted: one wide word can still overrun the guess.
    float widest = 1.0f;
    for (const auto& line : lines)
        widest = juce::jmax(widest, font.getStringWidthFloat(line));
    if (widest > area.getWidth())
    {
        size = juce::jmax(9.0f, size * static_cast<float>(area.getWidth()) / widest);
        font = juce::Font(juce::FontOptions("Fira Sans", size, juce::Font::bold));
    }

    g.setFont(font);
    g.setColour(colour);
    g.drawFittedText(text, area, juce::Justification::centred,
                     juce::jmax(lineCount, static_cast<int>(area.getHeight() / juce::jmax(1.0f, size))),
                     1.0f);
}

void PatchCanvas::paintComments(juce::Graphics& g)
{
    if (patch == nullptr)
        return;

    const bool wire = activeScheme_.wireframe;

    for (const auto& c : patch->getComments())
    {
        if (c.section != mySection)
            continue;

        auto bounds = getCommentBounds(c);
        const bool selected = isCommentSelected(c.id);

        // Painted as a module panel, because that is what it behaves like on the
        // grid: modules make room for it, it moves in one, and it should look
        // the part. It takes the module colours rather than any of its own, so
        // it sits with the patch under every theme — the palette themes set
        // moduleBg from their own ramp, and a colour picked here would fight
        // all of them.
        const auto bg = activeScheme_.moduleBg.isOpaque()
                            ? activeScheme_.moduleBg
                            : ModuleDescriptor{}.background;  // Classic: the XML default grey
        const auto ink = activeScheme_.moduleText;

        if (!wire)
        {
            g.setColour(bg);
            g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
        }

        if (wire)
        {
            g.setColour(ink.withAlpha(0.7f));
            g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 3.0f, 1.2f);
        }
        else
        {
            // The same four edge lines every module gets.
            g.setColour(activeScheme_.moduleBorder);
            const float x1 = static_cast<float>(bounds.getX());
            const float y1 = static_cast<float>(bounds.getY());
            const float x2 = static_cast<float>(bounds.getRight());
            const float y2 = static_cast<float>(bounds.getBottom());
            g.drawLine(x1, y1, x2, y1, 1.0f);
            g.drawLine(x1, y2, x2, y2, 1.0f);
            g.drawLine(x1, y1, x1, y2, 1.0f);
            g.drawLine(x2, y1, x2, y2, 1.0f);
        }

        if (c.text.isNotEmpty())
        {
            drawCommentText(g, c.text, bounds, ink);
        }
        else
        {
            g.setColour(ink.withAlpha(0.45f));
            g.setFont(juce::FontOptions("Fira Sans", 12.5f, juce::Font::italic));
            g.drawFittedText("double-click to write", bounds.reduced(8, 6),
                             juce::Justification::centred, 2, 1.0f);
        }

        // Corner grips, the handles that make the note bigger. Drawn only on the
        // one being pointed at or worked on, so a patch full of notes is not a
        // patch full of little triangles.
        const bool showGrips = selected || c.id == hoverCommentId
                                        || c.id == dragCommentId;
        if (showGrips)
        {
            const float s = static_cast<float>(commentGripSize);
            for (int corner = 0; corner < 2; ++corner)
            {
                const bool right = (corner == 1);
                const auto grip = right ? CommentGrip::BottomRight : CommentGrip::BottomLeft;
                const float gx = right ? bounds.getRight() - s : static_cast<float>(bounds.getX());
                const float gy = bounds.getBottom() - s;

                juce::Path tri;
                if (right)
                {
                    tri.startNewSubPath(gx + s, gy);
                    tri.lineTo(gx + s, gy + s);
                    tri.lineTo(gx, gy + s);
                }
                else
                {
                    tri.startNewSubPath(gx, gy);
                    tri.lineTo(gx, gy + s);
                    tri.lineTo(gx + s, gy + s);
                }
                tri.closeSubPath();

                const bool hot = (c.id == hoverCommentId && hoverCommentGrip == grip)
                              || (c.id == dragCommentId && dragCommentGrip == grip);
                g.setColour(hot ? activeScheme_.selectionRect : ink.withAlpha(0.45f));
                g.fillPath(tri);
            }
        }

        if (selected)
        {
            g.setColour(activeScheme_.selectionRect);
            g.drawRoundedRectangle(bounds.toFloat().reduced(1.5f), 2.5f, 1.5f);
        }
    }
}

void PatchCanvas::showCommentEditor(int commentId)
{
    if (patch == nullptr)
        return;
    auto* c = patch->getCommentById(commentId);
    if (c == nullptr)
        return;

    const auto oldText = c->text;

    auto* dialog = new juce::AlertWindow("Comment", {}, juce::MessageBoxIconType::NoIcon);
    auto editor = std::make_unique<juce::TextEditor>();
    editor->setMultiLine(true, true);
    editor->setReturnKeyStartsNewLine(true);
    editor->setSize(360, 110);
    editor->setText(oldText, false);
    auto* editorPtr = editor.get();
    dialog->addCustomComponent(editorPtr);
    dialog->addButton("OK", 1);
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog, editorPtr, commentId, oldText,
         keepAlive = std::shared_ptr<juce::TextEditor>(editor.release())](int r)
        {
            if (r == 1)
            {
                const auto newText = editorPtr->getText();
                if (newText != oldText)
                {
                    if (commentTextCallback)
                        commentTextCallback(commentId, oldText, newText);
                    else if (patch != nullptr)
                        if (auto* c2 = patch->getCommentById(commentId))
                            c2->text = newText;
                    repaint();
                }
            }
            delete dialog;
        }), true);

    editorPtr->grabKeyboardFocus();
}

void PatchCanvas::showCommentContextMenu(int commentId)
{
    auto* c = (patch != nullptr) ? patch->getCommentById(commentId) : nullptr;
    const juce::Rectangle<int> current = c != nullptr
        ? juce::Rectangle<int>(c->x, c->y, c->gridWidth(), c->gridHeight())
        : juce::Rectangle<int>(0, 0, commentDefaultWidth, commentDefaultHeight);

    juce::PopupMenu menu;
    menu.addItem(1, "Edit Text...");
    menu.addSeparator();
    menu.addItem(3, "Copy");
    menu.addItem(4, "Duplicate");
    menu.addSeparator();

    // The corner grips are the quick way; the menu is here for exact sizes.
    juce::PopupMenu heights;
    for (int h = 1; h <= 12; ++h)
        heights.addItem(100 + h, juce::String(h) + (h == 1 ? " row" : " rows"),
                        true, h == current.getHeight());
    menu.addSubMenu("Height", heights);

    juce::PopupMenu widths;
    for (int w = 1; w <= 6; ++w)
        widths.addItem(200 + w, juce::String(w) + (w == 1 ? " column" : " columns"),
                       true, w == current.getWidth());
    menu.addSubMenu("Width", widths);

    menu.addSeparator();
    menu.addItem(2, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options{}, [this, commentId, current](int result)
    {
        if (result == 1)
        {
            showCommentEditor(commentId);
        }
        else if (result == 2)
        {
            selectedCommentIds.erase(std::remove(selectedCommentIds.begin(),
                                                 selectedCommentIds.end(), commentId),
                                     selectedCommentIds.end());
            if (commentDeleteCallback)
                commentDeleteCallback(commentId);
            repaint();
        }
        else if (result == 3 || result == 4)
        {
            // Both work off the selection, and right-clicking a note selects it.
            if (!isCommentSelected(commentId))
                selectComment(commentId, false);
            if (result == 3)
                copySelectionToClipboard();
            else
                duplicateSelection(false);
        }
        else if (result > 100 && result <= 112)
        {
            auto next = current.withHeight(result - 100);
            if (next != current && commentResizeCallback)
                commentResizeCallback(commentId, current, next);
            repaint();
        }
        else if (result > 200 && result <= 206)
        {
            auto next = current.withWidth(juce::jlimit(1, 40 - current.getX(), result - 200));
            if (next != current && commentResizeCallback)
                commentResizeCallback(commentId, current, next);
            repaint();
        }
    });
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

// A seamless, faint grain overlay tiled over the canvas for themes that want
// texture (Nord Classic). It's an overlay so it works on top of any base colour
// without re-tinting. A very light, fine grain — values dialled in by ear.
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
    // Last line of defence: whatever destroyed a module we still point at (a
    // delete, an undone add or paste) may not have gone through this canvas at
    // all, and everything below dereferences those pointers (issue #61).
    forgetDeletedModules();

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

    if (patch == nullptr)
    {
        g.setColour(activeScheme_.moduleText.withAlpha(0.28f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", placeholderArea(), juce::Justification::centred, false);
        return;
    }

    // Each canvas instance is section-specific (mySection 0=common, 1=poly).
    // yOffset is always 0 since each canvas starts from the top.
    auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();

    if (container.getModules().empty())
    {
        g.setColour(activeScheme_.moduleText.withAlpha(0.28f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", placeholderArea(), juce::Justification::centred, false);
    }

    paintComments(g);
    paintModules(g, container, 0);
    paintCables(g, container, 0);
    paintOverlays(g, container, 0);
    spinner.paint(g, { activeScheme_.resetBg, activeScheme_.resetBorder, activeScheme_.resetText });
    paintHoverBadge(g);
    paintDragValueBadge(g);

    // The answer to a double-click on a module, kept up until the next click.
    if (costBadgeModule != nullptr)
        if (auto* desc = costBadgeModule->getDescriptor())
            paintModuleCostBadge(g, getModuleBounds(*costBadgeModule, 0),
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
    paintStaticIcons(g, m, bounds, theme);
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
            out.module        = &m;
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
        out.module        = &m;
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
            hoverCommentId = id;
            hoverCommentGrip = grip;
            setMouseCursor(grip == CommentGrip::BottomRight
                               ? juce::MouseCursor::BottomRightCornerResizeCursor
                               : grip == CommentGrip::BottomLeft
                                     ? juce::MouseCursor::BottomLeftCornerResizeCursor
                                     : juce::MouseCursor::NormalCursor);
            repaint();
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
        hoverCommentId = -1;
        hoverCommentGrip = CommentGrip::None;
        // Not while a block is hanging off the pointer: that cursor is the copy
        // one, and it belongs to the drop, not to the note we just left.
        if (!pendingDrop.active())
            setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
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
    if (hoverTarget.module == nullptr)
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

            out.module       = &m;
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

    // The pointer and the id together name the control: two modules of the same
    // type both have a "p1", and one module outlives a zoom that moves it.
    const juce::String key = next.module == nullptr ? juce::String()
        : juce::String::toHexString(reinterpret_cast<juce::pointer_sized_int>(next.module))
              + "/" + next.componentId;

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
    if (spinnerTarget.module == nullptr)
        return false;

    auto* param = findParameter(*spinnerTarget.module, spinnerTarget.componentId);
    if (param == nullptr)
        return false;

    spinnerValueBeforePress = param->getValue();
    return spinner.mouseDown(canvasPos.toFloat(), [this](int delta) { spinnerStep(delta); });
}

void PatchCanvas::spinnerStep(int delta)
{
    if (spinnerTarget.module == nullptr)
        return;

    auto* param = findParameter(*spinnerTarget.module, spinnerTarget.componentId);
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
        parameterChangeCallback(mySection, spinnerTarget.module->getContainerIndex(),
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

void PatchCanvas::spinnerRelease()
{
    if (!spinner.mouseUp())
        return;

    // One undo step for the whole press, however many times it repeated, the
    // way a knob drag records itself from where it started to where it landed.
    if (spinnerTarget.module != nullptr && paramDragCompleteCallback)
        if (auto* param = findParameter(*spinnerTarget.module, spinnerTarget.componentId))
            if (auto* pd = param->getDescriptor())
                if (param->getValue() != spinnerValueBeforePress)
                    paramDragCompleteCallback(mySection, spinnerTarget.module->getContainerIndex(),
                                              pd->index, spinnerValueBeforePress, param->getValue());
}

void PatchCanvas::paintHoverBadge(juce::Graphics& g)
{
    // The F5 readout already covers every control, so a hover box on top of it
    // would just be a second copy of the same number.
    if (!hoverBadgeVisible || hoverTarget.module == nullptr
        || overlayMode == OverlayMode::Values)
        return;

    // Hovering reads out controls only. The module's own cost is asked for by
    // double-clicking it, the way the original editor does: crossing modules is
    // something the cursor does constantly, and a cost box following it around
    // would be noise rather than an answer.
    if (hoverTarget.componentId.isEmpty())
        return;

    auto* param = findParameter(*hoverTarget.module, hoverTarget.componentId);
    if (param == nullptr)
        return;

    juce::String text = getParameterValueText(*param);

    // On a display that rotates its units, the readout answers what the value
    // is in the units the box is NOT showing, which is what the original puts
    // in its tooltip (issue #30).
    if (const auto* unitsParam = freqUnitsParamFor(*hoverTarget.module, hoverTarget.componentId))
    {
        const auto& m = *hoverTarget.module;
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
                      *hoverTarget.module, *param, text);
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

void PatchCanvas::paintLights(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    // Compute base indices for this module's LEDs and meters in the global arrays
    int ledBase   = computeModuleLightIndex(m, section, false);
    int meterBase = computeModuleLightIndex(m, section, true);

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

    // Connector→module lookup built once per paint; scanning every module's
    // connectors per cable made partial repaints needlessly expensive.
    std::map<const Connector*, const Module*> owners;
    for (auto& modulePtr : container.getModules())
        for (auto& c : modulePtr->getConnectors())
            owners[&c] = modulePtr.get();

    const auto clip = g.getClipBounds();

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
        auto itSrc = owners.find(conn.output);
        auto itDst = owners.find(conn.input);
        if (itSrc == owners.end() || itDst == owners.end())
            continue;

        const Module* srcModule = itSrc->second;
        const Module* dstModule = itDst->second;

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
    if (costBadgeModule != nullptr) { costBadgeModule = nullptr; repaint(); }

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
                            // the sequence alone (issue #34).
                            const auto stepIds = sequencerStepIds(*theme);
                            for (auto& p : m.getParameters())
                            {
                                auto* pd = p.getDescriptor();
                                if (!isSequencerStepParam(*pd, stepIds)) continue;
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
            int newX = juce::jmax(0, ms.startGridPos.x + dx);
            auto& container = patch->getContainer(ms.section);
            int rawY = juce::jmax(0, ms.startGridPos.y + dy);
            int newY = findNearestFreeY(container, ms.module, newX, rawY, ms.module->getDescriptor()->height);
            ms.module->setPosition({ newX, newY });
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
                auto newPos = ms.module->getPosition();
                if (newPos != ms.startGridPos)
                    moduleMoveCallback(ms.section, ms.module->getContainerIndex(),
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
            // that already have distinct outputs.
            if (outConn && inConn)
            {
                auto* drv1 = container.findNetOutput(outConn);
                auto* drv2 = container.findNetOutput(inConn);
                if (drv1 != nullptr && drv2 != nullptr && drv1 != drv2)
                    outConn = inConn = nullptr;
            }

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
                            outMod->getContainerIndex(), outConn->getDescriptor()->index, outConn->getDescriptor()->isOutput,
                            inMod->getContainerIndex(), inConn->getDescriptor()->index, inConn->getDescriptor()->isOutput);
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
                selection.push_back({ modulePtr.get(), mySection });
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
                undoManager->beginNewTransaction("Move Modules");
                for (auto& sel : selection)
                {
                    auto oldPos = sel.module->getPosition();
                    juce::Point<int> newPos(juce::jlimit(0, 39, oldPos.x + dx),
                                            juce::jlimit(0, 127, oldPos.y + dy));
                    if (newPos != oldPos)
                        moduleMoveCallback(sel.section, sel.module->getContainerIndex(),
                                           oldPos, newPos);
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
    if (isAreaFree(gx, gw, targetY, gh, excludeModule, excludeCommentId))
        return targetY;

    for (int offset = 1; offset < 256; ++offset)
    {
        const int above = targetY - offset;
        if (above >= 0 && isAreaFree(gx, gw, above, gh, excludeModule, excludeCommentId))
            return above;
        const int below = targetY + offset;
        if (isAreaFree(gx, gw, below, gh, excludeModule, excludeCommentId))
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

bool PatchCanvas::isSelected(const Module* m) const
{
    for (auto& s : selection)
        if (s.module == m) return true;
    return false;
}

void PatchCanvas::forgetDeletedModules()
{
    if (patch == nullptr) return;

    // Hover and the cost badge don't record which section they came from, so
    // ask both.
    auto stillAlive = [this](const Module* m, int section)
    {
        if (m == nullptr) return false;
        if (section >= 0) return patch->getContainer(section).contains(m);
        return patch->getCommonArea().contains(m) || patch->getPolyVoiceArea().contains(m);
    };

    selection.erase(std::remove_if(selection.begin(), selection.end(),
        [&](const SelectedModule& s) { return !stillAlive(s.module, s.section); }),
        selection.end());

    multiMoveState.erase(std::remove_if(multiMoveState.begin(), multiMoveState.end(),
        [&](const ModuleMoveState& s) { return !stillAlive(s.module, s.section); }),
        multiMoveState.end());

    if (!stillAlive(selectedModule, selectedSection))
    {
        selectedModule = nullptr;
        selectedSection = -1;
    }
    if (!stillAlive(hoverTarget.module, -1))
        clearHover();
    if (spinnerTarget.module != nullptr && !stillAlive(spinnerTarget.module, mySection))
    {
        spinner.mouseUp();          // the press has nothing left to step
        spinner.hide();
        spinnerTarget = SpinnerTarget{};
    }
    if (!stillAlive(costBadgeModule, -1))
        costBadgeModule = nullptr;
    if (dragState.module != nullptr && !stillAlive(dragState.module, dragState.section))
        dragState = DragState();
}

void PatchCanvas::clearSelection()
{
    selection.clear();
    // Notes are part of the selection, so letting go of it lets go of them too.
    selectedCommentIds.clear();
    selectedModule = nullptr;
    selectedSection = -1;
    if (moduleSelectedCallback) moduleSelectedCallback(nullptr, -1);
}

void PatchCanvas::beginMultiMove(juce::Point<int> pos)
{
    dragState = DragState();
    dragState.type = DragState::MultiModuleMove;
    dragState.startPos = pos;

    multiMoveState.clear();
    for (auto& sel : selection)
        multiMoveState.push_back({ sel.module, sel.section, sel.module->getPosition() });

    commentMoveState.clear();
    if (patch != nullptr)
        for (int id : selectedCommentIds)
            if (auto* c = patch->getCommentById(id))
                commentMoveState.push_back({ id, { c->x, c->y } });
}

bool PatchCanvas::isCommentSelected(int id) const
{
    return std::find(selectedCommentIds.begin(), selectedCommentIds.end(), id)
           != selectedCommentIds.end();
}

void PatchCanvas::selectComment(int id, bool addToSelection)
{
    if (!addToSelection)
        selectedCommentIds.clear();
    if (!isCommentSelected(id))
        selectedCommentIds.push_back(id);
}

PatchComment* PatchCanvas::soleSelectedComment()
{
    if (patch == nullptr || selectedCommentIds.size() != 1)
        return nullptr;
    return patch->getCommentById(selectedCommentIds.front());
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
    selectedCommentIds.clear();
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

    // The band catches text notes too: dragging a box round a corner of the
    // patch should take everything in it, labels included.
    for (const auto& c : patch->getComments())
        if (c.section == mySection && rect.intersects(getCommentBounds(c)))
            selectedCommentIds.push_back(c.id);
}

void PatchCanvas::deleteSelection()
{
    if (patch == nullptr || !hasSelection()) return;

    if (undoManager)
        undoManager->beginNewTransaction("Delete Selection");

    // Selected text notes go with the rest of the selection, which is what makes
    // Cut work when notes are all that is selected.
    const auto doomedComments = selectedCommentIds;
    selectedCommentIds.clear();
    for (int id : doomedComments)
        if (commentDeleteCallback)
            commentDeleteCallback(id);

    // Let go of the selection *before* anything is destroyed. Deleting repaints
    // the inspector while it still points at the module being deleted, which on
    // macOS crashed outright (issue #61); clearing first also tells the
    // inspector to drop it through the usual callback.
    const auto doomed = selection;
    clearSelection();

    for (auto& sel : doomed)
    {
        if (deleteModuleCallback)
            deleteModuleCallback(sel.section, sel.module);
        else
        {
            auto& container = patch->getContainer(sel.section);
            container.removeModule(sel.module);
        }
    }
    repaint();
}

void PatchCanvas::duplicateSelection(bool withCables)
{
    if (patch == nullptr || !hasSelection()) return;

    std::vector<ClipboardEntry> entries;
    std::vector<ClipboardCable> cables;
    collectSelection(entries, cables);
    if (!withCables) cables.clear();

    if (undoManager)
        undoManager->beginNewTransaction("Duplicate");

    // Copies land one column right and two rows down of their originals, each
    // in the area the original lives in — Duplicate never moves a module across.
    if (commentCreateCallback)
        for (int id : selectedCommentIds)
            if (auto* c = patch->getCommentById(id))
                commentCreateCallback(c->section,
                                      juce::jlimit(0, 39, c->x + 1), c->y + 2,
                                      c->gridWidth(), c->gridHeight(), c->text);

    if (!entries.empty() && snippetInsertCallback_)
        selectCreated(snippetInsertCallback_(toSnip(entries, cables, { 0, 0 }), -1, 1, 2));
    repaint();
}

void PatchCanvas::copySelectionToClipboard()
{
    if (patch == nullptr || !hasSelection()) return;

    collectSelection(clipboard, clipboardCables);

    // Selected text notes copy with whatever modules are selected, and on their
    // own when they are all there is.
    clipboardComments.clear();
    for (int id : selectedCommentIds)
        if (auto* c = patch->getCommentById(id))
            clipboardComments.push_back({ c->section, { c->x, c->y },
                                          c->gridWidth(), c->gridHeight(), c->text });
}

juce::Point<int> PatchCanvas::clipboardOrigin()
{
    juce::Point<int> origin { 0, 0 };
    bool first = true;

    auto take = [&origin, &first](juce::Point<int> p)
    {
        origin = first ? p : juce::Point<int> { std::min(origin.x, p.x),
                                                std::min(origin.y, p.y) };
        first = false;
    };

    for (const auto& e : clipboard)         take(e.gridPos);
    for (const auto& c : clipboardComments) take(c.gridPos);

    return origin;
}

void PatchCanvas::collectSelection(std::vector<ClipboardEntry>& entriesOut,
                                   std::vector<ClipboardCable>& cablesOut) const
{
    entriesOut.clear();
    cablesOut.clear();
    if (selection.empty() || patch == nullptr) return;

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
        entriesOut.push_back(entry);
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
                    cablesOut.push_back({ modToClipIdx[srcMod], srcDesc->index, srcDesc->isOutput,
                                          modToClipIdx[dstMod], dstDesc->index, dstDesc->isOutput });
            }
        }
    }
}

SnipData PatchCanvas::toSnip(const std::vector<ClipboardEntry>& entries,
                             const std::vector<ClipboardCable>& cables,
                             juce::Point<int> origin)
{
    SnipData snip;
    snip.name = "clipboard";
    if (entries.empty())
        return snip;

    const int minX = origin.x;
    const int minY = origin.y;

    for (auto& e : entries)
    {
        SnipEntry se;
        se.typeIndex   = e.typeIndex;
        se.name        = e.name;
        se.section     = e.section;
        se.gridPos     = { e.gridPos.x - minX, e.gridPos.y - minY };
        se.paramValues = e.paramValues;
        snip.entries.push_back(std::move(se));
    }

    for (auto& cb : cables)
    {
        SnipCable sc;
        sc.srcIdx      = cb.srcModuleClipIdx;
        sc.srcConn     = cb.srcConnectorIdx;
        sc.srcIsOutput = cb.srcIsOutput;
        sc.dstIdx      = cb.dstModuleClipIdx;
        sc.dstConn     = cb.dstConnectorIdx;
        sc.dstIsOutput = cb.dstIsOutput;
        snip.cables.push_back(sc);
    }

    return snip;
}

void PatchCanvas::selectCreated(const std::vector<std::pair<int, int>>& created)
{
    if (patch == nullptr)
        return;

    clearSelection();
    for (auto& [section, containerIndex] : created)
    {
        if (containerIndex < 0)
            continue;
        if (auto* m = patch->getContainer(section).getModuleByIndex(containerIndex))
            selection.push_back({ m, section });
    }
    repaint();
}

void PatchCanvas::beginPasteGhost()
{
    if (moduleDescs == nullptr)
        return;
    if (clipboard.empty() && clipboardComments.empty())
        return;

    const auto origin = clipboardOrigin();

    PendingDrop drop;
    drop.kind = PendingDrop::Kind::Paste;
    for (auto& e : clipboard)
        drop.ghosts.push_back({ e.typeIndex,
                                e.gridPos.x - origin.x, e.gridPos.y - origin.y });
    for (auto& c : clipboardComments)
        drop.ghosts.push_back({ PendingDrop::commentGhost,
                                c.gridPos.x - origin.x, c.gridPos.y - origin.y,
                                c.width, c.height });

    armPendingDrop(std::move(drop));
}

void PatchCanvas::beginAddModuleGhost(int typeIndex, const juce::String& name)
{
    if (moduleDescs == nullptr || moduleDescs->getModuleByIndex(typeIndex) == nullptr)
        return;

    PendingDrop drop;
    drop.kind = PendingDrop::Kind::AddModule;
    drop.addTypeIndex = typeIndex;
    drop.addName = name;
    drop.ghosts.push_back({ typeIndex, 0, 0 });

    armPendingDrop(std::move(drop));
}

void PatchCanvas::beginAddModuleDrop(int typeIndex, const juce::String& name)
{
    // Any live canvas will do: the ghost is editor-wide from here on, and every
    // canvas shares the same module descriptions.
    for (auto* c : liveCanvases)
        if (c != nullptr)
        {
            c->beginAddModuleGhost(typeIndex, name);
            return;
        }
}

void PatchCanvas::beginAddCommentGhost()
{
    PendingDrop drop;
    drop.kind = PendingDrop::Kind::AddComment;
    drop.ghosts.push_back({ PendingDrop::commentGhost, 0, 0,
                            commentDefaultWidth, commentDefaultHeight });

    armPendingDrop(std::move(drop));
}

void PatchCanvas::beginAddCommentDrop()
{
    for (auto* c : liveCanvases)
        if (c != nullptr)
        {
            c->beginAddCommentGhost();
            return;
        }
}

void PatchCanvas::armPendingDrop(PendingDrop drop)
{
    if (!drop.active())
        return;

    pendingDrop = std::move(drop);
    pendingHost = nullptr;

    // Every canvas takes the copy cursor, because the block can be dropped on
    // any of them: the other voice area, or another slot's window.
    for (auto* c : liveCanvases)
    {
        if (c == nullptr)
            continue;
        c->setMouseCursor(juce::MouseCursor::CopyingCursor);
        c->repaint();
    }

    // Show the outlines straight away rather than waiting for the mouse to
    // move. Asking JUCE where the pointer is beats asking the canvas whether it
    // is under it: the command often comes from a popup or a menu, which is
    // what holds the mouse at that moment, so the canvas would say no.
    if (auto* target = canvasUnderPointer())
    {
        auto screenPos = pointerScreenPosition();
        target->updatePendingGhost(
            target->screenToCanvas(target->getLocalPoint(nullptr, screenPos)));

        // Let the keyboard follow the outlines, so Enter drops them and Escape
        // calls them off however the command was given. Deferred because the
        // popup that armed this is usually still closing, and the main window
        // is asked for the focus first: Quick Add is a window in its own right,
        // so closing it hands the keyboard back to the window manager rather
        // than to the canvas underneath.
        juce::Component::SafePointer<PatchCanvas> safe(target);
        juce::MessageManager::callAsync([safe]()
        {
            if (safe == nullptr || !pendingDrop.active())
                return;
            if (auto* top = safe->getTopLevelComponent())
                top->toFront(true);
            safe->grabKeyboardFocus();
        });
    }
}

juce::Point<int> PatchCanvas::pointerScreenPosition()
{
    return juce::Desktop::getInstance().getMainMouseSource()
               .getScreenPosition().roundToInt();
}

PatchCanvas* PatchCanvas::canvasUnderPointer()
{
    const auto screenPos = pointerScreenPosition();

    for (auto* c : liveCanvases)
    {
        if (c == nullptr || !c->isShowing())
            continue;

        // A canvas is far taller than its window ever shows, so what counts is
        // the slice of it the viewport is showing.
        auto* viewport = c->findParentComponentOfClass<juce::Viewport>();
        auto visible = (viewport != nullptr) ? viewport->getScreenBounds()
                                             : c->getScreenBounds();
        if (visible.contains(screenPos))
            return c;
    }

    return nullptr;
}

bool PatchCanvas::dropPendingAtPointer()
{
    if (!pendingDrop.active())
        return false;

    // pendingHost is only set while the pointer is over it, so it is the same
    // canvas canvasUnderPointer() would find, and cheaper to ask.
    auto* target = (pendingHost != nullptr) ? pendingHost : canvasUnderPointer();
    if (target == nullptr)
        return false;

    const auto screenPos = pointerScreenPosition();
    target->dropPendingAt(target->screenToCanvas(target->getLocalPoint(nullptr, screenPos)));
    return true;
}

void PatchCanvas::cancelPendingDrop()
{
    if (pendingDrop.kind == PendingDrop::Kind::None)
        return;

    pendingDrop = {};
    pendingHost = nullptr;

    for (auto* c : liveCanvases)
        if (c != nullptr)
        {
            c->setMouseCursor(juce::MouseCursor::NormalCursor);
            c->repaint();
        }
}

void PatchCanvas::updatePendingGhost(juce::Point<int> canvasPos)
{
    if (!pendingDrop.active())
        return;

    auto* previousHost = pendingHost;
    pendingHost = this;
    pendingGrid = { juce::jlimit(0, 39,  canvasPos.x / gridX),
                    juce::jlimit(0, 127, canvasPos.y / gridY) };

    if (previousHost != nullptr && previousHost != this)
        previousHost->repaint();
    repaint();
}

void PatchCanvas::dropPendingAt(juce::Point<int> canvasPos)
{
    auto drop = pendingDrop;
    const int gx = juce::jlimit(0, 39,  canvasPos.x / gridX);
    const int gy = juce::jlimit(0, 127, canvasPos.y / gridY);
    cancelPendingDrop();

    if (drop.kind == PendingDrop::Kind::AddModule)
    {
        if (moduleDropCallback)
        {
            if (undoManager) undoManager->beginNewTransaction("Add Module");
            moduleDropCallback(drop.addTypeIndex, mySection, gx, gy, drop.addName);
        }
    }
    else if (drop.kind == PendingDrop::Kind::AddComment)
    {
        if (commentAddCallback)
            commentAddCallback(mySection, gx, gy);
    }
    else if (drop.kind == PendingDrop::Kind::Paste)
    {
        pasteBlockAt(gx, gy);
    }
}

void PatchCanvas::pasteBlockAt(int gx, int gy)
{
    if (patch == nullptr)
        return;
    if (clipboard.empty() && clipboardComments.empty())
        return;

    // The whole block goes into the area that was clicked, whichever areas the
    // modules were copied from, which is what makes poly-to-common pasting a
    // matter of aiming rather than a thing the editor refuses (issue #42).
    if (undoManager)
        undoManager->beginNewTransaction("Paste");

    const auto origin = clipboardOrigin();

    // The notes go down first, so the modules that follow make room around them
    // rather than the other way round, and both land inside the one transaction.
    if (commentCreateCallback)
        for (const auto& c : clipboardComments)
            commentCreateCallback(mySection,
                                  juce::jlimit(0, 39, gx + c.gridPos.x - origin.x),
                                  juce::jmax(0, gy + c.gridPos.y - origin.y),
                                  c.width, c.height, c.text);

    if (!clipboard.empty() && snippetInsertCallback_)
        selectCreated(snippetInsertCallback_(toSnip(clipboard, clipboardCables, origin),
                                             mySection, gx, gy));
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

    // Reads the selection directly rather than going through the clipboard,
    // which saving a snippet has no business overwriting.
    std::vector<ClipboardEntry> entries;
    std::vector<ClipboardCable> cables;
    collectSelection(entries, cables);
    if (entries.empty()) return;

    SnipData snip = toSnip(entries, cables, { 0, 0 });
    snip.name = "snippet";
    auto snipCables = std::move(snip.cables);
    snip.cables.clear();

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

    // Cables whose module was filtered out go with it; the rest follow their
    // modules to their new places in the list.
    for (auto& cb : snipCables)
    {
        auto srcIt = clipToSnip.find(cb.srcIdx);
        auto dstIt = clipToSnip.find(cb.dstIdx);
        if (srcIt == clipToSnip.end() || dstIt == clipToSnip.end())
            continue;

        SnipCable sc = cb;
        sc.srcIdx = srcIt->second;
        sc.dstIdx = dstIt->second;
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

    // Preset name text. A module nobody has recalled a preset into reads
    // "none", as the original editor's does. Naming the first preset in the
    // list said a preset was loaded when the module was still at its defaults.
    const int presetIdx = resolvedDrumPreset(m);

    juce::String presetName = "none";
    if (presetIdx >= 0 && presetIdx < static_cast<int>(drumPresets().size()))
        presetName = drumPresets()[static_cast<size_t>(presetIdx)].name;

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
// DrumSynth preset UI
//
// The presets themselves live in the shared ModulePresetLibrary, which is
// injected by the owner. This canvas only draws the display box, offers the
// menu, and applies a recall; it owns no preset data of its own, so what the
// Inspector shows and what this menu shows cannot drift apart.
// ============================================================

const std::vector<ModulePreset>& PatchCanvas::drumPresets() const
{
    static const std::vector<ModulePreset> none;
    return presetLibrary != nullptr ? presetLibrary->forType("DrumSynth") : none;
}

// A patch file does not record which preset a module was built from, so the
// original editor reads the name back off the values. Do the same, or every
// module in a loaded patch would claim to be set to nothing.
//
// A preset may name only the parameters it cares about, so it matches when
// every parameter it does name agrees. An empty preset would match anything.
static bool drumPresetMatches(const ModulePreset& preset,
                              const std::map<juce::String, int>& values)
{
    if (preset.values.empty())
        return false;

    for (const auto& kv : preset.values)
    {
        auto it = values.find(kv.first);
        if (it == values.end() || it->second != kv.second)
            return false;
    }
    return true;
}

int PatchCanvas::resolvedDrumPreset(const Module& m)
{
    const int key = m.getContainerIndex();
    auto it = drumPresetState.find(key);
    if (it != drumPresetState.end())
        return it->second;

    int found = -1;
    const auto values = ModulePresetLibrary::capture(m, {}).values;
    const auto& list = drumPresets();
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (drumPresetMatches(list[i], values))
        {
            found = static_cast<int>(i);
            break;
        }
    }

    // Remembered either way, "matches nothing" included, so the search never
    // runs from paint a second time.
    drumPresetState[key] = found;
    return found;
}

void PatchCanvas::applyDrumPreset(Module& m, int section, int presetIdx)
{
    if (presetIdx < 0 || presetIdx >= static_cast<int>(drumPresets().size())) return;
    auto& preset = drumPresets()[static_cast<size_t>(presetIdx)];

    // Only the parameters the preset actually names are touched, so a preset
    // written by hand with two lines in it stays a two-parameter preset.
    for (const auto& [componentId, value] : preset.values)
    {
        auto* param = findParameter(m, componentId);
        if (param == nullptr) continue;
        int oldVal = param->getValue();
        param->setValue(value);
        int newVal = param->getValue();   // clamped to the parameter's own range
        if (newVal == oldVal) continue;
        if (parameterChangeCallback)
            parameterChangeCallback(section, m.getContainerIndex(), param->getDescriptor()->index, newVal);
        if (paramDragCompleteCallback)
            paramDragCompleteCallback(section, m.getContainerIndex(), param->getDescriptor()->index, oldVal, newVal);
    }
    repaint();
}

// One row of the DrumSynth preset list. Clicking the name recalls the preset,
// clicking the x at its right end deletes it, so the library reads as a list
// rather than as a load menu plus a separate delete menu. Built-in presets get
// no x. The row reports which half was clicked through a shared int, because a
// menu item can only carry the one id that identifies the preset.
class DrumPresetMenuRow : public juce::PopupMenu::CustomComponent
{
public:
    DrumPresetMenuRow(juce::String presetName, bool canDelete, bool isCurrent,
                      std::shared_ptr<int> sharedAction)
        : juce::PopupMenu::CustomComponent(false),
          name(std::move(presetName)), deletable(canDelete), current(isCurrent),
          action(std::move(sharedAction)) {}

    void getIdealSize(int& idealWidth, int& idealHeight) override
    {
        idealWidth  = rowFont().getStringWidth(name) + tickW + deleteW + 16;
        idealHeight = 24;
    }

    void paint(juce::Graphics& g) override
    {
        auto& lf = getLookAndFeel();
        auto textColour = lf.findColour(juce::PopupMenu::textColourId);

        if (isItemHighlighted())
        {
            g.setColour(lf.findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(getLocalBounds());
            textColour = lf.findColour(juce::PopupMenu::highlightedTextColourId);
        }

        g.setFont(rowFont());
        if (current)
        {
            g.setColour(textColour);
            g.drawText(juce::String::fromUTF8("\xe2\x9c\x93"), 4, 0, tickW, getHeight(),
                       juce::Justification::centred);
        }
        g.setColour(textColour);
        g.drawText(name, tickW + 4, 0, getWidth() - tickW - deleteW - 8, getHeight(),
                   juce::Justification::centredLeft, true);

        if (deletable)
        {
            g.setColour(overDelete ? juce::Colour(0xffe05555) : textColour.withAlpha(0.55f));
            g.drawText(juce::String::fromUTF8("\xc3\x97"), getWidth() - deleteW, 0, deleteW - 4,
                       getHeight(), juce::Justification::centred);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override    { updateHover(e.x); }
    void mouseEnter(const juce::MouseEvent& e) override   { updateHover(e.x); }
    void mouseExit(const juce::MouseEvent&) override      { updateHover(-1); }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!getLocalBounds().contains(e.getPosition()))
            return;
        *action = isOverDelete(e.x) ? 1 : 0;
        triggerMenuItem();
    }

private:
    static juce::Font rowFont() { return juce::Font(juce::FontOptions(14.0f)); }
    bool isOverDelete(int x) const { return deletable && x >= getWidth() - deleteW; }
    void updateHover(int x)
    {
        const bool over = x >= 0 && isOverDelete(x);
        if (over != overDelete) { overDelete = over; repaint(); }
    }

    juce::String name;
    bool deletable = false;
    bool current   = false;
    bool overDelete = false;
    std::shared_ptr<int> action;
    static constexpr int tickW   = 18;
    static constexpr int deleteW = 26;
};

juce::PopupMenu PatchCanvas::buildDrumPresetMenu(Module& m, std::shared_ptr<int> action)
{
    juce::PopupMenu menu;

    const int currentIdx = resolvedDrumPreset(m);

    // The presets that ship with the editor go in a folder of their own. There
    // are 29 of them for the Drum Synthesizer alone, and a flat list buries the
    // handful you saved yourself under a wall of names you rarely pick from.
    juce::PopupMenu factory;
    bool anyFactory = false;
    bool anyUser = false;

    const auto& list = drumPresets();
    for (size_t i = 0; i < list.size(); ++i)
    {
        auto row = std::make_unique<DrumPresetMenuRow>(
            list[i].name,
            !list[i].builtIn,
            static_cast<int>(i) == currentIdx,
            action);
        const int id = drumPresetFirstId + static_cast<int>(i);

        if (list[i].builtIn)
        {
            factory.addCustomItem(id, std::move(row), nullptr, list[i].name);
            anyFactory = true;
        }
        else
        {
            menu.addCustomItem(id, std::move(row), nullptr, list[i].name);
            anyUser = true;
        }
    }

    if (anyFactory)
    {
        if (anyUser)
            menu.addSeparator();
        menu.addSubMenu("Factory", factory);
    }

    if (anyFactory || anyUser)
        menu.addSeparator();
    menu.addItem(drumPresetSaveId, "Save current settings as preset...");
    return menu;
}

void PatchCanvas::handleDrumPresetMenuResult(int result, int action, Module& m, int section)
{
    if (result == drumPresetSaveId)
    {
        saveDrumPresetFromModule(m);
        return;
    }

    const int index = result - drumPresetFirstId;
    if (index < 0 || index >= static_cast<int>(drumPresets().size()))
        return;

    if (action == 1)
        deleteDrumPreset(index);
    else
    {
        drumPresetState[m.getContainerIndex()] = index;
        applyDrumPreset(m, section, index);
    }
}

void PatchCanvas::saveDrumPresetFromModule(Module& m)
{
    if (presetLibrary == nullptr || !presetLibrary->canSave())
        return;

    // Named automatically rather than through a dialog: the name can be changed
    // later, and stopping to invent one is the friction that stops people saving.
    auto preset = ModulePresetLibrary::capture(m, presetLibrary->suggestName("DrumSynth"));
    if (presetLibrary->add(std::move(preset)) >= 0)
        repaint();
}

void PatchCanvas::deleteDrumPreset(int index)
{
    if (presetLibrary == nullptr || !presetLibrary->remove("DrumSynth", index))
        return;   // built-ins and failed writes leave the list untouched

    // Modules pointing past the deleted preset would otherwise show the wrong
    // name, and anything past the end would show none at all.
    const int last = static_cast<int>(drumPresets().size()) - 1;
    for (auto& kv : drumPresetState)
    {
        if (kv.second < 0)
            continue;                      // "none" stays none
        if (kv.second == index)
            kv.second = -1;                // the one it named is gone
        else if (kv.second > index)
            kv.second = juce::jlimit(0, juce::jmax(0, last), kv.second - 1);
    }
    repaint();
}

void PatchCanvas::showDrumPresetContextMenu(Module& m, int section)
{
    auto action = std::make_shared<int>(0);
    buildDrumPresetMenu(m, action).showMenuAsync(
        juce::PopupMenu::Options{},
        [this, &m, section, action](int result)
        {
            if (result != 0)
                handleDrumPresetMenuResult(result, *action, m, section);
        });
}
