#include "McpRequestHandler.h"
#include "../MainComponent.h"
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/SignalType.h"
#include "../model/Mutator.h"
#include "../undo/PatchActions.h"
#include <algorithm>
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

} // namespace

int McpRequestHandler::resolveSlot(const juce::var& params) const
{
    int slot = params.hasProperty("slot") ? static_cast<int>(params["slot"]) : owner_.getActiveSlot();
    if (slot < 0 || slot >= kNumSlots)
        throw McpError{ "invalid_slot", "slot must be 0-3 (A-D)" };
    return slot;
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
    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        auto* patch = owner_.getSlotPatch(slot);
        if (!patch)
            continue;
        auto* obj = new juce::DynamicObject();
        obj->setProperty("slot", slot);
        obj->setProperty("slotName", juce::String::charToString(static_cast<char>('A' + slot)));
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
    if (newName.length() > 16)
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

    Parameter* parameter = nullptr;
    if (hasId)
        parameter = module->getParameter(static_cast<int>(params["parameterId"]));
    if (hasName)
    {
        const auto wantedName = params["parameterName"].toString().trim();
        Parameter* namedParameter = nullptr;
        for (auto& candidate : module->getParameters())
        {
            auto* descriptor = candidate.getDescriptor();
            if (descriptor->paramClass == "parameter"
                && descriptor->name.equalsIgnoreCase(wantedName))
            {
                namedParameter = &candidate;
                break;
            }
        }
        if (!namedParameter)
            throw McpError{ "unknown_parameter", "No editable parameter named '" + wantedName
                + "' on module " + module->getTitle() };
        if (parameter && parameter != namedParameter)
            throw McpError{ "parameter_mismatch", "parameterName and parameterId identify different parameters" };
        parameter = namedParameter;
    }
    if (!parameter)
        throw McpError{ "unknown_parameter", "No editable synth parameter matches the supplied identifier" };

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
