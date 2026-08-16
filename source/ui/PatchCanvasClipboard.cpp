#include "PatchCanvasComponent.h"
#include <cmath>
#include <set>
#include <unordered_map>

// PatchCanvas: copy, paste, duplicate and the outlines that hang off the
// pointer until a click puts them down (issues #42 and #36).

PatchCanvas::PendingDrop PatchCanvas::pendingDrop;

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
        // Resolved one at a time: deleting the first of them can be what
        // destroys the rest (a cable's owner going with it).
        auto* module = resolve(sel);
        if (module == nullptr)
            continue;

        if (deleteModuleCallback)
            deleteModuleCallback(sel.section, module);
        else
            patch->getContainer(sel.section).removeModule(module);
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

    for (const auto& sel : selection)
    {
        auto* module = resolve(sel);
        if (module == nullptr)
            continue;

        ClipboardEntry entry;
        entry.typeIndex = module->getDescriptor() ? module->getDescriptor()->index : 0;
        entry.name = module->getTitle();
        entry.section = sel.section;
        entry.gridPos = module->getPosition();
        for (auto& p : module->getParameters())
            entry.paramValues.push_back(p.getValue());
        modToClipIdx[module] = static_cast<int>(entriesOut.size());
        entriesOut.push_back(entry);
    }

    // Store internal cables — scan each unique section once (NOT per selected module,
    // which would duplicate every cable N times for N selected modules in that section).
    std::set<Module*> selSet;
    std::set<int> sectionsToScan;
    for (auto& s : selection)
    {
        if (auto* m = resolve(s)) selSet.insert(m);
        sectionsToScan.insert(s.section);
    }

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
        if (patch->getContainer(section).getModuleByIndex(containerIndex) != nullptr)
            selection.push_back({ section, containerIndex });
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
                    if (auto* m = resolve(sel))
                        initModuleCallback(sel.section, m);
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
