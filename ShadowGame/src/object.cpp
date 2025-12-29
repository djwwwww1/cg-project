// File: src/object.cpp
#include "object.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

BoxObject::BoxObject() {
    ensureCubeMesh();
}

BoxObject::BoxObject(const glm::vec3& p, const glm::vec3& s, const glm::vec3& c)
    : position(p), scale(s), color(c) {
    ensureCubeMesh();
}

glm::mat4 BoxObject::model() const {
    glm::mat4 m(1.0f);
    m = glm::translate(m, position);
    m = glm::scale(m, scale);
    return m;
}

void BoxObject::draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const {
    shader.use();
    shader.setMat4("uModel", model());
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);
    shader.setVec3("uColor", color);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, s_count);
    glBindVertexArray(0);
}

std::array<glm::vec3, 8> BoxObject::worldCorners() const {
    const glm::mat4 m = model();
    const std::array<glm::vec3, 8> local = {
        glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3( 0.5f, -0.5f, -0.5f),
        glm::vec3( 0.5f,  0.5f, -0.5f),
        glm::vec3(-0.5f,  0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f,  0.5f),
        glm::vec3( 0.5f, -0.5f,  0.5f),
        glm::vec3( 0.5f,  0.5f,  0.5f),
        glm::vec3(-0.5f,  0.5f,  0.5f),
    };

    std::array<glm::vec3, 8> out{};
    for (int i = 0; i < 8; ++i) {
        const glm::vec4 wp = m * glm::vec4(local[i], 1.0f);
        out[i] = glm::vec3(wp);
    }
    return out;
}

void BoxObject::ensureCubeMesh() {
    if (s_inited) return;
    s_inited = true;

    const std::vector<VertexPN> verts = {
        // +Z
        {{-0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f, 0.5f, 0.5f},{0,0,1}},
        {{-0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f, 0.5f, 0.5f},{0,0,1}}, {{-0.5f, 0.5f, 0.5f},{0,0,1}},
        // -Z
        {{ 0.5f,-0.5f,-0.5f},{0,0,-1}}, {{-0.5f,-0.5f,-0.5f},{0,0,-1}}, {{-0.5f, 0.5f,-0.5f},{0,0,-1}},
        {{ 0.5f,-0.5f,-0.5f},{0,0,-1}}, {{-0.5f, 0.5f,-0.5f},{0,0,-1}}, {{ 0.5f, 0.5f,-0.5f},{0,0,-1}},
        // +X
        {{ 0.5f,-0.5f, 0.5f},{1,0,0}}, {{ 0.5f,-0.5f,-0.5f},{1,0,0}}, {{ 0.5f, 0.5f,-0.5f},{1,0,0}},
        {{ 0.5f,-0.5f, 0.5f},{1,0,0}}, {{ 0.5f, 0.5f,-0.5f},{1,0,0}}, {{ 0.5f, 0.5f, 0.5f},{1,0,0}},
        // -X
        {{-0.5f,-0.5f,-0.5f},{-1,0,0}}, {{-0.5f,-0.5f, 0.5f},{-1,0,0}}, {{-0.5f, 0.5f, 0.5f},{-1,0,0}},
        {{-0.5f,-0.5f,-0.5f},{-1,0,0}}, {{-0.5f, 0.5f, 0.5f},{-1,0,0}}, {{-0.5f, 0.5f,-0.5f},{-1,0,0}},
        // +Y
        {{-0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f,-0.5f},{0,1,0}},
        {{-0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f,-0.5f},{0,1,0}}, {{-0.5f, 0.5f,-0.5f},{0,1,0}},
        // -Y
        {{-0.5f,-0.5f,-0.5f},{0,-1,0}}, {{ 0.5f,-0.5f,-0.5f},{0,-1,0}}, {{ 0.5f,-0.5f, 0.5f},{0,-1,0}},
        {{-0.5f,-0.5f,-0.5f},{0,-1,0}}, {{ 0.5f,-0.5f, 0.5f},{0,-1,0}}, {{-0.5f,-0.5f, 0.5f},{0,-1,0}},
    };

    s_count = static_cast<GLsizei>(verts.size());

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VertexPN), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN), (void*)offsetof(VertexPN, pos));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN), (void*)offsetof(VertexPN, nrm));

    glBindVertexArray(0);
}
