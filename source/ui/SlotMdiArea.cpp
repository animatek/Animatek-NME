#include "SlotMdiArea.h"
#include "AppTheme.h"
#include <utility>
#include <vector>

namespace
{
// A sub-window that says whether it is the one you are editing. With four of
// them tiled edge to edge the title bar alone is not enough to tell at a glance,
// so the focused one is outlined in the theme's accent colour and the rest get a
// hairline in the border colour, which also keeps adjacent tiles from reading as
// one big canvas.
class SlotSubWindow : public juce::MultiDocumentPanelWindow
{
public:
    explicit SlotSubWindow(juce::Colour background)
        : juce::MultiDocumentPanelWindow(background)
    {
        // JUCE's default maximise button flips the whole panel into tabbed mode,
        // which is not one of the layouts this editor offers and would strand
        // the tiling in an unreachable state. Leave only the close button.
        setTitleBarButtonsRequired(juce::DocumentWindow::closeButton, false);
    }

    void setFocusedLook(bool shouldLookFocused)
    {
        if (focused == shouldLookFocused)
            return;
        focused = shouldLookFocused;
        repaint();
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        const auto& pal = AppTheme::palette();
        g.setColour(focused ? pal.accentActive : pal.borderColor);
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
        views[(size_t) s] = std::make_unique<SlotView>(s);

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

    addDocument(views[(size_t) slot].get(), getBackgroundColour(), /*deleteWhenRemoved*/ false);

    // Going from one document to two wraps both views in windows, so every
    // window is new as far as we are concerned, not only the one just opened.
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = windowFor(s))
            window->addComponentListener(&watcher);  // idempotent

    applyLayout();
    if (tileMode == TileMode::Free)
        for (int s = 0; s < numSlots; ++s)
            if (auto* window = windowFor(s))
                giveUsableBounds(*window, s);

    updateFocusHighlight();
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

    // The base class only lays out children in tabbed or single-document mode,
    // so the floating sub-windows are ours to look after.
    applyLayout();

    if (tileMode == TileMode::Free)
        for (int s = 0; s < numSlots; ++s)
            if (auto* window = windowFor(s))
                giveUsableBounds(*window, s);
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

    applyLayout();
    if (onLayoutChanged)
        onLayoutChanged();
}

void SlotMdiArea::applyLayout()
{
    if (tileMode != TileMode::Auto)
        return;

    const auto area = getLocalBounds();
    if (area.isEmpty())
        return;  // not laid out yet; resized() will come back to this

    std::vector<int> open;
    for (int s = 0; s < numSlots; ++s)
        if (isSlotOpen(s))
            open.push_back(s);

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
            window->setBounds(area);
            window->toFront(true);
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
            window->setBounds(x, y, w, h);
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
            window->setBounds(x, area.getY(), w, area.getHeight());
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

    setActiveDocument(views[(size_t) slot].get());
}

int SlotMdiArea::getFocusedSlot() const
{
    if (auto* view = dynamic_cast<SlotView*>(getActiveDocument()))
        return view->getSlot();

    return -1;
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
    applyLayout();
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

void SlotMdiArea::activeDocumentChanged()
{
    if (suppressFocusCallback)
        return;

    // In focus mode the maximised window is whichever one has focus, so moving
    // focus moves which window is blown up.
    if (focusMode)
        applyLayout();

    updateFocusHighlight();

    const int slot = getFocusedSlot();
    if (slot >= 0 && onSlotFocused != nullptr)
        onSlotFocused(slot);
}
