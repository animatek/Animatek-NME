#include "PatchSynchronizer.h"
#include "../protocol/NewCableMessage.h"
#include "../protocol/DeleteCableMessage.h"
#include "../protocol/MoveModuleMessage.h"
#include "../protocol/NewModuleMessage.h"
#include "../protocol/DeleteModuleMessage.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <iomanip>

PatchSynchronizer::PatchSynchronizer(Patch& patch, ConnectionManager& connMgr)
    : patch_(patch), connMgr_(connMgr)
{
    enable();
}

PatchSynchronizer::~PatchSynchronizer()
{
    disable();
}

void PatchSynchronizer::enable()
{
    if (enabled_)
        return;

    // Register cable event callbacks
    patch_.getPolyVoiceArea().setCableAddedCallback(
        [this](Connector* out, Connector* in) { onCableAdded(1, out, in); });
    patch_.getPolyVoiceArea().setCableRemovedCallback(
        [this](Connector* out, Connector* in) { onCableRemoved(1, out, in); });

    patch_.getCommonArea().setCableAddedCallback(
        [this](Connector* out, Connector* in) { onCableAdded(0, out, in); });
    patch_.getCommonArea().setCableRemovedCallback(
        [this](Connector* out, Connector* in) { onCableRemoved(0, out, in); });

    // Register module added callbacks
    patch_.getPolyVoiceArea().setModuleAddedCallback(
        [this](Module* m) { onModuleAdded(1, m); });
    patch_.getCommonArea().setModuleAddedCallback(
        [this](Module* m) { onModuleAdded(0, m); });

    // Register module removed callbacks
    patch_.getPolyVoiceArea().setModuleRemovedCallback(
        [this](Module* m) { onModuleRemoved(1, m); });
    patch_.getCommonArea().setModuleRemovedCallback(
        [this](Module* m) { onModuleRemoved(0, m); });

    // Register module moved callbacks for all modules
    auto registerModuleMoved = [this](ModuleContainer& container, int section) {
        for (auto& modPtr : container.getModules())
        {
            modPtr->setModuleMovedCallback([this, section](Module* m, int oldX, int oldY) {
                onModuleMoved(section, m, oldX, oldY);
            });
        }
    };

    registerModuleMoved(patch_.getPolyVoiceArea(), 1);
    registerModuleMoved(patch_.getCommonArea(), 0);

    enabled_ = true;
    std::cout << "[SYNC] PatchSynchronizer enabled" << std::endl;
}

void PatchSynchronizer::disable()
{
    if (!enabled_)
        return;

    // Clear callbacks
    patch_.getPolyVoiceArea().setCableAddedCallback(nullptr);
    patch_.getPolyVoiceArea().setCableRemovedCallback(nullptr);
    patch_.getCommonArea().setCableAddedCallback(nullptr);
    patch_.getCommonArea().setCableRemovedCallback(nullptr);
    patch_.getPolyVoiceArea().setModuleAddedCallback(nullptr);
    patch_.getCommonArea().setModuleAddedCallback(nullptr);
    patch_.getPolyVoiceArea().setModuleRemovedCallback(nullptr);
    patch_.getCommonArea().setModuleRemovedCallback(nullptr);

    // Clear module callbacks
    for (auto& m : patch_.getPolyVoiceArea().getModules())
        m->setModuleMovedCallback(nullptr);
    for (auto& m : patch_.getCommonArea().getModules())
        m->setModuleMovedCallback(nullptr);

    enabled_ = false;
    std::cout << "[SYNC] PatchSynchronizer disabled" << std::endl;
}

void PatchSynchronizer::onCableAdded(int section, Connector* output, Connector* input)
{
    if (!enabled_ || suppressed_ || !connMgr_.isConnected())
        return;

    auto& container = patch_.getContainer(section);

    // Find modules
    Module* outModule = findModuleForConnector(container, output);
    Module* inModule = findModuleForConnector(container, input);

    if (!outModule || !inModule)
    {
        std::cout << "[SYNC] ERROR: onCableAdded - could not find modules" << std::endl;
        return;
    }

    // Get indices
    int outModIdx = outModule->getContainerIndex();
    int inModIdx = inModule->getContainerIndex();
    int outConnIdx = getConnectorIndex(*outModule, output);
    int inConnIdx = getConnectorIndex(*inModule, input);

    bool outIsOutput = output->getDescriptor()->isOutput;
    bool inIsOutput = input->getDescriptor()->isOutput;
    SignalType color = output->getDescriptor()->signalType;

    // Create and send message
    // Protocol convention (from Java reference): destination (input) first, source (output) second
    int pid = connMgr_.getCurrentPatchId();
    NewCableMessage msg(pid, section, color,
                       inModIdx, inIsOutput, inConnIdx,
                       outModIdx, outIsOutput, outConnIdx);

    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.expectSyncEcho();
    connMgr_.sendAckedSysEx(sysex, true);

    // Debug logging with hex dump
    std::cout << "[SYNC] Sent CableInsert: "
        << "slot=" << connMgr_.getCurrentSlot()
        << " section=" << section
        << " color=" << (int)color
        << " dst=" << inModIdx << "(" << (inIsOutput ? "out" : "in") << ":" << inConnIdx << ")"
        << " src=" << outModIdx << "(" << (outIsOutput ? "out" : "in") << ":" << outConnIdx << ")"
        << std::endl;

    std::cout << "[SYNC]   SysEx: ";
    for (auto byte : sysex)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;
}

void PatchSynchronizer::onCableRemoved(int section, Connector* output, Connector* input)
{
    if (!enabled_ || suppressed_ || !connMgr_.isConnected())
        return;

    auto& container = patch_.getContainer(section);

    // Find modules
    Module* outModule = findModuleForConnector(container, output);
    Module* inModule = findModuleForConnector(container, input);

    if (!outModule || !inModule)
    {
        std::cout << "[SYNC] ERROR: onCableRemoved - could not find modules" << std::endl;
        return;
    }

    // Get indices
    int outModIdx = outModule->getContainerIndex();
    int inModIdx = inModule->getContainerIndex();
    int outConnIdx = getConnectorIndex(*outModule, output);
    int inConnIdx = getConnectorIndex(*inModule, input);

    bool outIsOutput = output->getDescriptor()->isOutput;
    bool inIsOutput = input->getDescriptor()->isOutput;
    SignalType color = output->getDescriptor()->signalType;

    // Create and send message
    // Protocol convention (from Java reference): destination (input) first, source (output) second
    int pid = connMgr_.getCurrentPatchId();
    DeleteCableMessage msg(pid, section, color,
                          inModIdx, inIsOutput, inConnIdx,
                          outModIdx, outIsOutput, outConnIdx);

    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.expectSyncEcho();
    connMgr_.sendAckedSysEx(sysex, true);

    // Debug logging with hex dump
    std::cout << "[SYNC] Sent CableDelete: "
        << "slot=" << connMgr_.getCurrentSlot()
        << " section=" << section
        << " dst=" << inModIdx << "(" << (inIsOutput ? "out" : "in") << ":" << inConnIdx << ")"
        << " src=" << outModIdx << "(" << (outIsOutput ? "out" : "in") << ":" << outConnIdx << ")"
        << std::endl;

    std::cout << "[SYNC]   SysEx: ";
    for (auto byte : sysex)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;
}

void PatchSynchronizer::onModuleMoved(int section, Module* module, int oldX, int oldY)
{
    if (!enabled_ || suppressed_ || !connMgr_.isConnected())
        return;

    int moduleIdx = module->getContainerIndex();
    auto pos = module->getPosition();

    int pid = connMgr_.getCurrentPatchId();
    MoveModuleMessage msg(pid, section, moduleIdx, pos.x, pos.y);
    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.sendAckedSysEx(sysex);

    // Debug logging with hex dump
    std::cout << "[SYNC] Sent ModuleMove: "
        << "slot=" << connMgr_.getCurrentSlot()
        << " section=" << section
        << " module=" << moduleIdx
        << " pos=(" << pos.x << "," << pos.y << ")"
        << std::endl;

    std::cout << "[SYNC]   SysEx: ";
    for (auto byte : sysex)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;
}

int PatchSynchronizer::getModuleIndex(const ModuleContainer& container, const Module* m) const
{
    return m->getContainerIndex();
}

int PatchSynchronizer::getConnectorIndex(const Module& m, const Connector* c) const
{
    return c->getDescriptor()->index;
}

Module* PatchSynchronizer::findModuleForConnector(const ModuleContainer& container, Connector* conn) const
{
    for (auto& modPtr : container.getModules())
    {
        for (auto& c : modPtr->getConnectors())
        {
            if (&c == conn)
                return modPtr.get();
        }
    }
    return nullptr;
}

void PatchSynchronizer::onModuleAdded(int section, Module* module)
{
    if (!enabled_ || suppressed_ || !connMgr_.isConnected())
        return;

    auto* descriptor = module->getDescriptor();
    if (!descriptor)
    {
        std::cout << "[SYNC] ERROR: onModuleAdded - module has no descriptor" << std::endl;
        return;
    }

    int pid = connMgr_.getCurrentPatchId();
    int typeId = descriptor->index;
    int moduleIndex = module->getContainerIndex();
    auto pos = module->getPosition();
    juce::String name = module->getTitle();

    // Build parameter values array (all parameters at their current values)
    std::vector<int> paramValues;
    for (auto& param : module->getParameters())
    {
        // Only include "parameter" class (not morph or custom)
        if (param.getDescriptor()->paramClass == "parameter")
            paramValues.push_back(param.getValue());
    }

    // Custom values (empty for most modules)
    std::vector<int> customValues;

    // Build and send NewModuleMessage
    NewModuleMessageProto msg(pid, typeId, section, moduleIndex,
                              pos.x, pos.y, name.toStdString(),
                              paramValues, customValues);

    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.expectSyncEcho();
    connMgr_.sendAckedSysEx(sysex, true);

    // Register moved callback so future moves are synced to the synth
    module->setModuleMovedCallback([this, section](Module* m, int oldX, int oldY) {
        onModuleMoved(section, m, oldX, oldY);
    });

    std::cout << "[SYNC] NewModule sent:"
        << " slot=" << connMgr_.getCurrentSlot()
        << " section=" << section
        << " type=" << typeId << " (" << descriptor->name << ")"
        << " index=" << moduleIndex
        << " pos=(" << pos.x << "," << pos.y << ")"
        << " params=" << paramValues.size()
        << std::endl;

    std::cout << "[SYNC]   SysEx: ";
    for (auto byte : sysex)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;
}

void PatchSynchronizer::onModuleRemoved(int section, Module* module)
{
    if (!enabled_ || suppressed_ || !connMgr_.isConnected())
        return;

    auto* descriptor = module->getDescriptor();
    if (!descriptor)
    {
        std::cout << "[SYNC] ERROR: onModuleRemoved - module has no descriptor" << std::endl;
        return;
    }

    int pid = connMgr_.getCurrentPatchId();
    int moduleIndex = module->getContainerIndex();

    // Build and send DeleteModuleMessage
    DeleteModuleMessage msg(pid, section, moduleIndex);
    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.expectSyncEcho();
    connMgr_.sendAckedSysEx(sysex, true);

    std::cout << "[SYNC] DeleteModule sent:"
        << " slot=" << connMgr_.getCurrentSlot()
        << " section=" << section
        << " index=" << moduleIndex
        << " (was " << descriptor->name << ")"
        << std::endl;

    std::cout << "[SYNC]   SysEx: ";
    for (auto byte : sysex)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;
}
