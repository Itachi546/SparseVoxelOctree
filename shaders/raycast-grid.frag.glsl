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
    if (IsInsideCube(textureCoord)) {
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

const int MAX_ITERATION = 512;
void main() {
    vec3 r0 = uCamPos.xyz;
    vec3 rd = GenerateCameraRay(uv, uInvP, uInvV);

    float halfSize = VOXEL_GRID_SIZE * 0.5f;
    vec2 tBox = BoxIntersection(r0, rd, vec3(halfSize));
    vec3 col = vec3(0.5f);
    tBox.x = max(tBox.x, 0.0f);
    if (tBox.y >= 0.0f) {
        r0 = r0 + tBox.x * rd;
        vec3 stepDir = sign(rd);
        vec3 p = floor(r0);
        vec3 tStep = 1.0f / rd;
        vec3 offset = 0.5 + stepDir * 0.5;
        vec3 nextPlane = p + offset;
        vec3 t = (nextPlane - r0) * tStep;

        float tMax = tBox.y - tBox.x;
        for (int i = 0; i < MAX_ITERATION; ++i) {
            vec3 nearestAxis = step(t, t.yzx) * step(t, t.zxy);
            p += nearestAxis * stepDir;
            t += nearestAxis * tStep * stepDir;

            float val = SampleVoxel(p);
            if (val > 0.1f) {
                vec3 id = p;
                p = p + 1.0 - offset;
                vec3 intersection = (p - r0) * tStep;
                float d = max(intersection.x, max(intersection.y, intersection.z));
                p = r0 + d * rd;
                vec3 n = (p - id - 0.5) * 2.0;
                n = pow(abs(n), vec3(5.0f)) * sign(n);
                n = normalize(n);
                col = n * 0.5 + 0.5;
                // col = vec3(float(i) / float(MAX_ITERATION));
                break;
            }

            if (min(t.x, min(t.y, t.z)) > tMax)
                break;
            /*
            if (p.x < -halfSize || p.y < -halfSize || p.z < -halfSize)
                break;
            if (p.x > halfSize || p.y > halfSize || p.z > halfSize)
                break;
            */
        }
    }

    fragColor = vec4(col, 1.0f);
}