#include "SnipFileIO.h"
#include <map>

bool isSnippetExcludedModuleType(int typeIndex)
{
    // Singleton hardware/global modules do not behave like reusable macro
    // modules. Saving or importing them inside snippets can leave the synth in
    // an invalid edit state, especially when the target patch already has one.
    return typeIndex == 63   // KeyboardPatch
        || typeIndex == 65;  // MIDIGlobal
}

SnipData patchToSnipData(const Patch& patch)
{
    SnipData snip;
    int clipIdx = 0;

    // Track containerIndex → clipIdx per section for cable mapping
    std::map<int, int> containerToClip[2];

    // Poly first (section=1), then common (section=0) — matches PchFileIO write order
    for (int sec : { 1, 0 })
    {
        const auto& container = patch.getContainer(sec);
        for (auto& m : container.getModules())
        {
            if (!m->getDescriptor() || isSnippetExcludedModuleType(m->getDescriptor()->index))
                continue;

            SnipEntry entry;
            entry.typeIndex = m->getDescriptor()->index;
            entry.name      = m->getTitle();
            entry.section   = sec;
            entry.gridPos   = m->getPosition();
            for (auto& p : m->getParameters())
                entry.paramValues.push_back(p.getValue());

            containerToClip[sec][m->getContainerIndex()] = clipIdx++;
            snip.entries.push_back(std::move(entry));
        }
    }

    // Cables
    for (int sec : { 1, 0 })
    {
        const auto& container = patch.getContainer(sec);

        std::map<const Connector*, int> connToContainerIdx;
        for (auto& m : container.getModules())
            for (auto& c : m->getConnectors())
                connToContainerIdx[&c] = m->getContainerIndex();

        for (auto& conn : container.getConnections())
        {
            auto itSrc = connToContainerIdx.find(conn.output);
            auto itDst = connToContainerIdx.find(conn.input);
            if (itSrc == connToContainerIdx.end() || itDst == connToContainerIdx.end()) continue;

            auto itSrcClip = containerToClip[sec].find(itSrc->second);
            auto itDstClip = containerToClip[sec].find(itDst->second);
            if (itSrcClip == containerToClip[sec].end() || itDstClip == containerToClip[sec].end()) continue;

            SnipCable cb;
            cb.srcIdx  = itSrcClip->second;
            cb.srcConn = conn.output->getDescriptor()->index;
            cb.srcIsOutput = conn.output->getDescriptor()->isOutput;
            cb.dstIdx  = itDstClip->second;
            cb.dstConn = conn.input->getDescriptor()->index;
            cb.dstIsOutput = conn.input->getDescriptor()->isOutput;
            snip.cables.push_back(cb);
        }
    }

    return snip;
}

std::unique_ptr<Patch> snipDataToPatch(const SnipData& snip, const ModuleDescriptions& descs)
{
    auto patch = std::make_unique<Patch>();
    patch->setName(snip.name);

    std::vector<Module*> created;
    for (auto& entry : snip.entries)
    {
        if (isSnippetExcludedModuleType(entry.typeIndex))
        {
            created.push_back(nullptr);
            continue;
        }

        auto* mod = patch->createModule(entry.section, entry.typeIndex,
                                        entry.gridPos.x, entry.gridPos.y,
                                        entry.name, descs);
        created.push_back(mod);
        if (!mod) continue;

        auto& params = mod->getParameters();
        for (size_t i = 0; i < entry.paramValues.size() && i < params.size(); ++i)
            params[i].setValue(entry.paramValues[i]);
    }

    for (auto& cb : snip.cables)
    {
        if (cb.srcIdx < 0 || cb.srcIdx >= (int)created.size()) continue;
        if (cb.dstIdx < 0 || cb.dstIdx >= (int)created.size()) continue;
        auto* src = created[static_cast<size_t>(cb.srcIdx)];
        auto* dst = created[static_cast<size_t>(cb.dstIdx)];
        if (!src || !dst) continue;

        int srcSec = snip.entries[static_cast<size_t>(cb.srcIdx)].section;
        int dstSec = snip.entries[static_cast<size_t>(cb.dstIdx)].section;
        if (srcSec != dstSec) continue;

        auto& container = patch->getContainer(srcSec);
        // Use isOutput-aware lookup: descriptor indices can collide between input
        // and output connectors of the same module (e.g. OscMaster has input "sync"
        // and output "out" both at index 0). By convention src=output, dst=input.
        auto* sc = src->getConnector(cb.srcConn, cb.srcIsOutput);
        auto* dc = dst->getConnector(cb.dstConn, cb.dstIsOutput);
        if (sc && dc) container.addConnection(sc, dc);
    }

    return patch;
}
