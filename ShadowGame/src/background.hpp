#pragma once
#ifndef BACKGROUND_HPP
#define BACKGROUND_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "object.hpp" // Shader

class BackgroundPlane final {
public:
    BackgroundPlane();
    ~BackgroundPlane();

    BackgroundPlane(const BackgroundPlane&) = delete;
    BackgroundPlane& operator=(const BackgroundPlane&) = delete;

    void draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec2& lightCenter, float lightRadius, float softness,
              float ambient) const;

    void drawShadowCompositeFromMask(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                                     GLuint maskTex, const glm::ivec2& resolution,
                                     const glm::vec4& shadowColor) const;

    glm::vec3 color{0.70f, 0.55f, 0.38f};

private:
    GLuint vao_ = 0, vbo_ = 0;
};

class BackgroundPlanes final {
public:
    BackgroundPlanes();
    ~BackgroundPlanes();

    float deathY() const { return 0.0f; }

    void drawWallLit(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                     const glm::vec2& lightCenter, float lightRadius, float softness,
                     float ambient) const;

    void drawFloorFlat(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const;

    void drawWallShadowCompositeFromMask(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                                         GLuint maskTex, const glm::ivec2& resolution,
                                         const glm::vec4& shadowColor) const;

private:
    BackgroundPlane wall_;
    BackgroundPlane floor_;
};

#endif
