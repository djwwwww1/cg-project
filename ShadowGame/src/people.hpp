// ============================================================================
// File: src/people.hpp
// ============================================================================
#pragma once
#ifndef PEOPLE_HPP
#define PEOPLE_HPP

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "LightSource.hpp"

class FlashlightOperator final {
public:
    explicit FlashlightOperator(LightSource* light) : light_(light) {}
    void update(GLFWwindow* window, float dt);

private:
    LightSource* light_ = nullptr;
};

#endif