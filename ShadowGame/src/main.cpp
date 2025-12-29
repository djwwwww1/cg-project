// ==============================
// File: main.cpp
// ==============================
#include <iostream>
#include <stdexcept>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "scene.hpp"

static void glfwErrorCallback(int code, const char* desc) {
    std::cerr << "[GLFW] Error " << code << ": " << (desc ? desc : "") << "\n";
}

static void framebufferSizeCallback(GLFWwindow* window, int w, int h) {
    glViewport(0, 0, w, h);
    auto* scene = reinterpret_cast<Scene*>(glfwGetWindowUserPointer(window));
    if (scene) scene->onResize(w, h);
}

int main() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int W = 1280;
    const int H = 720;

    GLFWwindow* window = glfwCreateWindow(W, H, "Shadow Game", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW init failed.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    try {
        Scene scene(W, H);
        glfwSetWindowUserPointer(window, &scene);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

        double last = glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            const float dt = static_cast<float>(now - last);
            last = now;

            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            scene.update(window, dt);
            scene.render();

            glfwSwapBuffers(window);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}