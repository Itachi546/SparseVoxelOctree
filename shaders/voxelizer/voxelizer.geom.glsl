#version 460

#extension GL_GOOGLE_include_directive : enable
#include "voxelizer.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 vWorldPos[];
layout(location = 1) in vec2 vUV[];
layout(location = 2) in flat uint vDrawID[];

layout(location = 0) out vec3 gPos01;
layout(location = 1) out vec2 gUV;
layout(location = 2) out flat uint gDrawID;

layout(push_constant) uniform PushConstant {
    float minExtent, maxExtent;
};

void main() {

    vec3 p0 = vWorldPos[0];
    vec3 p1 = vWorldPos[1];
    vec3 p2 = vWorldPos[2];

    vec3 normal = cross(p1 - p0, p2 - p0);
    uint dominantAxis = FindDominantAxis(normal);

    for (int i = 0; i < 3; ++i) {
        // Convert to clipspace position
        vec3 pos01 = ((vWorldPos[i] - minExtent) / (maxExtent - minExtent));
        gPos01 = pos01;
        gUV = vUV[i];
        gDrawID = vDrawID[i];

        vec3 projectedPosition = ProjectAlongDominantAxis(pos01 * 2.0f - 1.0f, dominantAxis);
        gl_Position = vec4(projectedPosition, 1.0f);
        EmitVertex();
    }

    EndPrimitive();
}