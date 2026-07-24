#pragma once

#include <juce_core/juce_core.h>

class MainComponent;

// Handles one parsed MCP-bridge JSON request, mutating the target slot's
// Patch through the same undoable actions the UI uses (AddModuleAction/
// AddCableAction) so hardware sync, undo/redo, and canvas repaint all happen
// exactly as they would for a UI-driven edit. Must only be called on the
// JUCE message thread - McpBridgeServer enforces this via
// juce::MessageManager::callFunctionOnMessageThread.
class McpRequestHandler
{
public:
    explicit McpRequestHandler(MainComponent& owner) : owner_(owner) {}

    // Returns the full response object: {"id":..., "ok":true, "result":...}
    // or {"id":..., "ok":false, "error":{"code":"...", "message":"..."}}.
    juce::var handle(const juce::var& request);

private:
    juce::var listModuleTypes(const juce::var& params);
    juce::var describeModuleType(const juce::var& params);
    juce::var mutatePatch(const juce::var& params);
    juce::var listModules(const juce::var& params);
    juce::var listPatches(const juce::var& params);
    juce::var addModule(const juce::var& params);
    juce::var moveModule(const juce::var& params);
    juce::var renameModule(const juce::var& params);
    juce::var deleteModule(const juce::var& params);
    juce::var connectCable(const juce::var& params);
    juce::var deleteCable(const juce::var& params);
    juce::var setParameter(const juce::var& params);
    juce::var createPatch(const juce::var& params);
    juce::var openPatch(const juce::var& params);
    juce::var savePatch(const juce::var& params);
    juce::var storeToBank(const juce::var& params);

    int resolveSlot(const juce::var& params) const;

    MainComponent& owner_;
};
