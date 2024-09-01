#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#define ENABLE_BINDLESS_SET
#include "material.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in flat uint drawId;
layout(location = 3) in vec3 vWorldPos;

void main() {
    vec3 n = normalize(vNormal);
    Material material = materials[drawId];

    vec4 diffuseColor;
    if (material.albedoMap != INVALID_TEXTURE) {
        diffuseColor = sampleTexture(material.albedoMap, vUV);
    } else
        diffuseColor = material.albedo;
    if (diffuseColor.a < 0.5)
        discard;

    vec3 ld = normalize(vec3(0.1f, 1.0f, 0.5f));

    vec3 col = vec3(0.0f);
    col += max(dot(n, ld), 0.1f) * diffuseColor.rgb;
    col /= (1.0f + col);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, diffuseColor.a);
}