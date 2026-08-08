#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/ModuleDescriptions.h"

class ModuleBrowserPanel : public juce::Component
{
public:
    ModuleBrowserPanel();

    // Members are destroyed in reverse declaration order, so rootItem goes
    // before treeView, and ~TreeView writes to whatever root it still points
    // at: "if (rootItem != nullptr) rootItem->setOwnerView (nullptr);". With
    // the item already gone that is a write into freed memory, which left the
    // heap corrupt and aborted the app on every exit. Let go of the root while
    // both are still alive.
    ~ModuleBrowserPanel() override;

    void setModuleDescriptions(ModuleDescriptions* descriptions);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildTree();
    void applyFilter();

    class CategoryItem : public juce::TreeViewItem
    {
    public:
        CategoryItem(const juce::String& categoryName,
                     const std::vector<const ModuleDescriptor*>& mods);

        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return name; }
        void paintItem(juce::Graphics& g, int width, int height) override;

    private:
        juce::String name;
    };

    class ModuleItem : public juce::TreeViewItem
    {
    public:
        ModuleItem(const ModuleDescriptor* desc);

        bool mightContainSubItems() override { return false; }
        juce::String getUniqueName() const override { return descriptor->name; }
        void paintItem(juce::Graphics& g, int width, int height) override;

        // Enable drag & drop - return ModuleDescriptor pointer as drag description
        juce::var getDragSourceDescription() override;

        const ModuleDescriptor* getDescriptor() const { return descriptor; }

    private:
        const ModuleDescriptor* descriptor;
    };

    ModuleDescriptions* moduleDescs = nullptr;
    juce::TextEditor filterField;
    juce::TreeView treeView;
    std::unique_ptr<juce::TreeViewItem> rootItem;

    // Custom root item that holds category children
    class RootItem : public juce::TreeViewItem
    {
    public:
        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return "root"; }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleBrowserPanel)
};
