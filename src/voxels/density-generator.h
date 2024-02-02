#pragma once

#include <glm/glm.hpp>
#include "FastNoiseLite/FastNoiseLite.h"

struct DensityGenerator {
    DensityGenerator() {
        noise = new FastNoiseLite();
        noise->SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2);
    }

    const float kFrequency = 100.0f;
    float Sample(glm::vec3 &p) {
        float offset = (noise->GetNoise(p.x * kFrequency, p.z * kFrequency)) * 0.5f;
        return glm::min(p.y + 12.0f + offset, glm::length(p) - 13.0f);
    };

    FastNoiseLite *noise;
};