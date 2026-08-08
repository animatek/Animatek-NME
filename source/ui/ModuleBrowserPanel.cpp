#include "ModuleBrowserPanel.h"
#include "AppTheme.h"

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
    g.setColour(AppTheme::palette().textPrimary);
    g.setFont(juce::Font(AppTheme::uiFont(14.0f).withStyle("Bold")));
    g.drawText(name, 4, 0, width - 4, height, juce::Justification::centredLeft);
}

// --- ModuleItem ---

ModuleBrowserPanel::ModuleItem::ModuleItem(const ModuleDescriptor* desc)
    : descriptor(desc) {}

void ModuleBrowserPanel::ModuleItem::paintItem(juce::Graphics& g, int width, int height)
{
    // Module name
    g.setColour(AppTheme::palette().textSecondary);
    g.setFont(juce::Font(AppTheme::uiFont(13.0f)));
    g.drawText(descriptor->fullname, 4, 0, width - 80, height, juce::Justification::centredLeft);

    // DSP cost on the right, formatted as the original editor prints it
    if (descriptor->cycles > 0)
    {
        g.setColour(AppTheme::palette().textMuted);
        g.setFont(juce::Font(AppTheme::uiFont(11.0f)));
        g.drawText(formatDspCost(descriptor->cycles), width - 70, 0, 66, height,
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

ModuleBrowserPanel::~ModuleBrowserPanel()
{
    // See the header: the tree outlives its root item, and ~TreeView writes to
    // the root it is still holding. Hand it a null root first.
    treeView.setRootItem(nullptr);
}

ModuleBrowserPanel::ModuleBrowserPanel()
{
    filterField.setTextToShowWhenEmpty("Filter modules...", juce::Colour(0xff666666));
    filterField.setColour(juce::TextEditor::backgroundColourId, AppTheme::palette().inputBackground);
    filterField.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    filterField.setColour(juce::TextEditor::outlineColourId, AppTheme::palette().buttonActive);
    filterField.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible(filterField);

    treeView.setColour(juce::TreeView::backgroundColourId, AppTheme::palette().backgroundPanel);
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
    g.fillAll(AppTheme::palette().backgroundPanel);
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
                        && !d->fullname.toLowerCase().contains(filterText)
                        && !d->tags.contains(filterText);
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
