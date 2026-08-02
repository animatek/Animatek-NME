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

void SlotMdiArea::showOnlySlot(int slot)
{
    if (slot < 0 || slot >= numSlots)
        return;

    // Close the others first so the panel never briefly holds two documents:
    // that would drop out of fullscreen mode, build a floating sub-window for
    // each, and tear it straight back down — a visible flash on every slot
    // change. Going through zero documents only shows the empty background for
    // the rest of this call.
    suppressFocusCallback = true;
    for (int s = 0; s < numSlots; ++s)
        if (s != slot)
            closeSlot(s);
    suppressFocusCallback = false;

    openSlot(slot);
    focusSlot(slot);
}

void SlotMdiArea::activeDocumentChanged()
{
    if (suppressFocusCallback || onSlotFocused == nullptr)
        return;

    const int slot = getFocusedSlot();
    if (slot >= 0)
        onSlotFocused(slot);
}
