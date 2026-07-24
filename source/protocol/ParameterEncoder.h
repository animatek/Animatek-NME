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
     * Encode parameters for a module type into the given values array,
     * using the correct bit widths from the PDL2 spec.
     * Returns the parameter bit widths so the caller knows how to encode them.
     */
    static std::vector<int> getEncodingInfo(int moduleType);
};
