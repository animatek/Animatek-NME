#pragma once

#include "Patch.h"
#include "ModuleDescriptions.h"
#include <algorithm>
#include <vector>

// Dropping a module where another one already sits does not hide it: the ones
// in the way move down their column, and whatever they then run into moves down
// too, which is what the original editor does (issue #36).
//
// The moves are handed back so the undo that removes the module can put the
// column back the way it was.

struct PushedModule
{
    int section = 0;
    int containerIndex = 0;
    juce::Point<int> oldPos;
};

// A patch is 40 columns of 128 rows; nothing can be pushed past the bottom.
inline constexpr int modulePlacementRows = 128;

/** Clears `height` rows at column `gx`, row `gy`, pushing the modules already
    there down the column. Modules whose container index is in `ignoreIndices`
    stay put: that is how a block being pasted keeps its own shape while
    shoving the rest of the patch out of the way. */
inline std::vector<PushedModule> makeRoomForModule (ModuleContainer& container, int section,
                                                    int gx, int gy, int height,
                                                    const std::vector<int>& ignoreIndices = {})
{
    std::vector<PushedModule> pushed;
    if (height < 1)
        height = 1;

    struct Occupant { Module* module; int y; int height; };
    std::vector<Occupant> column;

    for (auto& modulePtr : container.getModules())
    {
        auto* m = modulePtr.get();
        if (m == nullptr || m->getPosition().x != gx)
            continue;
        if (std::find (ignoreIndices.begin(), ignoreIndices.end(),
                       m->getContainerIndex()) != ignoreIndices.end())
            continue;

        auto* desc = m->getDescriptor();
        column.push_back ({ m, m->getPosition().y, desc != nullptr ? desc->height : 1 });
    }

    std::sort (column.begin(), column.end(),
               [] (const Occupant& a, const Occupant& b) { return a.y < b.y; });

    // Everything from the top of the block down gets shifted below whatever was
    // pushed before it, so a run of stacked modules stays a run.
    int frontier = gy + height;

    for (auto& occ : column)
    {
        if (occ.y + occ.height <= gy)   // sits entirely above the block
            continue;
        if (occ.y >= frontier)          // clear of it, and so is everything after
            break;

        const int newY = juce::jlimit (0, modulePlacementRows - occ.height, frontier);
        pushed.push_back ({ section, occ.module->getContainerIndex(), occ.module->getPosition() });
        occ.module->setPosition ({ gx, newY });
        frontier = newY + occ.height;
    }

    return pushed;
}

/** Puts pushed modules back where they were. Later pushes are undone first, so
    a module shoved twice ends up at the position it started from. */
inline void restorePushedModules (Patch& patch, const std::vector<PushedModule>& pushed)
{
    for (auto it = pushed.rbegin(); it != pushed.rend(); ++it)
    {
        auto* m = patch.getContainer (it->section).getModuleByIndex (it->containerIndex);
        if (m != nullptr)
            m->setPosition (it->oldPos);
    }
}
