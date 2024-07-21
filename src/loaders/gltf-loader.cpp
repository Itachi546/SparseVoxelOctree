#include "pch.h"
#include "gltf-loader.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf/tinygltf.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_access.hpp>

static uint8_t *getBufferPtr(tinygltf::Model *model, const tinygltf::Accessor &accessor) {
    tinygltf::BufferView &bufferView = model->bufferViews[accessor.bufferView];
    return model->buffers[bufferView.buffer].data.data() + accessor.byteOffset + bufferView.byteOffset;
}

static void parseMaterial(tinygltf::Model *model, MaterialInfo *component, uint32_t matIndex) {
    if (matIndex == -1)
        return;

    tinygltf::Material &material = model->materials[matIndex];
    // component->alphaCutoff = (float)material.alphaCutoff;
    /*
    if (material.alphaMode == "BLEND")
        component->alphaMode = ALPHAMODE_BLEND;
    else if (material.alphaMode == "MASK")
        component->alphaMode = ALPHAMODE_MASK;
        */

    // Parse Material value
    tinygltf::PbrMetallicRoughness &pbr = material.pbrMetallicRoughness;
    std::vector<double> &baseColor = pbr.baseColorFactor;
    component->albedo = glm::vec4((float)baseColor[0], (float)baseColor[1], (float)baseColor[2], (float)baseColor[3]);
    component->transparency = (float)baseColor[3];
    component->metallic = (float)pbr.metallicFactor;
    component->roughness = (float)pbr.roughnessFactor;
    std::vector<double> &emissiveColor = material.emissiveFactor;
    component->emissive = glm::vec4((float)emissiveColor[0], (float)emissiveColor[1], (float)emissiveColor[2], 1.0f);
    /*
    // Parse Material texture
    auto loadTexture = [&](uint32_t index) {
        tinygltf::Texture& texture = model->textures[index];
        tinygltf::Image& image = model->images[texture.source];
        const std::string& name = image.uri.length() == 0 ? image.name : image.uri;
        return TextureCache::LoadTexture(name, image.width, image.height, image.image.data(), image.component, true);
        };


    if (pbr.baseColorTexture.index >= 0)
        component->albedoMap = loadTexture(pbr.baseColorTexture.index);

    if (pbr.metallicRoughnessTexture.index >= 0)
        component->metallicMap = component->roughnessMap = loadTexture(pbr.metallicRoughnessTexture.index);

    if (material.normalTexture.index >= 0)
        component->normalMap = loadTexture(material.normalTexture.index);

    if (material.occlusionTexture.index >= 0)
        component->ambientOcclusionMap = loadTexture(material.occlusionTexture.index);

    if (material.emissiveTexture.index >= 0)
        component->emissiveMap = loadTexture(material.emissiveTexture.index);
     */
}

static bool parseMesh(tinygltf::Model *model, tinygltf::Mesh &mesh, MeshGroup *meshGroup, const glm::mat4 &transform) {
    for (auto &primitive : mesh.primitives) {
        // Parse position
        const tinygltf::Accessor &positionAccessor = model->accessors[primitive.attributes["POSITION"]];
        float *positions = (float *)getBufferPtr(model, positionAccessor);
        uint32_t numPosition = (uint32_t)positionAccessor.count;

        // Parse normals
        float *normals = nullptr;
        auto normalAttributes = primitive.attributes.find("NORMAL");
        if (normalAttributes != primitive.attributes.end()) {
            const tinygltf::Accessor &normalAccessor = model->accessors[normalAttributes->second];
            assert(numPosition == normalAccessor.count);
            normals = (float *)getBufferPtr(model, normalAccessor);
        }

        // Parse tangents
        float *tangents = nullptr;
        auto tangentAttributes = primitive.attributes.find("TANGENT");
        if (tangentAttributes != primitive.attributes.end()) {
            const tinygltf::Accessor &tangentAccessor = model->accessors[normalAttributes->second];
            assert(numPosition == tangentAccessor.count);
            tangents = (float *)getBufferPtr(model, tangentAccessor);
        }

        // Parse UV
        float *uvs = nullptr;
        auto uvAttributes = primitive.attributes.find("TEXCOORD_0");
        if (uvAttributes != primitive.attributes.end()) {
            const tinygltf::Accessor &uvAccessor = model->accessors[uvAttributes->second];
            assert(numPosition == uvAccessor.count);
            uvs = (float *)getBufferPtr(model, uvAccessor);
        }

        std::vector<Vertex> &vertices = meshGroup->vertices;
        std::vector<uint32_t> &indices = meshGroup->indices;
        uint32_t vertexOffset = (uint32_t)(vertices.size() * sizeof(Vertex));
        uint32_t indexOffset = (uint32_t)(indices.size() * sizeof(uint32_t));

        for (uint32_t i = 0; i < numPosition; ++i) {
            Vertex vertex;
            vertex.position = {positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]};

            if (normals)
                vertex.normal = {normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]};
            else
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            if (uvs)
                vertex.uv = {uvs[i * 2 + 0], 1.0f - uvs[i * 2 + 1]};
            else
                vertex.uv = {0.0f, 0.0f};

            vertices.push_back(std::move(vertex));
        }

        const tinygltf::Accessor &indicesAccessor = model->accessors[primitive.indices];
        uint32_t indexCount = (uint32_t)indicesAccessor.count;
        if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            uint32_t *indicesPtr = (uint32_t *)getBufferPtr(model, indicesAccessor);
            indices.insert(indices.end(), indicesPtr, indicesPtr + indexCount);
        } else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            uint16_t *indicesPtr = (uint16_t *)getBufferPtr(model, indicesAccessor);
            indices.insert(indices.end(), indicesPtr, indicesPtr + indexCount);
        } else {
            std::cerr << "Undefined indices componentType: " + std::to_string(indicesAccessor.componentType) << std::endl;
            assert(0);
        }

        glm::vec3 minExtent = glm::vec3(positionAccessor.minValues[0], positionAccessor.minValues[1], positionAccessor.minValues[2]);
        glm::vec3 maxExtent = glm::vec3(positionAccessor.maxValues[0], positionAccessor.maxValues[1], positionAccessor.maxValues[2]);

        meshGroup->transforms.push_back(transform);
        RD::DrawElementsIndirectCommand drawCommand = {};
        drawCommand.count = indexCount;
        drawCommand.instanceCount = 1;
        drawCommand.firstIndex = indexOffset / sizeof(uint32_t);
        drawCommand.baseVertex = vertexOffset / sizeof(Vertex);
        drawCommand.baseInstance = 0;
        meshGroup->drawCommands.push_back(std::move(drawCommand));

        MaterialInfo material = {};
        std::string materialName = model->materials[primitive.material].name;
        meshGroup->names.push_back(materialName);
        parseMaterial(model, &material, primitive.material);
        meshGroup->materials.push_back(std::move(material));
    }

    return true;
}

void parseNodeHierarchy(tinygltf::Model *model, int nodeIndex, MeshGroup *meshGroup) {
    tinygltf::Node &node = model->nodes[nodeIndex];

    // Create entity and write the transforms
    glm::mat4 translation = glm::mat4(1.0f);
    glm::mat4 rotation = glm::mat4(1.0f);
    glm::mat4 scale = glm::mat4(1.0f);
    if (node.translation.size() > 0)
        translation = glm::translate(glm::mat4(1.0f), glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));
    if (node.rotation.size() > 0)
        rotation = glm::mat4_cast(glm::fquat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]));
    if (node.scale.size() > 0)
        scale = glm::scale(glm::mat4(1.0f), glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));

    glm::mat4 transform = translation * rotation * scale;
    // Update MeshData
    if (node.mesh >= 0) {
        tinygltf::Mesh &mesh = model->meshes[node.mesh];
        parseMesh(model, mesh, meshGroup, transform);
    }

    for (auto child : node.children)
        parseNodeHierarchy(model, child, meshGroup);
}

void parseScene(tinygltf::Model *model,
                tinygltf::Scene *scene,
                MeshGroup *meshGroup) {
    for (auto node : scene->nodes)
        parseNodeHierarchy(model, node, meshGroup);
}

bool GLTFLoader::Load(const char *filename, MeshGroup *meshGroup) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!ret) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
        if (!ret) {
            if (!warn.empty()) {
                LOGW(warn);
            }
            if (!err.empty()) {
                LOGE("Error::" + err);
            }
            return false;
        }
    }

    for (auto &scene : model.scenes)
        parseScene(&model, &scene, meshGroup);

    return true;
}