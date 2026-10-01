// nme_pch_dump: what NME's .pch reader understands, one JSON line per patch.
//
//   nme_pch_dump <modules.xml> < list-of-pch-paths > dump.jsonl
//
// Reads every path on stdin with PchFileIO (the reader the editor uses) and
// prints the modules (index, type, position, "parameter"-class values,
// "custom"-class values, title) and the cables (both ends as module,
// connector, isOutput) of each area. A reference reader elsewhere (g1-taller)
// dumps the same shape from the raw file, and the differences are the bugs.
// No window, no synth: only the model and the file reader.

#include "model/ModuleDescriptions.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"

#include <juce_core/juce_core.h>

#include <iostream>
#include <map>
#include <string>

namespace
{
juce::var areaToVar(const ModuleContainer& area)
{
    std::map<const Connector*, juce::Array<juce::var>> ends;
    juce::Array<juce::var> modules;

    for (const auto& m : area.getModules())
    {
        const auto* d = m->getDescriptor();
        juce::Array<juce::var> params, custom;
        for (const auto& p : m->getParameters())
        {
            const auto* pd = p.getDescriptor();
            if (pd->paramClass == "parameter") params.add(p.getValue());
            else if (pd->paramClass == "custom") custom.add(p.getValue());
        }
        for (const auto& c : m->getConnectors())
        {
            const auto* cd = c.getDescriptor();
            ends[&c] = { m->getContainerIndex(), cd->index, cd->isOutput ? 1 : 0 };
        }
        modules.add(juce::Array<juce::var> { m->getContainerIndex(), d != nullptr ? d->index : -1,
                                             m->getPosition().x, m->getPosition().y,
                                             params, custom, m->getTitle() });
    }

    juce::Array<juce::var> cables;
    for (const auto& c : area.getConnections())
    {
        auto a = ends.count(c.output) ? juce::var(ends[c.output]) : juce::var();
        auto b = ends.count(c.input) ? juce::var(ends[c.input]) : juce::var();
        cables.add(juce::Array<juce::var> { a, b });
    }

    auto* o = new juce::DynamicObject();
    o->setProperty("m", modules);
    o->setProperty("c", cables);
    return juce::var(o);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: nme_pch_dump <modules.xml> < paths\n";
        return 2;
    }

    ModuleDescriptions descs;
    if (!descs.loadFromFile(juce::File(juce::String(juce::CharPointer_UTF8(argv[1])))))
    {
        std::cerr << "could not load " << argv[1] << "\n";
        return 2;
    }
    PchFileIO io(descs);

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty()) continue;
        juce::File file(juce::String(juce::CharPointer_UTF8(line.c_str())));
        auto* o = new juce::DynamicObject();
        o->setProperty("ruta", juce::String(juce::CharPointer_UTF8(line.c_str())));

        auto patch = io.readFile(file);
        o->setProperty("leido", patch != nullptr);
        if (patch != nullptr)
        {
            o->setProperty("poly", areaToVar(patch->getContainer(1)));
            o->setProperty("common", areaToVar(patch->getContainer(0)));
        }
        std::cout << juce::JSON::toString(juce::var(o), true).toStdString() << "\n";
    }
    return 0;
}
