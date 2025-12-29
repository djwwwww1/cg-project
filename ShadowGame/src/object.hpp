// #pragma once

// #ifndef OBJECT_HPP
// #define OBJECT_HPP

// #include <GL/glew.h>
// #include <glm/glm.hpp>
// #include <glm/gtc/type_ptr.hpp>

// #include <array>
// #include <fstream>
// #include <sstream>
// #include <stdexcept>
// #include <string>
// #include <vector>

// class Shader final {
// public:
//     GLuint program = 0;

//     Shader() = default;

//     Shader(const std::string& vsPath, const std::string& fsPath) {
//         loadFromFiles(vsPath, fsPath);
//     }

//     ~Shader() {
//         if (program) glDeleteProgram(program);
//     }

//     Shader(const Shader&) = delete;
//     Shader& operator=(const Shader&) = delete;

//     Shader(Shader&& other) noexcept {
//         program = other.program;
//         other.program = 0;
//     }
//     Shader& operator=(Shader&& other) noexcept {
//         if (this != &other) {
//             if (program) glDeleteProgram(program);
//             program = other.program;
//             other.program = 0;
//         }
//         return *this;
//     }

//     void loadFromFiles(const std::string& vsPath, const std::string& fsPath) {
//         const std::string vs = readTextFile(vsPath);
//         const std::string fs = readTextFile(fsPath);

//         GLuint vsId = compile(GL_VERTEX_SHADER, vs.c_str(), vsPath);
//         GLuint fsId = compile(GL_FRAGMENT_SHADER, fs.c_str(), fsPath);

//         GLuint prog = glCreateProgram();
//         glAttachShader(prog, vsId);
//         glAttachShader(prog, fsId);
//         glLinkProgram(prog);

//         GLint ok = 0;
//         glGetProgramiv(prog, GL_LINK_STATUS, &ok);
//         if (!ok) {
//             GLint len = 0;
//             glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
//             std::string log(len, '\0');
//             glGetProgramInfoLog(prog, len, nullptr, log.data());
//             glDeleteShader(vsId);
//             glDeleteShader(fsId);
//             glDeleteProgram(prog);
//             throw std::runtime_error("Shader link failed:\n" + log);
//         }

//         glDeleteShader(vsId);
//         glDeleteShader(fsId);

//         if (program) glDeleteProgram(program);
//         program = prog;
//     }

//     void use() const { glUseProgram(program); }

//     GLint loc(const char* name) const { return glGetUniformLocation(program, name); }

//     void setMat4(const char* name, const glm::mat4& m) const {
//         glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
//     }

//     void setVec3(const char* name, const glm::vec3& v) const {
//         glUniform3fv(loc(name), 1, glm::value_ptr(v));
//     }

//     void setVec2(const char* name, const glm::vec2& v) const {
//         glUniform2fv(loc(name), 1, glm::value_ptr(v));
//     }

//     void setFloat(const char* name, float v) const { glUniform1f(loc(name), v); }

//     void setInt(const char* name, int v) const { glUniform1i(loc(name), v); }

// private:
//     static std::string readTextFile(const std::string& path) {
//         std::ifstream in(path);
//         if (!in) throw std::runtime_error("Failed to open file: " + path);
//         std::stringstream ss;
//         ss << in.rdbuf();
//         return ss.str();
//     }

//     static GLuint compile(GLenum type, const char* src, const std::string& tag) {
//         GLuint id = glCreateShader(type);
//         glShaderSource(id, 1, &src, nullptr);
//         glCompileShader(id);

//         GLint ok = 0;
//         glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
//         if (!ok) {
//             GLint len = 0;
//             glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
//             std::string log(len, '\0');
//             glGetShaderInfoLog(id, len, nullptr, log.data());
//             glDeleteShader(id);
//             throw std::runtime_error("Shader compile failed (" + tag + "):\n" + log);
//         }
//         return id;
//     }
// };

// struct VertexPN {
//     glm::vec3 pos;
//     glm::vec3 nrm;
// };

// class BoxObject final {
// public:
//     glm::vec3 position{0.0f};
//     glm::vec3 scale{1.0f};
//     glm::vec3 color{0.8f, 0.8f, 0.85f};

//     BoxObject() { ensureCubeMesh(); }

//     BoxObject(const glm::vec3& p, const glm::vec3& s, const glm::vec3& c)
//         : position(p), scale(s), color(c) {
//         ensureCubeMesh();
//     }

//     glm::mat4 model() const;

//     void draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const;

//     // returns 8 corners in world space
//     std::array<glm::vec3, 8> worldCorners() const;

// private:
//     static inline bool s_inited = false;
//     static inline GLuint s_vao = 0;
//     static inline GLuint s_vbo = 0;
//     static inline GLsizei s_count = 0;

//     static void ensureCubeMesh();
// };

// #endif // OBJECT_HPP

#pragma once
#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Shader final {
public:
    GLuint program = 0;

    Shader() = default;
    Shader(const std::string& vsPath, const std::string& fsPath) { loadFromFiles(vsPath, fsPath); }

    ~Shader() { if (program) glDeleteProgram(program); }

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept { program = other.program; other.program = 0; }
    Shader& operator=(Shader&& other) noexcept {
        if (this != &other) {
            if (program) glDeleteProgram(program);
            program = other.program;
            other.program = 0;
        }
        return *this;
    }

    void loadFromFiles(const std::string& vsPath, const std::string& fsPath) {
        const std::string vs = readTextFile(vsPath);
        const std::string fs = readTextFile(fsPath);

        GLuint vsId = compile(GL_VERTEX_SHADER, vs.c_str(), vsPath);
        GLuint fsId = compile(GL_FRAGMENT_SHADER, fs.c_str(), fsPath);

        GLuint prog = glCreateProgram();
        glAttachShader(prog, vsId);
        glAttachShader(prog, fsId);
        glLinkProgram(prog);

        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetProgramInfoLog(prog, len, nullptr, log.data());
            glDeleteShader(vsId);
            glDeleteShader(fsId);
            glDeleteProgram(prog);
            throw std::runtime_error("Shader link failed:\n" + log);
        }

        glDeleteShader(vsId);
        glDeleteShader(fsId);

        if (program) glDeleteProgram(program);
        program = prog;
    }

    void use() const { glUseProgram(program); }
    GLint loc(const char* name) const { return glGetUniformLocation(program, name); }

    void setMat4(const char* name, const glm::mat4& m) const {
        glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
    }
    void setVec4(const char* name, const glm::vec4& v) const {
        glUniform4fv(loc(name), 1, glm::value_ptr(v));
    }
    void setVec3(const char* name, const glm::vec3& v) const {
        glUniform3fv(loc(name), 1, glm::value_ptr(v));
    }
    void setVec2(const char* name, const glm::vec2& v) const {
        glUniform2fv(loc(name), 1, glm::value_ptr(v));
    }
    void setFloat(const char* name, float v) const { glUniform1f(loc(name), v); }
    void setInt(const char* name, int v) const { glUniform1i(loc(name), v); }

private:
    static std::string readTextFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) throw std::runtime_error("Failed to open file: " + path);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static GLuint compile(GLenum type, const char* src, const std::string& tag) {
        GLuint id = glCreateShader(type);
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);

        GLint ok = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetShaderInfoLog(id, len, nullptr, log.data());
            glDeleteShader(id);
            throw std::runtime_error("Shader compile failed (" + tag + "):\n" + log);
        }
        return id;
    }
};

struct VertexPN {
    glm::vec3 pos;
    glm::vec3 nrm;
};

class BoxObject final {
public:
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 color{0.8f, 0.8f, 0.85f};

    BoxObject();
    BoxObject(const glm::vec3& p, const glm::vec3& s, const glm::vec3& c);

    glm::mat4 model() const;
    void draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const;
    std::array<glm::vec3, 8> worldCorners() const;

private:
    static inline bool s_inited = false;
    static inline GLuint s_vao = 0;
    static inline GLuint s_vbo = 0;
    static inline GLsizei s_count = 0;
    static void ensureCubeMesh();
};

#endif
