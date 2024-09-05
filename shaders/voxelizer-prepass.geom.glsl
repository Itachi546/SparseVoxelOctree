#version 460

#extension GL_GOOGLE_include_directive : enable
#include "voxelizer.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) out vec3 gPos01;

layout(push_constant) uniform PushConstant {
    float minExtent, maxExtent;
};

void main() {

    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;

    vec3 normal = cross(p1 - p0, p2 - p0);
    uint dominantAxis = FindDominantAxis(normal);

    for (int i = 0; i < 3; ++i) {
        // Convert to clipspace position, project it along dominant axis
        vec3 pos01 = ((gl_in[i].gl_Position.xyz - minExtent) / (maxExtent - minExtent));
        gPos01 = pos01;

        vec3 projectedPosition = ProjectAlongDominantAxis(pos01 * 2.0f - 1.0f, dominantAxis);
        gl_Position = vec4(projectedPosition, 1.0f);
        EmitVertex();
    }

    EndPrimitive();
}