// ============================================================================
// File: src/people.cpp
// ============================================================================
#include "people.hpp"
#include <algorithm>

void FlashlightOperator::update(GLFWwindow* window, float dt) {
    if (!light_) return;

    glm::vec2 delta(0.0f);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  delta.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) delta.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  delta.y -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    delta.y += 1.0f;

    if (glm::length(delta) > 0.0f) {
        delta = glm::normalize(delta);
        const glm::vec2 applied = delta * light_->moveSpeed * dt;
        light_->position.x += applied.x;
        light_->position.y += applied.y;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        light_->fovDeg = std::max(10.0f, light_->fovDeg - 40.0f * dt);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        light_->fovDeg = std::min(80.0f, light_->fovDeg + 40.0f * dt);
}