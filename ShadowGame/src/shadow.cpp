// ============================================================================
// File: src/shadow.cpp
// Fixes:
//  1) walk-off edge can't fall -> use radius-aware overlap on support
//  2) side can be jumped-through -> circle vs segment (capsule) penetration
//  3) "air walls" on edge extension -> closest-point-on-segment only, outside->penetrate only
//  4) stuck-in-air near corners -> remove "bottom edge" as wall; only side + ceiling
// ============================================================================

#include "shadow.hpp"
#include "object.hpp" // Shader lives here in your project

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

static float dot2(const glm::vec2& a, const glm::vec2& b) { return a.x * b.x + a.y * b.y; }
static float len2(const glm::vec2& v) { return dot2(v, v); }

static glm::vec2 closestPointOnSegment(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 ab = b - a;
    const float ab2 = dot2(ab, ab);
    if (ab2 < 1e-10f) return a;
    float t = dot2(p - a, ab) / ab2;
    t = std::clamp(t, 0.0f, 1.0f);
    return a + t * ab;
}

// CCW convex: inside if for all edges, point is on left side (cross >= 0)
bool ShadowBall::isInsideConvexCCW(const std::vector<glm::vec2>& poly, const glm::vec2& p) {
    if (poly.size() < 3) return false;
    for (size_t i = 0; i < poly.size(); ++i) {
        const glm::vec2 a = poly[i];
        const glm::vec2 b = poly[(i + 1) % poly.size()];
        const glm::vec2 ab = b - a;
        const glm::vec2 ap = p - a;
        const float cross = ab.x * ap.y - ab.y * ap.x;
        if (cross < -1e-6f) return false;
    }
    return true;
}

ShadowBall::ShadowBall() {
    const int segments = 48;
    std::vector<glm::vec3> verts;
    verts.reserve(segments + 2);

    verts.push_back(glm::vec3(0.0f, 0.0f, 0.03f));
    for (int i = 0; i <= segments; ++i) {
        const float a = (float)i / (float)segments * 2.0f * 3.1415926f;
        verts.push_back(glm::vec3(std::cos(a), std::sin(a), 0.03f));
    }
    count_ = (GLsizei)verts.size();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(glm::vec3)), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

ShadowBall::~ShadowBall() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool ShadowBall::jumpPressedEdge(GLFWwindow* window) const {
    static bool last = false;
    const bool now = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) ||
                     (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    const bool edge = now && !last;
    last = now;
    return edge;
}

void ShadowBall::xRange(const std::vector<glm::vec2>& poly, float& outMinX, float& outMaxX) {
    outMinX = std::numeric_limits<float>::infinity();
    outMaxX = -std::numeric_limits<float>::infinity();
    for (const auto& v : poly) {
        outMinX = std::min(outMinX, v.x);
        outMaxX = std::max(outMaxX, v.x);
    }
}

bool ShadowBall::topYAtX(const std::vector<glm::vec2>& poly, float x, float& outYTop) {
    float best = -std::numeric_limits<float>::infinity();
    bool any = false;

    for (size_t i = 0; i < poly.size(); ++i) {
        const glm::vec2 a = poly[i];
        const glm::vec2 b = poly[(i + 1) % poly.size()];

        const float minx = std::min(a.x, b.x);
        const float maxx = std::max(a.x, b.x);
        if (x < minx - 1e-5f || x > maxx + 1e-5f) continue;

        const float dx = b.x - a.x;
        if (std::fabs(dx) < 1e-6f) {
            if (std::fabs(a.x - x) < 1e-4f) {
                best = std::max(best, std::max(a.y, b.y));
                any = true;
            }
            continue;
        }

        const float t = (x - a.x) / dx;
        if (t < -1e-4f || t > 1.0f + 1e-4f) continue;

        const float y = a.y + t * (b.y - a.y);
        best = std::max(best, y);
        any = true;
    }

    outYTop = best;
    return any;
}

// ---------------------------------------------------------------------------
// Collision: prevent entering shadow polygon through side / ceiling
// Key points:
//  - do NOT use "center inside polygon" as trigger (misses when circle crosses)
//  - use circle vs segment (capsule) penetration, and ONLY handle outside->penetrate
//  - ONLY handle:
//      * side edges: |normal.x| large
//      * ceiling edges: normal.y < 0 (typically top edge outward normal points downward in CCW)
//    (we intentionally do NOT treat bottom edges as walls -> removes "air wall" on lower boundary)
// ---------------------------------------------------------------------------
void ShadowBall::preventEnterSideWalls(const std::vector<glm::vec2>& poly,
                                       const glm::vec2& prevPos,
                                       glm::vec2& pos,
                                       glm::vec2& vel,
                                       float r) {
    if (poly.size() < 3) return;

    const float skin = 1e-3f;

    // Only handle "outside -> penetrated"
    // A robust gate: previous frame circle not intersecting any relevant edge, now intersecting.
    auto edgeCirclePenetration = [&](const glm::vec2& center,
                                    const glm::vec2& a,
                                    const glm::vec2& b,
                                    glm::vec2& outN,
                                    float& outPen) -> bool {
        const glm::vec2 e = b - a;
        if (len2(e) < 1e-10f) return false;

        // CCW outward normal (right normal)
        glm::vec2 n(e.y, -e.x);
        const float n2 = len2(n);
        if (n2 < 1e-10f) return false;
        n *= 1.0f / std::sqrt(n2);

        const bool isSide = (std::fabs(n.x) > 0.6f);
        const bool isCeil = (n.y < -0.6f); // outward points downward
        if (!isSide && !isCeil) return false;

        // Don't block ceiling unless moving upward (prevents weird "air ceilings" when falling nearby)
        if (isCeil && vel.y <= 0.0f) return false;

        const glm::vec2 c = closestPointOnSegment(center, a, b);
        const glm::vec2 d = center - c;
        const float d2 = len2(d);
        if (d2 >= r * r) return false;

        const float dist = std::sqrt(std::max(d2, 1e-12f));
        // normal points from boundary -> center
        outN = (dist > 1e-6f) ? (d * (1.0f / dist)) : n;
        outPen = (r - dist);
        return outPen > 0.0f;
    };

    auto penetratesAnyRelevantEdge = [&](const glm::vec2& center) -> bool {
        for (size_t i = 0; i < poly.size(); ++i) {
            const glm::vec2 a = poly[i];
            const glm::vec2 b = poly[(i + 1) % poly.size()];
            glm::vec2 n;
            float pen = 0.0f;
            if (edgeCirclePenetration(center, a, b, n, pen)) return true;
        }
        return false;
    };

    const bool prevPen = penetratesAnyRelevantEdge(prevPos);
    const bool nowPen  = penetratesAnyRelevantEdge(pos);
    if (prevPen || !nowPen) return;

    // Resolve with a few iterations to handle corners
    for (int iter = 0; iter < 4; ++iter) {
        float bestPen = 0.0f;
        glm::vec2 bestN(0.0f);

        for (size_t i = 0; i < poly.size(); ++i) {
            const glm::vec2 a = poly[i];
            const glm::vec2 b = poly[(i + 1) % poly.size()];
            glm::vec2 n;
            float pen = 0.0f;
            if (!edgeCirclePenetration(pos, a, b, n, pen)) continue;

            if (pen > bestPen) {
                bestPen = pen;
                bestN = n;
            }
        }

        if (bestPen <= 0.0f) break;

        // push out
        pos += bestN * (bestPen + skin);

        // remove velocity component into obstacle
        const float vn = dot2(vel, bestN);
        if (vn < 0.0f) vel -= bestN * vn;

        if (!penetratesAnyRelevantEdge(pos)) break;
    }
}

void ShadowBall::updatePhysics(GLFWwindow* window, float dt,
                               const std::vector<ShadowPoly>& platforms,
                               const glm::vec2& lightCenter,
                               float lightRadius) {
    float dir = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dir -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dir += 1.0f;
    vel.x = dir * moveSpeed;

    vel.y += gravity * dt;

    if (grounded_ && jumpPressedEdge(window)) {
        vel.y = jumpSpeed;
        grounded_ = false;
        supportObjectId_ = -1;
    }

    const glm::vec2 prevPos = pos;
    pos += vel * dt;

    // If ball is outside light (circle test), it should fall: drop + return
    {
        const float d = glm::distance(pos, lightCenter);
        const float eps = 1e-3f;
        if (d + radius > lightRadius - eps) {
            drop();
            return; // important: do not "land" or "wall-collide" outside light
        }
    }

    // -----------------------------------------------------------------------
    // Grounded support check: allow staying only if circle still overlaps x-range
    // and has a valid top, otherwise drop (walk-off edge works).
    // -----------------------------------------------------------------------
    if (grounded_ && supportObjectId_ >= 0) {
        const ShadowPoly* sp = nullptr;
        for (const auto& p : platforms) {
            if (p.objectId == supportObjectId_) { sp = &p; break; }
        }
        if (!sp || sp->hull.size() < 3) {
            drop();
            return;
        }

        float minX, maxX;
        xRange(sp->hull, minX, maxX);

        // ✅ overlap test (NOT center test)
        if (pos.x + radius < minX || pos.x - radius > maxX) {
            drop();
            return;
        }

        // stable query near edges
        const float xQuery = std::clamp(pos.x, minX, maxX);

        float yTop = 0.0f;
        if (!topYAtX(sp->hull, xQuery, yTop)) {
            drop();
            return;
        }

        const float targetY = yTop + radius;
        pos.y = targetY;     // must stick to ground (platform can move down)
        vel.y = 0.0f;
    }

    // -----------------------------------------------------------------------
    // Air: prevent entering shadow through side / ceiling
    // (do not run while grounded, otherwise it blocks walk-off)
    // -----------------------------------------------------------------------
    if (!grounded_) {
        for (const auto& sp : platforms) {
            preventEnterSideWalls(sp.hull, prevPos, pos, vel, radius);
        }
    }

    // -----------------------------------------------------------------------
    // Landing (one-way): falling only, choose highest platform under x-overlap
    // -----------------------------------------------------------------------
    if (!grounded_ && vel.y <= 0.0f) {
        const float prevBottom = prevPos.y - radius;
        const float newBottom  = pos.y - radius;

        int bestObj = -1;
        float bestYTop = -std::numeric_limits<float>::infinity();

        for (const auto& sp : platforms) {
            if (sp.hull.size() < 3) continue;

            float minX, maxX;
            xRange(sp.hull, minX, maxX);

            // ✅ overlap test (NOT center test)
            if (pos.x + radius < minX || pos.x - radius > maxX) continue;

            const float xQuery = std::clamp(pos.x, minX, maxX);

            float yTop = 0.0f;
            if (!topYAtX(sp.hull, xQuery, yTop)) continue;

            const float eps = 1e-3f;
            const bool crossed = (prevBottom >= yTop - eps) && (newBottom <= yTop + eps);
            if (!crossed) continue;

            // landing must be in light (circle test)
            const glm::vec2 landingCenter(pos.x, yTop + radius);
            if (glm::distance(landingCenter, lightCenter) + radius > lightRadius) continue;

            if (yTop > bestYTop) {
                bestYTop = yTop;
                bestObj = sp.objectId;
            }
        }

        if (bestObj >= 0) {
            pos.y = bestYTop + radius;
            vel.y = 0.0f;
            grounded_ = true;
            supportObjectId_ = bestObj;
        }
    }

    // supportU update
    if (grounded_ && supportObjectId_ >= 0) {
        const ShadowPoly* sp = nullptr;
        for (const auto& p : platforms) {
            if (p.objectId == supportObjectId_) { sp = &p; break; }
        }
        if (sp && sp->hull.size() >= 3) {
            float minX, maxX;
            xRange(sp->hull, minX, maxX);
            const float w = std::max(maxX - minX, 1e-5f);
            supportU_ = std::clamp((pos.x - minX) / w, 0.0f, 1.0f);
        }
    }
}

void ShadowBall::draw(const Shader& shader, const glm::mat4& view, const glm::mat4& proj) const {
    shader.use();

    glm::mat4 m(1.0f);
    m = glm::translate(m, glm::vec3(pos.x, pos.y, 0.0f));
    m = glm::scale(m, glm::vec3(radius, radius, 1.0f));

    shader.setMat4("uModel", m);
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);

    shader.setInt("uMode", 0);
    shader.setVec4("uColor4", glm::vec4(0.90f, 0.20f, 0.20f, 1.0f));

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_FAN, 0, count_);
    glBindVertexArray(0);
}
