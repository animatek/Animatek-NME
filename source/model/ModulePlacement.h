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
    int containerIndex = 0;   // -1 when this entry is a comment
    int commentId = -1;       // >= 0 when this entry is an editor text note
    juce::Point<int> oldPos;
};

// A patch is 40 columns of 128 rows; nothing can be pushed past the bottom.
inline constexpr int modulePlacementRows = 128;

/** True when `height` rows at column `gx`, row `gy` can be cleared without
    shoving anything past the bottom of the area. makeRoomForModule() clamps
    what will not fit, and a clamped occupant lands back on top of whatever
    pushed it (issue #54) — so a placement is checked here first and refused
    outright when the column has no room, which is what the original editor
    does. Same walk as makeRoomForModule, minus the moves. */
inline bool canMakeRoomForModule (const ModuleContainer& container, int section,
                                  int gx, int gy, int height,
                                  const std::vector<int>& ignoreIndices = {},
                                  const std::vector<PatchComment>* comments = nullptr,
                                  int ignoreCommentId = -1)
{
    if (height < 1)
        height = 1;
    if (gy < 0 || gy + height > modulePlacementRows)
        return false;

    struct Occupant { int y; int height; };
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
        column.push_back ({ m->getPosition().y, desc != nullptr ? desc->height : 1 });
    }

    if (comments != nullptr)
        for (auto& c : *comments)
            if (c.section == section && c.coversColumn (gx) && c.id != ignoreCommentId)
                column.push_back ({ c.y, c.gridHeight() });

    std::sort (column.begin(), column.end(),
               [] (const Occupant& a, const Occupant& b) { return a.y < b.y; });

    int frontier = gy + height;
    for (auto& occ : column)
    {
        if (occ.y + occ.height <= gy)
            continue;
        if (occ.y >= frontier)
            break;
        if (frontier + occ.height > modulePlacementRows)
            return false;
        frontier += occ.height;
    }
    return true;
}

/** Clears `height` rows at column `gx`, row `gy`, pushing the modules already
    there down the column. Modules whose container index is in `ignoreIndices`
    stay put: that is how a block being pasted keeps its own shape while
    shoving the rest of the patch out of the way.

    `comments` (optional) lets the editor's own text notes take part: they hold a
    rectangle of the grid like any module, so they get pushed as well, and
    `ignoreCommentId` keeps the note being placed from pushing itself. */
inline std::vector<PushedModule> makeRoomForModule (ModuleContainer& container, int section,
                                                    int gx, int gy, int height,
                                                    const std::vector<int>& ignoreIndices = {},
                                                    std::vector<PatchComment>* comments = nullptr,
                                                    int ignoreCommentId = -1)
{
    std::vector<PushedModule> pushed;
    if (height < 1)
        height = 1;

    struct Occupant { Module* module; PatchComment* comment; int y; int height; };
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
        column.push_back ({ m, nullptr, m->getPosition().y, desc != nullptr ? desc->height : 1 });
    }

    // A note can be several columns wide, so it counts as an occupant of every
    // column it covers, not just the one it starts in.
    if (comments != nullptr)
        for (auto& c : *comments)
            if (c.section == section && c.coversColumn (gx) && c.id != ignoreCommentId)
                column.push_back ({ nullptr, &c, c.y, c.gridHeight() });

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
        if (occ.module != nullptr)
        {
            pushed.push_back ({ section, occ.module->getContainerIndex(), -1,
                                occ.module->getPosition() });
            occ.module->setPosition ({ gx, newY });
        }
        else
        {
            pushed.push_back ({ section, -1, occ.comment->id,
                                juce::Point<int> { occ.comment->x, occ.comment->y } });
            occ.comment->y = newY;
        }
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
        if (it->commentId >= 0)
        {
            if (auto* c = patch.getCommentById (it->commentId))
            {
                c->x = it->oldPos.x;
                c->y = it->oldPos.y;
            }
            continue;
        }

        auto* m = patch.getContainer (it->section).getModuleByIndex (it->containerIndex);
        if (m != nullptr)
            m->setPosition (it->oldPos);
    }
}
