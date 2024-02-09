#version 460

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(binding = 0) readonly buffer NodeBuffer { uint nodePools[]; };

uniform vec3 uCamPos;
uniform mat4 uInvP;
uniform mat4 uInvV;
uniform vec3 uAABBMin;
uniform vec3 uAABBMax;

#define EPSILON 10e-7f
const vec3 ld = normalize(vec3(0.01, 1.0, -0.5));
#define MAX_ITERATIONS 1000

const int MAX_LEVELS = 10;
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

#define REFLECT(p, c) 2.0f * c - p
#define GET_MASK(p) p >> 30
#define GET_FIRST_CHILD(p) p & 0x3fffffff
#define InternalLeafNode 0
#define InternalNode 1
#define LeafNode 2
#define LeafNodeWithPtr 3

int FindEntryNode(inout vec3 p, float scale, float t, vec3 tM) {
    ivec3 mask = ivec3(step(vec3(t), tM));
    p -= mask * scale;
    return mask.x | (mask.y << 1) | (mask.z << 2);
}

bool Trace(vec3 r0, vec3 rd, out vec3 intersection, out vec3 normal) {
    vec3 r0_orig = r0;
    int octaneMask = 7;
    vec3 aabbMin = uAABBMin;
    vec3 aabbMax = uAABBMax;
    vec3 center = (aabbMin + aabbMax) * 0.5f;

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

    vec2 t = vec2(max(tMin.x, max(tMin.y, tMin.z)), min(tMax.x, min(tMax.y, tMax.z)));
    if (t.x > t.y || t.y < 0.0)
        return false;

    float currentSize = (uAABBMax.x - uAABBMin.x) * 0.5f;
    float octreeSize = currentSize;

    // Determine Entry Node
    vec3 p = aabbMax;
    vec3 tM = (tMin + tMax) * 0.5f;

    bool processChild = true;
    int idx = FindEntryNode(p, currentSize, t.x, tM);

    uint firstSibling = GET_FIRST_CHILD(nodePools[0]);
    vec3 col = vec3(0.5, 0.7, 1.0);
    vec3 n = vec3(0.0);
    int i = 0;

    bool hasIntersect = false;
    for (; i < 1000; ++i) {
        uint nodeIndex = firstSibling + (idx ^ octaneMask);
        uint nodeDescriptor = nodePools[nodeIndex];
        vec3 tCorner = p * invRayDir - r0_rd;
        float tcMax = min(min(tCorner.x, tCorner.y), tCorner.z);

        if (processChild) {
            uint mask = GET_MASK(nodeDescriptor);
            if (mask == LeafNode) {
                intersection = r0_orig + t.x * rd;
                hasIntersect = true;
                break;
            } else if (mask == LeafNodeWithPtr) {
                intersection = r0_orig + t.x * rd;
                // BrickMarch
                // if intersect break
                hasIntersect = true;
                break;
            }

            if (mask == InternalNode) {

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
    return hasIntersect;
}

vec3 hash33(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));

    return fract(sin(p) * 43758.5453123);
}

void main() {
    vec3 r0 = uCamPos;
    vec3 rd = GenerateCameraRay(uv * 2.0 - 1.0);

    vec3 n = vec3(0.0f);
    vec3 p = vec3(0.0f);
    vec3 col = vec3(0.0f);
    if (Trace(r0, rd, p, n)) {
        /*
        vec3 _unused, _unused2;
        stack_reset();
        float shadow = Trace(p + n, ld, _unused, _unused2) ? 0.0f : 1.0f;
        col = max(dot(n, ld), 0.0) * vec3(1.28, 1.20, 0.99) * shadow;
        col += (n.y * 0.5 + 0.5) * vec3(0.16, 0.20, 0.28);
        // col *= (n * 0.5 + 0.5);
        col *= hash33(floor(p));
        col = pow(col, vec3(0.4545));
        col /= (1.0 + col);
        // col = vec3(shadow);
        */
        col = vec3(1.0);
    }

#if 1
    fragColor = vec4(col, 1.0f);
#else
    float iteration = (float(i) / MAX_ITERATIONS);
    fragColor = vec4(iteration, iteration, iteration, 1.0);
#endif
}