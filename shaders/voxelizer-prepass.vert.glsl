#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "globaldata.glsl"

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

layout(binding = 0, set = 1) readonly buffer Vertices {
    VertexData vertices[];
};

layout(binding = 1, set = 1) readonly buffer DrawCommands {
    MeshDrawCommand drawCommands[];
};

layout(binding = 2, set = 1) readonly buffer Transforms {
    mat4 transforms[];
};

layout(location = 0) out vec3 vWorldPos;

void main() {
    MeshDrawCommand drawCommand = drawCommands[gl_DrawID];
    VertexData vertex = vertices[gl_VertexIndex];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);

    mat4 worldTransform = transforms[drawCommand.drawId];
    vec4 worldPos = worldTransform * vec4(position, 1.0f);
    vWorldPos = worldPos.xyz;

    gl_Position = VP * worldPos;
}
