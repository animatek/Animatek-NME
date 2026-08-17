#include "SlotMdiArea.h"
#include "AppTheme.h"
#include <utility>
#include <vector>
#include <algorithm>

namespace
{
// A sub-window that says whether it is the one you are editing. With four of
// them tiled edge to edge the title bar alone is not enough to tell at a glance,
// so the focused one is outlined in the theme's primary colour and the rest get a
// hairline in the border colour, which also keeps adjacent tiles from reading as
// one big canvas.
class SlotSubWindow : public juce::MultiDocumentPanelWindow
{
public:
    explicit SlotSubWindow(juce::Colour background)
        : juce::MultiDocumentPanelWindow(background)
    {
        setTitleBarButtonsRequired(juce::DocumentWindow::maximiseButton
                                       | juce::DocumentWindow::closeButton,
                                   false);
    }

    void maximiseButtonPressed() override
    {
        // NOT the base class's behaviour, which flips the whole panel into
        // tabbed mode — not a layout this editor offers, and it would strand
        // the tiling somewhere the user cannot get out of. Maximising a slot
        // means focus mode: fill the work area with it, press again to go back.
        if (auto* owner = findParentComponentOfClass<SlotMdiArea>())
            owner->requestMaximise(getContentComponent());
    }

    void setFocusedLook(bool shouldLookFocused)
    {
        if (focused == shouldLookFocused)
            return;
        focused = shouldLookFocused;
        repaint();
    }

    void visibilityChanged() override
    {
        // Deliberately NOT TopLevelWindow::visibilityChanged(), which calls
        // toFront(true) whenever a window becomes visible. These are child
        // windows inside the work area, and the animator hides and re-shows
        // each one when it animates through a proxy — letting them pull
        // themselves to the front as they finish would hand the active
        // document, and the keyboard focus, to whichever animation ended last.
        // Opening a slot brings its window forward explicitly anyway
        // (MultiDocumentPanel::addWindow).
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        const auto& pal = AppTheme::palette();
        auto focusColour = pal.textPrimary;
        if (focusColour.getPerceivedBrightness() < 0.5f)
            focusColour = focusColour.contrasting();

        g.setColour(focused ? focusColour.withAlpha(0.75f) : pal.borderColor);
        g.drawRect(getLocalBounds(), focused ? focusedBorder : idleBorder);
    }

private:
    bool focused = false;
    static constexpr int focusedBorder = 2;
    static constexpr int idleBorder = 1;
};
}  // namespace

SlotMdiArea::SlotMdiArea()
{
    for (int s = 0; s < numSlots; ++s)
    {
        views[(size_t) s] = std::make_unique<SlotView>(s);
        views[(size_t) s]->addMouseListener(&watcher, /*allNestedChildren*/ true);
    }

    setLayoutMode(FloatingWindows);
    useFullscreenWhenOneDocument(true);
    setBackgroundColour(AppTheme::palette().backgroundPanel);
}

SlotMdiArea::~SlotMdiArea()
{
    // ~MultiDocumentPanel closes its documents in its own body, by which point
    // `views` has already been destroyed and every document pointer dangles.
    // Close them here, while they are still alive.
    suppressFocusCallback = true;
    closeAllDocumentsAsync(false, nullptr);  // synchronous when not checking first
}

void SlotMdiArea::openSlot(int slot)
{
    if (slot < 0 || slot >= numSlots || isSlotOpen(slot))
        return;

    // A slot that has just been opened belongs at the end of the tile order, so
    // new windows appear to the right of what is already there rather than
    // jumping in front of it because their letter happens to come first.
    const auto entry = std::find(tileOrder.begin(), tileOrder.end(), slot);
    std::rotate(entry, entry + 1, tileOrder.end());

    addDocument(views[(size_t) slot].get(), getBackgroundColour(), /*deleteWhenRemoved*/ false);

    // Going from one document to two wraps both views in windows, so every
    // window is new as far as we are concerned, not only the one just opened.
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = windowFor(s))
            window->addComponentListener(&watcher);  // idempotent

    applyLayout(/*animate*/ true);
    if (tileMode == TileMode::Free)
        for (int s = 0; s < numSlots; ++s)
            if (auto* window = windowFor(s))
                giveUsableBounds(*window, s);

    // Opening a slot focuses it, the way clicking one does.
    focusSlot(slot);

    if (onLayoutChanged)
        onLayoutChanged();
}

juce::MultiDocumentPanelWindow* SlotMdiArea::windowFor(int slot) const
{
    if (slot < 0 || slot >= numSlots)
        return nullptr;

    const auto* view = views[(size_t) slot].get();
    for (auto* child : getChildren())
        if (auto* window = dynamic_cast<juce::MultiDocumentPanelWindow*>(child))
            if (window->getContentComponent() == view)
                return window;

    return nullptr;
}

void SlotMdiArea::giveUsableBounds(juce::MultiDocumentPanelWindow& window, int slot)
{
    const auto area = getLocalBounds();
    if (area.isEmpty())
        return;  // not laid out yet; resized() will come back to this

    const juce::ScopedValueSetter<bool> guard(applyingLayout, true);

    const bool degenerate = window.getWidth()  < minUsableSize
                         || window.getHeight() < minUsableSize;
    const bool adrift = !area.contains(window.getPosition());

    if (! degenerate && ! adrift)
        return;  // wherever the user put it is fine

    if (degenerate)
    {
        // Big enough to actually patch in, small enough that a second window
        // beside it is still useful.
        window.setSize(juce::jmax(minUsableSize, (area.getWidth()  * 3) / 4),
                       juce::jmax(minUsableSize, (area.getHeight() * 3) / 4));
    }

    // Cascade by slot so A..D never land exactly on top of each other.
    const int step = 24;
    window.setTopLeftPosition(
        juce::jmin(slot * step, juce::jmax(0, area.getWidth()  - window.getWidth())),
        juce::jmin(slot * step, juce::jmax(0, area.getHeight() - window.getHeight())));
}

void SlotMdiArea::resized()
{
    MultiDocumentPanel::resized();

    const auto area = getLocalBounds();

    // The base class only lays out children in tabbed or single-document mode,
    // so the floating sub-windows are ours to look after.
    if (tileMode == TileMode::Free)
    {
        rescaleFreeWindows(area);
        for (int s = 0; s < numSlots; ++s)
            if (auto* window = windowFor(s))
                giveUsableBounds(*window, s);
    }
    else
    {
        applyLayout();
    }

    if (! area.isEmpty())
        lastArea = area;
}

void SlotMdiArea::rescaleFreeWindows(juce::Rectangle<int> area)
{
    if (area.isEmpty() || lastArea.isEmpty()
        || (area.getWidth() == lastArea.getWidth() && area.getHeight() == lastArea.getHeight()))
        return;

    const double sx = (double) area.getWidth()  / lastArea.getWidth();
    const double sy = (double) area.getHeight() / lastArea.getHeight();

    const juce::ScopedValueSetter<bool> guard(applyingLayout, true);
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = windowFor(s))
            window->setBounds(
                area.getX() + juce::roundToInt((window->getX() - lastArea.getX()) * sx),
                area.getY() + juce::roundToInt((window->getY() - lastArea.getY()) * sy),
                juce::roundToInt(window->getWidth()  * sx),
                juce::roundToInt(window->getHeight() * sy));
}

void SlotMdiArea::setTileMode(TileMode mode)
{
    if (tileMode == mode)
        return;

    tileMode = mode;
    if (tileMode == TileMode::Free)
        focusMode = false;  // focus mode is a view of the tiling, not of Free

    applyLayout();
    if (onLayoutChanged)
        onLayoutChanged();
}

juce::Rectangle<float> SlotMdiArea::getNormalisedSlotBounds(int slot) const
{
    const auto area = getLocalBounds();
    const auto* window = windowFor(slot);
    if (window == nullptr || area.isEmpty())
        return {};

    return { (float) (window->getX() - area.getX()) / (float) area.getWidth(),
             (float) (window->getY() - area.getY()) / (float) area.getHeight(),
             (float) window->getWidth()  / (float) area.getWidth(),
             (float) window->getHeight() / (float) area.getHeight() };
}

void SlotMdiArea::setNormalisedSlotBounds(int slot, juce::Rectangle<float> bounds)
{
    const auto area = getLocalBounds();
    auto* window = windowFor(slot);
    if (window == nullptr || area.isEmpty() || bounds.isEmpty())
        return;

    const juce::ScopedValueSetter<bool> guard(applyingLayout, true);
    window->setBounds(area.getX() + juce::roundToInt(bounds.getX() * area.getWidth()),
                      area.getY() + juce::roundToInt(bounds.getY() * area.getHeight()),
                      juce::roundToInt(bounds.getWidth()  * area.getWidth()),
                      juce::roundToInt(bounds.getHeight() * area.getHeight()));
}

int SlotMdiArea::getNumOpenSlots() const
{
    int n = 0;
    for (int s = 0; s < numSlots; ++s)
        if (isSlotOpen(s))
            ++n;
    return n;
}

void SlotMdiArea::retile()
{
    tileMode = TileMode::Auto;
    applyLayout();
    if (onLayoutChanged)
        onLayoutChanged();
}

void SlotMdiArea::setFocusMode(bool shouldBeFocused)
{
    if (focusMode == shouldBeFocused)
        return;

    focusMode = shouldBeFocused;
    // Focus mode is a view of the tiling, not an alternative to it: turning it
    // on from Free would have nothing coherent to fall back to.
    if (focusMode)
        tileMode = TileMode::Auto;

    // F11 moves only the focused window. Repaint that real window as it grows
    // instead of scaling a bitmap snapshot, which visibly stretched modules
    // and cables until the animation completed.
    applyLayout(/*animate*/ true, /*useProxy*/ false);
    // Entering or leaving focus mode changes whether the outline is shown at all.
    updateFocusHighlight();
    if (onLayoutChanged)
        onLayoutChanged();
}

std::vector<int> SlotMdiArea::openSlotsInTileOrder() const
{
    std::vector<int> open;
    for (int slot : tileOrder)
        if (isSlotOpen(slot))
            open.push_back(slot);
    return open;
}

void SlotMdiArea::placeWindow(juce::MultiDocumentPanelWindow& window,
                              juce::Rectangle<int> bounds, bool animate, bool useProxy)
{
    if (window.getBounds() == bounds)
        return;

    if (! animate || ! animateLayout)
    {
        window.setBounds(bounds);
        return;
    }

    // Proxy snapshots stay useful when several windows move during a re-tile:
    // they avoid relaying out up to four canvases on every frame. F11 passes
    // useProxy=false because it moves only one window and a scaled snapshot
    // visibly stretches its modules and cables. Start at speed and ease to
    // rest: the previous 0 -> 1 curve lingered and then stopped abruptly.
    juce::Desktop::getInstance().getAnimator().animateComponent(
        &window, bounds, 1.0f, animationMs, useProxy, 1.0, 0.0);
}

void SlotMdiArea::applyLayout(bool animate, bool useProxy)
{
    if (tileMode != TileMode::Auto)
        return;

    const auto area = getLocalBounds();
    if (area.isEmpty())
        return;  // not laid out yet; resized() will come back to this

    const auto open = openSlotsInTileOrder();

    // One document has no window frame at all — the base class gives the view
    // the whole area — so there is nothing to place.
    if (open.size() < 2)
        return;

    const juce::ScopedValueSetter<bool> guard(applyingLayout, true);

    if (focusMode)
    {
        // Leave the others tiled where they are and lay the focused one over the
        // top. Coming back out is then just a re-tile, with nothing to restore.
        if (auto* window = windowFor(getFocusedSlot()))
        {
            placeWindow(*window, area, animate, useProxy);
            window->toFront(false);
            return;
        }
    }

    const int n = static_cast<int>(open.size());

    // Boundaries are computed from the area rather than accumulated from a
    // width, so the columns always add up to exactly the full width with no
    // rounding gap between them.
    auto split = [](int start, int extent, int index, int count)
    {
        const int a = start + (extent * index)       / count;
        const int b = start + (extent * (index + 1)) / count;
        return std::make_pair(a, b - a);
    };

    if (n == 4)
    {
        // 2x2, in slot order: A top-left, B top-right, C bottom-left, D bottom-right.
        for (int i = 0; i < 4; ++i)
        {
            auto* window = windowFor(open[(size_t) i]);
            if (window == nullptr)
                continue;
            const auto [x, w] = split(area.getX(), area.getWidth(),  i % 2, 2);
            const auto [y, h] = split(area.getY(), area.getHeight(), i / 2, 2);
            placeWindow(*window, { x, y, w, h }, animate, useProxy);
        }
    }
    else
    {
        // Two or three: side-by-side columns. A patch canvas is much wider than
        // it is tall, so splitting the width keeps both halves readable in a way
        // that stacking them would not.
        for (int i = 0; i < n; ++i)
        {
            auto* window = windowFor(open[(size_t) i]);
            if (window == nullptr)
                continue;
            const auto [x, w] = split(area.getX(), area.getWidth(), i, n);
            placeWindow(*window, { x, area.getY(), w, area.getHeight() }, animate, useProxy);
        }
    }
}

void SlotMdiArea::windowMovedOrResized(juce::Component& component,
                                       bool wasMoved, bool wasResized)
{
    if (applyingLayout || !(wasMoved || wasResized))
        return;
    if (dynamic_cast<juce::MultiDocumentPanelWindow*>(&component) == nullptr)
        return;

    // Only a move the user is actually making counts. applyingLayout covers our
    // own setBounds calls, but JUCE moves these windows too — it positions each
    // new one as it is created, and re-wraps them all when the document count
    // crosses one — and a stray programmatic move used to switch tiling off for
    // good, since the mode is persisted and restoring it re-armed the same
    // state next launch. A drag means a button is down.
    if (juce::Desktop::getInstance().getNumDraggingMouseSources() == 0)
        return;

    // The user dragged or resized a sub-window. Stop re-flowing it out from
    // under them; the View menu's Tile Slots is the way back.
    if (tileMode == TileMode::Auto)
    {
        tileMode = TileMode::Free;
        focusMode = false;
        if (onLayoutChanged)
            onLayoutChanged();
    }
}

void SlotMdiArea::closeSlot(int slot)
{
    if (slot < 0 || slot >= numSlots || !isSlotOpen(slot))
        return;

    // checkItsOkToCloseFirst=true so this goes through tryToCloseDocumentAsync,
    // the one place that re-flows the layout and reports the close — the
    // window's own close button can only reach us that way.
    closeDocumentAsync(views[(size_t) slot].get(), true, nullptr);
}

bool SlotMdiArea::isSlotOpen(int slot) const
{
    if (slot < 0 || slot >= numSlots)
        return false;

    auto* view = views[(size_t) slot].get();
    for (int i = getNumDocuments(); --i >= 0;)
        if (getDocument(i) == view)
            return true;

    return false;
}

void SlotMdiArea::focusSlot(int slot)
{
    if (! isSlotOpen(slot))
        return;

    const bool changed = (focusedSlot != slot);
    focusedSlot = slot;

    // Do not call MultiDocumentPanel::setActiveDocument() here. In floating
    // mode it calls toFront(true), stealing keyboard focus from the canvas that
    // was just clicked. The deep mouse listener reaches focusSlot() after the
    // canvas's own mouseDown(), so that made canvas shortcuts such as zoom stop
    // working as soon as the documents were wrapped in two or more windows.
    // We track the focused slot ourselves because JUCE cannot reliably derive
    // an active document from these child windows anyway.
    if (auto* window = windowFor(slot))
        window->toFront(false);

    // In focus mode the maximised window is whichever one has focus.
    if (changed && focusMode)
        applyLayout(/*animate*/ true);

    updateFocusHighlight();

    if (changed && ! suppressFocusCallback && onSlotFocused != nullptr)
        onSlotFocused(slot);
}

void SlotMdiArea::viewClicked(const juce::MouseEvent& e)
{
    for (int s = 0; s < numSlots; ++s)
        if (views[(size_t) s].get() == e.eventComponent
            || views[(size_t) s]->isParentOf(e.eventComponent))
        {
            focusSlot(s);
            return;
        }
}

void SlotMdiArea::tryToCloseDocumentAsync(juce::Component* component,
                                          std::function<void(bool)> callback)
{
    // Closing a sub-window only takes the slot off screen; its patch, undo
    // history and variations stay exactly where they are. So there is never
    // anything to save first, and this always says yes.
    const auto* view = dynamic_cast<SlotView*>(component);
    const int slot = view != nullptr ? view->getSlot() : -1;

    // The panel closes the document from inside this callback, so by the time
    // it returns the window is already gone and onSlotClosed can report a fact
    // rather than an intention. This is also the only hook that catches the
    // window's own close button, which never goes through closeSlot().
    if (callback)
        callback(true);

    if (suppressFocusCallback)
        return;

    // One fewer window: the remaining ones re-flow to fill the area.
    applyLayout(/*animate*/ true);

    if (slot == focusedSlot)
    {
        focusedSlot = -1;
        const auto open = openSlotsInTileOrder();
        if (! open.empty())
            focusSlot(open.front());
    }
    updateFocusHighlight();
    if (onLayoutChanged)
        onLayoutChanged();

    if (slot >= 0 && onSlotClosed != nullptr)
        onSlotClosed(slot);
}

juce::MultiDocumentPanelWindow* SlotMdiArea::createNewDocumentWindow()
{
    return new SlotSubWindow(getBackgroundColour());
}

void SlotMdiArea::moveFocusedTile(Direction direction)
{
    const auto open = openSlotsInTileOrder();
    const int focused = getFocusedSlot();
    const int n = static_cast<int>(open.size());
    if (n < 2 || focused < 0)
        return;

    const auto it = std::find(open.begin(), open.end(), focused);
    if (it == open.end())
        return;
    const int from = static_cast<int>(std::distance(open.begin(), it));

    // Where the tiles actually are, mirroring applyLayout: four is a 2x2 grid,
    // two and three are columns. An edge move is a no-op rather than a wrap, so
    // the arrow key always means what it says.
    int to = -1;
    if (n == 4)
    {
        switch (direction)
        {
            case Direction::Left:  if (from % 2 == 1) to = from - 1; break;
            case Direction::Right: if (from % 2 == 0) to = from + 1; break;
            case Direction::Up:    if (from >= 2)     to = from - 2; break;
            case Direction::Down:  if (from < 2)      to = from + 2; break;
        }
    }
    else  // columns: up and down have nowhere to go
    {
        switch (direction)
        {
            case Direction::Left:  if (from > 0)     to = from - 1; break;
            case Direction::Right: if (from < n - 1) to = from + 1; break;
            case Direction::Up:
            case Direction::Down:  break;
        }
    }

    if (to < 0 || to >= n)
        return;

    // tileOrder holds every slot, open or not, so swap the two entries there
    // rather than in the filtered list.
    const auto posOf = [this](int slot) {
        return std::distance(tileOrder.begin(),
                             std::find(tileOrder.begin(), tileOrder.end(), slot));
    };
    std::swap(tileOrder[(size_t) posOf(open[(size_t) from])],
              tileOrder[(size_t) posOf(open[(size_t) to])]);

    applyLayout(/*animate*/ true);
    // The moved slot keeps focus: only its tile changed, not which patch you
    // are editing.
    reassertFocus(focused);
    if (onLayoutChanged)
        onLayoutChanged();
}

bool SlotMdiArea::canResetTileOrder() const
{
    if (getNumOpenSlots() < 2)
        return false;

    if (tileMode != TileMode::Auto || focusMode)
        return true;

    for (int i = 0; i < numSlots; ++i)
        if (tileOrder[(size_t) i] != i)
            return true;

    return false;
}

void SlotMdiArea::resetTileOrder()
{
    if (!canResetTileOrder())
        return;

    for (int i = 0; i < numSlots; ++i)
        tileOrder[(size_t) i] = i;

    // Free mode and focus mode are the other two ways the layout stops being
    // the canonical one, so both go with the shuffle. Set directly rather than
    // through the setters: each of those lays out on its own, and three
    // animations racing each other is what a re-tile should not look like.
    focusMode = false;
    tileMode  = TileMode::Auto;

    const int focused = getFocusedSlot();
    applyLayout(/*animate*/ true);
    reassertFocus(focused);
    updateFocusHighlight();
    if (onLayoutChanged)
        onLayoutChanged();
}

void SlotMdiArea::rotateTiles(int direction)
{
    if (getNumOpenSlots() < 2 || direction == 0)
        return;

    if (direction > 0)
        std::rotate(tileOrder.begin(), tileOrder.begin() + 1, tileOrder.end());
    else
        std::rotate(tileOrder.rbegin(), tileOrder.rbegin() + 1, tileOrder.rend());

    const int focused = getFocusedSlot();
    applyLayout(/*animate*/ true);
    reassertFocus(focused);
    if (onLayoutChanged)
        onLayoutChanged();
}

// Put the active document back where it was. Reordering only changes which tile
// a slot occupies, never which slot you are editing, but the windows are hidden
// and re-shown by the animator and JUCE re-derives the active document from
// window state along the way.
void SlotMdiArea::reassertFocus(int slot)
{
    if (slot < 0 || slot >= numSlots || !isSlotOpen(slot))
        return;

    const juce::ScopedValueSetter<bool> guard(suppressFocusCallback, true);
    focusSlot(slot);
    updateFocusHighlight();
}

juce::String SlotMdiArea::getTileOrderString() const
{
    juce::String out;
    for (int slot : tileOrder)
        out += juce::String(slot);
    return out;
}

void SlotMdiArea::setTileOrderString(const juce::String& order)
{
    // Only accept a genuine permutation of 0..numSlots-1; anything else and the
    // tiling would drop or duplicate a slot.
    if (order.length() != numSlots)
        return;

    std::array<int, numSlots> candidate {};
    bool seen[numSlots] = {};
    for (int i = 0; i < numSlots; ++i)
    {
        const int slot = order[i] - '0';
        if (slot < 0 || slot >= numSlots || seen[slot])
            return;
        seen[slot] = true;
        candidate[(size_t) i] = slot;
    }

    tileOrder = candidate;
    applyLayout();
}

void SlotMdiArea::requestMaximise(juce::Component* view)
{
    if (auto* slotView = dynamic_cast<SlotView*>(view))
        if (onSlotMaximiseRequested)
            onSlotMaximiseRequested(slotView->getSlot());
}

void SlotMdiArea::updateFocusHighlight()
{
    const int focused = getFocusedSlot();
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = dynamic_cast<SlotSubWindow*>(windowFor(s)))
            window->setFocusedLook(s == focused);
}

void SlotMdiArea::applyTheme()
{
    setBackgroundColour(AppTheme::palette().backgroundPanel);
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = windowFor(s))
        {
            // The window captured the old colour when it was created.
            window->setBackgroundColour(getBackgroundColour());
            window->repaint();
        }
}

// Deliberately empty. MultiDocumentPanel derives its active document from
// TopLevelWindow::isActiveWindow(), which child windows inside a work area do
// not report dependably — following it here is what made focus jump to an
// unrelated slot. focusSlot() is the only thing that moves focus now, driven by
// clicks (WindowWatcher::mouseDown), opening a slot, and closing one.
void SlotMdiArea::activeDocumentChanged() {}
