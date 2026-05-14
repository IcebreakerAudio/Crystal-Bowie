#pragma once

namespace SymmetryHelper
{
    static constexpr float symmetryScale = 0.009f;

    inline std::pair<float, float> toThresholds(float newSymmetry)
    {
        const auto symmetry = newSymmetry * symmetryScale;
        auto negThresh = -1.0f, posThresh = 1.0f;
        if (symmetry < 0.0f) {
            posThresh = 1.0f + symmetry;
        }
        else {
            negThresh = symmetry - 1.0f;
        }

        return { negThresh, posThresh };
    }
}