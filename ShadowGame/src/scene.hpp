#pragma once
#ifndef SCENE_HPP
#define SCENE_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>

#include "background.hpp"
#include "camera.hpp"
#include "LightSource.hpp"
#include "object.hpp"
#include "people.hpp"
#include "shadow.hpp"

namespace collision {
// one-way platform resolve：只提供顶面支撑，返回 groundObjectId
glm::vec2 resolveCircleAgainstPlatforms(glm::vec2 pos, float radius,
                                       const std::vector<ShadowPoly>& platforms,
                                       glm::vec2& vel, bool& grounded, int& groundObjectId);
} // namespace collision

class Scene final {
public:
    Scene(int w, int h);
    ~Scene();
    void onResize(int w, int h);
    void update(GLFWwindow* window, float dt);
    void render();

private:
    int width_ = 1280, height_ = 720;

    Shader objectShader_;
    Shader backgroundShader_;

    Camera camera_;
    LightSource light_;
    FlashlightOperator op_;

    BackgroundPlanes planes_;
    ShadowBall ball_;

    std::vector<BoxObject> objects_;

    // 完整阴影平台（稳定绑定）
    std::vector<ShadowPoly> shadowPlatforms_;

    // shadow mesh (render hulls)
    GLuint shadowVao_ = 0, shadowVbo_ = 0;
    GLsizei shadowVertCount_ = 0;

    glm::vec2 spawnBall_{-10.0f, 7.0f};
    glm::vec3 spawnLight_{-6.0f, 10.0f, 12.0f};
    // shadow mask FBO (avoid darker overlap)
    GLuint shadowMaskFbo_ = 0;
    GLuint shadowMaskTex_ = 0;

    void recreateShadowMaskResources();
    void destroyShadowMaskResources();

    void resetLevel();
    void initSceneObjects();

    void rebuildShadowPlatforms();
    void uploadShadowMeshFromHulls();

    // geom helpers
    static std::vector<glm::vec2> convexHull(std::vector<glm::vec2> pts);
    static void xRange(const std::vector<glm::vec2>& poly, float& outMinX, float& outMaxX);
    static bool topYAtX(const std::vector<glm::vec2>& poly, float x, float& outYTop);

    // sticky support logic
    void dropBallIfOutOfLight();
    void stickBallToSupportAfterLightMove();
    const ShadowPoly* findPlatformByObjectId(int objectId) const;
};

#endif // SCENE_HPP