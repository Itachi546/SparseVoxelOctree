#pragma once

#include "render-scene.h"

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
    bool Initialize(const std::vector<std::string> &filenames, std::shared_ptr<AsyncLoader> loader) override;
    void PrepareDraws(BufferID globalUB) override;
    void Render(CommandBufferID commandBuffer) override;

    void AddTexturesToUpdate(TextureID texture) override {
        std::lock_guard lock{textureUpdateMutex};
        texturesToUpdate.push_back(texture);
    }

    void UpdateTextures(CommandBufferID commandBuffer) override;

    void Shutdown() override;

    virtual ~GLTFScene() {}

  private:
    std::unordered_map<uint32_t, TextureID> textureMap;

    bool LoadFile(const std::string &filename, MeshGroup *meshGroup);
    void ParseScene(tinygltf::Model *model, tinygltf::Scene *scene, MeshGroup *meshGroup);
    void ParseNodeHierarchy(tinygltf::Model *model, int nodeIndex, MeshGroup *meshGroup, const glm::mat4 &parentTransform);
    bool ParseMesh(tinygltf::Model *model, tinygltf::Mesh &mesh, MeshGroup *meshGroup, const glm::mat4 &transform);
    void ParseMaterial(tinygltf::Model *model, MaterialInfo *component, uint32_t matIndex);

    RD *device;
    std::shared_ptr<AsyncLoader> asyncLoader;
    std::string _meshBasePath;

    // @TODO shared among different scene
    PipelineID renderPipeline;
    UniformSetID bindingSet;

    std::mutex textureUpdateMutex;
    std::vector<TextureID> texturesToUpdate;

    // @NOTE this submitInfo is used for synchronous transfer of texture and upload
    // buffer resources to the gpu. This is destroyed at the end of PrepareDraws
    RD::ImmediateSubmitInfo stagingSubmitInfo;
};