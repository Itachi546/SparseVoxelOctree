#pragma once

#include <glm/glm.hpp>

struct ogt_vox_scene;

struct VoxelData {

    virtual uint32_t Sample(glm::vec3 p) = 0;

    virtual ~VoxelData() = default;
};

struct VoxModelData : public VoxelData {

    void Load(const char *filename, float scale = 1.0f);

    uint32_t Sample(glm::vec3 p) override;

    void Destroy();

    ogt_vox_scene const *scene;

    glm::vec3 translation;
    float scale;
};