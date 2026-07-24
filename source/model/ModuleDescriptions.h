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

    const ModuleDescriptor* getModuleByIndex(int index) const;
    const ModuleDescriptor* getModuleByName(const juce::String& name) const;

    const std::vector<ModuleDescriptor>& getAllModules() const { return modules; }
    juce::StringArray getCategories() const;

    std::vector<const ModuleDescriptor*> getModulesInCategory(const juce::String& category) const;

    int getModuleCount() const { return static_cast<int>(modules.size()); }

private:
    bool loadFromXml(std::unique_ptr<juce::XmlElement> xml);
    void parseModule(const juce::XmlElement& elem);
    double getAttributeDouble(const juce::XmlElement& moduleElem, const juce::String& attrName, double defaultVal = 0.0) const;
    int getAttributeInt(const juce::XmlElement& moduleElem, const juce::String& attrName, int defaultVal = 0) const;
    juce::String getAttributeString(const juce::XmlElement& moduleElem, const juce::String& attrName, const juce::String& defaultVal = {}) const;

    std::vector<ModuleDescriptor> modules;
    std::map<int, size_t> indexLookup;
    std::map<juce::String, size_t> nameLookup;
};
