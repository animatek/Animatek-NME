#pragma once

#include <vector>
#include <cstdint>

/**
 * ParameterEncoder - Encodes module parameters according to PDL2 spec
 *
 * Each module type has a specific parameter structure with varying bit widths.
 * This matches the Param<N> definitions in patch.pdl2.
 */
class ParameterEncoder
{
public:
    /**
     * Get the bit widths for a module type's parameters.
     * Returns empty vector if module type is unknown.
     */
    static std::vector<int> getParameterBitWidths(int moduleType);

    /**
     * Widths for a type the switch does not know, from data: the G1-Mods layer's modules, each
     * parameter as wide as its maxValue needs (as PatchSerializer packs a whole patch). Set once
     * at startup, before anything is sent; a type the switch knows keeps the switch's.
     */
    static void setWidthsFromData(int moduleType, std::vector<int> widths);

    /**
     * Encode parameters for a module type into the given values array,
     * using the correct bit widths from the PDL2 spec.
     * Returns the parameter bit widths so the caller knows how to encode them.
     */
    static std::vector<int> getEncodingInfo(int moduleType);
};
