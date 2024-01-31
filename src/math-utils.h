#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

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
