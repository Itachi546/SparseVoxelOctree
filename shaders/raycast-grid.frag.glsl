#version 460

#extension GL_GOOGLE_include_directive : enable
#include "voxelizer.glsl"

layout(r8, set = 0, binding = 0) uniform image3D uTexture;

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

float SampleVoxel(vec3 p) {
#if 1
    ivec3 textureCoord = ivec3(WorldToTextureSpace(p));
    if (textureCoord.x >= 0 && textureCoord.y >= 0 && textureCoord.z >= 0 &&
        textureCoord.x < VOXEL_GRID_SIZE && textureCoord.y < VOXEL_GRID_SIZE && textureCoord.z < VOXEL_GRID_SIZE) {
        float val = imageLoad(uTexture, textureCoord).r;
        return val;
    }
    return 0.0f;
#else
    if (length(p) < 5.0f)
        return 1.0f;
    return 0.0f;
#endif
}

const int MAX_ITERATION = 512;
void main() {
    vec3 r0 = uCamPos.xyz;
    vec3 rd = GenerateCameraRay(uv, uInvP, uInvV);

    vec3 stepDir = sign(rd);
    vec3 p = floor(r0);
    vec3 tStep = 1.0f / rd;
    vec3 offset = 0.5 + stepDir * 0.5;
    vec3 nextPlane = p + offset;
    vec3 t = (nextPlane - r0) * tStep;

    vec3 col = vec3(0.5f);
    for (int i = 0; i < MAX_ITERATION; ++i) {
        vec3 nearestAxis = step(t, t.yzx) * step(t, t.zxy);
        p += nearestAxis * stepDir;
        t += nearestAxis * tStep * stepDir;

        float val = SampleVoxel(p);
        if (val > 0.5f) {
            vec3 id = p;
            p = p + 1.0 - offset;
            vec3 intersection = (p - r0) * tStep;
            float d = max(intersection.x, max(intersection.y, intersection.z));
            p = r0 + d * rd;
            vec3 n = (p - id - 0.5) * 2.0;
            n = pow(abs(n), vec3(5.0f)) * sign(n);
            n = normalize(n);
            col = n * 0.5 + 0.5;
            break;
        }

        /*
         if (p.x < 0 || p.y < 0 || p.z < 0)
             break;
         if (p.x > VOXEL_GRID_SIZE || p.y > VOXEL_GRID_SIZE || p.z > VOXEL_GRID_SIZE)
             break;
             */
    }

    fragColor = vec4(col, 1.0f);
}