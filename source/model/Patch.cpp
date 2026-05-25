#include "Patch.h"
#include "ModuleDescriptions.h"

// --- Module ---

std::unique_ptr<Module> Module::createFromDescriptor(const ModuleDescriptor& desc)
{
    auto module = std::unique_ptr<Module>(new Module());
    module->descriptor = &desc;
    module->title = desc.fullname;

    for (auto& pd : desc.parameters)
        module->parameters.emplace_back(pd);
    for (auto& cd : desc.connectors)
        module->connectors.emplace_back(cd);
    for (auto& ld : desc.lights)
        module->lights.emplace_back(ld);

    return module;
}

Parameter* Module::getParameter(int index)
{
    for (auto& p : parameters)
        if (p.getDescriptor()->index == index && p.getDescriptor()->paramClass == "parameter")
            return &p;
    return nullptr;
}

Connector* Module::getConnector(int index)
{
    for (auto& c : connectors)
        if (c.getDescriptor()->index == index)
            return &c;
    return nullptr;
}

Connector* Module::getConnector(int index, bool isOutput)
{
    for (auto& c : connectors)
        if (c.getDescriptor()->index == index && c.getDescriptor()->isOutput == isOutput)
            return &c;
    return nullptr;
}

void Module::setPosition(juce::Point<int> p)
{
    auto oldPos = position;
    position = p;
    if (onMoved && (oldPos.x != p.x || oldPos.y != p.y))
        onMoved(this, oldPos.x, oldPos.y);
}

// --- ModuleContainer ---

Module* ModuleContainer::addModule(std::unique_ptr<Module> module)
{
    auto* ptr = module.get();
    modules.push_back(std::move(module));

    if (onModuleAdded)
        onModuleAdded(ptr);

    return ptr;
}

Module* ModuleContainer::addModuleSilent(std::unique_ptr<Module> module)
{
    auto* ptr = module.get();
    modules.push_back(std::move(module));
    return ptr;
}

std::unique_ptr<Module> ModuleContainer::extractModule(Module* module)
{
    // Remove connections involving this module's connectors (silently)
    for (auto& conn : module->getConnectors())
    {
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&conn](const Connection& c) { return c.output == &conn || c.input == &conn; }),
            connections.end());
    }

    // Find and extract the unique_ptr
    std::unique_ptr<Module> extracted;
    for (auto it = modules.begin(); it != modules.end(); ++it)
    {
        if (it->get() == module)
        {
            extracted = std::move(*it);
            modules.erase(it);
            break;
        }
    }
    return extracted;
}

void ModuleContainer::removeModule(Module* module)
{
    // First: fire onCableRemoved for every cable connected to this module.
    // This sends DeleteCableMessage to the synth BEFORE DeleteModuleMessage,
    // matching the Java NmPatchSynchronizer order (removeAllConnections first).
    // Without this, the synth receives DeleteModule while cables are still
    // attached, causing the synth to freeze/hang.
    if (onCableRemoved)
    {
        for (auto& conn : module->getConnectors())
        {
            for (auto it = connections.begin(); it != connections.end(); )
            {
                if (it->output == &conn || it->input == &conn)
                {
                    onCableRemoved(it->output, it->input);
                    it = connections.erase(it);
                }
                else
                    ++it;
            }
        }
    }
    else
    {
        // No cable callback: just remove from model silently
        for (auto& conn : module->getConnectors())
        {
            connections.erase(
                std::remove_if(connections.begin(), connections.end(),
                    [&conn](const Connection& c) { return c.output == &conn || c.input == &conn; }),
                connections.end());
        }
    }

    // Then: fire onModuleRemoved (sends DeleteModuleMessage to synth)
    if (onModuleRemoved)
        onModuleRemoved(module);

    modules.erase(
        std::remove_if(modules.begin(), modules.end(),
            [module](const std::unique_ptr<Module>& m) { return m.get() == module; }),
        modules.end());
}

bool ModuleContainer::canAdd(const ModuleDescriptor& desc) const
{
    if (desc.limit <= 0)
        return desc.instantiable;

    int count = 0;
    for (auto& m : modules)
        if (m->getDescriptor()->index == desc.index)
            ++count;

    return count < desc.limit && desc.instantiable;
}

void ModuleContainer::addConnection(Connector* output, Connector* input)
{
    if (output == nullptr || input == nullptr)
        return;

    for (const auto& c : connections)
        if (c.output == output && c.input == input)
            return;

    connections.push_back({ output, input });
    if (onCableAdded)
        onCableAdded(output, input);
}

void ModuleContainer::removeConnection(Connector* output, Connector* input)
{
    bool removed = false;
    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [output, input, &removed](const Connection& c)
            {
                const bool match = c.output == output && c.input == input;
                removed = removed || match;
                return match;
            }),
        connections.end());
    if (removed && onCableRemoved)
        onCableRemoved(output, input);
}

void ModuleContainer::removeConnectionsForConnector(Connector* conn)
{
    // Fire removal events for each connection being removed
    if (onCableRemoved)
    {
        for (auto& c : connections)
        {
            if (c.output == conn || c.input == conn)
                onCableRemoved(c.output, c.input);
        }
    }

    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [conn](const Connection& c) { return c.output == conn || c.input == conn; }),
        connections.end());
}

Module* ModuleContainer::getModuleByIndex(int containerIndex)
{
    for (auto& m : modules)
        if (m->getContainerIndex() == containerIndex)
            return m.get();
    return nullptr;
}

const Module* ModuleContainer::getModuleByIndex(int containerIndex) const
{
    for (auto& m : modules)
        if (m->getContainerIndex() == containerIndex)
            return m.get();
    return nullptr;
}

// --- Patch ---

Patch::Patch() = default;

Module* Patch::createModule(int section, int typeId, int gridX, int gridY,
                             const juce::String& moduleName, const ModuleDescriptions& descs)
{
    // Get module descriptor
    auto* descriptor = descs.getModuleByIndex(typeId);
    if (descriptor == nullptr)
        return nullptr;

    // Check if module can be added to this section
    auto& container = getContainer(section);
    if (!container.canAdd(*descriptor))
        return nullptr;

    // Generate unique container index (find max existing + 1, starting from 1)
    int maxIndex = 0;
    for (auto& m : container.getModules())
    {
        int idx = m->getContainerIndex();
        if (idx > maxIndex)
            maxIndex = idx;
    }
    int newIndex = maxIndex + 1;

    // Create module from descriptor
    auto module = Module::createFromDescriptor(*descriptor);
    if (module == nullptr)
        return nullptr;

    // Configure module
    module->setContainerIndex(newIndex);
    module->setPosition({ gridX, gridY });
    module->setTitle(moduleName.isNotEmpty() ? moduleName : descriptor->name);

    // Add to container
    Module* modulePtr = container.addModule(std::move(module));

    return modulePtr;
}
