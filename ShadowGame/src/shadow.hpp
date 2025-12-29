#pragma once
#ifndef SHADOW_HPP
#define SHADOW_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <vector>

// ShadowPoly: 用于表示物体的完整阴影（凸包）
struct ShadowPoly {
    int objectId = -1;               // 稳定 ID：用 objects_ 下标即可
    std::vector<glm::vec2> hull;     // 完整阴影（凸包），用于平台/等比移动/物理
};
class Shader;

class ShadowBall final {
public:
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 vel{0.0f, 0.0f};

    float radius = 0.22f;
    float moveSpeed = 4.2f;
    float jumpSpeed = 15.0f;
    float gravity = -18.0f;

    ShadowBall();
    ~ShadowBall();

    ShadowBall(const ShadowBall&) = delete;
    ShadowBall& operator=(const ShadowBall&) = delete;

    void reset(const glm::vec2& p) {
        pos = p;
        vel = glm::vec2(0.0f);
        grounded_ = false;
        supportObjectId_ = -1;
        supportU_ = 0.5f;
    }

    void updatePhysics(GLFWwindow* window, float dt,
                       const std::vector<ShadowPoly>& platforms,
                       const glm::vec2& lightCenter,
                       float lightRadius);

    bool grounded() const { return grounded_; }
    int supportObjectId() const { return supportObjectId_; }
    float supportU() const { return supportU_; }

    void setSupportObjectId(int id) { supportObjectId_ = id; }
    void setSupportU(float u) { supportU_ = u; }
    void forceGrounded(bool g) { grounded_ = g; }
    void drop() { grounded_ = false; supportObjectId_ = -1; supportU_ = 0.0f;}

    void draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const;

private:
    bool grounded_ = false;
    int supportObjectId_ = -1;
    float supportU_ = 0.5f;

    GLuint vao_ = 0, vbo_ = 0;
    GLsizei count_ = 0;

    bool jumpPressedEdge(GLFWwindow* window) const;

    static void xRange(const std::vector<glm::vec2>& poly, float& outMinX, float& outMaxX);
    static bool topYAtX(const std::vector<glm::vec2>& poly, float x, float& outYTop);

    static bool isInsideConvexCCW(const std::vector<glm::vec2>& poly, const glm::vec2& p);
    // src/shadow.hpp (replace the private helper declaration)
    static void preventEnterSideWalls(const std::vector<glm::vec2>& poly,
                                    const glm::vec2& prevPos,
                                    glm::vec2& pos,
                                    glm::vec2& vel,
                                    float radius);

};

#endif

