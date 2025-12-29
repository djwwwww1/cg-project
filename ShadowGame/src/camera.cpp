#include "camera.hpp"
float Camera::expSmoothingAlpha(float lag, float dt) {
    // alpha = 1 - exp(-lag*dt)
    return 1.0f - std::exp(-lag * dt);
}

void Camera::updateFollow(const glm::vec2& ballXY, float dt) {
    const glm::vec3 desired(ballXY.x + followOffset.x, ballXY.y + followOffset.y, followOffset.z);
    const float a = expSmoothingAlpha(followLag, dt);
    position = position + a * (desired - position);

    target = glm::vec3(ballXY.x, ballXY.y, 0.0f);
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position, target, glm::vec3(0, 1, 0));
}

glm::mat4 Camera::proj() const {
    return glm::perspective(glm::radians(fovDeg), aspect, nearZ, farZ);
}