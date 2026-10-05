#include "ThemeEditorWindow.h"
#include "AppTheme.h"

namespace
{
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
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(juce::Colour(0xff999999));
        for (float y = r.getY(); y < r.getBottom(); y += 6.0f)
            for (float x = r.getX() + (static_cast<int>((y - r.getY()) / 6.0f) % 2 ? 6.0f : 0.0f); x < r.getRight(); x += 12.0f)
                g.fillRect(juce::Rectangle<float>(x, y, 6.0f, 6.0f).getIntersection(r));
        g.setColour(c);
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        if (autoWhenTransparent && c.isTransparent())
        {
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
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
        addAndMakeVisible(label);
        swatch.getColour = std::move(get);
        swatch.setColour = std::move(set);
        swatch.autoWhenTransparent = autoCol;
        addAndMakeVisible(swatch);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(8, 2);
        swatch.setBounds(r.removeFromRight(64));
        label.setBounds(r);
    }
    juce::Label label;
    Swatch swatch;
};

class GroupHeader : public juce::Label
{
public:
    explicit GroupHeader(const juce::String& t)
    {
        setText(t, juce::dontSendNotification);
        setFont(juce::FontOptions(14.0f, juce::Font::bold));
        setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
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
        nameEditor.setTextToShowWhenEmpty("Theme name", juce::Colours::grey);
        addAndMakeVisible(nameEditor);
        saveButton.onClick = [this] { save(); };
        resetButton.onClick = [this] { doc = original; refreshRows(); changed(); };
        addAndMakeVisible(saveButton);
        addAndMakeVisible(resetButton);
        addAndMakeVisible(status);
        status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

        const char* lastGroup = nullptr;
        for (const auto& e : ThemeFile::entries())
        {
            if (lastGroup == nullptr || juce::String(lastGroup) != e.group)
            {
                headers.push_back(std::make_unique<GroupHeader>(e.group));
                body.addAndMakeVisible(*headers.back());
                items.push_back({ headers.back().get(), 28 });
                lastGroup = e.group;
            }
            const auto* entry = &e;
            rows.push_back(std::make_unique<Row>(e.label,
                [this, entry] { return ThemeFile::colourRef(doc, *entry); },
                [this, entry](juce::Colour c) { ThemeFile::colourRef(doc, *entry) = c; changed(); },
                e.autoWhenTransparent));
            body.addAndMakeVisible(*rows.back());
            items.push_back({ rows.back().get(), 26 });
        }
        int h = 4;
        for (auto& it : items) h += it.height;
        body.setSize(400, h);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);
        setSize(420, 640);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8);
        auto top = r.removeFromTop(28);
        resetButton.setBounds(top.removeFromRight(60));
        top.removeFromRight(6);
        saveButton.setBounds(top.removeFromRight(60));
        top.removeFromRight(6);
        nameEditor.setBounds(top);
        status.setBounds(r.removeFromBottom(22));
        r.removeFromTop(6);
        viewport.setBounds(r);
        const int w = viewport.getMaximumVisibleWidth();
        int y = 2;
        for (auto& it : items) { it.comp->setBounds(0, y, w, it.height); y += it.height; }
        body.setSize(w, y + 2);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff2b2b2b)); }

private:
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
    juce::Viewport viewport;
    juce::Component body;
    std::vector<std::unique_ptr<GroupHeader>> headers;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<Item> items;
};

ThemeEditorWindow::ThemeEditorWindow(const EditorTheme& start, const juce::String& baseName)
    : juce::DocumentWindow("Theme editor", juce::Colour(0xff2b2b2b), juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(true);
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
