#pragma once

#include <rendering/rendering-device.h>
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
    bool Initialize(const std::vector<std::string> &filenames, std::shared_ptr<AsyncLoader> loader) override;
    void PrepareDraws() override;
    void Render() override;
    void Shutdown() override;

    BufferID vertexBuffer;
    BufferID indexBuffer;
    BufferID transformBuffer;
    BufferID materialBuffer;
    BufferID drawCommands;

    virtual ~GLTFScene() {}

  private:
    std::vector<MeshGroup> meshGroups;
    std::unordered_map<uint32_t, TextureID> textureMap;

    bool LoadFile(const std::string &filename, MeshGroup *meshGroup);
    void ParseScene(tinygltf::Model *model, tinygltf::Scene *scene, MeshGroup *meshGroup);
    void ParseNodeHierarchy(tinygltf::Model *model, int nodeIndex, MeshGroup *meshGroup);
    bool ParseMesh(tinygltf::Model *model, tinygltf::Mesh &mesh, MeshGroup *meshGroup, const glm::mat3 &transform);
    void ParseMaterial(tinygltf::Model *model, MaterialInfo *component, uint32_t matIndex);

    RD *device;
    std::shared_ptr<AsyncLoader> asyncLoader;
    std::string _meshBasePath;
};