#pragma once

#include "Descriptors.h"

/**
 * Quick Lock categories for the Patch Mutator (modeled on the G2's seven
 * buttons). Matching is heuristic over the G1 modules.xml category and
 * parameter names — see MutationCategories.cpp for the rules.
 */
enum class MutCategory
{
    OscFreq = 0,   // main pitch of oscillators (avoid transpositions)
    OscFine,       // oscillator fine tune
    Envelope,      // all envelope times and levels
    SeqValue,      // step/ctrl/note faders of the sequencers
    SeqEvent,      // gate/trigger steps of the sequencers
    Delays,        // Delay module parameters
    Effects,       // Audio group (chorus, phaser, overdrive, compressor...)
};

constexpr int kNumMutCategories = 7;

const char* mutCategoryName(MutCategory cat);

bool mutCategoryMatches(MutCategory cat,
                        const ModuleDescriptor& module,
                        const ParameterDescriptor& param);
