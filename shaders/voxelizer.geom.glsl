#version 460

#extension GL_GOOGLE_include_directive : enable
#include "voxelizer.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 vWorldPos[];
layout(location = 1) in vec2 vUV[];
layout(location = 2) in flat uint vDrawID[];

layout(location = 0) out vec3 gWorldPos;
layout(location = 1) out vec2 gUV;
layout(location = 2) out flat uint gDrawID;

void main() {

    vec3 p0 = vWorldPos[0];
    vec3 p1 = vWorldPos[1];
    vec3 p2 = vWorldPos[2];

    vec3 normal = cross(p1 - p0, p2 - p0);
    uint dominantAxis = FindDominantAxis(normal);

    for (int i = 0; i < 3; ++i) {
        // Convert to clipspace position and project it along dominant axis
        vec3 voxelSpacePos = ToVoxelSpace(vWorldPos[i]);
        gWorldPos = vWorldPos[i];
        gUV = vUV[i];
        gDrawID = vDrawID[i];

        vec3 projectedPosition = ProjectAlongDominantAxis(voxelSpacePos, dominantAxis);
        gl_Position = vec4(projectedPosition, 1.0f);
        EmitVertex();
    }

    EndPrimitive();
}