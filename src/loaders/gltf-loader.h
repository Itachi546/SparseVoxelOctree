#pragma once

#include "gfx/mesh.h"

#include <vector>
#include <string>

namespace GLTFLoader {

    bool Load(const std::string& filename, MeshGroup* meshGroup, std::vector<std::string>& textures);
};