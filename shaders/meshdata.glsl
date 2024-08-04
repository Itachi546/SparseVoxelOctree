struct VertexData {
    float px, py, pz;
    float nx, ny, nz;
    float tu, tv;
};

struct MeshDrawCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseVertex;
    uint baseInstance;
    uint drawId;
};

layout(binding = 1, set = 0) readonly buffer Vertices {
    VertexData vertices[];
};

layout(binding = 2, set = 0) readonly buffer DrawCommands {
    MeshDrawCommand drawCommands[];
};

layout(binding = 3, set = 0) readonly buffer Transforms {
    mat4 transforms[];
};
