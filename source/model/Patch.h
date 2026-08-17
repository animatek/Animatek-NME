#pragma once

#include "Descriptors.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <array>
#include <functional>

class Parameter
{
public:
    Parameter(const ParameterDescriptor& desc)
        : descriptor(&desc), value(desc.defaultValue) {}

    const ParameterDescriptor* getDescriptor() const { return descriptor; }

    int getValue() const { return value; }
    void setValue(int v) { value = juce::jlimit(descriptor->minValue, descriptor->maxValue, v); }

    int getMorphGroup() const { return morphGroup; }
    void setMorphGroup(int g) { morphGroup = g; }

    int getMorphRange() const { return morphRange; }
    void setMorphRange(int r) { morphRange = r; }

    bool isLocked() const { return locked; }
    void setLocked(bool l) { locked = l; }

private:
    const ParameterDescriptor* descriptor;
    int value;
    int morphGroup = -1;  // -1 = none
    int morphRange = 0;   // signed -127..127
    bool locked = false;  // locked from randomization
};

class Connector
{
public:
    Connector(const ConnectorDescriptor& desc) : descriptor(&desc) {}
    const ConnectorDescriptor* getDescriptor() const { return descriptor; }

private:
    const ConnectorDescriptor* descriptor;
};

class Light
{
public:
    Light(const LightDescriptor& desc) : descriptor(&desc) {}
    const LightDescriptor* getDescriptor() const { return descriptor; }

    int getValue() const { return value; }
    void setValue(int v) { value = v; }

private:
    const LightDescriptor* descriptor;
    int value = 0;
};

struct Connection
{
    Connector* output = nullptr;
    Connector* input = nullptr;
};

class Module
{
public:
    using ModuleMovedCallback = std::function<void(Module*, int oldX, int oldY)>;

    static std::unique_ptr<Module> createFromDescriptor(const ModuleDescriptor& desc);

    const ModuleDescriptor* getDescriptor() const { return descriptor; }

    const juce::String& getTitle() const { return title; }
    void setTitle(const juce::String& t) { title = t; }

    juce::Point<int> getPosition() const { return position; }
    void setPosition(juce::Point<int> p);

    void setModuleMovedCallback(ModuleMovedCallback cb) { onMoved = std::move(cb); }

    const std::vector<Parameter>& getParameters() const { return parameters; }
    std::vector<Parameter>& getParameters() { return parameters; }

    const std::vector<Connector>& getConnectors() const { return connectors; }
    std::vector<Connector>& getConnectors() { return connectors; }

    const std::vector<Light>& getLights() const { return lights; }
    std::vector<Light>& getLights() { return lights; }

    Parameter* getParameter(int index);
    Connector* getConnector(int index);
    Connector* getConnector(int index, bool isOutput);

    int getContainerIndex() const { return containerIndex; }
    void setContainerIndex(int idx) { containerIndex = idx; }

    // Patch Mutator: excluded modules are never touched by Mutate/Randomize.
    // Persisted in the .var sidecar, not the .pch (keeps the format standard).
    bool isExcludedFromMutation() const { return excludedFromMutation; }
    void setExcludedFromMutation(bool e) { excludedFromMutation = e; }

private:
    Module() = default;

    const ModuleDescriptor* descriptor = nullptr;
    juce::String title;
    juce::Point<int> position { 0, 0 };
    int containerIndex = 0;  // slot index within the voice area
    std::vector<Parameter> parameters;
    std::vector<Connector> connectors;
    std::vector<Light> lights;
    bool excludedFromMutation = false;
    ModuleMovedCallback onMoved;
};

/** Names a module by where it lives in the patch rather than by where it
    happens to sit in memory.

    The UI holds on to modules across events that destroy them: a delete, an
    undo of an add or a paste, a patch arriving from the synth. A raw `Module*`
    kept across one of those is a dangling pointer, silent on Linux and fatal
    on macOS, and the editor had to sweep every one of them before each paint
    to stay upright (issue #61). A container index cannot dangle: it finds
    whatever is at that spot now, or nothing.

    Undo re-inserts a module at the index it had, so a reference taken before a
    delete is still the right one after the undo, which a pointer never was. */
struct ModuleRef
{
    int section = -1;         // 0 = common, 1 = poly (PDL2/Java convention)
    int containerIndex = -1;  // the module's index within that area

    bool isValid() const { return section >= 0 && containerIndex >= 0; }
    void clear() { section = -1; containerIndex = -1; }

    bool operator== (const ModuleRef& o) const
    { return section == o.section && containerIndex == o.containerIndex; }
    bool operator!= (const ModuleRef& o) const { return !(*this == o); }
    bool operator<  (const ModuleRef& o) const
    { return section != o.section ? section < o.section
                                  : containerIndex < o.containerIndex; }
};

class ModuleContainer
{
public:
    using CableAddedCallback = std::function<void(Connector* output, Connector* input)>;
    using CableRemovedCallback = std::function<void(Connector* output, Connector* input)>;
    using ModuleAddedCallback = std::function<void(Module* module)>;
    using ModuleRemovedCallback = std::function<void(Module* module)>;

    Module* addModule(std::unique_ptr<Module> module);
    /** Add module without firing the onModuleAdded callback (for undo re-insertion) */
    Module* addModuleSilent(std::unique_ptr<Module> module);
    void removeModule(Module* module);

    /** Remove module without destroying it — returns ownership for undo storage.
     *  Also removes all connections involving this module's connectors.
     *  Does NOT fire the onModuleRemoved callback. */
    std::unique_ptr<Module> extractModule(Module* module);

    bool canAdd(const ModuleDescriptor& desc) const;

    void addConnection(Connector* output, Connector* input);
    void removeConnection(Connector* output, Connector* input);
    void removeConnectionsForConnector(Connector* conn);

    /** Walk the undirected cable net containing `start` and return the output
     *  connector driving it, or nullptr for an undriven net. Cables may chain
     *  input→input (the Connection "output" slot then holds an input), so a
     *  net has at most one real output. */
    // The output driving the net `start` belongs to, or nullptr for an undriven
    // net. `ignoreOutput`/`ignoreInput` name one cable to walk around: a
    // re-route in flight has to judge the net as it will be once the cable has
    // moved, not as it still is while the drag runs.
    Connector* findNetOutput(Connector* start,
                             const Connector* ignoreOutput = nullptr,
                             const Connector* ignoreInput = nullptr);

    Module* getModuleByIndex(int containerIndex);
    const Module* getModuleByIndex(int containerIndex) const;

    /** True while this container still owns `module`. The UI keeps raw Module*
     *  pointers (selection, inspector, hover) and a delete or an undone add
     *  destroys the object under them, so anything holding one must ask this
     *  before dereferencing it (issue #61). */
    bool contains(const Module* module) const;

    const std::vector<std::unique_ptr<Module>>& getModules() const { return modules; }
    const std::vector<Connection>& getConnections() const { return connections; }

    // Event callbacks
    void setCableAddedCallback(CableAddedCallback cb) { onCableAdded = std::move(cb); }
    void setCableRemovedCallback(CableRemovedCallback cb) { onCableRemoved = std::move(cb); }
    void setModuleAddedCallback(ModuleAddedCallback cb) { onModuleAdded = std::move(cb); }
    void setModuleRemovedCallback(ModuleRemovedCallback cb) { onModuleRemoved = std::move(cb); }

private:
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<Connection> connections;
    CableAddedCallback onCableAdded;
    CableRemovedCallback onCableRemoved;
    ModuleAddedCallback onModuleAdded;
    ModuleRemovedCallback onModuleRemoved;
};

// A text note pinned to the patch grid: editor-only, because the G1 has no such
// module and never hears about it. It takes up its rectangle of the grid the way
// a module does, so modules make room for it and it makes room for them.
// Persisted in the .pch's [Comments] section, an extension in the same spirit as
// [Notes] - editors that don't know it skip it.
struct PatchComment
{
    int id = 0;         // stable while the patch is open; undo refers to it
    int section = 1;    // 1 = poly voice area, 0 = common
    int x = 0;          // grid column
    int y = 0;          // grid row
    int width = 1;      // grid columns (a module is always exactly one)
    int height = 2;     // grid rows (2 is the size the module bar drops)
    juce::String text;

    int gridWidth()  const { return juce::jmax(1, width); }
    int gridHeight() const { return juce::jmax(1, height); }
    /** True when the note covers column `gx`, which is how the placement code
        asks whether it is in the way. */
    bool coversColumn(int gx) const { return gx >= x && gx < x + gridWidth(); }
};

struct PatchHeader
{
    int keyRangeMin = 0;
    int keyRangeMax = 127;
    int velRangeMin = 0;
    int velRangeMax = 127;
    int bendRange = 2;
    int portamentoTime = 0;
    bool portamento = false;
    int pedalMode = 0;
    int voices = 1;
    int octaveShift = 2;
    // Voice retrigger
    int voiceRetriggerPoly = 1;
    int voiceRetriggerCommon = 1;
    // Unknown fields (preserved for round-trip fidelity)
    int unknown1 = 0, unknown2 = 0, unknown3 = 0, unknown4 = 0;
    // Cable visibility flags
    bool cableVisRed = true;
    bool cableVisBlue = true;
    bool cableVisYellow = true;
    bool cableVisGray = true;
    bool cableVisGreen = true;
    bool cableVisPurple = true;
    bool cableVisWhite = true;
    // Section separator position
    int separatorPosition = 4000;
};

struct MorphAssignment
{
    int section = 0;   // 0=common, 1=poly
    int module = 0;
    int param = 0;
    int morph = 0;     // 0-3
    int range = 0;     // signed 8-bit
};

struct KnobAssignment
{
    bool assigned = false;
    int section = 0;
    int module = 0;
    int param = 0;
};

struct CtrlAssignment
{
    int control = 0;
    int section = 0;
    int module = 0;
    int param = 0;
};

struct NoteSlot
{
    int note = 0;
    int attack = 0;
    int release = 0;
};

class Patch
{
public:
    Patch();

    const juce::String& getName() const { return name; }
    void setName(const juce::String& n) { name = n; }

    // Which entry in the editor's extras library this patch belongs to. Written
    // into the .pch's [NME] section, so a file says outright which comments,
    // notes and variations are its own, and empty for a patch that came off the
    // wire (the synth has nowhere to keep it) until the editor works it out.
    juce::String extrasId;

    PatchHeader& getHeader() { return header; }
    const PatchHeader& getHeader() const { return header; }

    ModuleContainer& getPolyVoiceArea() { return polyVoiceArea; }
    ModuleContainer& getCommonArea() { return commonArea; }

    const ModuleContainer& getPolyVoiceArea() const { return polyVoiceArea; }
    const ModuleContainer& getCommonArea() const { return commonArea; }

    // PDL2/Java convention: section 0 = common, section 1 = poly
    ModuleContainer& getContainer(int section) { return section == 1 ? polyVoiceArea : commonArea; }
    const ModuleContainer& getContainer(int section) const { return section == 1 ? polyVoiceArea : commonArea; }

    /** The module a reference names, or nullptr when nothing is there any
        more. Every read of a ModuleRef goes through here, which is what makes
        holding one across a delete safe. */
    Module* getModule(ModuleRef ref)
    {
        if (!ref.isValid())
            return nullptr;
        return getContainer(ref.section).getModuleByIndex(ref.containerIndex);
    }
    const Module* getModule(ModuleRef ref) const
    {
        if (!ref.isValid())
            return nullptr;
        return getContainer(ref.section).getModuleByIndex(ref.containerIndex);
    }

    /** A reference to a module known to be in this patch's given area. */
    static ModuleRef refTo(const Module& module, int section)
    {
        return { section, module.getContainerIndex() };
    }

    // Create a new module and add it to the specified section
    // Returns pointer to the created module, or nullptr if failed
    Module* createModule(int section, int typeId, int gridX, int gridY,
                         const juce::String& moduleName, const class ModuleDescriptions& descs);

    // Morph map
    std::array<int, 4> morphValues = { 0, 0, 0, 0 };
    std::array<int, 4> morphKeyboard = { 0, 0, 0, 0 };
    std::vector<MorphAssignment> morphAssignments;

    // Knob map (23 knobs)
    std::array<KnobAssignment, 23> knobAssignments;

    // Control map (MIDI CC assignments)
    std::vector<CtrlAssignment> ctrlAssignments;

    // Custom parameter dump data (per-module custom values)
    // Stored as module containerIndex -> vector of parameter values
    struct CustomDumpEntry { int index; std::vector<int> values; };
    std::vector<CustomDumpEntry> polyCustomDump;
    std::vector<CustomDumpEntry> commonCustomDump;

    // Apply a custom dump entry to the matching module's custom-class
    // parameters (sequencer events, clock-divider displays, ...)
    void applyCustomDumpEntry(int section, const CustomDumpEntry& entry);

    // Notes
    std::vector<NoteSlot> notes;

    // Free-text patch notes ([Notes] section, not MIDI NoteSlots)
    juce::String patchNotes;

    // --- Editor-only text notes on the canvas ---
    const std::vector<PatchComment>& getComments() const { return comments; }
    std::vector<PatchComment>& getComments() { return comments; }

    PatchComment* getCommentById(int id)
    {
        for (auto& c : comments)
            if (c.id == id)
                return &c;
        return nullptr;
    }

    /** Adds a comment and gives it an id. Returns the id, which is what undo
        and the canvas hold on to: the vector reallocates, pointers do not
        survive, ids do. */
    int addComment(PatchComment comment)
    {
        comment.id = ++lastCommentId;
        comments.push_back(std::move(comment));
        return comments.back().id;
    }

    /** Re-inserts a comment keeping its id (undo of a delete). */
    void restoreComment(const PatchComment& comment)
    {
        comments.push_back(comment);
        lastCommentId = juce::jmax(lastCommentId, comment.id);
    }

    /** Takes over another patch's notes wholesale, keeping their ids valid: the
        id counter has to follow, or the next new note would reuse an id that is
        already on the canvas. */
    void adoptComments(std::vector<PatchComment> incoming)
    {
        comments = std::move(incoming);
        for (const auto& c : comments)
            lastCommentId = juce::jmax(lastCommentId, c.id);

        // Notes arriving without an id get one now. Ids only mean anything while
        // a patch is open, so nothing outside the editor stores them, and the
        // extras library does not either: a note left holding id 0 would answer
        // to every lookup for "no note at all" and could not be deleted.
        for (auto& c : comments)
            if (c.id <= 0)
                c.id = ++lastCommentId;
    }

    bool removeCommentById(int id)
    {
        for (auto it = comments.begin(); it != comments.end(); ++it)
            if (it->id == id) { comments.erase(it); return true; }
        return false;
    }

private:
    juce::String name { "Init Patch" };
    PatchHeader header;
    ModuleContainer polyVoiceArea;
    ModuleContainer commonArea;
    std::vector<PatchComment> comments;
    int lastCommentId = 0;
};
