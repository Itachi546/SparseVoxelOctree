#version 460

#extension GL_GOOGLE_include_directive : enable
#include "globaldata.glsl"

struct VertexData {
    float px, py, pz;
    uint color;
};

layout(binding = 0, set = 1) readonly buffer Vertices {
    VertexData vertices[];
};

layout(location = 0) out vec3 vColor;

void main() {
   VertexData vertex = vertices[gl_VertexIndex];
   gl_Position = VP * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
   vColor = vec3(vertex.color >> 24, vertex.color >> 16, vertex.color >> 8) / 255.0f;
}