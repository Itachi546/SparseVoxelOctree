#version 460

#extension GL_GOOGLE_include_directive : enable
#include "voxelizer.glsl"

layout(rgba8, set = 0, binding = 0) uniform image3D uTexture;

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

vec4 SampleVoxel(vec3 p) {
    ivec3 textureCoord = ivec3(WorldToTextureSpace(p));
    if (IsInsideCube(textureCoord)) {
        vec4 val = imageLoad(uTexture, textureCoord);
        return val;
    }
    return vec4(0.0f);
}

float CalculateAO(vec3 p) {
    vec3 ip = round(p);
    float ao = 0.0f;
    float totalSample = 0.0f;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            for (int k = -1; k <= 1; ++k) {
                if (i == 0 && j == 0 && k == 0)
                    continue;
                ao += SampleVoxel(ip + vec3(i, j, k)).a;
                totalSample++;
            }
        }
    }
    return ao / totalSample;
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

vec2 rand22(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 843.43)),
             dot(p, vec2(269.5, 183.3)));

    return fract(sin(p) * 43758.5453123);
}

const vec3 lp = vec3(0.0f, 200.0f, 0.0f);
const float lr = 10.0f;

const int MAX_ITERATION = 1024;
bool Raycast(vec3 r0, vec3 rd, out vec3 out_p, out vec3 n, out vec3 col) {
    float halfSize = VOXEL_GRID_SIZE * 0.5f;
    vec2 tBox = BoxIntersection(r0, rd, vec3(halfSize));
    col = vec3(0.5f);
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
            vec4 val = SampleVoxel(p);
            if (val.a > 0.1f) {
                vec3 id = p;
                p = p + 1.0 - offset;
                vec3 intersection = (p - r0) * tStep;
                float d = max(intersection.x, max(intersection.y, intersection.z));
                p = r0 + d * rd;
                out_p = p;
                n = step(t, t.yzx) * step(t, t.zxy); //(p - id - 0.5) * 2.0;
                n = normalize(n);
                // n = pow(abs(n), vec3(15.0f)) * sign(n);
                // n = normalize(n);
                vec3 ld = normalize(lp - p);
                col = max(dot(n, ld), 0.1f) * vec3(1.0f, 1.01f, 1.01f) * 5.;

                vec3 h = normalize(-rd + ld);
                vec3 spec = pow(max(dot(n, h), 0.0f), 16.0f) * vec3(1.);
                col += spec;
                col *= val.rgb;

                // col = vec3(float(i) / float(MAX_ITERATION));
                return true;
            }

            t += nearestAxis * tStep * stepDir;

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
    return false;
}

void main() {
    vec3 r0 = uCamPos.xyz;
    vec3 rd = GenerateCameraRay(uv, uInvP, uInvV);
    vec3 col, p, _unused, _unused1, n, _unused2;
    if (Raycast(r0, rd, p, n, col)) {
        vec3 lpR = lp;
        // lpR.xz += rand22(p.xz) * lr;
        // float ao = 1 - CalculateAO(p);
        vec3 ld = normalize(lpR - p);
        if (Raycast(p + 0.001 * n, ld, _unused, _unused1, _unused2))
            col *= 0.01f;
        // col *= pow(ao, 3.0f);
    }

    col /= (1.0f + col);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0f);
}