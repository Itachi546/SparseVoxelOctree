#pragma once

#include <rendering/rendering-device.h>
#include "mesh.h"

#include <vector>
#include <string>

struct Scene {
    static bool LoadMeshes(const std::vector<std::string>& filenames, std::vector<MeshGroup>& meshGroups);

    void PrepareDrawData(const std::vector<MeshGroup>& meshGroups);

    void Shutdown();

    BufferID vertexBuffer;
    BufferID indexBuffer;
    BufferID transformBuffer;
    BufferID materialBuffer;
    BufferID drawCommands;
};