#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 vWorldPos[];
layout(location = 1) in vec2 vUV[];
layout(location = 2) in flat uint vDrawId[];

layout(location = 0) out vec3 gWorldPos;
layout(location = 1) out vec2 gUV;
layout(location = 2) out flat uint gDrawId;
layout(location = 3) out vec3 gVoxelWorldPos;
// Writing to gl_Position outside the range results in
// clipping which is discarded by the rasterizer. Our goal
// is to maximize the number of triangle generated within
// the viewport
const float VOXEL_SIZE = 32;
vec3 ToVoxelSpace(vec3 p) {
    return p / (VOXEL_SIZE * 0.5);
}

void main() {

    vec3 p0 = vWorldPos[0];
    vec3 p1 = vWorldPos[1];
    vec3 p2 = vWorldPos[2];

    vec3 normal = cross(p1 - p0, p2 - p0);
    vec3 axisWeight = abs(normal);

    uint dominantAxis = axisWeight.x > axisWeight.y ? 0 : 1;
    dominantAxis = dominantAxis > axisWeight.z ? dominantAxis : 2;
    for (int i = 0; i < 3; ++i) {
        // Convert to clipspace position and project it along dominant axis
        vec3 voxelSpacePos = ToVoxelSpace(vWorldPos[i]);
        vec3 projectedPosition = voxelSpacePos;
        if (dominantAxis == 0)
            projectedPosition = projectedPosition.zyx;
        else if (dominantAxis == 1)
            projectedPosition = projectedPosition.xzy;
        projectedPosition.z = 0;

        gWorldPos = vWorldPos[i];
        gUV = vUV[i];
        gDrawId = vDrawId[i];
        gVoxelWorldPos = (voxelSpacePos * 0.5 + 0.5) * VOXEL_SIZE;

        gl_Position = vec4(projectedPosition, 1.0f);
        EmitVertex();
    }

    EndPrimitive();
}