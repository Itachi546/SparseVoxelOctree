#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#include "globaldata.glsl"
#include "math.glsl"

layout(binding = 0, set = 1) readonly buffer NodeBuffer { uint nodePools[]; };
layout(binding = 1, set = 1) readonly buffer BrickBuffer { uint brickPools[]; };
layout(rgba8, binding = 2, set = 1) writeonly uniform image2D outputTexture;
layout(rgba8, binding = 3, set = 1) uniform image2D accumulationTexture;

layout(push_constant) uniform PushConstants {
    vec4 uAABBMin; // w component of both contains spp
    vec4 uAABBMax;
};

vec3 GenerateCameraRay(vec2 uv) {
    vec4 clipPos = uInvP * vec4(uv, -1.0, 0.0);
    vec4 worldPos = uInvV * vec4(clipPos.x, clipPos.y, -1.0, 0.0);
    return normalize(worldPos.xyz);
}

#include "raycast.glsl"

vec3 uintToRGB(uint color) {
    return vec3((color >> 16) & 0xff,
                (color >> 8) & 0xff,
                color & 0xff) /
           255.0f;
}

vec3 ACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    const uint width = 1920;
    const uint height = 1080;
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);

    g_seed = float(base_hash(floatBitsToUint(iuv.xy))) / float(0xffffffffU) + time;

    if (iuv.x > width || iuv.y > height)
        return;

    vec2 uv = ((vec2(iuv) + hash2(g_seed)) / vec2(width, height)) * 2.0f - 1.0f;

    vec3 r0 = uCameraPosition;
    vec3 rd = GenerateCameraRay(uv);

    RayHit hit = Trace(r0, rd);
    vec3 col = vec3(0.2f);
    if (hit.intersect) {
        vec3 diffuseColor = pow(uintToRGB(hit.color), vec3(2.2f));
        vec3 n = normalize(hit.normal);
        vec3 p = r0 + hit.t * rd;
        vec3 L = uLightPosition;
        L.xz += hash2(g_seed);
        vec3 ld = normalize(L - p);

        stack_reset();
        float shadow = 1.0f;
        RayHit shadowHit = Trace(p + n * 0.2f, ld);
        if (shadowHit.intersect) {
            shadow = 0.1f;
        }
        col = max(dot(n, ld), 0.05f) * diffuseColor * shadow;
    }

#if 1
    vec4 prevColor = imageLoad(accumulationTexture, iuv);
    float spp = uAABBMin.w;
    col = (prevColor.xyz * spp + col) / (spp + 1.0f);
    imageStore(accumulationTexture, iuv, vec4(col, 1.0f));

    col = ACES(col);
    col = pow(col, vec3(0.4545));
    imageStore(outputTexture, iuv, vec4(col, 1.0f));
#else
    // float iter = (float(hit.iteration) / (MAX_ITERATIONS * 0.5f));
    float iter = hit.iteration / 200.0f;
    imageStore(outputTexture, iuv, vec4(vec3(iter), 1.0));
#endif
}