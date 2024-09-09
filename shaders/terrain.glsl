#ifndef TERRAIN_GLSL
#define TERRAIN_GLSL

float GetNoise(vec3 p) {
    float d = length(p) - 50.0f;
    return min(d, p.y - 1.0f);
}

#endif