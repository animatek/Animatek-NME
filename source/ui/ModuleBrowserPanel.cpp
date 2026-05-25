#include "ModuleBrowserPanel.h"

// --- CategoryItem ---

ModuleBrowserPanel::CategoryItem::CategoryItem(const juce::String& categoryName,
                                                const std::vector<const ModuleDescriptor*>& mods)
    : name(categoryName)
{
    for (auto* desc : mods)
        addSubItem(new ModuleItem(desc));
}

void ModuleBrowserPanel::CategoryItem::paintItem(juce::Graphics& g, int width, int height)
{
    g.setColour(juce::Colour(0xffdddddd));
    g.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
    g.drawText(name, 4, 0, width - 4, height, juce::Justification::centredLeft);
}

// --- ModuleItem ---

ModuleBrowserPanel::ModuleItem::ModuleItem(const ModuleDescriptor* desc)
    : descriptor(desc) {}

void ModuleBrowserPanel::ModuleItem::paintItem(juce::Graphics& g, int width, int height)
{
    // Module name
    g.setColour(juce::Colour(0xffcccccc));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(descriptor->fullname, 4, 0, width - 80, height, juce::Justification::centredLeft);

    // Cycles cost on the right
    if (descriptor->cycles > 0)
    {
        g.setColour(juce::Colour(0xff888888));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::String(descriptor->cycles, 1), width - 70, 0, 66, height,
                   juce::Justification::centredRight);
    }
}

juce::var ModuleBrowserPanel::ModuleItem::getDragSourceDescription()
{
    // Return a var containing the ModuleDescriptor pointer encoded as int64
    // The drop target will decode this to identify which module was dragged
    auto desc = new juce::DynamicObject();
    desc->setProperty("type", "module");
    desc->setProperty("descriptorPtr", reinterpret_cast<juce::int64>(descriptor));
    desc->setProperty("typeId", descriptor->index);
    desc->setProperty("name", descriptor->name);
    return juce::var(desc);
}

// --- ModuleBrowserPanel ---

ModuleBrowserPanel::ModuleBrowserPanel()
{
    filterField.setTextToShowWhenEmpty("Filter modules...", juce::Colour(0xff666666));
    filterField.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff25282E));
    filterField.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    filterField.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff444A53));
    filterField.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible(filterField);

    treeView.setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff323232));
    treeView.setDefaultOpenness(false);
    addAndMakeVisible(treeView);
}

void ModuleBrowserPanel::setModuleDescriptions(ModuleDescriptions* descriptions)
{
    moduleDescs = descriptions;
    rebuildTree();
}

void ModuleBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff323232));
}

void ModuleBrowserPanel::resized()
{
    auto area = getLocalBounds();
    filterField.setBounds(area.removeFromTop(28).reduced(2));
    treeView.setBounds(area);
}

void ModuleBrowserPanel::rebuildTree()
{
    treeView.setRootItem(nullptr);
    rootItem.reset();

    if (moduleDescs == nullptr)
        return;

    rootItem = std::make_unique<RootItem>();

    auto categories = moduleDescs->getCategories();
    for (auto& cat : categories)
    {
        auto mods = moduleDescs->getModulesInCategory(cat);
        if (!mods.empty())
            rootItem->addSubItem(new CategoryItem(cat, mods));
    }

    treeView.setRootItem(rootItem.get());
    treeView.setRootItemVisible(false);
}

void ModuleBrowserPanel::applyFilter()
{
    auto filterText = filterField.getText().toLowerCase();

    treeView.setRootItem(nullptr);
    rootItem.reset();

    if (moduleDescs == nullptr)
        return;

    rootItem = std::make_unique<RootItem>();

    auto categories = moduleDescs->getCategories();
    for (auto& cat : categories)
    {
        auto mods = moduleDescs->getModulesInCategory(cat);

        if (filterText.isNotEmpty())
        {
            mods.erase(std::remove_if(mods.begin(), mods.end(),
                [&filterText](const ModuleDescriptor* d)
                {
                    return !d->name.toLowerCase().contains(filterText)
                        && !d->fullname.toLowerCase().contains(filterText);
                }),
                mods.end());
        }

        if (!mods.empty())
        {
            auto* catItem = new CategoryItem(cat, mods);
            rootItem->addSubItem(catItem);
            if (filterText.isNotEmpty())
                catItem->setOpen(true);
        }
    }

    treeView.setRootItem(rootItem.get());
    treeView.setRootItemVisible(false);
}
