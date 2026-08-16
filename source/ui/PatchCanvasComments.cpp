#include "PatchCanvasComponent.h"
#include <cmath>
#include <unordered_map>

// PatchCanvas: the editor's own text notes. They hold a rectangle of the grid
// like a module does, but they never reach the synth.

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

void PatchCanvas::repaintComment(int commentId)
{
    if (commentId < 0 || patch == nullptr)
        return;
    if (const auto* c = patch->getCommentById(commentId))
        repaintCanvasArea(getCommentBounds(*c));
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
