#pragma once
//==============================================================================
// ModuleHelpData.h
// Nord Modular G1 — Editor v3.03 help content
// Auto-generated from Nord_Modular_Editor_v3_03_Helpfile.hlp
// © Clavia DMI AB 1999  |  Parser: Animatek NME project
//==============================================================================

#include <juce_core/juce_core.h>

namespace NordHelp
{

struct ParamHelp
{
    juce::String name;
    juce::String description;
};

struct ModuleHelp
{
    juce::String name;
    juce::String description;
    juce::Array<ParamHelp> params;

    /** Returns the help text for a specific parameter, or empty string if not found. */
    juce::String getParamHelp (const juce::String& paramName) const
    {
        for (auto& p : params)
            if (p.name.equalsIgnoreCase (paramName))
                return p.description;
        return {};
    }
};

/** Returns the full help database (157 modules). */
const juce::Array<ModuleHelp>& getHelpDatabase();

/** Finds help for a module by name (case-insensitive). Returns nullptr if not found. */
const ModuleHelp* findModuleHelp (const juce::String& moduleName);

} // namespace NordHelp
