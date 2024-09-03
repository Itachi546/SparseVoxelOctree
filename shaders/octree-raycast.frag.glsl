#version 460

#extension GL_GOOGLE_include_directive : enable

#include "octree.glsl"

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

void main() {
    vec3 r0 = uCamPos.xyz;
    vec3 rd = GenerateCameraRay(uv, uInvP, uInvV);

    vec3 outPos, outColor, outNormal;
    uint outIter;

    vec3 col = vec3(0.0f);
    if (Octree_RayMarchLeaf(r0, rd, outPos, outColor, outNormal, outIter)) {
        col = vec3(1.0f);
    }
    col += outIter / 100.0f;

    col /= (1.0f + col);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0f);
}