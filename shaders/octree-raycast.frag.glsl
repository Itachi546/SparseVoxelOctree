#version 460

#extension GL_GOOGLE_include_directive : enable
#include "voxelizer.glsl"

layout(set = 0, binding = 0) readonly buffer OctreeBuffer {
    uint octree[];
};

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 uInvP;
    mat4 uInvV;

    vec4 uCamPos;
};

vec3 GenerateCameraRay(vec2 uv, mat4 invP, mat4 invV) {
    vec4 clipPos = invP * vec4(uv, -1.0, 0.0);
    vec4 worldPos = invV * vec4(clipPos.x, clipPos.y, -1.0, 0.0);
    return normalize(worldPos.xyz);
}

vec2 BoxIntersection(in vec3 ro, in vec3 rd, vec3 boxSize) {
    vec3 m = 1.0 / rd;
    vec3 n = m * ro;
    vec3 k = abs(m) * boxSize;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    float tN = max(max(t1.x, t1.y), t1.z);
    float tF = min(min(t2.x, t2.y), t2.z);
    if (tN > tF || tF < 0.0)
        return vec2(-1.0);
    return vec2(tN, tF);
}

const int MAX_ITERATION = 1024;

bool Raycast(vec3 r0, vec3 rd, out vec3 out_p, out vec3 n, out vec3 col) {
    return false;
}

void main() {
    vec3 r0 = uCamPos.xyz;
    vec3 rd = GenerateCameraRay(uv, uInvP, uInvV);
    vec3 col, p, _unused, _unused1, n, _unused2;
    if (Raycast(r0, rd, p, n, col)) {
        col = n * 0.5 + 0.5;
    }

    col /= (1.0f + col);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0f);
}