#include "ThemeEditorWindow.h"
#include "AppTheme.h"
#include "FlatCloseButton.h"

namespace
{
// This window keeps colours of its own instead of following the theme: while a theme is being edited the
// palette moves under the mouse, and an editor that changes colour as you drag its swatches is not one
// you can read. They are the new look's dark ground and amber accent.
namespace fx
{
const juce::Colour ground   { 0xff1b1f26 };
const juce::Colour panel    { 0xff222730 };
const juce::Colour field    { 0xff15181e };
const juce::Colour border   { 0xff353c47 };
const juce::Colour text     { 0xffe4e8ee };
const juce::Colour muted    { 0xff7f8896 };
const juce::Colour accent   { 0xffe2b765 };
const juce::Colour button   { 0xff2d343f };
}

class Swatch : public juce::Component
{
public:
    std::function<juce::Colour()> getColour;
    std::function<void(juce::Colour)> setColour;
    bool autoWhenTransparent = false;

    void paint(juce::Graphics& g) override
    {
        const auto c = getColour();
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        // Checkerboard under translucent colours, so "empty" is visible as such.
        g.setColour(juce::Colour(0xff777777));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(juce::Colour(0xff999999));
        for (float y = r.getY(); y < r.getBottom(); y += 6.0f)
            for (float x = r.getX() + (static_cast<int>((y - r.getY()) / 6.0f) % 2 ? 6.0f : 0.0f); x < r.getRight(); x += 12.0f)
                g.fillRect(juce::Rectangle<float>(x, y, 6.0f, 6.0f).getIntersection(r));
        g.setColour(c);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(fx::border.brighter(0.5f));
        g.drawRoundedRectangle(r, 6.0f, 1.0f);
        if (autoWhenTransparent && c.isTransparent())
        {
            g.setColour(juce::Colours::white);
            g.setFont(12.0f);
            g.drawText("auto", getLocalBounds(), juce::Justification::centred);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!e.mouseWasClicked())
            return;
        struct Picker : juce::ColourSelector, juce::ChangeListener
        {
            std::function<void(juce::Colour)> apply;
            Picker(std::function<void(juce::Colour)> a)
                : juce::ColourSelector(showAlphaChannel | showColourAtTop | showSliders | showColourspace), apply(std::move(a))
            { addChangeListener(this); }
            ~Picker() override { removeChangeListener(this); }
            void changeListenerCallback(juce::ChangeBroadcaster*) override { apply(getCurrentColour()); }
        };
        auto picker = std::make_unique<Picker>([this](juce::Colour c) { setColour(c); repaint(); });
        auto start = getColour();
        if (start.isTransparent())
            start = juce::Colour(0xffffffff);
        picker->setCurrentColour(start, juce::dontSendNotification);
        // Its own light-on-dark colours, like the rest of this window: the value boxes took
        // the theme's text colour, black on the dark box with a light theme or mid-edit.
        picker->setColour(juce::ColourSelector::labelTextColourId, fx::text);
        for (auto* c : picker->getChildren())
            if (auto* s = dynamic_cast<juce::Slider*>(c))
            {
                s->setColour(juce::Slider::textBoxTextColourId, fx::text);
                s->setColour(juce::Slider::textBoxBackgroundColourId, fx::field);
                s->setColour(juce::Slider::textBoxOutlineColourId, fx::border);
                s->setColour(juce::Slider::textBoxHighlightColourId, fx::accent.darker(0.4f));
            }
        picker->setSize(280, 320);
        juce::CallOutBox::launchAsynchronously(std::move(picker), getScreenBounds(), nullptr);
    }
};

class Row : public juce::Component
{
public:
    Row(const juce::String& text, std::function<juce::Colour()> get, std::function<void(juce::Colour)> set, bool autoCol)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(14.0f)));
        label.setColour(juce::Label::textColourId, fx::text);
        addAndMakeVisible(label);
        swatch.getColour = std::move(get);
        swatch.setColour = std::move(set);
        swatch.autoWhenTransparent = autoCol;
        addAndMakeVisible(swatch);
        if (autoCol)
        {
            // Back to "automatic" (transparent) without dragging the alpha down to 0.
            autoButton.onClick = [this] { swatch.setColour(juce::Colour()); swatch.repaint(); };
            autoButton.setTooltip("Leave it to the theme: transparent, shown as \"auto\"");
            autoButton.setColour(juce::TextButton::buttonColourId, fx::button);
            autoButton.setColour(juce::TextButton::textColourOffId, fx::muted);
            addAndMakeVisible(autoButton);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(4, 0);
        swatch.setBounds(r.removeFromRight(72).withSizeKeepingCentre(72, 28));
        if (autoButton.isVisible())
        {
            r.removeFromRight(8);
            autoButton.setBounds(r.removeFromRight(52).withSizeKeepingCentre(52, 28));
        }
        label.setBounds(r);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(fx::border.withAlpha(0.6f));
        g.fillRect(0, getHeight() - 1, getWidth(), 1);
    }
    juce::Label label;
    Swatch swatch;
    juce::TextButton autoButton { "Auto" };
};

class GroupHeader : public juce::Label
{
public:
    explicit GroupHeader(const juce::String& t)
    {
        setText(t, juce::dontSendNotification);
        setFont(juce::FontOptions(12.5f, juce::Font::bold));
        setColour(juce::Label::textColourId, fx::muted);
        setText(t.toUpperCase(), juce::dontSendNotification);
        setBorderSize(juce::BorderSize<int>(10, 4, 0, 0));
    }
};
}

class ThemeEditorWindow::Content : public juce::Component
{
public:
    Content(ThemeEditorWindow& o, const EditorTheme& start, const juce::String& base)
        : owner(o), baseName(base), doc(ThemeFile::makeDoc(start)), original(doc)
    {
        nameEditor.setText(start.name + " edited", juce::dontSendNotification);
        nameEditor.setTextToShowWhenEmpty("Theme name", fx::muted);
        nameEditor.setFont(juce::Font(juce::FontOptions(14.0f)));
        nameEditor.setJustification(juce::Justification::centredLeft);
        nameEditor.setIndents(12, 0);
        nameEditor.setColour(juce::TextEditor::backgroundColourId, fx::field);
        nameEditor.setColour(juce::TextEditor::textColourId, fx::text);
        nameEditor.setColour(juce::TextEditor::outlineColourId, fx::border);
        nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, fx::accent);
        addAndMakeVisible(nameEditor);
        closeButton.onClick = [this] { owner.closeButtonPressed(); };
        addAndMakeVisible(closeButton);
        saveButton.setColour(juce::TextButton::buttonColourId, fx::accent);          // Save is the thing to press
        saveButton.setColour(juce::TextButton::textColourOffId, fx::ground);
        resetButton.setColour(juce::TextButton::buttonColourId, fx::button);
        resetButton.setColour(juce::TextButton::textColourOffId, fx::text);
        saveButton.onClick = [this] { save(); };
        resetButton.onClick = [this] { doc = original; refreshRows(); changed(); };
        addAndMakeVisible(saveButton);
        addAndMakeVisible(resetButton);
        addAndMakeVisible(status);
        // Ticked when the theme already sets one, so saving keeps what it had.
        keepRender.setToggleState(start.render.any(), juce::dontSendNotification);
        keepRender.setTooltip("When this theme is chosen it also turns Wireframe, Hardware look "
                              "and Flat knobs on or off as they are now. The theme's own while the "
                              "box is ticked; untick it to leave the render style to Editor Options.");
        keepRender.setColour(juce::ToggleButton::textColourId, fx::text);
        addAndMakeVisible(keepRender);
        status.setColour(juce::Label::textColourId, fx::muted);
        status.setFont(juce::Font(juce::FontOptions(13.0f)));

        const char* lastGroup = nullptr;
        for (const auto& e : ThemeFile::entries())
        {
            if (lastGroup == nullptr || juce::String(lastGroup) != e.group)
            {
                headers.push_back(std::make_unique<GroupHeader>(e.group));
                body.addAndMakeVisible(*headers.back());
                items.push_back({ headers.back().get(), 38 });
                lastGroup = e.group;
            }
            const auto* entry = &e;
            rows.push_back(std::make_unique<Row>(e.label,
                [this, entry] { return ThemeFile::colourRef(doc, *entry); },
                [this, entry](juce::Colour c) { ThemeFile::colourRef(doc, *entry) = c; changed(); },
                e.autoWhenTransparent));
            body.addAndMakeVisible(*rows.back());
            items.push_back({ rows.back().get(), 40 });
        }
        int h = 8;
        for (auto& it : items) h += it.height;
        body.setSize(400, h);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);
        setSize(480, 700);
    }

    void resized() override
    {
        closeButton.setBounds(getWidth() - 36, 6, 28, 28);

        // Name, Save and Reset under the title bar
        auto r = getLocalBounds().withTrimmedTop(titleH + 16).withTrimmedBottom(footerH).reduced(24, 0);
        auto top = r.removeFromTop(38);
        resetButton.setBounds(top.removeFromRight(76));
        top.removeFromRight(8);
        saveButton.setBounds(top.removeFromRight(76));
        top.removeFromRight(10);
        nameEditor.setBounds(top);
        r.removeFromTop(12);
        viewport.setBounds(r.withTrimmedRight(-8));
        const int w = viewport.getMaximumVisibleWidth();
        int y = 4;
        for (auto& it : items) { it.comp->setBounds(0, y, w, it.height); y += it.height; }
        body.setSize(w, y + 4);

        // The footer band: the render-style switch and what Save said
        auto f = getLocalBounds().removeFromBottom(footerH).reduced(24, 10);
        keepRender.setBounds(f.removeFromTop(28));
        status.setBounds(f);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(fx::ground);
        g.setColour(fx::text);
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        g.drawText("Theme editor", 18, 0, getWidth() - 60, titleH, juce::Justification::centredLeft);
        g.setColour(fx::border);
        g.fillRect(0, titleH - 1, getWidth(), 1);

        g.setColour(fx::panel);
        g.fillRect(0, getHeight() - footerH, getWidth(), footerH);
        g.setColour(fx::border);
        g.fillRect(0, getHeight() - footerH, getWidth(), 1);
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        g.setColour(fx::border.brighter(0.4f));
        g.drawRect(getLocalBounds(), 1);
    }

    // Only a press on the title bar drags the window (it has no system title bar any more)
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragging = e.getPosition().getY() < titleH;
        if (dragging) dragger.startDraggingComponent(&owner, e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (dragging) dragger.dragComponent(&owner, e, nullptr); }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }

private:
    static constexpr int titleH = 40, footerH = 72;
    FlatCloseButton closeButton;
    juce::ComponentDragger dragger;
    bool dragging = false;

    struct Item { juce::Component* comp; int height; };

    void changed()
    {
        doc.name = nameEditor.getText().trim();
        if (owner.onChange)
            owner.onChange(ThemeFile::makeTheme(doc));
    }
    void refreshRows() { for (auto& r : rows) r->repaint(); }

    void save()
    {
        doc.name = nameEditor.getText().trim();
        if (doc.name.isEmpty() || ThemeRegistry::findBuiltin(doc.name) != nullptr)
        {
            status.setText("Give it a name that is not a built-in theme.", juce::dontSendNotification);
            return;
        }
        const auto folder = ThemeFile::userFolder();
        folder.createDirectory();
        const auto file = folder.getChildFile(juce::File::createLegalFileName(doc.name) + ".json");
        if (!keepRender.getToggleState())
            doc.render = {};
        else if (owner.currentRender)
            doc.render = ThemeRender::all(owner.currentRender());
        const auto theme = ThemeFile::makeTheme(doc);
        if (file.replaceWithText(ThemeFile::toJson(theme, baseName)))
        {
            status.setText("Saved " + file.getFileName(), juce::dontSendNotification);
            if (owner.onSaved)
                owner.onSaved(theme, file);
        }
        else
            status.setText("Could not write " + file.getFullPathName(), juce::dontSendNotification);
    }

    ThemeEditorWindow& owner;
    juce::String baseName;
    ThemeFile::Doc doc, original;
    juce::TextEditor nameEditor;
    juce::TextButton saveButton { "Save" }, resetButton { "Reset" };
    juce::Label status;
    juce::ToggleButton keepRender { "Save the render style with the theme" };
    juce::Viewport viewport;
    juce::Component body;
    std::vector<std::unique_ptr<GroupHeader>> headers;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<Item> items;
};

ThemeEditorWindow::ThemeEditorWindow(const EditorTheme& start, const juce::String& baseName)
    : juce::DocumentWindow("Theme editor", fx::ground, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(false);
    setTitleBarHeight(0);   // the content draws its own, in this window's own colours
    content = new Content(*this, start, baseName);
    setContentOwned(content, true);
    setResizable(true, false);
    centreWithSize(getWidth(), getHeight());
}

ThemeEditorWindow::~ThemeEditorWindow() = default;

void ThemeEditorWindow::closeButtonPressed()
{
    if (onClosed)
        onClosed();
}
