#pragma once

#include "Descriptors.h"
#include <vector>
#include <map>
#include <memory>

class ModuleDescriptions
{
public:
    ModuleDescriptions();

    bool loadFromFile(const juce::File& xmlFile);
    bool loadFromXmlString(const juce::String& xmlString);
    // Adds the modules of another file (the G1-Mods layer, data/mods-modules.xml) to those
    // loaded. Only before any descriptor is handed out: the vector may move them.
    bool addFromXmlString(const juce::String& xmlString);

    const ModuleDescriptor* getModuleByIndex(int index) const;
    const ModuleDescriptor* getModuleByName(const juce::String& name) const;

    const std::vector<ModuleDescriptor>& getAllModules() const { return modules; }
    juce::StringArray getCategories() const;

    // Modules a modified OS adds (G1-Mods) live in "<family> (mods)", listed right after the
    // family, so they never mix with Clavia's and are easy to find while testing.
    static juce::String modsCategoryFor(const juce::String& category) { return category + " (mods)"; }
    static bool isModsCategory(const juce::String& category) { return category.endsWith(" (mods)"); }

    // They are only offered (categories, menus, the MCP's list) while the user has asked for
    // them: off, NME offers exactly Clavia's modules. A patch with one still opens either way.
    void setModsShown(bool shown) { modsShown = shown; }
    bool areModsShown() const { return modsShown; }
    bool isOffered(const ModuleDescriptor& d) const { return modsShown || !isModsCategory(d.category); }

    std::vector<const ModuleDescriptor*> getModulesInCategory(const juce::String& category) const;

    int getModuleCount() const { return static_cast<int>(modules.size()); }

private:
    bool loadFromXml(std::unique_ptr<juce::XmlElement> xml, bool add = false);
    void parseModule(const juce::XmlElement& elem);
    double getAttributeDouble(const juce::XmlElement& moduleElem, const juce::String& attrName, double defaultVal = 0.0) const;
    int getAttributeInt(const juce::XmlElement& moduleElem, const juce::String& attrName, int defaultVal = 0) const;
    juce::String getAttributeString(const juce::XmlElement& moduleElem, const juce::String& attrName, const juce::String& defaultVal = {}) const;

    std::vector<ModuleDescriptor> modules;
    std::map<int, size_t> indexLookup;
    std::map<juce::String, size_t> nameLookup;
    bool modsShown = false;
};
