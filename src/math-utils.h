#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <array>

constexpr const float EPSILON = 10e-7f;

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 CalculateCenter() const {
        return (min + max) * 0.5f;
    }

    glm::vec3 CalculateSize() const {
        return (max - min);
    }

    bool ContainPoint(const glm::vec3 &p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }
};

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;

    Ray() = default;
    Ray(const glm::vec3 &origin, const glm::vec3 &direction) : origin(origin), direction(direction) {}

    glm::vec3 GetPointAt(float t) const {
        return origin + t * direction;
    }
};

inline float Reflect(float p, float c) {
    return 2.0f * c - p;
}

template <typename T>
T Remap(T p, T a0, T a1, T b0, T b1) {
    T t = (p - a0) / (a1 - a0);
    return b0 + t * (b1 - b0);
}

template <typename T>
bool IsBitSet(T data, int index) {
    T mask = 1 << index;
    return (data & mask) == mask;
}

inline bool IntersectAABB(const AABB &a, const AABB &b) {
    return a.min.x <= b.max.x &&
           a.max.x >= b.min.x &&
           a.min.y <= b.max.x &&
           a.max.y >= b.min.y &&
           a.min.z <= b.max.z &&
           a.max.z >= b.min.z;
}

inline glm::vec2 ConvertFromWindowToNDC(const glm::vec2 &mousePos, const glm::vec2 &windowSize) {
    glm::vec2 ndcCoord = mousePos / windowSize;
    ndcCoord.x = ndcCoord.x * 2.0f - 1.0f;
    ndcCoord.y = 1.0f - 2.0f * ndcCoord.y;
    return ndcCoord;
}

inline bool AABBFrustumIntersection(glm::vec3 aabbMin, glm::vec3 aabbMax, std::array<glm::vec4, 6> &frustumPlanes) {
    glm::vec3 p = glm::vec3(0.0f);
    for (int i = 0; i < 6; ++i) {
        p = aabbMin;
        if (frustumPlanes[i].x > 0)
            p.x = aabbMax.x;
        if (frustumPlanes[i].y > 0)
            p.y = aabbMax.y;
        if (frustumPlanes[i].z > 0)
            p.z = aabbMax.z;

        glm::vec3 n = glm::vec3(frustumPlanes[i]);
        if (glm::dot(n, p) + frustumPlanes[i].w < 0.0f)
            return false;
    }
    return true;
}