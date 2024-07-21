#include "pch.h"
#include "scene.h"
#include "loaders/gltf-loader.h"

bool Scene::LoadMeshes(const std::vector<std::string> &filenames, std::vector<MeshGroup> &meshGroups) {
    meshGroups.resize(filenames.size());
    for (int i = 0; i < filenames.size(); ++i) {
        MeshGroup &meshGroup = meshGroups[i];
        if (!GLTFLoader::Load(filenames[i].c_str(), &meshGroup))
            return false;
    }
    return true;
}

void Scene::PrepareDrawData(const std::vector<MeshGroup> &meshGroups) {
    
}

void Scene::Shutdown() {
}
