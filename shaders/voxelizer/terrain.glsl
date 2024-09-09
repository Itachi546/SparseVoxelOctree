#ifndef TERRAIN_GLSL
#define TERRAIN_GLSL

#include "noise2D.glsl"

const float maxHeight = 100.0f;
float GetNoise(vec3 p) {
    float amplitude = 1.0f;
    float frequency = 0.002f;
    int octaves = 5;

    float total = 0.0f;
    float noise = 0.0f;
    for (int i = 0; i < 5; ++i) {
        noise += amplitude * snoise(p.xz * frequency);
        total += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    float normNoise = (noise / total) * 2.0f - 1.0f;
    return p.y + normNoise * maxHeight;
}

#endif