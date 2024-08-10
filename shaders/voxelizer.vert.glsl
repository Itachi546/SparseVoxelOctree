#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "meshdata.glsl"

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec2 vUV;
layout(location = 2) out flat uint vDrawID;

void main() {
    MeshDrawCommand drawCommand = drawCommands[gl_DrawID];
    VertexData vertex = vertices[gl_VertexIndex];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);

    mat4 worldTransform = transforms[drawCommand.drawId];

    vec4 worldPos = worldTransform * vec4(position, 1.0f);
    gl_Position = worldPos;

    vWorldPos = worldPos.xyz;
    vUV = vec2(vertex.tu, vertex.tv);
    vDrawID = drawCommand.drawId;
}
