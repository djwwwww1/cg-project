// ==============================
// File: LightSource.hpp
// ==============================
#pragma once

#ifndef LIGHTSOURCE_HPP
#define LIGHTSOURCE_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct LightSource final {
    glm::vec3 position{0.0f, 0.0f, 6.0f}; // z fixed
    glm::vec3 color{1.0f, 0.98f, 0.92f};

    float fovDeg = 75.0f; // spotlight cone angle
    float moveSpeed = 5.0f; // units/sec on x/y

    float planeZ = 0.0f;

    glm::vec3 directionToPlaneCenter(const glm::vec2& centerXY = glm::vec2(0.0f)) const {
        glm::vec3 center(centerXY.x, centerXY.y, planeZ);
        return glm::normalize(center - position);
    }

    glm::vec2 footprintCenter() const {
        // assume direction points towards (x,y,planeZ) by dropping z
        // so center is directly beneath the light on plane (simple first version)
        return glm::vec2(position.x, position.y);
    }

    float footprintRadius() const {
        const float h = position.z - planeZ;
        const float half = glm::radians(fovDeg * 0.5f);
        return tanf(half) * h;
    }
};

#endif // LIGHTSOURCE_HPP
