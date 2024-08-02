#pragma once

#include "render-scene.h"
#include "mesh.h"

#include <memory>
#include <unordered_map>

namespace tinygltf {
    class Model;
    struct Scene;
    struct Mesh;
}; // namespace tinygltf

class AsyncLoader;

class GLTFScene : public RenderScene {
  public:
    bool Initialize(const std::vector<std::string> &filenames, std::shared_ptr<AsyncLoader> loader, BufferID globalUB) override;
    void PrepareDraws() override;
    void Render(CommandBufferID commandBuffer) override;

    void AddTexturesToUpdate(TextureID texture) {
        std::lock_guard lock{textureUpdateMutex};
        texturesToUpdate.push_back(texture);
    }

    // @TODO TEMP Fix this later
    void UpdateTextures(CommandBufferID commandBuffer);

    void Shutdown() override;

    BufferID vertexBuffer;
    BufferID indexBuffer;
    BufferID transformBuffer;
    BufferID materialBuffer;
    BufferID drawCommandBuffer;
    MeshGroup meshGroup;

    virtual ~GLTFScene() {}

  private:
    std::unordered_map<uint32_t, TextureID> textureMap;

    bool LoadFile(const std::string &filename, MeshGroup *meshGroup);
    void ParseScene(tinygltf::Model *model, tinygltf::Scene *scene, MeshGroup *meshGroup);
    void ParseNodeHierarchy(tinygltf::Model *model, int nodeIndex, MeshGroup *meshGroup, const glm::mat4& parentTransform);
    bool ParseMesh(tinygltf::Model *model, tinygltf::Mesh &mesh, MeshGroup *meshGroup, const glm::mat4 &transform);
    void ParseMaterial(tinygltf::Model *model, MaterialInfo *component, uint32_t matIndex);

    RD *device;
    std::shared_ptr<AsyncLoader> asyncLoader;
    std::string _meshBasePath;

    // @TODO shared among different scene
    PipelineID renderPipeline;
    UniformSetID globalSet, meshBindingSet;

    std::mutex textureUpdateMutex;
    std::vector<TextureID> texturesToUpdate;
};