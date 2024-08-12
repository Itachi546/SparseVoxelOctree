#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "meshdata.glsl"

void main() {
    MeshDrawCommand drawCommand = drawCommands[gl_DrawID];
    VertexData vertex = vertices[gl_VertexIndex];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);

    mat4 worldTransform = transforms[drawCommand.drawId];

    vec4 worldPos = worldTransform * vec4(position, 1.0f);
    gl_Position = worldPos;
}
