#pragma once

#include "voxel-data.h"

#include <glm/glm.hpp>
#include "FastNoiseLite/FastNoiseLite.h"

struct PerlinVoxData : public VoxelData {
    PerlinVoxData() {
        noise = new FastNoiseLite();
        noise->SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2);
    }

    uint32_t Sample(glm::vec3 p) override {
        float frequency = 0.5f;
        float amplitude = 0.5f;
        float res = 0.0f;
        for (uint32_t i = 0; i < 5; ++i) {
            res += noise->GetNoise(p.x * frequency, p.z * frequency);
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }

        float detailOffset = 0.1f;
        float offset = noise->GetNoise(p.x * 200.0f, p.z * 200.0f) * detailOffset;
        float height = 2.0f;
        res *= height;

        uint32_t colors[] = {
            0x0000ff00,
            0x0033ff33,
            0x0000ff44,
        };
        uint32_t numColors = static_cast<uint32_t>(std::size(colors));
        return std::abs(p.y + res + offset) > (height + detailOffset)
                   ? 0u
                   : colors[rand() % numColors];
    };

    ~PerlinVoxData() override {
        delete noise;
    }

    FastNoiseLite *noise;
};