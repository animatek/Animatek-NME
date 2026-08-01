#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

class Module;

// A named snapshot of one module's parameter values.
//
// Values are keyed by the parameter's component-id ("p1", "p2", ...) rather than
// by position, so a preset survives any reordering of a descriptor's parameter
// list, and a hand-written preset may list only the parameters it cares about.
// Recalling a preset sets exactly the parameters it names and leaves every other
// one alone, which is what makes a two-line preset ("just the filter mode") as
// valid as a full capture.
struct ModulePreset
{
    juce::String name;
    juce::String moduleType;              // ModuleDescriptor::name, e.g. "DrumSynth"
    std::map<juce::String, int> values;   // component-id -> value
    bool builtIn = false;                 // ships with the editor: never written, never deleted
};

// Presets for every module type, stored as one .pchp pack per type under the
// user's preset library.
//
// Nothing here knows about any particular module: a type is just a string, so
// enabling presets for the sequencers or anything else costs no code. The
// DrumSynth is simply the first type that has any.
class ModulePresetLibrary
{
public:
    // <library root>/Presets. Rereads every pack found there. Passing an empty
    // file (no library configured yet) leaves only the built-ins, and saving is
    // then refused rather than silently writing somewhere the user won't find.
    void setFolder(const juce::File& presetsFolder);
    const juce::File& getFolder() const { return folder; }
    bool canSave() const { return folder != juce::File(); }

    // Registered at startup. Built-ins always sort before user presets.
    void addBuiltIn(ModulePreset preset);

    const std::vector<ModulePreset>& forType(const juce::String& moduleType) const;
    const ModulePreset* find(const juce::String& moduleType, int index) const;

    // Each returns false if the pack could not be written; the in-memory list is
    // only changed when the write succeeds, so what is on screen matches disk.
    int  add(ModulePreset preset);        // returns the new index, or -1
    bool remove(const juce::String& moduleType, int index);
    bool rename(const juce::String& moduleType, int index, const juce::String& newName);

    // The lowest free "<type> <n>", so saving never needs a dialog to get a
    // usable name and never silently overwrites another preset.
    juce::String suggestName(const juce::String& moduleType) const;

    // Every parameter the module exposes as a plain parameter. Morph twins and
    // custom controls are not settings a preset should carry.
    static ModulePreset capture(const Module& m, juce::String name);

    // Converts the flat drum_presets.txt written before presets became a library.
    // Does nothing if the DrumSynth pack already exists.
    void migrateLegacyDrumPresets(const juce::File& legacyFile);

private:
    juce::File packFile(const juce::String& moduleType) const;
    bool writePack(const juce::String& moduleType);
    void readPack(const juce::File& file);
    void reload();

    juce::File folder;
    std::map<juce::String, std::vector<ModulePreset>> byType;
};
