// ==============================
// File: camera.hpp
// ==============================
#pragma once

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera final {
public:
    float fovDeg = 55.0f;
    float aspect = 16.0f / 9.0f;
    float nearZ = 0.05f;
    float farZ = 200.0f;

    float followLag = 10.0f; // bigger => less lag
    glm::vec3 position{0.0f, 6.0f, 12.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};

    glm::vec3 followOffset{0.0f, 4.5f, 12.0f};

    void updateFollow(const glm::vec2& ballXY, float dt);

    glm::mat4 view() const;
    glm::mat4 proj() const;

private:
    static float expSmoothingAlpha(float lag, float dt);
};

#endif // CAMERA_HPP
