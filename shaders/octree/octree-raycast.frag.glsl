#version 460

#extension GL_GOOGLE_include_directive : enable

#include "octree.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 uInvP;
    mat4 uInvV;
    mat4 uInvM;
    vec4 uCamPos;
};

vec3 GenerateCameraRay(vec2 uv, mat4 invP, mat4 invV) {
    vec3 clipPos;
    clipPos.x = invP[0][0] * uv.x + invP[0][1] * uv.y - invP[0][2];
    clipPos.y = invP[1][0] * uv.x + invP[1][1] * uv.y - invP[1][2];
    clipPos.z = -1.0f;

    vec3 worldPos = mat3(invV) * clipPos;
    return normalize(worldPos.xyz);
}

const vec3 ld = normalize(vec3(0.01f, 0.8f, 0.1f));
void main() {
    vec3 r0 = uCamPos.xyz;
    vec3 rd = GenerateCameraRay(uv, uInvP, uInvV);

    r0 = vec3(uInvM * vec4(r0, 1.0f));
    rd = normalize(vec3(uInvM * vec4(rd, 0.0f)));

    vec3 outPos, outColor, outNormal;
    uint outIter;

    vec3 col = vec3(0.0f);
    if (Octree_RayMarchLeaf(r0, rd, outPos, outColor, outNormal, outIter)) {
        vec3 p = outPos;
        col = max(dot(outNormal, ld), 0.1f) * vec3(1.0f, 1.01f, 1.01f) * 5.;

        vec3 sPos, sColor, sNormal;
        uint sOutIter;

        vec3 sp = vec3(uInvM * vec4(outPos + outNormal * 0.1f, 1.0f));
        bool shadow = Octree_RayMarchLeaf(outPos, ld, sPos, sColor, sNormal, sOutIter);
        if (shadow) {
            col *= 0.01f;
        }

        // vec3 h = normalize(-rd + ld);
        //  vec3 spec = pow(max(dot(outNormal, h), 0.0f), 16.0f) * vec3(1.);
        //  col += spec;
        col *= outColor;
    }

    col /= (1.0f + col);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0f);
}