#include "voxel-data.h"

#include <ogt_vox/ogt_vox.h>
#include "math-utils.h"

#include <iostream>

void VoxModelData::Load(const char *filename, float scale) {
    this->scale = scale;

    FILE *fp;
    if (fopen_s(&fp, filename, "rb") != 0) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return;
    }

    fseek(fp, 0L, SEEK_END);
    uint32_t bufferSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    uint8_t *buffer = new uint8_t[bufferSize];
    fread(buffer, bufferSize, 1, fp);
    fclose(fp);
    scene = ogt_vox_read_scene(buffer, bufferSize);
    delete[] buffer;

    std::cout << "# models: " << scene->num_models << std::endl;
    std::cout << "# instance: " << scene->num_instances << std::endl;

    glm::vec3 max = glm::vec3{-FLT_MAX};
    glm::vec3 min = glm::vec3{FLT_MAX};
    for (uint32_t i = 0; i < scene->num_models; ++i) {
        const ogt_vox_model *model = scene->models[i];
        const ogt_vox_instance *instance = (scene->instances + i);

        glm::vec3 currentMin = glm::vec3(instance->transform.m30, instance->transform.m31, instance->transform.m32);
        glm::vec3 currentMax = currentMin + glm::vec3{model->size_x, model->size_y, model->size_z};
        min = glm::min(min, currentMin);
        max = glm::max(max, currentMax);
    }
    translation = (min + max) * 0.5f;
}

uint32_t packColor(ogt_vox_rgba color) {
    return color.r << 24 | color.g << 16 | color.b << 8 | color.a;
}

uint32_t VoxModelData::Sample(glm::vec3 p) {
    p = {p.x, -p.z, p.y};
    for (uint32_t i = 0; i < scene->num_models; ++i) {
        const ogt_vox_model *model = scene->models[i];
        const ogt_vox_instance *instance = (scene->instances + i);

        glm::vec3 min = glm::vec3(instance->transform.m30, instance->transform.m31, instance->transform.m32) - translation;
        glm::vec3 size = glm::vec3{model->size_x, model->size_y, model->size_z};

        AABB aabb = {
            min * scale,
            (min + size - 0.1f) * scale};

        int maxIndex = model->size_x * model->size_y * model->size_z;
        if (aabb.ContainPoint(p)) {
            glm::vec3 p01 = (p - aabb.min) / (aabb.CalculateSize());
            glm::ivec3 ip = static_cast<glm::ivec3>(glm::floor(p01 * size));
            int index = ip.x + (ip.y * model->size_x) + (ip.z * model->size_x * model->size_y);
            assert(index >= 0 && index <= maxIndex);
            uint8_t color = model->voxel_data[index];
            if (color == 0)
                return 0;
            else
                return packColor(scene->palette.color[color]);
        }
    }
    return 0;
}

void VoxModelData::Destroy() {
    ogt_vox_destroy_scene(scene);
}