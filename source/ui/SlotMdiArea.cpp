#include "SlotMdiArea.h"
#include "AppTheme.h"

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

    // Going from one document to two wraps both views in windows, so check them
    // all, not only the one just opened.
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = windowFor(s))
            giveUsableBounds(*window, s);
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
    // so floating sub-windows are ours to look after. Phase 3 adds real tiling;
    // this only rescues windows that are unusable or have drifted off the area,
    // and leaves anything the user has arranged alone.
    for (int s = 0; s < numSlots; ++s)
        if (auto* window = windowFor(s))
            giveUsableBounds(*window, s);
}

void SlotMdiArea::closeSlot(int slot)
{
    if (slot < 0 || slot >= numSlots || !isSlotOpen(slot))
        return;

    closeDocumentAsync(views[(size_t) slot].get(), false, nullptr);
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

    if (slot >= 0 && ! suppressFocusCallback && onSlotClosed != nullptr)
        onSlotClosed(slot);
}

juce::MultiDocumentPanelWindow* SlotMdiArea::createNewDocumentWindow()
{
    auto* window = MultiDocumentPanel::createNewDocumentWindow();
    // JUCE's default maximise button flips the whole panel into tabbed mode,
    // which is not one of the layouts this editor offers, and would strand the
    // tiling in an unreachable state. Leave only the close button.
    window->setTitleBarButtonsRequired(juce::DocumentWindow::closeButton, false);
    return window;
}

void SlotMdiArea::activeDocumentChanged()
{
    if (suppressFocusCallback || onSlotFocused == nullptr)
        return;

    const int slot = getFocusedSlot();
    if (slot >= 0)
        onSlotFocused(slot);
}
