#version 460

layout(location = 0) out vec4 fragColor;

layout(std430, binding = 0) readonly buffer NodeBuffer { uint nodePools[]; };
layout(std430, binding = 1) readonly buffer BrickBuffer { uint brickPools[]; };

uniform vec3 uCamPos;
uniform vec3 uLightPos;
uniform mat4 uInvP;
uniform mat4 uInvV;
uniform vec3 uAABBMin;
uniform vec3 uAABBMax;
uniform vec2 uResolution;

const int MAX_LEVELS = 12;
struct StackData {
    vec3 position;
    uint firstSibling;
    int idx;
    float size;
    float tMax;
} stack[MAX_LEVELS];

int stackPtr = 0;
void stack_reset() { stackPtr = 0; }
void stack_push(StackData data) { stack[stackPtr++] = data; }

StackData stack_pop() { return stack[--stackPtr]; }
bool stack_empty() { return stackPtr == 0; }

vec3 GenerateCameraRay(vec2 uv) {
    vec4 clipPos = uInvP * vec4(uv, -1.0, 0.0);
    vec4 worldPos = uInvV * vec4(clipPos.x, clipPos.y, -1.0, 0.0);
    return normalize(worldPos.xyz);
}

#define EPSILON 10e-7f
#define MAX_ITERATIONS 1000
#define BRICK_SIZE 8
#define LEAF_NODE_SIZE 1
#define BRICK_SIZE2 (BRICK_SIZE * BRICK_SIZE)
#define BRICK_SIZE3 (BRICK_SIZE2 * BRICK_SIZE)
#define UNIT_BRICK_SIZE (float(LEAF_NODE_SIZE) / float(BRICK_SIZE))
#define GRID_MARCH_MAX_ITERATION 100

#define REFLECT(p, c) (2.0f * c - p)
#define GET_MASK(p) (p >> 30)
#define GET_FIRST_CHILD(p) (p & 0x3fffffff)
#define InternalLeafNode 0
#define InternalNode 1
#define LeafNode 2
#define LeafNodeWithPtr 3

int FindEntryNode(inout vec3 p, float scale, float t, vec3 tM) {
    ivec3 mask = ivec3(step(vec3(t), tM));
    p -= mask * scale;
    return mask.x | (mask.y << 1) | (mask.z << 2);
}

vec3 Remap(vec3 p, vec3 a0, vec3 a1, vec3 b0, vec3 b1) {
    vec3 t = clamp((p - a0) / (a1 - a0), 0.0f, 1.0f);
    return b0 + t * (b1 - b0);
}

bool IsBitSet(int data, int index) {
    int mask = 1 << index;
    return (data & mask) == mask;
}

struct RayHit {
    float t;
    bool intersect;
    vec3 normal;
    uint color;
    int iteration;
};

vec3 uintToRGB(uint color) {
    return vec3((color >> 16) & 0xff,
                (color >> 8) & 0xff,
                color & 0xff) /
           255.0f;
}

const int gridEndMargin = BRICK_SIZE - 1;
RayHit RaycastDDA(vec3 r0, vec3 invRd, vec3 dirMask, uint brickStart) {
    vec3 stepDir = mix(vec3(-1.0f), vec3(1.0f), dirMask);
    vec3 tStep = invRd;

    vec3 p = floor(r0);
    p = mix(gridEndMargin - p, p, dirMask);

    RayHit rayHit;
    rayHit.intersect = false;
    rayHit.t = 0.0f;
    rayHit.iteration = 0;
    vec3 t = (1.0f - fract(r0)) * tStep;

    vec3 nearestAxis = step(t, t.yzx) * step(t, t.zxy);
    for (int i = 0; i < GRID_MARCH_MAX_ITERATION; ++i) {
        uint voxelIndex = brickStart + uint(p.x * BRICK_SIZE2 + p.y * BRICK_SIZE + p.z);
        uint color = brickPools[voxelIndex];
        if (color > 0) {
            // Undo the reflection
            p = mix(gridEndMargin - p, p, dirMask);
            vec3 t = (p - r0) * tStep;
            float tMax = max(max(t.x, t.y), t.z);
            rayHit.normal = -nearestAxis;
            rayHit.t = tMax * UNIT_BRICK_SIZE;
            rayHit.intersect = true;
            rayHit.color = color;
            rayHit.iteration = i;
            return rayHit;
        }

        nearestAxis = step(t, t.yzx) * step(t, t.zxy);
        p += nearestAxis * stepDir;
        t += nearestAxis * tStep;
        if (p.x < 0.0f || p.x > gridEndMargin || p.y < 0.0f || p.y > gridEndMargin || p.z < 0.0f || p.z > gridEndMargin)
            break;
    }
    return rayHit;
}

RayHit Trace(vec3 r0, vec3 rd) {
    vec3 r0_orig = r0;
    vec3 aabbMin = uAABBMin;
    vec3 aabbMax = uAABBMax;
    vec3 center = (aabbMin + aabbMax) * 0.5f;

    int octaneMask = 7;
    if (rd.x < 0.0) {
        octaneMask ^= 1;
        r0.x = REFLECT(r0.x, center.x);
    }
    if (rd.y < 0.0) {
        octaneMask ^= 2;
        r0.y = REFLECT(r0.y, center.y);
    }
    if (rd.z < 0.0) {
        octaneMask ^= 4;
        r0.z = REFLECT(r0.z, center.z);
    }

    vec3 d = abs(rd);
    vec3 invRayDir = 1.0f / (d + EPSILON);
    vec3 r0_rd = r0 * invRayDir;

    vec3 tMin = aabbMin * invRayDir - r0_rd;
    vec3 tMax = aabbMax * invRayDir - r0_rd;

    RayHit rayHit;
    rayHit.intersect = false;
    rayHit.iteration = 0;

    vec2 t = vec2(max(tMin.x, max(tMin.y, tMin.z)), min(tMax.x, min(tMax.y, tMax.z)));
    if (t.x > t.y || t.y < 0.0)
        return rayHit;

    float currentSize = (uAABBMax.x - uAABBMin.x) * 0.5f;
    float octreeSize = currentSize;

    // Determine Entry Node
    vec3 p = aabbMax;
    vec3 tM = (tMin + tMax) * 0.5f;

    bool processChild = true;
    int idx = FindEntryNode(p, currentSize, t.x, tM);

    uint firstSibling = GET_FIRST_CHILD(nodePools[0]);
    int i = 0;
    for (; i < 1000; ++i) {
        uint nodeIndex = firstSibling + (idx ^ octaneMask);
        uint nodeDescriptor = nodePools[nodeIndex];
        vec3 tCorner = p * invRayDir - r0_rd;
        float tcMax = min(min(tCorner.x, tCorner.y), tCorner.z);

        if (processChild && tcMax >= 0.0f) {
            uint mask = GET_MASK(nodeDescriptor);
            if (mask == LeafNode) {
                vec3 dir = p - (r0 + t.x * d);
                rayHit.normal = -step(dir.yzx, dir.xyz) * step(dir.zxy, dir.xyz) * sign(rd);
                rayHit.intersect = true;
                rayHit.t = t.x;
                rayHit.color = GET_FIRST_CHILD(nodeDescriptor);
                break;
            } else if (mask == LeafNodeWithPtr) {
                uint brickPointer = GET_FIRST_CHILD(nodeDescriptor) * BRICK_SIZE3;
                vec3 intersectPos = r0 + max(t.x, 0.0f) * d;
                vec3 brickMax = vec3(BRICK_SIZE);
                vec3 brickPos = Remap(intersectPos, p - currentSize, p, vec3(0.0f), brickMax);

                RayHit brickHit = RaycastDDA(brickPos, invRayDir, ivec3(greaterThan(rd, vec3(0.0f))), brickPointer);
                if (brickHit.intersect) {
                    // Debug draw nodes
                    rayHit.normal = brickHit.normal * sign(rd);
                    rayHit.intersect = true;
                    rayHit.t = max(t.x, 0.0f) + brickHit.t;
                    rayHit.color = brickHit.color;
                    rayHit.iteration = brickHit.iteration;
                    break;
                }
            } else if (mask == InternalNode) {

                // Intersect with t-span of cube
                float tvMax = min(t.y, tcMax);
                float halfSize = currentSize * 0.5f;
                vec3 tCenter = (p - halfSize) * invRayDir - r0_rd;

                if (t.x <= tvMax) {
                    StackData stackData;
                    stackData.position = p;
                    stackData.firstSibling = firstSibling;
                    stackData.idx = idx;
                    stackData.tMax = t.y;
                    stackData.size = currentSize;
                    stack_push(stackData);
                }

                // Find Entry Node
                currentSize = halfSize;
                idx = FindEntryNode(p, currentSize, t.x, tCenter);
                firstSibling = GET_FIRST_CHILD(nodeDescriptor);
                t.y = tvMax;
                continue;
            }
        }
        processChild = true;

        // Advance
        ivec3 stepDir = ivec3(step(tCorner, vec3(tcMax)));
        int stepMask = stepDir.x | (stepDir.y << 1) | (stepDir.z << 2);
        p += vec3(stepDir) * currentSize;
        t.x = tcMax;
        idx ^= stepMask;
        // If the idx is set for corresponding direction and stepMask is also
        // in same direction then we are exiting parent
        // E.g.
        /*
         * +-----------+
         * |  10 |  11 |
         * +-----------+
         * |  00 |  01 |
         * +-----------+
         * if we are currently at 10 and stepMask is 01
         *  Conclusion: Valid because X is currently 0 and can take the step to 11
         * if we are currently at 10 and stepMask is 10
         *  Conclusion: Invalid because we have already taken a step in Y-Axis
         */

        if ((idx & stepMask) != 0) {
            // Pop operation
            if (!stack_empty()) {
                StackData stackData = stack_pop();
                p = stackData.position;
                firstSibling = stackData.firstSibling;
                t.y = stackData.tMax;
                idx = stackData.idx;
                currentSize = stackData.size;
            } else
                currentSize = currentSize * 2.0f;
            processChild = false;
        }

        if (currentSize > octreeSize)
            break;
    }

    rayHit.iteration += i;
    return rayHit;
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
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0f - 1.0f;
    vec3 r0 = uCamPos;
    vec3 rd = GenerateCameraRay(uv);

    RayHit hit = Trace(r0, rd);
    vec3 col = vec3(0.5f);
    if (hit.intersect) {
        vec3 diffuseColor = pow(uintToRGB(hit.color), vec3(2.2f));
        vec3 n = normalize(hit.normal);
        vec3 p = r0 + hit.t * rd;
        vec3 ld = normalize(uLightPos - p);

        stack_reset();
        float shadow = 1.0f;
        RayHit shadowHit = Trace(p + n * UNIT_BRICK_SIZE * 0.5f, ld);
        if (shadowHit.intersect) {
            shadow = 0.1f;
        }
        col = max(dot(n, ld), 0.0f) * diffuseColor * shadow;
        col += 0.05f;
        col = ACES(col);
        col = pow(col, vec3(0.4545));
    }

#if 0
    fragColor = vec4(col, 1.0f);
#else
    // float iter = (float(hit.iteration) / (MAX_ITERATIONS * 0.5f));
    float iter = hit.iteration / 200.0f;
    fragColor = vec4(vec3(iter), 1.0);
#endif
}