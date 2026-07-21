#include "McpRequestHandler.h"
#include "../MainComponent.h"
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/SignalType.h"
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

juce::var parameterToVar(const Parameter& p)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", p.getDescriptor()->name);
    obj->setProperty("parameterId", p.getDescriptor()->index);
    obj->setProperty("parameterClass", p.getDescriptor()->paramClass);
    obj->setProperty("componentId", p.getDescriptor()->componentId);
    obj->setProperty("value", p.getValue());
    obj->setProperty("min", p.getDescriptor()->minValue);
    obj->setProperty("max", p.getDescriptor()->maxValue);
    return juce::var(obj);
}

juce::var moduleTypeToVar(const ModuleDescriptor& d)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("typeId", d.index);
    obj->setProperty("name", d.name);
    obj->setProperty("fullname", d.fullname);
    obj->setProperty("category", d.category);
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
        else if (method == "list_modules")     result = listModules(params);
        else if (method == "list_patches")     result = listPatches(params);
        else if (method == "add_module")       result = addModule(params);
        else if (method == "move_module")      result = moveModule(params);
        else if (method == "delete_module")    result = deleteModule(params);
        else if (method == "connect_cable")    result = connectCable(params);
        else if (method == "delete_cable")     result = deleteCable(params);
        else if (method == "set_parameter")    result = setParameter(params);
        else if (method == "create_patch")     result = createPatch(params);
        else if (method == "open_patch")       result = openPatch(params);
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

juce::var McpRequestHandler::listModuleTypes(const juce::var&)
{
    juce::Array<juce::var> modules;
    for (auto& d : owner_.getModuleDescriptions().getAllModules())
        modules.add(moduleTypeToVar(d));

    auto* obj = new juce::DynamicObject();
    obj->setProperty("modules", modules);
    return juce::var(obj);
}

juce::var McpRequestHandler::listModules(const juce::var& params)
{
    int slot = resolveSlot(params);
    Patch* patch = owner_.getSlotPatch(slot);
    if (!patch)
        throw McpError{ "no_patch", "No patch loaded in slot " + juce::String(slot) };

    const bool hasSectionFilter = params.hasProperty("section");
    const int sectionFilter = hasSectionFilter ? resolveSection(params) : -1;

    juce::Array<juce::var> modulesOut;
    juce::Array<juce::var> cablesOut;

    for (int section = 0; section <= 1; ++section)
    {
        if (hasSectionFilter && section != sectionFilter)
            continue;

        auto& container = patch->getContainer(section);

        for (auto& modPtr : container.getModules())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("section", section);
            obj->setProperty("containerIndex", modPtr->getContainerIndex());
            obj->setProperty("typeId", modPtr->getDescriptor()->index);
            obj->setProperty("name", modPtr->getTitle());
            obj->setProperty("gridX", modPtr->getPosition().x);
            obj->setProperty("gridY", modPtr->getPosition().y);
            obj->setProperty("height", modPtr->getDescriptor()->height);

            juce::Array<juce::var> paramsOut;
            for (auto& param : modPtr->getParameters())
                paramsOut.add(parameterToVar(param));
            obj->setProperty("parameters", paramsOut);

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
