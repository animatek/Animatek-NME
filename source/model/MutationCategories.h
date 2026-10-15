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

/** Determine which category (if any) a parameter belongs to. Returns -1 if none. */
int getCategoryForParam(const ModuleDescriptor& module, const ParameterDescriptor& param);

/**
 * How the Patch Mutator moves a parameter, after the G2's own mutator: each
 * parameter has a shape, and the random change is drawn evenly in that shape's
 * space, so it is finer where the ear is finer.
 */
enum class MutCurve
{
    Fixed,      // never mutated: on/off, modes, KBT, ranges, pads... the patch's structure
    Linear,     // even across the range
    FineLow,    // finer near the bottom: attack, release, hold, times, feedback, FM amount
    FineHigh,   // finer near the top: the levels of an oscillator's or drum's own mixer
    Centered,   // finer around the middle: coarse/fine tune, detune, pan, EQ gain, decay
};

/** The mutation shape of a G1 parameter (rules in MutationCategories.cpp). */
MutCurve getMutationCurve(const ModuleDescriptor& module, const ParameterDescriptor& param);
