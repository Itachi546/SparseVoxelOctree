#pragma once

#include <glm/glm.hpp>

struct ogt_vox_scene;

struct VoxelData {

    virtual uint32_t Sample(glm::vec3 p) = 0;

    virtual ~VoxelData() = default;
};

struct VoxProceduralData : public VoxelData {
    uint32_t Sample(glm::vec3 p) {
        return (length(p) - 25.0f) > 0.0f ? 0 : 1;
    }
};

struct VoxModelData : public VoxelData {

    void Load(const char *filename, float scale = 1.0f);

    uint32_t Sample(glm::vec3 p) override;

    void Destroy();

    ogt_vox_scene const *scene;

    glm::vec3 translation;
    float scale;
};