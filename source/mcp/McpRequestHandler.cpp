#include "McpRequestHandler.h"
#include "../MainComponent.h"
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/SignalType.h"
#include "../model/Mutator.h"
#include "../undo/PatchActions.h"
#include "../model/LightMeterLayout.h"
#include "../protocol/KnobAssignmentMessage.h"
#include "McpRules.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace {

// Thrown by the private handlers below and caught in handle() - carries a
// machine-readable code alongside a human-readable message so the bridge
// (and the LLM driving it) can branch on failure kind, not just fail.
struct McpError
{
    juce::String code;
    juce::String message;
};

constexpr int kNumSlots = 4;  // fixed A-D slot count, same constant as MainComponent's
constexpr int kMaxGridColumns = 40;
constexpr int kMaxGridRows = 128;
constexpr int kMaxModulesPerColumn = 8;

struct ResolvedEndpoint
{
    Module* module = nullptr;
    Connector* connector = nullptr;
};

int resolveSection(const juce::var& params)
{
    if (!params.hasProperty("section"))
        throw McpError{ "missing_param", "section is required (0=common, 1=poly)" };
    int section = static_cast<int>(params["section"]);
    if (section != 0 && section != 1)
        throw McpError{ "invalid_section", "section must be 0 (common) or 1 (poly)" };
    return section;
}

bool isPositionFree(const ModuleContainer& container, const Module* exclude,
                    int gridX, int gridY, int height)
{
    if (gridX < 0 || gridX >= kMaxGridColumns || gridY < 0
        || gridY + height > kMaxGridRows)
        return false;

    for (auto& module : container.getModules())
    {
        if (module.get() == exclude || module->getPosition().x != gridX)
            continue;
        const int otherY = module->getPosition().y;
        const int otherHeight = module->getDescriptor()->height;
        if (gridY < otherY + otherHeight && otherY < gridY + height)
            return false;
    }
    return true;
}

std::optional<juce::Point<int>> findAutomaticPosition(const ModuleContainer& container,
                                                       int height)
{
    for (int gridX = 0; gridX < kMaxGridColumns; ++gridX)
    {
        std::vector<const Module*> column;
        for (auto& module : container.getModules())
            if (module->getPosition().x == gridX)
                column.push_back(module.get());

        if (static_cast<int>(column.size()) >= kMaxModulesPerColumn)
            continue;

        std::sort(column.begin(), column.end(), [](const Module* a, const Module* b) {
            return a->getPosition().y < b->getPosition().y;
        });

        int candidateY = 0;
        for (auto* module : column)
        {
            const int moduleY = module->getPosition().y;
            // Strict inequality leaves one clear grid row between modules.
            if (candidateY + height < moduleY)
                break;
            candidateY = std::max(candidateY,
                                  moduleY + module->getDescriptor()->height + 1);
        }

        if (isPositionFree(container, nullptr, gridX, candidateY, height))
            return juce::Point<int>{ gridX, candidateY };
    }
    return std::nullopt;
}

ResolvedEndpoint resolveEndpoint(ModuleContainer& container, const juce::var& endpoint,
                                 const char* label)
{
    if (!endpoint.hasProperty("containerIndex") || !endpoint.hasProperty("connector"))
        throw McpError{ "missing_param", juce::String(label) + " must have containerIndex and connector" };

    int index = static_cast<int>(endpoint["containerIndex"]);
    auto* module = container.getModuleByIndex(index);
    if (!module)
        throw McpError{ "unknown_module", juce::String(label) + ": no module with containerIndex " + juce::String(index) };

    const auto connectorName = endpoint["connector"].toString();
    const bool directionGiven = endpoint.hasProperty("isOutput");
    const bool wantOutput = directionGiven && static_cast<bool>(endpoint["isOutput"]);
    Connector* outputVariant = nullptr;
    Connector* inputVariant = nullptr;
    for (auto& connector : module->getConnectors())
    {
        if (connector.getDescriptor()->name != connectorName)
            continue;
        if (connector.getDescriptor()->isOutput) outputVariant = &connector;
        else                                     inputVariant = &connector;
    }

    Connector* match = nullptr;
    if (outputVariant && inputVariant)
    {
        if (!directionGiven)
            throw McpError{ "ambiguous_connector",
                juce::String(label) + ": '" + connectorName
                + "' exists as both an input and an output on this module - specify isOutput" };
        match = wantOutput ? outputVariant : inputVariant;
    }
    else
    {
        match = outputVariant ? outputVariant : inputVariant;
    }

    if (!match)
        throw McpError{ "unknown_connector", juce::String(label) + ": no connector named '"
            + connectorName + "' on this module" };
    return { module, match };
}

Module* findConnectorOwner(ModuleContainer& container, const Connector* connector)
{
    for (auto& module : container.getModules())
        for (auto& candidate : module->getConnectors())
            if (&candidate == connector)
                return module.get();
    return nullptr;
}

void ensurePatchEditable(MainComponent& owner)
{
    if (owner.isPatchTransferInProgress())
        throw McpError{ "patch_transfer_busy", "A patch transfer is in progress; retry when it completes" };
}

juce::String signalTypeToString(SignalType t)
{
    switch (t)
    {
        case SignalType::Audio:       return "audio";
        case SignalType::Control:     return "control";
        case SignalType::Logic:       return "logic";
        case SignalType::MasterSlave: return "master-slave";
        case SignalType::User1:       return "user1";
        case SignalType::User2:       return "user2";
        case SignalType::None:
        default:                      return "none";
    }
}

juce::var connectorToVar(const ConnectorDescriptor& c)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", c.name);
    obj->setProperty("isOutput", c.isOutput);
    obj->setProperty("signalType", signalTypeToString(c.signalType));
    return juce::var(obj);
}

// verbose=false trims the fields a caller rarely needs when it is just reading
// the patch back: componentId is an internal theme handle, and min/max only
// matter when picking a value, which set_parameter clamps and reports anyway.
juce::var parameterToVar(const Parameter& p, bool verbose = true)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", p.getDescriptor()->name);
    obj->setProperty("parameterId", p.getDescriptor()->index);
    obj->setProperty("parameterClass", p.getDescriptor()->paramClass);
    obj->setProperty("value", p.getValue());
    if (verbose)
    {
        obj->setProperty("componentId", p.getDescriptor()->componentId);
        obj->setProperty("min", p.getDescriptor()->minValue);
        obj->setProperty("max", p.getDescriptor()->maxValue);
    }
    return juce::var(obj);
}

bool isMorphParameter(const Parameter& p)
{
    return p.getDescriptor()->paramClass == "morph";
}

// The compact form is what a browsing client needs: enough to choose a type and
// call describe_module_type for the detail. Serialising all 110 types with every
// connector produced a response too large for a client to hold in context.
juce::var moduleTypeToVar(const ModuleDescriptor& d, bool includeConnectors)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("typeId", d.index);
    obj->setProperty("name", d.name);
    obj->setProperty("category", d.category);

    if (!includeConnectors)
        return juce::var(obj);

    obj->setProperty("fullname", d.fullname);
    obj->setProperty("instantiable", d.instantiable);
    obj->setProperty("limit", d.limit);
    obj->setProperty("height", d.height);

    juce::Array<juce::var> connectors;
    for (auto& c : d.connectors)
        connectors.add(connectorToVar(c));
    obj->setProperty("connectors", connectors);

    return juce::var(obj);
}


// Parameters we have renamed since the bridge became something other people
// script against. A caller can always address a parameter by parameterId, but a
// script that wrote the old name should not break because the editor found a
// better word for it (issue #78).
bool parameterNameMatches(const juce::String& current, const juce::String& wanted)
{
    if (current.equalsIgnoreCase(wanted))
        return true;

    struct RetiredName { const char* was; const char* now; };
    static const RetiredName retired[] = {
        { "in sense", "in level" },   // the two mixers' input attenuators
    };

    for (const auto& r : retired)
    {
        const juce::String was(r.was);
        if (!wanted.startsWithIgnoreCase(was))
            continue;
        // Both names carry the same trailing index ("in sense 3"), so keep it.
        if (current.equalsIgnoreCase(juce::String(r.now) + wanted.substring(was.length())))
            return true;
    }
    return false;
}

// The parameter a request names, by parameterId, parameterName or both (which
// must then agree). A name only matches an editable "parameter"-class entry,
// never a morph twin, and retired names still work (parameterNameMatches).
Parameter* resolveParameter(Module& module, const juce::var& params)
{
    const bool hasName = params.hasProperty("parameterName");
    const bool hasId = params.hasProperty("parameterId");
    if (!hasName && !hasId)
        throw McpError{ "missing_param", "parameterName or parameterId is required" };

    Parameter* parameter = nullptr;
    if (hasId)
        parameter = module.getParameter(static_cast<int>(params["parameterId"]));
    if (hasName)
    {
        const auto wantedName = params["parameterName"].toString().trim();
        Parameter* namedParameter = nullptr;
        for (auto& candidate : module.getParameters())
        {
            auto* descriptor = candidate.getDescriptor();
            if (descriptor->paramClass == "parameter"
                && parameterNameMatches(descriptor->name, wantedName))
            {
                namedParameter = &candidate;
                break;
            }
        }
        if (!namedParameter)
            throw McpError{ "unknown_parameter", "No editable parameter named '" + wantedName
                + "' on module " + module.getTitle() };
        if (parameter && parameter != namedParameter)
            throw McpError{ "parameter_mismatch", "parameterName and parameterId identify different parameters" };
        parameter = namedParameter;
    }
    if (!parameter)
        throw McpError{ "unknown_parameter", "No editable synth parameter matches the supplied identifier" };
    return parameter;
}

// What a knob, morph or MIDI CC assignment points at, in the same terms the
// other tools use, plus a label a person can read. Section 2 module 1 is not a
// module: it is the patch's four morph groups, which knobs and CCs can drive.
juce::var assignmentTargetToVar(Patch& patch, int section, int moduleIndex, int paramId)
{
    auto* obj = new juce::DynamicObject();
    if (section == McpRules::kMorphSection && moduleIndex == McpRules::kMorphModule)
    {
        obj->setProperty("morphGroup", paramId);
        obj->setProperty("label", "Morph group " + juce::String(paramId));
        return juce::var(obj);
    }

    obj->setProperty("section", section);
    obj->setProperty("containerIndex", moduleIndex);
    obj->setProperty("parameterId", paramId);
    juce::String label = "section " + juce::String(section) + " module " + juce::String(moduleIndex)
                       + " parameter " + juce::String(paramId);
    if (section == 0 || section == 1)
    {
        if (auto* module = patch.getContainer(section).getModuleByIndex(moduleIndex))
        {
            juce::String parameterName;
            if (auto* parameter = module->getParameter(paramId))
                parameterName = parameter->getDescriptor()->name;
            obj->setProperty("moduleName", module->getTitle());
            obj->setProperty("parameterName", parameterName);
            label = module->getTitle() + ": " + parameterName;
        }
    }
    obj->setProperty("label", label);
    return juce::var(obj);
}

struct AssignTarget
{
    int section = 0;
    int module = 0;
    int param = 0;
};

// Either a module parameter (section + containerIndex + parameterName/Id) or,
// where the synth allows it, a morph group's dial (morphGroup 0-3).
AssignTarget resolveAssignTarget(Patch& patch, const juce::var& params, bool allowMorphGroup)
{
    if (params.hasProperty("morphGroup"))
    {
        if (!allowMorphGroup)
            throw McpError{ "invalid_param", "morphGroup is not a valid target for this tool" };
        if (params.hasProperty("containerIndex"))
            throw McpError{ "invalid_param", "Give either morphGroup or section/containerIndex/parameter, not both" };
        const int group = static_cast<int>(params["morphGroup"]);
        if (!McpRules::isValidMorphGroup(group))
            throw McpError{ "invalid_param", "morphGroup must be 0-3" };
        return { McpRules::kMorphSection, McpRules::kMorphModule, group };
    }

    const int section = resolveSection(params);
    if (!params.hasProperty("containerIndex"))
        throw McpError{ "missing_param", "containerIndex is required" };
    const int containerIndex = static_cast<int>(params["containerIndex"]);
    auto* module = patch.getContainer(section).getModuleByIndex(containerIndex);
    if (!module)
        throw McpError{ "unknown_module", "No module with containerIndex " + juce::String(containerIndex) };

    auto* parameter = resolveParameter(*module, params);
    const auto* descriptor = parameter->getDescriptor();
    if (descriptor->paramClass != "parameter")
        throw McpError{ "not_assignable", "'" + descriptor->name + "' is a " + descriptor->paramClass
            + " entry, not a synth parameter; only synth parameters can be assigned" };
    return { section, containerIndex, descriptor->index };
}

// A knob as an index 0-22 or as the name list_assignments prints ("Knob 7").
int resolveKnob(const juce::var& params)
{
    if (!params.hasProperty("knob"))
        throw McpError{ "missing_param", "knob is required: an index 0-22 or a name such as \"Knob 7\"" };

    const auto& value = params["knob"];
    int knob = -1;
    if (value.isString())
    {
        const auto found = McpRules::knobFromName(value.toString());
        if (!found)
            throw McpError{ "invalid_knob", "Unknown knob name '" + value.toString()
                + "': use \"Knob 1\" to \"Knob 18\", \"Pedal\", \"After touch\" or \"On/Off switch\"" };
        knob = *found;
    }
    else
    {
        knob = static_cast<int>(value);
    }

    if (!KnobAssignmentMessage::isValidKnob(knob))
        throw McpError{ "invalid_knob", "knob index must be 0-17 (Knob 1-18), 19 (Pedal), 20 (After touch) or 22 (On/Off switch)" };
    return knob;
}

juce::String knobName(int knob)
{
    return juce::String(KnobAssignmentMessage::getKnobName(knob));
}
} // namespace

// "slot" is 0-3 (A-D) on a port: "port" (1-based, as MIDI Setup numbers them) picks the
// synth and defaults to the one being edited. The result is the editor's global slot,
// port * 4 + slot, which is also what the replies report.
int McpRequestHandler::resolveSlot(const juce::var& params) const
{
    const int port = params.hasProperty("port") ? static_cast<int>(params["port"]) - 1
                                                 : owner_.getActiveSynth();
    if (port < 0 || port >= kMaxSynths)
        throw McpError{ "invalid_port", "port must be 1-" + juce::String(kMaxSynths) };
    if (! params.hasProperty("slot"))
        return port == owner_.getActiveSynth() ? owner_.getActiveSlot() : SynthSlot::global(port, 0);
    const int local = static_cast<int>(params["slot"]);
    if (local < 0 || local >= kNumSlots)
        throw McpError{ "invalid_slot", "slot must be 0-3 (A-D)" };
    return SynthSlot::global(port, local);
}

juce::var McpRequestHandler::handle(const juce::var& request)
{
    auto* obj = new juce::DynamicObject();
    juce::var response(obj);
    obj->setProperty("id", request.getProperty("id", juce::var()));

    auto method = request.getProperty("method", juce::var()).toString();
    auto params = request.getProperty("params", juce::var());
    if (!params.isObject())
        params = juce::var(new juce::DynamicObject());

    try
    {
        juce::var result;
        if (method == "list_module_types")     result = listModuleTypes(params);
        else if (method == "describe_module_type") result = describeModuleType(params);
        else if (method == "list_modules")     result = listModules(params);
        else if (method == "list_patches")     result = listPatches(params);
        else if (method == "add_module")       result = addModule(params);
        else if (method == "move_module")      result = moveModule(params);
        else if (method == "rename_module")    result = renameModule(params);
        else if (method == "delete_module")    result = deleteModule(params);
        else if (method == "connect_cable")    result = connectCable(params);
        else if (method == "delete_cable")     result = deleteCable(params);
        else if (method == "set_parameter")    result = setParameter(params);
        else if (method == "mutate_patch")     result = mutatePatch(params);
        else if (method == "create_patch")     result = createPatch(params);
        else if (method == "open_patch")       result = openPatch(params);
        else if (method == "save_patch")       result = savePatch(params);
        else if (method == "store_to_bank")    result = storeToBank(params);
        else if (method == "get_synth_status") result = getSynthStatus(params);
        else if (method == "get_events")       result = getEvents(params);
        else if (method == "read_lights")      result = readLights(params);
        else if (method == "list_assignments") result = listAssignments(params);
        else if (method == "assign_knob")      result = assignKnob(params);
        else if (method == "unassign_knob")    result = unassignKnob(params);
        else if (method == "assign_morph")     result = assignMorph(params);
        else if (method == "unassign_morph")   result = unassignMorph(params);
        else if (method == "assign_midi_cc")   result = assignMidiCc(params);
        else if (method == "unassign_midi_cc") result = unassignMidiCc(params);
        else if (method == "set_morph_value")  result = setMorphValue(params);
        else if (method == "play_note")        result = playNote(params);
        else if (method == "list_bank")        result = listBank(params);
        else if (method == "replace_module")   result = replaceModule(params);
        else if (method == "list_midi_ports")  result = listMidiPorts(params);
        else if (method == "connect_midi")     result = connectMidi(params);
        else if (method == "disconnect_midi")  result = disconnectMidi(params);
        else if (method == "fetch_patch")      result = fetchPatch(params);
        else throw McpError{ "unknown_method", "Unknown method: " + method };

        obj->setProperty("ok", true);
        obj->setProperty("result", result);
    }
    catch (const McpError& e)
    {
        obj->setProperty("ok", false);
        auto* err = new juce::DynamicObject();
        err->setProperty("code", e.code);
        err->setProperty("message", e.message);
        obj->setProperty("error", juce::var(err));
    }

    return response;
}

juce::var McpRequestHandler::listModuleTypes(const juce::var& params)
{
    // Compact by default — see moduleTypeToVar. Callers that genuinely want every
    // connector of every type can still ask, and a category filter narrows it
    // enough that the verbose form stays reasonable.
    const bool includeConnectors = params.hasProperty("includeConnectors")
                                && static_cast<bool>(params["includeConnectors"]);
    const juce::String category = params.hasProperty("category")
                                ? params["category"].toString().trim() : juce::String();

    juce::Array<juce::var> modules;
    for (auto& d : owner_.getModuleDescriptions().getAllModules())
    {
        if (category.isNotEmpty() && !d.category.equalsIgnoreCase(category))
            continue;
        modules.add(moduleTypeToVar(d, includeConnectors));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty("modules", modules);
    obj->setProperty("count", modules.size());
    if (!includeConnectors)
        obj->setProperty("hint", "Call describe_module_type for a type's connectors and parameters.");
    return juce::var(obj);
}

juce::var McpRequestHandler::mutatePatch(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    const auto op = params.hasProperty("operation")
                  ? params["operation"].toString().trim().toLowerCase()
                  : juce::String("mutate");
    if (op != "mutate" && op != "randomize")
        throw McpError{ "bad_param",
                        "operation must be \"mutate\" or \"randomize\". Interpolate and cross "
                        "need a second parent snapshot, which this API cannot yet supply." };

    // The same exclusions the Mutator panel applies, minus its Quick Lock
    // categories, which are UI state with no equivalent here: per-parameter
    // locks, per-module mutation exclusion, and Output modules — those set
    // level and routing rather than timbre, and are never mutated.
    Mutator::LockPredicate isLocked =
        [patch](int section, int moduleId, int paramId) {
            auto* mod = patch->getContainer(section).getModuleByIndex(moduleId);
            if (!mod) return true;
            if (mod->isExcludedFromMutation()) return true;
            auto* param = mod->getParameter(paramId);
            if (!param || param->isLocked()) return true;
            const auto* md = mod->getDescriptor();
            if (!md) return true;
            return md->category == "In/Out" && md->name.endsWithIgnoreCase("Output");
        };

    const auto clamp01 = [](double v) { return static_cast<float>(juce::jlimit(0.0, 1.0, v)); };
    const float probability = params.hasProperty("probability")
                            ? clamp01(static_cast<double>(params["probability"])) : 0.5f;
    const float range = params.hasProperty("range")
                      ? clamp01(static_cast<double>(params["range"])) : 0.25f;

    auto& rng = juce::Random::getSystemRandom();
    const auto mother = Mutator::captureCurrent(*patch);
    const auto child = (op == "randomize")
                     ? Mutator::randomize(mother, *patch, rng, isLocked)
                     : Mutator::mutate(mother, *patch, probability, range, rng, isLocked);
    if (!child.filled)
        throw McpError{ "mutate_failed", "The mutator returned an empty snapshot" };

    // One RandomizeAction for the whole batch: a single undo step, and delivery
    // through the connection's coalesced queue rather than a burst of sends.
    std::vector<RandomizeAction::ParamChange> changes;
    for (auto& e : child.entries)
    {
        auto* mod = patch->getContainer(e.section).getModuleByIndex(e.moduleId);
        if (!mod) continue;
        auto* param = mod->getParameter(e.paramId);
        if (!param || param->isLocked()) continue;
        const int oldValue = param->getValue();
        if (oldValue != e.value)
            changes.push_back({ e.section, e.moduleId, e.paramId, oldValue, e.value });
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("operation", op);
    result->setProperty("changed", static_cast<int>(changes.size()));

    if (changes.empty())
    {
        result->setProperty("note", "Nothing changed: every parameter was locked or excluded.");
        return juce::var(result);
    }

    owner_.getSlotUndoManager(slot).beginNewTransaction(
        op == "randomize" ? "Randomize (MCP)" : "Mutate (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(new RandomizeAction(*ctx, std::move(changes))))
        throw McpError{ "mutate_failed", "Failed to apply the mutation (unexpected)" };

    return juce::var(result);
}

juce::var McpRequestHandler::describeModuleType(const juce::var& params)
{
    const bool hasId = params.hasProperty("typeId");
    const bool hasName = params.hasProperty("typeName")
                      && params["typeName"].toString().trim().isNotEmpty();
    if (!hasId && !hasName)
        throw McpError{ "missing_param", "typeId or typeName is required" };

    const ModuleDescriptor* descriptor = nullptr;
    if (hasId)
    {
        const int typeId = static_cast<int>(params["typeId"]);
        descriptor = owner_.getModuleDescriptions().getModuleByIndex(typeId);
        if (!descriptor)
            throw McpError{ "unknown_module_type", "No module type with typeId " + juce::String(typeId) };
    }
    else
    {
        const auto wanted = params["typeName"].toString().trim();
        descriptor = owner_.getModuleDescriptions().getModuleByName(wanted);
        if (!descriptor)
            throw McpError{ "unknown_module_type", "No module type named " + wanted };
    }

    auto result = moduleTypeToVar(*descriptor, /*includeConnectors=*/true);

    // Parameter descriptors, so a caller can see what set_parameter accepts
    // without having to add the module first. Morph entries are omitted for the
    // same reason they are in list_modules: they roughly double the payload and
    // shadow every real parameter with a "morph:" twin.
    const bool includeMorph = params.hasProperty("includeMorph")
                           && static_cast<bool>(params["includeMorph"]);
    juce::Array<juce::var> paramsOut;
    for (auto& pd : descriptor->parameters)
    {
        if (!includeMorph && pd.paramClass == "morph")
            continue;
        auto* p = new juce::DynamicObject();
        p->setProperty("name", pd.name);
        p->setProperty("parameterId", pd.index);
        p->setProperty("parameterClass", pd.paramClass);
        p->setProperty("min", pd.minValue);
        p->setProperty("max", pd.maxValue);
        paramsOut.add(juce::var(p));
    }
    result.getDynamicObject()->setProperty("parameters", paramsOut);
    return result;
}

juce::var McpRequestHandler::listModules(const juce::var& params)
{
    int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };

    const bool hasSectionFilter = params.hasProperty("section");
    const int sectionFilter = hasSectionFilter ? resolveSection(params) : -1;

    // Defaults trade completeness for a response a client can actually hold: a
    // 62-module patch serialised in full came to 109 KB, over half of it morph
    // parameters. Connector names are included instead, since cabling what is
    // already in the patch is the common reason to call this at all.
    const auto flag = [&params](const char* key, bool fallback) {
        return params.hasProperty(key) ? static_cast<bool>(params[key]) : fallback;
    };
    const bool includeMorph      = flag("includeMorph", false);
    const bool verboseParams     = flag("verboseParameters", false);

    // Narrowing to specific modules is the escape hatch for very large patches:
    // ask for the structure with includeParameters=false, then come back for the
    // few modules that matter. When a filter is given, parameters and connectors
    // default to on regardless — the caller is explicitly asking for detail.
    std::vector<int> onlyIndices;
    if (params.hasProperty("containerIndex"))
    {
        const auto& v = params["containerIndex"];
        if (auto* arr = v.getArray())
            for (auto& e : *arr) onlyIndices.push_back(static_cast<int>(e));
        else
            onlyIndices.push_back(static_cast<int>(v));
    }
    const bool filtered = !onlyIndices.empty();
    const auto wanted = [&onlyIndices](int idx) {
        return std::find(onlyIndices.begin(), onlyIndices.end(), idx) != onlyIndices.end();
    };

    const bool includeParameters = flag("includeParameters", true);
    const bool includeConnectors = flag("includeConnectors", true);

    juce::Array<juce::var> modulesOut;
    juce::Array<juce::var> cablesOut;

    for (int section = 0; section <= 1; ++section)
    {
        if (hasSectionFilter && section != sectionFilter)
            continue;

        auto& container = patch->getContainer(section);

        for (auto& modPtr : container.getModules())
        {
            if (filtered && !wanted(modPtr->getContainerIndex()))
                continue;

            auto* obj = new juce::DynamicObject();
            obj->setProperty("section", section);
            obj->setProperty("containerIndex", modPtr->getContainerIndex());
            obj->setProperty("typeId", modPtr->getDescriptor()->index);
            obj->setProperty("name", modPtr->getTitle());
            obj->setProperty("gridX", modPtr->getPosition().x);
            obj->setProperty("gridY", modPtr->getPosition().y);
            obj->setProperty("height", modPtr->getDescriptor()->height);

            if (includeConnectors)
            {
                juce::Array<juce::var> connectorsOut;
                for (auto& c : modPtr->getDescriptor()->connectors)
                    connectorsOut.add(connectorToVar(c));
                obj->setProperty("connectors", connectorsOut);
            }

            if (includeParameters)
            {
                juce::Array<juce::var> paramsOut;
                for (auto& param : modPtr->getParameters())
                {
                    if (!includeMorph && isMorphParameter(param))
                        continue;
                    paramsOut.add(parameterToVar(param, verboseParams));
                }
                obj->setProperty("parameters", paramsOut);
            }

            modulesOut.add(juce::var(obj));
        }

        // Connections store raw Connector*; find which module owns each end
        // so the response can address them the same way add_module/
        // connect_cable do (containerIndex + connector name).
        auto findOwnerAndName = [&container](const Connector* c, int& outIndex,
                                             juce::String& outName, bool& outIsOutput) -> bool {
            for (auto& mp : container.getModules())
                for (auto& conn : mp->getConnectors())
                    if (&conn == c) {
                        outIndex = mp->getContainerIndex();
                        outName = conn.getDescriptor()->name;
                        outIsOutput = conn.getDescriptor()->isOutput;
                        return true;
                    }
            return false;
        };

        for (auto& conn : container.getConnections())
        {
            int outIdx = -1, inIdx = -1;
            bool outIsOutput = false, inIsOutput = false;
            juce::String outName, inName;
            if (!findOwnerAndName(conn.output, outIdx, outName, outIsOutput)) continue;
            if (!findOwnerAndName(conn.input, inIdx, inName, inIsOutput)) continue;

            // A filtered query wants that module's own wiring, not the whole
            // patch's — keep cables with at least one end on a requested module.
            if (filtered && !wanted(outIdx) && !wanted(inIdx))
                continue;

            auto* outObj = new juce::DynamicObject();
            outObj->setProperty("containerIndex", outIdx);
            outObj->setProperty("connector", outName);
            outObj->setProperty("isOutput", outIsOutput);

            auto* inObj = new juce::DynamicObject();
            inObj->setProperty("containerIndex", inIdx);
            inObj->setProperty("connector", inName);
            inObj->setProperty("isOutput", inIsOutput);

            auto* cableObj = new juce::DynamicObject();
            cableObj->setProperty("section", section);
            cableObj->setProperty("out", juce::var(outObj));
            cableObj->setProperty("in", juce::var(inObj));
            cablesOut.add(juce::var(cableObj));
        }
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("patchName", patch->getName());
    result->setProperty("patchNotes", patch->patchNotes);
    result->setProperty("voices", patch->getHeader().voices);
    result->setProperty("keyRangeMin", patch->getHeader().keyRangeMin);
    result->setProperty("keyRangeMax", patch->getHeader().keyRangeMax);
    result->setProperty("modules", modulesOut);
    result->setProperty("cables", cablesOut);
    return juce::var(result);
}

juce::var McpRequestHandler::listPatches(const juce::var& params)
{
    const auto query = params.hasProperty("query")
        ? params["query"].toString().trim().toLowerCase() : juce::String();
    const auto& root = owner_.getPresetLibraryRoot();

    juce::Array<juce::var> loadedSlots;
    for (int slot = 0; slot < kTotalSlots; ++slot)
    {
        auto* patch = owner_.getSlotPatch(slot);
        if (!patch)
            continue;
        auto* obj = new juce::DynamicObject();
        obj->setProperty("slot", slot);
        obj->setProperty("port", SynthSlot::synthOf(slot) + 1);
        obj->setProperty("slotName", juce::String::charToString(static_cast<char>('A' + SynthSlot::localOf(slot))));
        obj->setProperty("patchName", patch->getName());
        const auto& file = owner_.getSlotPatchFile(slot);
        if (file != juce::File())
            obj->setProperty("path", file.getFullPathName());
        loadedSlots.add(juce::var(obj));
    }

    struct PatchFileEntry
    {
        juce::File file;
        juce::String source;
        juce::String relativePath;
    };
    std::vector<PatchFileEntry> files;
    auto scan = [&files, &root](const juce::String& folderName, const juce::String& source) {
        auto folder = root.getChildFile(folderName);
        if (!folder.isDirectory())
            return;
        for (juce::RangedDirectoryIterator it(folder, true, "*.pch", juce::File::findFiles);
             it != juce::RangedDirectoryIterator(); ++it)
        {
            auto file = it->getFile();
            files.push_back({ file, source, file.getRelativePathFrom(root) });
        }
    };

    if (root.isDirectory())
    {
        scan("Patches", "disk");
        scan("Banks", "bank-backup");
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.relativePath.compareIgnoreCase(b.relativePath) < 0;
    });

    juce::Array<juce::var> patches;
    for (const auto& entry : files)
    {
        const auto name = entry.file.getFileNameWithoutExtension();
        const auto haystack = (name + " " + entry.relativePath).toLowerCase();
        if (query.isNotEmpty() && !haystack.contains(query))
            continue;
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", name);
        obj->setProperty("source", entry.source);
        obj->setProperty("relativePath", entry.relativePath);
        obj->setProperty("path", entry.file.getFullPathName());
        patches.add(juce::var(obj));
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("libraryRoot", root == juce::File() ? juce::String() : root.getFullPathName());
    result->setProperty("libraryConfigured", root.isDirectory());
    result->setProperty("loadedSlots", loadedSlots);
    result->setProperty("patches", patches);
    return juce::var(result);
}

juce::var McpRequestHandler::addModule(const juce::var& params)
{
    int slot = resolveSlot(params);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    int section = resolveSection(params);

    const ModuleDescriptor* descriptor = nullptr;
    if (params.hasProperty("typeId"))
        descriptor = owner_.getModuleDescriptions().getModuleByIndex(static_cast<int>(params["typeId"]));
    else if (params.hasProperty("typeName"))
        descriptor = owner_.getModuleDescriptions().getModuleByName(params["typeName"].toString());
    else
        throw McpError{ "missing_param", "typeId or typeName is required" };

    if (!descriptor)
        throw McpError{ "unknown_type", "No module type matches the given typeId/typeName" };
    if (!descriptor->instantiable)
        throw McpError{ "not_instantiable", descriptor->name + " cannot be instantiated" };

    auto& container = patch->getContainer(section);
    if (!container.canAdd(*descriptor))
        throw McpError{ "limit_reached", descriptor->name + " has reached its instance limit in this section" };

    const bool hasGridX = params.hasProperty("gridX");
    const bool hasGridY = params.hasProperty("gridY");
    const bool autoPlace = !params.hasProperty("autoPlace") || static_cast<bool>(params["autoPlace"]);

    int gridX = 0;
    int gridY = 0;
    if (!autoPlace)
    {
        if (!hasGridX || !hasGridY)
            throw McpError{ "missing_param", "gridX and gridY are required when autoPlace is false" };
        gridX = static_cast<int>(params["gridX"]);
        gridY = static_cast<int>(params["gridY"]);
        if (!isPositionFree(container, nullptr, gridX, gridY, descriptor->height))
            throw McpError{ "position_occupied", "Requested position is outside the 40x128 grid or overlaps another module" };
    }
    else
    {
        auto position = findAutomaticPosition(container, descriptor->height);
        if (!position)
            throw McpError{ "no_free_position", "No free position remains within the 40x128 module grid" };
        gridX = position->x;
        gridY = position->y;
    }
    juce::String name = params.hasProperty("name") ? params["name"].toString() : descriptor->name;

    owner_.getSlotUndoManager(slot).beginNewTransaction("Add Module (MCP)");
    auto* action = new AddModuleAction(*ctx, section, descriptor->index, gridX, gridY, name);
    if (!owner_.getSlotUndoManager(slot).perform(action))
        throw McpError{ "add_failed", "Failed to add module (unexpected)" };

    auto* result = new juce::DynamicObject();
    result->setProperty("containerIndex", action->getContainerIndex());
    result->setProperty("gridX", gridX);
    result->setProperty("gridY", gridY);
    result->setProperty("height", descriptor->height);
    result->setProperty("autoPlaced", autoPlace);
    return juce::var(result);
}

juce::var McpRequestHandler::moveModule(const juce::var& params)
{
    int slot = resolveSlot(params);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    int section = resolveSection(params);

    if (!params.hasProperty("containerIndex"))
        throw McpError{ "missing_param", "containerIndex is required" };
    if (!params.hasProperty("gridX") || !params.hasProperty("gridY"))
        throw McpError{ "missing_param", "gridX and gridY are required" };

    int containerIndex = static_cast<int>(params["containerIndex"]);
    int gridX = static_cast<int>(params["gridX"]);
    int gridY = static_cast<int>(params["gridY"]);
    auto& container = patch->getContainer(section);
    auto* module = container.getModuleByIndex(containerIndex);
    if (!module)
        throw McpError{ "unknown_module", "No module with containerIndex " + juce::String(containerIndex) };
    if (!isPositionFree(container, module, gridX, gridY, module->getDescriptor()->height))
        throw McpError{ "position_occupied", "Requested position is outside the 40x128 grid or overlaps another module" };

    const auto oldPos = module->getPosition();
    const juce::Point<int> newPos{ gridX, gridY };
    owner_.getSlotUndoManager(slot).beginNewTransaction("Move Module (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(
            new MoveModuleAction(*ctx, section, containerIndex, oldPos, newPos)))
        throw McpError{ "move_failed", "Failed to move module (unexpected)" };

    auto* result = new juce::DynamicObject();
    result->setProperty("containerIndex", containerIndex);
    result->setProperty("gridX", gridX);
    result->setProperty("gridY", gridY);
    result->setProperty("height", module->getDescriptor()->height);
    return juce::var(result);
}

juce::var McpRequestHandler::renameModule(const juce::var& params)
{
    int slot = resolveSlot(params);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    int section = resolveSection(params);

    if (!params.hasProperty("containerIndex"))
        throw McpError{ "missing_param", "containerIndex is required" };
    if (!params.hasProperty("name"))
        throw McpError{ "missing_param", "name is required" };

    int containerIndex = static_cast<int>(params["containerIndex"]);
    juce::String newName = params["name"].toString().trim();
    if (newName.isEmpty())
        throw McpError{ "invalid_name", "name must not be empty" };
    if (newName.length() > Module::kMaxTitleLength)
        throw McpError{ "invalid_name", "name must be 16 characters or fewer (G1 module-name limit)" };

    auto& container = patch->getContainer(section);
    auto* module = container.getModuleByIndex(containerIndex);
    if (!module)
        throw McpError{ "unknown_module", "No module with containerIndex " + juce::String(containerIndex) };

    const juce::String oldName = module->getTitle();
    owner_.getSlotUndoManager(slot).beginNewTransaction("Rename Module (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(
            new RenameModuleAction(*ctx, section, containerIndex, oldName, newName)))
        throw McpError{ "rename_failed", "Failed to rename module (unexpected)" };

    auto* result = new juce::DynamicObject();
    result->setProperty("containerIndex", containerIndex);
    result->setProperty("name", newName);
    result->setProperty("previousName", oldName);
    return juce::var(result);
}

juce::var McpRequestHandler::deleteModule(const juce::var& params)
{
    int slot = resolveSlot(params);
    int section = resolveSection(params);
    auto* ctx = owner_.getSlotUndoContext(slot);
    auto* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);
    if (!params.hasProperty("containerIndex"))
        throw McpError{ "missing_param", "containerIndex is required" };

    int containerIndex = static_cast<int>(params["containerIndex"]);
    auto* module = patch->getContainer(section).getModuleByIndex(containerIndex);
    if (!module)
        throw McpError{ "unknown_module", "No module with containerIndex " + juce::String(containerIndex) };
    const auto name = module->getTitle();

    auto* action = new DeleteModuleAction(*ctx, section, module);
    owner_.prepareSlotModuleDeletion(slot);
    owner_.getSlotUndoManager(slot).beginNewTransaction("Delete Module (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(action))
        throw McpError{ "delete_failed", "Failed to delete module (unexpected)" };

    auto* result = new juce::DynamicObject();
    result->setProperty("containerIndex", containerIndex);
    result->setProperty("name", name);
    return juce::var(result);
}

juce::var McpRequestHandler::replaceModule(const juce::var& params)
{
    int slot = resolveSlot(params);
    auto* ctx = owner_.getSlotUndoContext(slot);
    auto* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    int section = resolveSection(params);
    if (!params.hasProperty("containerIndex"))
        throw McpError{ "missing_param", "containerIndex is required" };
    int containerIndex = static_cast<int>(params["containerIndex"]);
    auto* module = patch->getContainer(section).getModuleByIndex(containerIndex);
    if (!module)
        throw McpError{ "unknown_module", "No module with containerIndex " + juce::String(containerIndex) };

    const ModuleDescriptor* descriptor = nullptr;
    if (params.hasProperty("typeId"))
        descriptor = owner_.getModuleDescriptions().getModuleByIndex(static_cast<int>(params["typeId"]));
    else if (params.hasProperty("typeName"))
        descriptor = owner_.getModuleDescriptions().getModuleByName(params["typeName"].toString());
    else
        throw McpError{ "missing_param", "typeId or typeName is required" };
    if (!descriptor)
        throw McpError{ "unknown_type", "No module type matches the given typeId/typeName" };

    const auto* current = module->getDescriptor();
    if (descriptor->index == current->index)
        throw McpError{ "invalid_param", "The module is already a " + current->name };
    if (!descriptor->instantiable)
        throw McpError{ "not_instantiable", descriptor->name + " cannot be instantiated" };
    if (descriptor->category != current->category)
    {
        juce::StringArray family;
        for (auto* d : ModuleReplacement::candidates(owner_.getModuleDescriptions(), *current))
            family.add(d->name);
        throw McpError{ "not_same_family", descriptor->name + " is in " + descriptor->category + ", not "
            + current->category + ": a module can only be replaced by one of its family ("
            + family.joinIntoString(", ") + ")" };
    }

    const juce::String previousType = current->name;
    owner_.prepareSlotModuleDeletion(slot);
    auto* action = new ReplaceModuleAction(*ctx, section, module, descriptor->index);
    const int droppedCables = action->getDroppedCables();
    const int keptCables = action->getKeptCables();
    const int droppedAssignments = action->getDroppedAssignments();
    const int keptParameters = action->getKeptParameters();

    owner_.getSlotUndoManager(slot).beginNewTransaction("Replace Module (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(action))
        throw McpError{ "replace_failed", "Could not replace the module: no room below it in its column "
            "for the taller one, or the module limit is reached" };

    auto* replaced = patch->getContainer(section).getModuleByIndex(containerIndex);
    auto* result = new juce::DynamicObject();
    result->setProperty("containerIndex", containerIndex);
    result->setProperty("previousType", previousType);
    result->setProperty("type", descriptor->name);
    result->setProperty("name", replaced != nullptr ? replaced->getTitle() : juce::String());
    result->setProperty("height", descriptor->height);
    result->setProperty("keptCables", keptCables);
    result->setProperty("droppedCables", droppedCables);
    result->setProperty("keptParameters", keptParameters);
    result->setProperty("droppedAssignments", droppedAssignments);
    return juce::var(result);
}

juce::var McpRequestHandler::connectCable(const juce::var& params)
{
    int slot = resolveSlot(params);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    int section = resolveSection(params);

    if (!params.hasProperty("out") || !params.hasProperty("in"))
        throw McpError{ "missing_param", "out and in endpoint objects are required" };

    auto& container = patch->getContainer(section);

    auto [modA, connA] = resolveEndpoint(container, params["out"], "out");
    auto [modB, connB] = resolveEndpoint(container, params["in"], "in");

    // Replicate the exact polarity + one-output-per-net logic from
    // PatchCanvasComponent.cpp's cable-drag handler (~line 5770-5793): auto-
    // swap so a true output ends up as outConn; two inputs may chain
    // (allowed); two outputs never join. The model itself does not enforce
    // this - only callers do - so it must be replicated here.
    const bool aIsOut = connA->getDescriptor()->isOutput;
    const bool bIsOut = connB->getDescriptor()->isOutput;

    Module* outMod = nullptr; Connector* outConn = nullptr;
    Module* inMod  = nullptr; Connector* inConn  = nullptr;

    if (aIsOut && !bIsOut)       { outMod = modA; outConn = connA; inMod = modB; inConn = connB; }
    else if (!aIsOut && bIsOut)  { outMod = modB; outConn = connB; inMod = modA; inConn = connA; }
    else if (!aIsOut && !bIsOut) { outMod = modA; outConn = connA; inMod = modB; inConn = connB; }
    else
        throw McpError{ "two_outputs", "Cannot connect two outputs together" };

    auto* drv1 = container.findNetOutput(outConn);
    auto* drv2 = container.findNetOutput(inConn);
    if (drv1 != nullptr && drv2 != nullptr && drv1 != drv2)
        throw McpError{ "net_conflict", "Both connectors are already driven by different outputs - joining them would short two outputs together" };

    owner_.getSlotUndoManager(slot).beginNewTransaction("Connect Cable (MCP)");
    auto* action = new AddCableAction(*ctx, section,
        outMod->getContainerIndex(), outConn->getDescriptor()->index, outConn->getDescriptor()->isOutput,
        inMod->getContainerIndex(), inConn->getDescriptor()->index, inConn->getDescriptor()->isOutput);
    if (!owner_.getSlotUndoManager(slot).perform(action))
        throw McpError{ "connect_failed", "Failed to connect cable (unexpected)" };

    return juce::var(new juce::DynamicObject());
}

juce::var McpRequestHandler::deleteCable(const juce::var& params)
{
    int slot = resolveSlot(params);
    int section = resolveSection(params);
    auto* ctx = owner_.getSlotUndoContext(slot);
    auto* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);
    if (!params.hasProperty("out") || !params.hasProperty("in"))
        throw McpError{ "missing_param", "out and in endpoint objects are required" };

    auto& container = patch->getContainer(section);
    auto endpointA = resolveEndpoint(container, params["out"], "out");
    auto endpointB = resolveEndpoint(container, params["in"], "in");

    Connector* storedOut = nullptr;
    Connector* storedIn = nullptr;
    for (const auto& connection : container.getConnections())
    {
        if ((connection.output == endpointA.connector && connection.input == endpointB.connector)
            || (connection.output == endpointB.connector && connection.input == endpointA.connector))
        {
            storedOut = connection.output;
            storedIn = connection.input;
            break;
        }
    }
    if (!storedOut || !storedIn)
        throw McpError{ "unknown_cable", "No direct cable exists between the specified connectors" };

    auto* outModule = findConnectorOwner(container, storedOut);
    auto* inModule = findConnectorOwner(container, storedIn);
    if (!outModule || !inModule)
        throw McpError{ "unknown_cable", "Could not resolve the cable's owning modules" };

    owner_.getSlotUndoManager(slot).beginNewTransaction("Delete Cable (MCP)");
    auto* action = new DeleteCableAction(*ctx, section,
        outModule->getContainerIndex(), storedOut->getDescriptor()->index,
        storedOut->getDescriptor()->isOutput,
        inModule->getContainerIndex(), storedIn->getDescriptor()->index,
        storedIn->getDescriptor()->isOutput);
    if (!owner_.getSlotUndoManager(slot).perform(action))
        throw McpError{ "delete_failed", "Failed to delete cable (unexpected)" };

    return juce::var(new juce::DynamicObject());
}

juce::var McpRequestHandler::setParameter(const juce::var& params)
{
    int slot = resolveSlot(params);
    int section = resolveSection(params);
    auto* ctx = owner_.getSlotUndoContext(slot);
    auto* patch = owner_.getSlotPatch(slot);
    if (!ctx || !patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);
    if (!params.hasProperty("containerIndex"))
        throw McpError{ "missing_param", "containerIndex is required" };

    const bool hasName = params.hasProperty("parameterName");
    const bool hasId = params.hasProperty("parameterId");
    if (!hasName && !hasId)
        throw McpError{ "missing_param", "parameterName or parameterId is required" };
    const bool hasValue = params.hasProperty("value");
    const bool hasDelta = params.hasProperty("delta");
    if (hasValue == hasDelta)
        throw McpError{ "invalid_param", "Provide exactly one of value or delta" };

    int containerIndex = static_cast<int>(params["containerIndex"]);
    auto* module = patch->getContainer(section).getModuleByIndex(containerIndex);
    if (!module)
        throw McpError{ "unknown_module", "No module with containerIndex " + juce::String(containerIndex) };

    Parameter* parameter = resolveParameter(*module, params);

    auto* descriptor = parameter->getDescriptor();
    const int oldValue = parameter->getValue();
    const int requestedValue = hasValue
        ? static_cast<int>(params["value"])
        : oldValue + static_cast<int>(params["delta"]);
    const int newValue = juce::jlimit(descriptor->minValue, descriptor->maxValue, requestedValue);

    if (newValue != oldValue)
    {
        owner_.getSlotUndoManager(slot).beginNewTransaction("Set Parameter (MCP)");
        if (!owner_.getSlotUndoManager(slot).perform(new ParameterChangeAction(
                *ctx, section, containerIndex, descriptor->index, oldValue, newValue)))
            throw McpError{ "parameter_failed", "Failed to set parameter (unexpected)" };
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("containerIndex", containerIndex);
    result->setProperty("parameterName", descriptor->name);
    result->setProperty("parameterId", descriptor->index);
    result->setProperty("oldValue", oldValue);
    result->setProperty("value", newValue);
    result->setProperty("min", descriptor->minValue);
    result->setProperty("max", descriptor->maxValue);
    return juce::var(result);
}

juce::var McpRequestHandler::savePatch(const juce::var& params)
{
    int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };

    if (!params.hasProperty("path") || params["path"].toString().trim().isEmpty())
        throw McpError{ "missing_param", "path is required" };

    const auto path = params["path"].toString().trim();
    juce::File file;
    if (juce::File::isAbsolutePath(path))
    {
        file = juce::File(path);
    }
    else
    {
        // Relative paths resolve against the configured patches folder and must
        // stay inside it (same safety rule as open_patch's library paths).
        auto folder = owner_.getPatchesFolder();
        if (!folder.isDirectory())
            throw McpError{ "library_not_configured", "The patches folder is not configured" };
        file = folder.getChildFile(path);
        if (!file.isAChildOf(folder))
            throw McpError{ "invalid_path", "Relative paths must stay inside the patches folder" };
    }

    if (file.getFileExtension().isEmpty())
        file = file.withFileExtension("pch");
    if (!file.hasFileExtension("pch"))
        throw McpError{ "invalid_path", "path must have a .pch extension" };

    auto parent = file.getParentDirectory();
    if (!parent.isDirectory() && !parent.createDirectory())
        throw McpError{ "save_failed", "Could not create the destination folder" };

    if (!owner_.saveSlotPatchToFile(slot, file))
        throw McpError{ "save_failed", "Failed to write the patch file" };

    auto* result = new juce::DynamicObject();
    result->setProperty("path", file.getFullPathName());
    result->setProperty("name", patch->getName());
    result->setProperty("slot", slot);
    return juce::var(result);
}

juce::var McpRequestHandler::storeToBank(const juce::var& params)
{
    int slot = resolveSlot(params);
    ensurePatchEditable(owner_);

    if (!params.hasProperty("bank"))
        throw McpError{ "missing_param", "bank is required (1-9)" };
    if (!params.hasProperty("position"))
        throw McpError{ "missing_param", "position is required (1-99)" };

    int bank = static_cast<int>(params["bank"]);
    int position = static_cast<int>(params["position"]);
    if (bank < 1 || bank > 9)
        throw McpError{ "invalid_param", "bank must be 1-9" };
    if (position < 1 || position > 99)
        throw McpError{ "invalid_param", "position must be 1-99" };

    // The store uploads the patch to the synth slot first, then writes it to the
    // bank once the synth ACKs — so this overwrites the synth's working slot.
    juce::String error;
    if (!owner_.storeSlotPatchToBank(slot, bank - 1, position - 1, error))
        throw McpError{ "store_failed", error };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("bank", bank);
    result->setProperty("position", position);
    result->setProperty("location", bank * 100 + position);
    return juce::var(result);
}

juce::var McpRequestHandler::createPatch(const juce::var& params)
{
    int slot = resolveSlot(params);
    const auto name = params.hasProperty("name") ? params["name"].toString().trim()
                                                  : juce::String("Init Patch");
    const bool activate = !params.hasProperty("activate") || static_cast<bool>(params["activate"]);
    juce::String error;
    if (!owner_.createEmptyPatchInSlot(slot, name, activate, error))
        throw McpError{ "create_patch_failed", error };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("patchName", owner_.getSlotPatch(slot)->getName());
    return juce::var(result);
}

juce::var McpRequestHandler::openPatch(const juce::var& params)
{
    int slot = resolveSlot(params);
    const bool hasPath = params.hasProperty("path") && params["path"].toString().isNotEmpty();
    const bool hasName = params.hasProperty("name") && params["name"].toString().isNotEmpty();
    if (hasPath == hasName)
        throw McpError{ "invalid_param", "Provide exactly one of path or name" };

    juce::File selectedFile;
    if (hasPath)
    {
        const auto path = params["path"].toString();
        if (juce::File::isAbsolutePath(path))
        {
            selectedFile = juce::File(path);
        }
        else
        {
            const auto& root = owner_.getPresetLibraryRoot();
            if (!root.isDirectory())
                throw McpError{ "library_not_configured", "The preset library folder is not configured" };
            selectedFile = root.getChildFile(path);
            if (!selectedFile.isAChildOf(root))
                throw McpError{ "invalid_path", "Relative patch paths must stay inside the configured library" };
        }
    }
    else
    {
        const auto wantedName = params["name"].toString().trim();
        const auto& root = owner_.getPresetLibraryRoot();
        if (!root.isDirectory())
            throw McpError{ "library_not_configured", "The preset library folder is not configured" };

        std::vector<juce::File> matches;
        for (const auto& folderName : { juce::String("Patches"), juce::String("Banks") })
        {
            auto folder = root.getChildFile(folderName);
            if (!folder.isDirectory())
                continue;
            for (juce::RangedDirectoryIterator it(folder, true, "*.pch", juce::File::findFiles);
                 it != juce::RangedDirectoryIterator(); ++it)
            {
                auto file = it->getFile();
                if (file.getFileNameWithoutExtension().equalsIgnoreCase(wantedName))
                    matches.push_back(file);
            }
        }
        if (matches.empty())
            throw McpError{ "patch_not_found", "No library patch is named '" + wantedName + "'" };
        if (matches.size() > 1)
        {
            juce::String paths;
            for (const auto& match : matches)
                paths += (paths.isEmpty() ? "" : "; ") + match.getFullPathName();
            throw McpError{ "ambiguous_patch", "Multiple patches are named '" + wantedName
                + "'; use an exact path: " + paths };
        }
        selectedFile = matches.front();
    }

    const bool activate = !params.hasProperty("activate") || static_cast<bool>(params["activate"]);
    juce::String error;
    if (!owner_.loadPatchFileIntoSlot(slot, selectedFile, activate, error))
        throw McpError{ "open_patch_failed", error };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("patchName", owner_.getSlotPatch(slot)->getName());
    result->setProperty("path", selectedFile.getFullPathName());
    return juce::var(result);
}

// ---------------------------------------------------------------------------
// Reading the synth back. None of these send anything: they report what the
// editor has already been told, so a client can check an edit landed.
// ---------------------------------------------------------------------------

juce::var McpRequestHandler::getSynthStatus(const juce::var& /*params*/)
{
    const auto& connection = owner_.getConnectionManager();
    const auto& status = connection.getStatus();
    const bool connected = connection.isConnected();

    auto* result = new juce::DynamicObject();
    result->setProperty("connection",
        connected ? "connected"
                  : status.state == ConnectionManager::State::Connecting ? "connecting" : "disconnected");
    result->setProperty("message", status.message);
    if (connected)
        result->setProperty("synthOsVersion", juce::String(status.synthVersionHigh) + "."
            + juce::String(status.synthVersionLow).paddedLeft('0', 2));
    // null until the synth on this connection has sent its settings, so a client that
    // just switched synths never reads the previous one's name.
    result->setProperty("synthName", owner_.areSynthSettingsFromThisConnection()
                                         ? juce::var(juce::String(owner_.getCachedSynthSettings().name))
                                         : juce::var());
    result->setProperty("editorActiveSlot", owner_.getActiveSlot());
    result->setProperty("synthFocusedSlot", connected ? connection.getCurrentSlot() : -1);
    result->setProperty("patchListLoaded", connection.isPatchListLoaded());

    auto* transfer = new juce::DynamicObject();
    transfer->setProperty("fetching", connection.isFetchingPatch());
    transfer->setProperty("uploading", connection.isUploadingPatch());
    transfer->setProperty("editQueueIdle", connection.isAckedQueueIdle());
    result->setProperty("transfer", juce::var(transfer));

    const bool maskKnown = owner_.isSlotEnableStateKnown();
    // Everything above and below is about the synth being edited (its port); the
    // slots listed are its four, numbered globally like every other "slot".
    result->setProperty("port", owner_.getActiveSynth() + 1);
    result->setProperty("slotsPerPort", kNumSlots);
    juce::Array<juce::var> slots;
    for (int local = 0; local < kNumSlots; ++local)
    {
        const int slot = SynthSlot::global(owner_.getActiveSynth(), local);
        auto* s = new juce::DynamicObject();
        s->setProperty("slot", slot);
        s->setProperty("slotName", juce::String::charToString(static_cast<char>('A' + local)));
        auto* patch = owner_.getSlotPatch(slot);
        s->setProperty("patchName", patch != nullptr ? juce::var(patch->getName()) : juce::var());
        // LOCAL: the editor's patch is not known to match the synth's. Edits to
        // a LOCAL slot are still sent while connected (plan item S4).
        s->setProperty("local", owner_.isSlotLocal(slot));
        s->setProperty("enabled", maskKnown ? juce::var(owner_.getLastEnabledSlots()[static_cast<size_t>(local)])
                                            : juce::var());
        s->setProperty("voices", owner_.getSynthVoiceCounts()[static_cast<size_t>(local)]);
        const int bankSection = connection.getSlotBankSection(local);
        const int bankPosition = connection.getSlotBankPosition(local);
        if (bankSection >= 0 && bankPosition >= 0)
            s->setProperty("bankLocation", (bankSection + 1) * 100 + bankPosition + 1);
        slots.add(juce::var(s));
    }
    result->setProperty("slots", slots);

    const auto& frame = owner_.getLastLightMeterFrame();
    result->setProperty("lightsSlot", frame.slot);
    result->setProperty("lightsLastChangeAgeMs", frame.timeMs > 0
        ? juce::var(static_cast<juce::int64>(juce::Time::currentTimeMillis() - frame.timeMs))
        : juce::var());
    result->setProperty("latestEventSeq", static_cast<juce::int64>(owner_.getMcpEventLog().latestSeq()));
    return juce::var(result);
}

juce::var McpRequestHandler::getEvents(const juce::var& params)
{
    const juce::int64 after = params.hasProperty("after") ? static_cast<juce::int64>(params["after"]) : 0;
    const int limit = juce::jlimit(1, 500, params.hasProperty("limit") ? static_cast<int>(params["limit"]) : 100);

    juce::StringArray types;
    if (auto* wanted = params["types"].getArray())
        for (const auto& type : *wanted)
            types.add(type.toString());
    else if (params["types"].isString())
        types.add(params["types"].toString());

    const auto page = owner_.getMcpEventLog().since(after, std::numeric_limits<size_t>::max());

    juce::Array<juce::var> events;
    juce::int64 resumeFrom = static_cast<juce::int64>(page.latestSeq);
    juce::int64 lastIncluded = after;
    bool hasMore = false;
    for (const auto& event : page.events)
    {
        if (!types.isEmpty() && !types.contains(event.type))
            continue;
        if (events.size() >= limit)
        {
            hasMore = true;
            resumeFrom = lastIncluded;
            break;
        }
        auto* obj = new juce::DynamicObject();
        obj->setProperty("seq", static_cast<juce::int64>(event.seq));
        obj->setProperty("timeMs", static_cast<juce::int64>(event.timeMs));
        obj->setProperty("type", event.type);
        obj->setProperty("slot", event.slot);
        if (!event.data.isVoid())
            obj->setProperty("data", event.data);
        events.add(juce::var(obj));
        lastIncluded = static_cast<juce::int64>(event.seq);
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("events", events);
    result->setProperty("latestSeq", resumeFrom);
    result->setProperty("truncated", page.truncated);
    result->setProperty("hasMore", hasMore);
    return juce::var(result);
}

juce::var McpRequestHandler::readLights(const juce::var& params)
{
    const auto& connection = owner_.getConnectionManager();
    if (!connection.isConnected())
        throw McpError{ "not_connected", "No synth connected: LEDs and meters are streamed by the synth" };

    // The G1 streams lights for the one slot it has focused, which the editor
    // keeps equal to its active slot.
    const int slot = owner_.getActiveSlot();
    if (params.hasProperty("slot") && static_cast<int>(params["slot"]) != slot)
        throw McpError{ "lights_unavailable", "The synth only streams LEDs and meters for its focused slot ("
            + juce::String(slot) + "); focus the other slot first" };
    Patch* patch = owner_.getSlotPatch(slot);
    if (!patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };

    const bool hasSectionFilter = params.hasProperty("section");
    const int sectionFilter = hasSectionFilter ? resolveSection(params) : -1;
    std::vector<int> onlyIndices;
    if (params.hasProperty("containerIndex"))
    {
        const auto& v = params["containerIndex"];
        if (auto* arr = v.getArray())
            for (auto& e : *arr) onlyIndices.push_back(static_cast<int>(e));
        else
            onlyIndices.push_back(static_cast<int>(v));
    }

    const auto& frame = owner_.getLastLightMeterFrame();
    const auto table = LightMeterLayout::build(patch, &owner_.getThemeData());

    juce::Array<juce::var> modules;
    for (const auto& range : table.ranges)
    {
        if (range.lightCount == 0 && range.meterCount == 0)
            continue;
        if (hasSectionFilter && range.section != sectionFilter)
            continue;
        if (!onlyIndices.empty()
            && std::find(onlyIndices.begin(), onlyIndices.end(), range.containerIndex) == onlyIndices.end())
            continue;

        auto* module = patch->getContainer(range.section).getModuleByIndex(range.containerIndex);
        if (module == nullptr)
            continue;

        juce::Array<juce::var> leds;
        for (int i = 0; i < range.lightCount && range.lightBase + i < 128; ++i)
            leds.add(frame.lights[static_cast<size_t>(range.lightBase + i)]);
        juce::Array<juce::var> meters;
        for (int i = 0; i < range.meterCount && range.meterBase + i < 128; ++i)
            meters.add(frame.meters[static_cast<size_t>(range.meterBase + i)]);

        auto* obj = new juce::DynamicObject();
        obj->setProperty("section", range.section);
        obj->setProperty("containerIndex", range.containerIndex);
        obj->setProperty("name", module->getTitle());
        obj->setProperty("type", module->getDescriptor()->name);
        obj->setProperty("leds", leds);
        obj->setProperty("meters", meters);
        modules.add(juce::var(obj));
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("modules", modules);
    result->setProperty("lastChangeAgeMs", frame.timeMs > 0
        ? juce::var(static_cast<juce::int64>(juce::Time::currentTimeMillis() - frame.timeMs))
        : juce::var());
    // Values recorded while another slot had focus are that slot's, not these.
    result->setProperty("stale", frame.timeMs == 0 || frame.slot != slot);
    result->setProperty("hint", "LEDs are 0-3. Meters come in wire-order pairs (channel B, channel A): "
                                "a stereo meter's left side is the second value, a single meter reads the first.");
    return juce::var(result);
}

// ---------------------------------------------------------------------------
// Knob, morph and MIDI CC assignments (plan item M1). Every change goes through
// the same undo actions as the canvas, the inspector and the header bar, so it
// is drawn, sent to a connected synth and undone with Ctrl+Z like theirs.
// ---------------------------------------------------------------------------

juce::var McpRequestHandler::listAssignments(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };

    juce::Array<juce::var> knobs;
    for (int k = 0; k < KnobAssignmentMessage::numKnobs; ++k)
    {
        const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
        if (!KnobAssignmentMessage::isValidKnob(k) || !ka.assigned)
            continue;
        auto entry = assignmentTargetToVar(*patch, ka.section, ka.module, ka.param);
        entry.getDynamicObject()->setProperty("knob", k);
        entry.getDynamicObject()->setProperty("knobName", knobName(k));
        knobs.add(entry);
    }

    juce::Array<juce::var> groups;
    for (int g = 0; g < McpRules::kNumMorphGroups; ++g)
    {
        juce::Array<juce::var> members;
        for (const auto& ma : patch->morphAssignments)
        {
            if (ma.morph != g)
                continue;
            auto entry = assignmentTargetToVar(*patch, ma.section, ma.module, ma.param);
            entry.getDynamicObject()->setProperty("range", ma.range);
            members.add(entry);
        }
        auto* group = new juce::DynamicObject();
        group->setProperty("morphGroup", g);
        group->setProperty("value", patch->morphValues[static_cast<size_t>(g)]);
        group->setProperty("keyboard", patch->morphKeyboard[static_cast<size_t>(g)]);
        group->setProperty("assignments", members);
        groups.add(juce::var(group));
    }

    juce::Array<juce::var> ccs;
    for (const auto& ca : patch->ctrlAssignments)
    {
        auto entry = assignmentTargetToVar(*patch, ca.section, ca.module, ca.param);
        entry.getDynamicObject()->setProperty("cc", ca.control);
        ccs.add(entry);
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("knobs", knobs);
    result->setProperty("morphGroups", groups);
    result->setProperty("morphAssignmentCount", static_cast<int>(patch->morphAssignments.size()));
    result->setProperty("morphAssignmentLimit", McpRules::kMaxMorphAssignments);
    result->setProperty("midiCcs", ccs);
    return juce::var(result);
}

juce::var McpRequestHandler::assignKnob(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    const auto target = resolveAssignTarget(*patch, params, /*allowMorphGroup=*/true);
    const int knob = resolveKnob(params);
    const bool replace = params.hasProperty("replace") && static_cast<bool>(params["replace"]);

    int previousKnob = -1;
    for (int k = 0; k < KnobAssignmentMessage::numKnobs; ++k)
    {
        const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
        if (ka.assigned && ka.section == target.section && ka.module == target.module
            && ka.param == target.param)
        {
            previousKnob = k;
            break;
        }
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("knob", knob);
    result->setProperty("knobName", knobName(knob));
    result->setProperty("target", assignmentTargetToVar(*patch, target.section, target.module, target.param));
    result->setProperty("previousKnob", previousKnob);
    if (previousKnob == knob)
    {
        result->setProperty("changed", false);
        return juce::var(result);
    }

    // A copy: the actions below overwrite this entry.
    const KnobAssignment occupant = patch->knobAssignments[static_cast<size_t>(knob)];
    if (occupant.assigned && !replace)
        throw McpError{ "knob_in_use", knobName(knob) + " already drives "
            + assignmentTargetToVar(*patch, occupant.section, occupant.module, occupant.param)["label"].toString()
            + "; pass replace=true to take it over" };

    // Freeing the knob first, as its own action in the same transaction, is what
    // lets one Ctrl+Z give it back to whatever it drove before.
    auto& undo = owner_.getSlotUndoManager(slot);
    undo.beginNewTransaction("Assign Knob (MCP)");
    if (occupant.assigned)
    {
        result->setProperty("replaced",
            assignmentTargetToVar(*patch, occupant.section, occupant.module, occupant.param));
        if (!undo.perform(new KnobAssignAction(*ctx, occupant.section, occupant.module,
                                               occupant.param, -1, knob)))
            throw McpError{ "assign_failed", "Failed to free the knob (unexpected)" };
    }
    if (!undo.perform(new KnobAssignAction(*ctx, target.section, target.module, target.param,
                                           knob, previousKnob)))
        throw McpError{ "assign_failed", "Failed to assign the knob (unexpected)" };

    result->setProperty("changed", true);
    return juce::var(result);
}

juce::var McpRequestHandler::unassignKnob(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    const int knob = resolveKnob(params);
    const KnobAssignment occupant = patch->knobAssignments[static_cast<size_t>(knob)];
    if (!occupant.assigned)
        throw McpError{ "knob_not_assigned", knobName(knob) + " is not assigned" };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("knob", knob);
    result->setProperty("knobName", knobName(knob));
    result->setProperty("target", assignmentTargetToVar(*patch, occupant.section, occupant.module, occupant.param));

    owner_.getSlotUndoManager(slot).beginNewTransaction("Unassign Knob (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(new KnobAssignAction(
            *ctx, occupant.section, occupant.module, occupant.param, -1, knob)))
        throw McpError{ "assign_failed", "Failed to unassign the knob (unexpected)" };
    return juce::var(result);
}

juce::var McpRequestHandler::assignMorph(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    const auto target = resolveAssignTarget(*patch, params, /*allowMorphGroup=*/false);
    if (!params.hasProperty("group"))
        throw McpError{ "missing_param", "group is required (morph group 0-3)" };
    const int group = static_cast<int>(params["group"]);
    if (!McpRules::isValidMorphGroup(group))
        throw McpError{ "invalid_param", "group must be 0-3" };
    const int range = params.hasProperty("range") ? static_cast<int>(params["range"]) : 0;
    if (!McpRules::isValidMorphRange(range))
        throw McpError{ "invalid_param", "range must be -127 to 127" };

    int oldGroup = -1;
    int oldRange = 0;
    for (const auto& ma : patch->morphAssignments)
        if (ma.section == target.section && ma.module == target.module && ma.param == target.param)
        {
            oldGroup = ma.morph;
            oldRange = ma.range;
            break;
        }

    if (oldGroup < 0 && static_cast<int>(patch->morphAssignments.size()) >= McpRules::kMaxMorphAssignments)
        throw McpError{ "morph_limit_reached", "The patch already has "
            + juce::String(McpRules::kMaxMorphAssignments)
            + " morph assignments, the most a G1 patch can hold; unassign one first" };

    // The assignment starts at range 0, as it does from the canvas menu; the
    // range is its own action so undoing lands on the same two steps.
    auto& undo = owner_.getSlotUndoManager(slot);
    undo.beginNewTransaction("Assign Morph (MCP)");
    if (!undo.perform(new MorphAssignAction(*ctx, target.section, target.module, target.param,
                                            group, oldGroup, oldRange)))
        throw McpError{ "assign_failed", "Failed to assign the morph group (unexpected)" };
    if (range != 0
        && !undo.perform(new MorphRangeChangeAction(*ctx, target.section, target.module, target.param,
                                                    0, range)))
        throw McpError{ "assign_failed", "Failed to set the morph range (unexpected)" };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("target", assignmentTargetToVar(*patch, target.section, target.module, target.param));
    result->setProperty("group", group);
    result->setProperty("range", range);
    result->setProperty("previousGroup", oldGroup);
    result->setProperty("previousRange", oldRange);
    return juce::var(result);
}

juce::var McpRequestHandler::unassignMorph(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    const auto target = resolveAssignTarget(*patch, params, /*allowMorphGroup=*/false);
    int oldGroup = -1;
    int oldRange = 0;
    for (const auto& ma : patch->morphAssignments)
        if (ma.section == target.section && ma.module == target.module && ma.param == target.param)
        {
            oldGroup = ma.morph;
            oldRange = ma.range;
            break;
        }
    if (oldGroup < 0)
        throw McpError{ "morph_not_assigned", "That parameter is not in a morph group" };

    owner_.getSlotUndoManager(slot).beginNewTransaction("Unassign Morph (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(new MorphAssignAction(
            *ctx, target.section, target.module, target.param, -1, oldGroup, oldRange)))
        throw McpError{ "assign_failed", "Failed to remove the morph assignment (unexpected)" };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("target", assignmentTargetToVar(*patch, target.section, target.module, target.param));
    result->setProperty("previousGroup", oldGroup);
    result->setProperty("previousRange", oldRange);
    return juce::var(result);
}

juce::var McpRequestHandler::assignMidiCc(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    const auto target = resolveAssignTarget(*patch, params, /*allowMorphGroup=*/true);
    if (!params.hasProperty("cc"))
        throw McpError{ "missing_param", "cc is required (0-119)" };
    const int cc = static_cast<int>(params["cc"]);
    if (!McpRules::isValidMidiCc(cc))
        throw McpError{ "invalid_param", "cc must be 0-119; 120-127 are MIDI channel mode messages" };
    const bool replace = params.hasProperty("replace") && static_cast<bool>(params["replace"]);

    int previousCc = -1;
    std::optional<CtrlAssignment> occupant;
    for (const auto& ca : patch->ctrlAssignments)
    {
        const bool sameTarget = ca.section == target.section && ca.module == target.module
                             && ca.param == target.param;
        if (sameTarget)
            previousCc = ca.control;
        else if (ca.control == cc && !occupant)
            occupant = ca;
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("cc", cc);
    result->setProperty("target", assignmentTargetToVar(*patch, target.section, target.module, target.param));
    result->setProperty("previousCc", previousCc);
    if (previousCc == cc)
    {
        result->setProperty("changed", false);
        return juce::var(result);
    }

    if (occupant && !replace)
        throw McpError{ "cc_in_use", "CC " + juce::String(cc) + " already drives "
            + assignmentTargetToVar(*patch, occupant->section, occupant->module, occupant->param)["label"].toString()
            + "; pass replace=true to take it over" };

    auto& undo = owner_.getSlotUndoManager(slot);
    undo.beginNewTransaction("Assign MIDI CC (MCP)");
    if (occupant)
    {
        result->setProperty("replaced",
            assignmentTargetToVar(*patch, occupant->section, occupant->module, occupant->param));
        if (!undo.perform(new MidiCtrlAssignAction(*ctx, occupant->section, occupant->module,
                                                   occupant->param, -1, cc)))
            throw McpError{ "assign_failed", "Failed to free the CC (unexpected)" };
    }
    if (!undo.perform(new MidiCtrlAssignAction(*ctx, target.section, target.module, target.param,
                                               cc, previousCc)))
        throw McpError{ "assign_failed", "Failed to assign the CC (unexpected)" };

    result->setProperty("changed", true);
    return juce::var(result);
}

juce::var McpRequestHandler::unassignMidiCc(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    UndoContext* ctx = owner_.getSlotUndoContext(slot);
    if (!patch || !ctx)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    if (!params.hasProperty("cc"))
        throw McpError{ "missing_param", "cc is required (0-119)" };
    const int cc = static_cast<int>(params["cc"]);

    std::optional<CtrlAssignment> assigned;
    for (const auto& ca : patch->ctrlAssignments)
        if (ca.control == cc)
        {
            assigned = ca;
            break;
        }
    if (!assigned)
        throw McpError{ "cc_not_assigned", "CC " + juce::String(cc) + " is not assigned" };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("cc", cc);
    result->setProperty("target", assignmentTargetToVar(*patch, assigned->section, assigned->module, assigned->param));

    owner_.getSlotUndoManager(slot).beginNewTransaction("Unassign MIDI CC (MCP)");
    if (!owner_.getSlotUndoManager(slot).perform(new MidiCtrlAssignAction(
            *ctx, assigned->section, assigned->module, assigned->param, -1, cc)))
        throw McpError{ "assign_failed", "Failed to unassign the CC (unexpected)" };
    return juce::var(result);
}

// ---------------------------------------------------------------------------
// Playing the synth: a morph dial and a note, for checking an edit by ear or
// by its meters.
// ---------------------------------------------------------------------------

juce::var McpRequestHandler::setMorphValue(const juce::var& params)
{
    const int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };
    ensurePatchEditable(owner_);

    if (!params.hasProperty("morphGroup"))
        throw McpError{ "missing_param", "morphGroup is required (0-3)" };
    const int group = static_cast<int>(params["morphGroup"]);
    if (!McpRules::isValidMorphGroup(group))
        throw McpError{ "invalid_param", "morphGroup must be 0-3" };
    const bool hasValue = params.hasProperty("value");
    const bool hasDelta = params.hasProperty("delta");
    if (hasValue == hasDelta)
        throw McpError{ "invalid_param", "Provide exactly one of value or delta" };

    const int oldValue = patch->morphValues[static_cast<size_t>(group)];
    const int requested = hasValue ? static_cast<int>(params["value"])
                                   : oldValue + static_cast<int>(params["delta"]);
    juce::String error;
    if (!owner_.setSlotMorphValue(slot, group, juce::jlimit(0, 127, requested), error))
        throw McpError{ "morph_value_failed", error };

    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("morphGroup", group);
    result->setProperty("oldValue", oldValue);
    result->setProperty("value", patch->morphValues[static_cast<size_t>(group)]);
    return juce::var(result);
}

juce::var McpRequestHandler::playNote(const juce::var& params)
{
    const auto& connection = owner_.getConnectionManager();
    if (!connection.isConnected())
        throw McpError{ "not_connected", "No synth connected" };
    if (!params.hasProperty("note"))
        throw McpError{ "missing_param", "note is required (0-127, 60 = middle C)" };

    const int note = static_cast<int>(params["note"]);
    const int durationMs = juce::jlimit(10, 10000,
        params.hasProperty("durationMs") ? static_cast<int>(params["durationMs"]) : 500);
    juce::String error;
    if (!owner_.playNoteOnSynth(note, durationMs, error))
        throw McpError{ "invalid_param", error };

    auto* result = new juce::DynamicObject();
    result->setProperty("note", note);
    result->setProperty("durationMs", durationMs);
    result->setProperty("synthFocusedSlot", connection.getCurrentSlot());
    return juce::var(result);
}

// The MIDI ports the editor could connect to, and which pair it is on now. Lets a
// client move the editor between a real synth and G1-Emu without the window.
juce::var McpRequestHandler::listMidiPorts(const juce::var&)
{
    auto portList = [](const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        juce::Array<juce::var> out;
        for (const auto& d : devices)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty("id", d.identifier);
            o->setProperty("name", d.name);
            out.add(juce::var(o));
        }
        return out;
    };
    const auto& connection = owner_.getConnectionManager();
    auto* result = new juce::DynamicObject();
    result->setProperty("inputs", portList(ConnectionManager::getAvailableInputDevices()));
    result->setProperty("outputs", portList(ConnectionManager::getAvailableOutputDevices()));
    result->setProperty("connected", connection.isConnected());
    result->setProperty("message", connection.getStatus().message);
    result->setProperty("currentInputId", owner_.getLastInputId());
    result->setProperty("currentOutputId", owner_.getLastOutputId());
    return juce::var(result);
}

namespace
{
    // A port by its id, its exact name, or a unique part of its name (McpRules::matchPort).
    juce::MidiDeviceInfo resolvePort(const juce::Array<juce::MidiDeviceInfo>& devices,
                                     const juce::String& wanted, const char* what)
    {
        juce::StringArray ids, names;
        for (const auto& d : devices)
        {
            ids.add(d.identifier);
            names.add(d.name);
        }
        const auto matches = McpRules::matchPort(ids, names, wanted);
        if (matches.size() == 1)
            return devices[matches.getFirst()];
        juce::StringArray listed;
        if (matches.isEmpty())
            listed = names;
        else
            for (const int i : matches)
                listed.add(names[i]);
        throw McpError{ matches.isEmpty() ? "not_found" : "ambiguous",
                        juce::String(what) + " \"" + wanted + "\" "
                            + (matches.isEmpty() ? "matches no port" : "matches several ports")
                            + "; available: " + listed.joinIntoString(", ") };
    }
}

// Connect the editor to an input and an output port (by id, name, or a unique part
// of the name), the way the MIDI settings dialog does: the old connection is
// dropped, the handshake runs, and the ports are remembered once the synth answers.
juce::var McpRequestHandler::connectMidi(const juce::var& params)
{
    if (!params.hasProperty("input") || !params.hasProperty("output"))
        throw McpError{ "missing_param", "input and output are required (a port id, name, or unique part of a name)" };
    const auto in = resolvePort(ConnectionManager::getAvailableInputDevices(), params["input"].toString(), "input");
    const auto out = resolvePort(ConnectionManager::getAvailableOutputDevices(), params["output"].toString(), "output");
    owner_.connectToPorts(in.identifier, out.identifier);

    auto* result = new juce::DynamicObject();
    result->setProperty("input", in.name);
    result->setProperty("output", out.name);
    result->setProperty("note", "The handshake runs in the background: poll get_synth_status until connection is \"connected\" and check synthName");
    return juce::var(result);
}

juce::var McpRequestHandler::disconnectMidi(const juce::var&)
{
    owner_.disconnectFromSynth();
    auto* result = new juce::DynamicObject();
    result->setProperty("connected", false);
    return juce::var(result);
}

// Ask the synth for a slot's patch again. The editor's copy of that slot is
// replaced when the patch arrives, so unsaved editor-only changes are lost; the
// synth is not changed.
juce::var McpRequestHandler::fetchPatch(const juce::var& params)
{
    const int slot = params.hasProperty("slot") ? static_cast<int>(params["slot"])
                                                : owner_.getConnectionManager().getCurrentSlot();
    juce::String error;
    if (!owner_.refetchSlotFromSynth(slot, error))
        throw McpError{ owner_.getConnectionManager().isConnected() ? "busy_or_invalid" : "not_connected", error };
    auto* result = new juce::DynamicObject();
    result->setProperty("slot", slot);
    result->setProperty("note", "Fetching: poll get_synth_status until transfer.fetching is false, or get_events for patch_received");
    return juce::var(result);
}

// What the synth holds in each position of one bank, from the patch list the
// editor already fetched on connect. store_to_bank overwrites without asking,
// so this is how a client finds a free position first.
juce::var McpRequestHandler::listBank(const juce::var& params)
{
    const auto& connection = owner_.getConnectionManager();
    if (!connection.isConnected())
        throw McpError{ "not_connected", "No synth connected: the bank list comes from the synth" };
    if (!connection.isPatchListLoaded())
        throw McpError{ "patch_list_loading", "The synth's patch list has not finished loading; retry shortly" };
    if (!params.hasProperty("bank"))
        throw McpError{ "missing_param", "bank is required (1-9)" };
    const int bank = static_cast<int>(params["bank"]);
    if (bank < 1 || bank > 9)
        throw McpError{ "invalid_param", "bank must be 1-9" };
    const bool includeEmpty = !params.hasProperty("includeEmpty") || static_cast<bool>(params["includeEmpty"]);

    const auto& names = connection.getPatchList();
    juce::Array<juce::var> positions;
    int used = 0;
    for (int position = 0; position < 99; ++position)
    {
        const auto index = static_cast<size_t>((bank - 1) * 99 + position);
        const auto name = index < names.size() ? juce::String(names[index]).trim() : juce::String();
        if (name.isNotEmpty())
            ++used;
        else if (!includeEmpty)
            continue;

        auto* entry = new juce::DynamicObject();
        entry->setProperty("position", position + 1);
        entry->setProperty("location", bank * 100 + position + 1);
        entry->setProperty("name", name.isNotEmpty() ? juce::var(name) : juce::var());
        positions.add(juce::var(entry));
    }

    auto* result = new juce::DynamicObject();
    result->setProperty("bank", bank);
    result->setProperty("used", used);
    result->setProperty("free", 99 - used);
    result->setProperty("positions", positions);
    return juce::var(result);
}
