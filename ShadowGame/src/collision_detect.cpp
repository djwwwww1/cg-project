#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace collision {

static float dot2(const glm::vec2& a, const glm::vec2& b) { return a.x*b.x + a.y*b.y; }
static glm::vec2 perp(const glm::vec2& v) { return glm::vec2(-v.y, v.x); }

static glm::vec2 normalizeSafe(const glm::vec2& v) {
    const float l2 = dot2(v, v);
    if (l2 < 1e-10f) return glm::vec2(0.0f, 1.0f);
    return v / std::sqrt(l2);
}

static void projectPoly(const std::vector<glm::vec2>& p, const glm::vec2& axis, float& mn, float& mx) {
    mn = mx = dot2(p[0], axis);
    for (size_t i=1;i<p.size();++i) {
        const float v = dot2(p[i], axis);
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
}

static void projectCircle(const glm::vec2& c, float r, const glm::vec2& axis, float& mn, float& mx) {
    const float p = dot2(c, axis);
    mn = p - r;
    mx = p + r;
}

static bool overlapOnAxis(const std::vector<glm::vec2>& poly, const glm::vec2& c, float r,
                          const glm::vec2& axis, float& outOverlap, float& outSign) {
    float pMin, pMax, cMin, cMax;
    projectPoly(poly, axis, pMin, pMax);
    projectCircle(c, r, axis, cMin, cMax);

    if (pMax < cMin || cMax < pMin) return false;

    const float o1 = pMax - cMin;
    const float o2 = cMax - pMin;
    if (o1 < o2) { outOverlap = o1; outSign = +1.0f; }
    else         { outOverlap = o2; outSign = -1.0f; }
    return true;
}

// SAT MTV for circle vs convex poly
static bool mtvCirclePoly(glm::vec2 pos, float r, const std::vector<glm::vec2>& poly, glm::vec2& outMtv) {
    if (poly.size() < 3) return false;

    float minOverlap = std::numeric_limits<float>::infinity();
    glm::vec2 bestAxis(0.0f);
    float bestSign = 1.0f;

    // edge normals
    for (size_t i=0;i<poly.size();++i) {
        const glm::vec2 a = poly[i];
        const glm::vec2 b = poly[(i+1)%poly.size()];
        const glm::vec2 axis = normalizeSafe(perp(b-a));

        float overlap=0.0f, sign=1.0f;
        if (!overlapOnAxis(poly, pos, r, axis, overlap, sign)) return false;
        if (overlap < minOverlap) { minOverlap = overlap; bestAxis = axis; bestSign = sign; }
    }

    // vertex axis
    size_t bestV = 0;
    float bestD = std::numeric_limits<float>::infinity();
    for (size_t i=0;i<poly.size();++i) {
        const float d = dot2(poly[i]-pos, poly[i]-pos);
        if (d < bestD) { bestD = d; bestV = i; }
    }
    const glm::vec2 vAxis = normalizeSafe(pos - poly[bestV]);
    {
        float overlap=0.0f, sign=1.0f;
        if (overlapOnAxis(poly, pos, r, vAxis, overlap, sign)) {
            if (overlap < minOverlap) { minOverlap = overlap; bestAxis = vAxis; bestSign = sign; }
        }
    }

    outMtv = bestAxis * (minOverlap * bestSign);
    return true;
}

// One-way platform resolve:
// 只在“球向下运动/落下”时，对 mtv.y>0 的情况提供支撑；不做 mtv.x 推回 -> 不会卡边
glm::vec2 resolveCircleAgainstPlatforms(glm::vec2 pos, float radius,
                                       const std::vector<ShadowPoly>& platforms,
                                       glm::vec2& vel, bool& grounded, int& groundObjectId) {
    grounded = false;
    groundObjectId = -1;

    float bestUp = 0.0f;

    for (int iter=0; iter<4; ++iter) {
        bool any = false;

        for (const auto& pl : platforms) {
            if (pl.hull.size() < 3) continue;

            glm::vec2 mtv(0.0f);
            if (!mtvCirclePoly(pos, radius, pl.hull, mtv)) continue;

            // only resolve "standing" collisions
            if (mtv.y > 0.0f && vel.y <= 0.0f) {
                pos += mtv;
                vel.y = 0.0f;
                any = true;

                grounded = true;
                if (mtv.y > bestUp) {
                    bestUp = mtv.y;
                    groundObjectId = pl.objectId;
                }
            }
            // ignore side/underside pushes to allow falling off edges
        }

        if (!any) break;
    }

    return pos;
}

} // namespace collision