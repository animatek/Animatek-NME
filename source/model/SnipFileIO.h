#pragma once

#include <juce_core/juce_core.h>
#include "Patch.h"
#include "ModuleDescriptions.h"

// Internal clipboard representation for snippet data (section-and-module-type aware).
// Used as the bridge between PchFileIO (file I/O) and InsertSnippetAction (undo model).
struct SnipEntry
{
    int typeIndex = 0;
    juce::String name;
    int section = 1;  // 1=poly, 0=common
    juce::Point<int> gridPos;
    std::vector<int> paramValues;
};

struct SnipCable
{
    int srcIdx = 0, srcConn = 0;  // clip-local entry index + connector descriptor index
    bool srcIsOutput = true;
    int dstIdx = 0, dstConn = 0;
    bool dstIsOutput = false;
};

struct SnipData
{
    juce::String name;
    std::vector<SnipEntry> entries;
    std::vector<SnipCable> cables;
};

bool isSnippetExcludedModuleType(int typeIndex);

// Convert a Patch loaded from .pch into SnipData for insertion into another patch.
// Module container indices are remapped to 0-based clip indices to avoid collisions.
SnipData patchToSnipData(const Patch& patch);

// Convert SnipData into a temporary Patch suitable for writing with PchFileIO.
// Modules get fresh 1-based container indices; parameter values are applied.
std::unique_ptr<Patch> snipDataToPatch(const SnipData& snip, const ModuleDescriptions& descs);
