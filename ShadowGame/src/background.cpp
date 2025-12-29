#include "background.hpp"

#include <glm/gtc/matrix_transform.hpp>

BackgroundPlane::BackgroundPlane() {
    // A simple quad on the wall plane (z=0). Coordinates are in world-space.
    // You project everything onto z=0 already, so this matches.
    const float z = 0.0f;

    // 2 triangles, positions only
    const float verts[] = {
        -40.0f,  0.0f,  z,
         40.0f,  0.0f,  z,
         40.0f, 40.0f,  z,

        -40.0f,  0.0f,  z,
         40.0f, 40.0f,  z,
        -40.0f, 40.0f,  z
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

BackgroundPlane::~BackgroundPlane() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void BackgroundPlane::draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                           const glm::vec2& lightCenter, float lightRadius, float softness,
                           float ambient) const {
    shader.use();

    glm::mat4 m(1.0f);
    shader.setMat4("uModel", m);
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);

    // ✅ 关键：每次画墙/地面都强制走 base 分支
    shader.setInt("uMode", 1);

    shader.setVec3("uColor", color);

    // ✅ floor 那种 lightRadius=0 的情况不要参与光圈计算，避免 smoothstep(edge0=edge1) 未定义
    const int useLighting = (lightRadius > 1e-4f && softness > 1e-4f) ? 1 : 0;
    shader.setInt("uUseLighting", useLighting);

    shader.setVec2("uLightCenter", lightCenter);
    shader.setFloat("uLightRadius", lightRadius);
    shader.setFloat("uSoftness", softness);
    shader.setFloat("uAmbient", ambient);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}


void BackgroundPlane::drawShadowCompositeFromMask(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                                                 GLuint maskTex, const glm::ivec2& resolution,
                                                 const glm::vec4& shadowColor) const {
    shader.use();

    shader.setMat4("uModel", glm::mat4(1.0f));
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);

    shader.setInt("uMode", 6);
    shader.setVec2("uResolution", glm::vec2((float)resolution.x, (float)resolution.y));
    shader.setVec4("uColor4", shadowColor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, maskTex);
    shader.setInt("uShadowMask", 0);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

BackgroundPlanes::BackgroundPlanes() {
    // warm wood-ish base
    wall_.color  = glm::vec3(0.64f, 0.48f, 0.30f);
    floor_.color = glm::vec3(0.56f, 0.41f, 0.26f);
}

BackgroundPlanes::~BackgroundPlanes() = default;

void BackgroundPlanes::drawWallLit(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                                   const glm::vec2& lightCenter, float lightRadius, float softness,
                                   float ambient) const {
    wall_.draw(shader, view, proj, lightCenter, lightRadius, softness, ambient);
}

void BackgroundPlanes::drawFloorFlat(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const {
    // floor: reuse same plane for now (you can later make a real floor at y=0 in 3D)
    // Here just draw nothing or keep as-is if you already have a separate floor plane.
    floor_.draw(shader, view, proj, glm::vec2(0.0f), 0.0f, 0.0f, 1.0f);
}

void BackgroundPlanes::drawWallShadowCompositeFromMask(const Shader& shader, const glm::mat4& view, const glm::mat4& proj,
                                                      GLuint maskTex, const glm::ivec2& resolution,
                                                      const glm::vec4& shadowColor) const {
    wall_.drawShadowCompositeFromMask(shader, view, proj, maskTex, resolution, shadowColor);
}
