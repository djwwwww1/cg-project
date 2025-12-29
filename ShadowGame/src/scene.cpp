// ============================================================================
// File: src/scene.cpp  (只贴关键逻辑：重建平台、软边渲染、粘连、掉出光圈)
// ============================================================================
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

static bool g_hasLastLightPos = false;
static glm::vec3 g_lastLightPos(0.0f);

static glm::vec3 cardboard(float t) {
    // t 用来做一点点颜色变化，范围随意
    return glm::vec3(0.72f + 0.05f * t, 0.60f + 0.04f * t, 0.42f + 0.02f * t);
}

static bool pickSpawnOnPlatformTopInLight(const ShadowPoly& sp,
                                         const glm::vec2& lightCenter,
                                         float lightRadius,
                                         float ballRadius,
                                         glm::vec2& outSpawn,
                                         float& outU) {
    if (sp.hull.size() < 3) return false;

    float minX = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    for (const auto& v : sp.hull) { minX = std::min(minX, v.x); maxX = std::max(maxX, v.x); }

    const float w = std::max(maxX - minX, 1e-5f);

    // 采样一些 x，找一个“顶面存在 + 落点在光圈内”的点
    // 选策略：优先靠近中间，同时尽量 yTop 更高（更像站在平台上沿）
    const int samples = 11;
    bool found = false;
    float bestScore = -std::numeric_limits<float>::infinity();

    for (int i = 0; i < samples; ++i) {
        const float u = (float)i / (float)(samples - 1);
        const float x = minX + u * w;

        float yTop = -std::numeric_limits<float>::infinity();
        // 直接复用你 Scene::topYAtX 的逻辑（这里为了不引入依赖，用同样思路再算一次）
        // 你如果愿意，也可以把 Scene::topYAtX 改成 static 并在此调用。
        bool any = false;
        float bestY = -std::numeric_limits<float>::infinity();
        for (size_t e = 0; e < sp.hull.size(); ++e) {
            const glm::vec2 a = sp.hull[e];
            const glm::vec2 b = sp.hull[(e + 1) % sp.hull.size()];

            const float minx = std::min(a.x, b.x);
            const float maxx = std::max(a.x, b.x);
            if (x < minx - 1e-5f || x > maxx + 1e-5f) continue;

            const float dx = b.x - a.x;
            if (std::fabs(dx) < 1e-6f) {
                if (std::fabs(a.x - x) < 1e-4f) {
                    bestY = std::max(bestY, std::max(a.y, b.y));
                    any = true;
                }
                continue;
            }

            const float t = (x - a.x) / dx;
            if (t < -1e-4f || t > 1.0f + 1e-4f) continue;

            const float y = a.y + t * (b.y - a.y);
            bestY = std::max(bestY, y);
            any = true;
        }

        if (!any) continue;
        yTop = bestY;

        const glm::vec2 landing(x, yTop + ballRadius);

        // 必须在光圈内才算“可站立”
        if (glm::distance(landing, lightCenter) > lightRadius) continue;

        // score：更靠近中间更好，yTop 更高更好
        const float centerBias = 1.0f - std::fabs(u - 0.5f) * 2.0f; // [0,1]
        const float score = yTop * 10.0f + centerBias;

        if (!found || score > bestScore) {
            found = true;
            bestScore = score;
            outSpawn = landing;
            outU = u;
        }
    }

    // 如果完全找不到，就失败
    return found;
}

static bool computeSpawnOnLeftmostPlatformInLight(const std::vector<ShadowPoly>& platforms,
                                                  const glm::vec2& lightCenter,
                                                  float lightRadius,
                                                  float ballRadius,
                                                  glm::vec2& outSpawn,
                                                  int& outObjectId,
                                                  float& outU) {
    bool foundAny = false;
    float bestMinX = std::numeric_limits<float>::infinity();

    for (const auto& sp : platforms) {
        if (sp.hull.size() < 3) continue;

        float minX = std::numeric_limits<float>::infinity();
        for (const auto& v : sp.hull) minX = std::min(minX, v.x);

        glm::vec2 spawn;
        float u = 0.5f;
        if (!pickSpawnOnPlatformTopInLight(sp, lightCenter, lightRadius, ballRadius, spawn, u)) continue;

        // 选最左的平台
        if (!foundAny || minX < bestMinX) {
            foundAny = true;
            bestMinX = minX;
            outSpawn = spawn;
            outObjectId = sp.objectId;
            outU = u;
        }
    }

    return foundAny;
}

// project corner to wall z=0 along ray from light
static glm::vec2 projectToWallZ0(const glm::vec3& lightPos, const glm::vec3& p) {
    const float denom = (p.z - lightPos.z);
    if (std::fabs(denom) < 1e-6f) return glm::vec2(p.x, p.y);
    const float t = (0.0f - lightPos.z) / denom;
    const glm::vec3 hit = lightPos + t * (p - lightPos);
    return glm::vec2(hit.x, hit.y);
}

static float cross2(const glm::vec2& o, const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 oa = a - o;
    const glm::vec2 ob = b - o;
    return oa.x * ob.y - oa.y * ob.x;
}

std::vector<glm::vec2> Scene::convexHull(std::vector<glm::vec2> pts) {
    if (pts.size() <= 3) return pts;

    std::sort(pts.begin(), pts.end(), [](auto& a, auto& b){
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    });
    pts.erase(std::unique(pts.begin(), pts.end(), [](auto& a, auto& b){
        return std::fabs(a.x-b.x)<1e-5f && std::fabs(a.y-b.y)<1e-5f;
    }), pts.end());

    std::vector<glm::vec2> lower, upper;
    for (auto& p : pts) {
        while (lower.size() >= 2 && cross2(lower[lower.size()-2], lower.back(), p) <= 0.0f) lower.pop_back();
        lower.push_back(p);
    }
    for (int i=(int)pts.size()-1; i>=0; --i) {
        auto p = pts[(size_t)i];
        while (upper.size() >= 2 && cross2(upper[upper.size()-2], upper.back(), p) <= 0.0f) upper.pop_back();
        upper.push_back(p);
    }
    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

void Scene::xRange(const std::vector<glm::vec2>& poly, float& outMinX, float& outMaxX) {
    outMinX = std::numeric_limits<float>::infinity();
    outMaxX = -std::numeric_limits<float>::infinity();
    for (auto& v : poly) { outMinX = std::min(outMinX, v.x); outMaxX = std::max(outMaxX, v.x); }
}

bool Scene::topYAtX(const std::vector<glm::vec2>& poly, float x, float& outYTop) {
    float best = -std::numeric_limits<float>::infinity();
    bool any = false;

    for (size_t i=0;i<poly.size();++i) {
        const glm::vec2 a = poly[i];
        const glm::vec2 b = poly[(i+1)%poly.size()];

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

const ShadowPoly* Scene::findPlatformByObjectId(int objectId) const {
    for (const auto& p : shadowPlatforms_) if (p.objectId == objectId) return &p;
    return nullptr;
}

Scene::Scene(int w, int h)
    : width_(w),
      height_(h),
      objectShader_("shaders/object_shader.vert", "shaders/object_shader.frag"),
      backgroundShader_("shaders/background_shader.vert", "shaders/background_shader.frag"),
      op_(&light_) {

    camera_.aspect = float(width_) / float(height_);
    initSceneObjects();

    glGenVertexArrays(1, &shadowVao_);
    glGenBuffers(1, &shadowVbo_);
    glBindVertexArray(shadowVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shadowVbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
    recreateShadowMaskResources();
}
Scene::~Scene(){
    destroyShadowMaskResources();
}

void Scene::onResize(int w, int h) {
    width_ = std::max(1, w);
    height_ = std::max(1, h);
    camera_.aspect = float(width_) / float(height_);
    recreateShadowMaskResources();
}

// File: src/scene.cpp
void Scene::resetLevel() {
    // 1) reset light first
    light_.position = spawnLight_;

    // 2) rebuild platforms for this light (so spawn uses correct shadow)
    rebuildShadowPlatforms();
    uploadShadowMeshFromHulls();

    // 3) compute a VALID spawn: top boundary of leftmost platform AND inside light
    const glm::vec2 lc = light_.footprintCenter();
    const float lr = light_.footprintRadius();

    glm::vec2 spawn = spawnBall_;
    int supportObj = -1;
    float supportU = 0.5f;

    if (!computeSpawnOnLeftmostPlatformInLight(shadowPlatforms_, lc, lr, ball_.radius,
                                               spawn, supportObj, supportU)) {
        // fallback: 只要能看到球就行
        spawn = glm::vec2(lc.x, lc.y);
        supportObj = -1;
        supportU = 0.5f;
    }

    // 4) place ball directly on platform and mark grounded
    ball_.reset(spawn);
    ball_.vel = glm::vec2(0.0f);

    if (supportObj >= 0) {
        ball_.forceGrounded(true);
        ball_.setSupportObjectId(supportObj);
        ball_.setSupportU(supportU);
    } else {
        ball_.drop();
    }
}


void Scene::initSceneObjects() {
    objects_.clear();

    auto addTower = [&](glm::vec3 centerXZ, float w, float d, float h, glm::vec3 col) {
        BoxObject b;
        b.scale = glm::vec3(w, h, d);
        b.position = glm::vec3(centerXZ.x, h*0.5f, centerXZ.z);
        b.color = col;
        objects_.push_back(b);
    };

    addTower(glm::vec3(-8.0f, 0.0f, 6.0f), 3.2f, 3.2f, 9.0f,  cardboard(-0.10f));
    addTower(glm::vec3(-2.0f, 0.0f, 6.5f), 3.8f, 3.4f, 12.0f, cardboard( 0.05f));
    addTower(glm::vec3( 6.0f, 0.0f, 6.0f), 3.0f, 4.6f, 10.0f, cardboard( 0.10f));

    // plank on top
    BoxObject plank;
    plank.scale = glm::vec3(18.0f, 0.45f, 1.2f);
    plank.position = glm::vec3(1.5f, 12.2f, 6.0f);
    plank.color = glm::vec3(0.46f, 0.30f, 0.18f);
    objects_.push_back(plank);

    spawnLight_ = glm::vec3(-6.0f, 10.0f, 12.0f);
    light_.position = spawnLight_;

    // 初始先算阴影，出生点最好在最左阴影平台上（你如果已有 computeSpawn... 就用你的）
    spawnBall_ = glm::vec2(-10.0f, 7.0f);
    resetLevel();
}

// 重建完整平台 hull（不裁剪！）
void Scene::rebuildShadowPlatforms() {
    shadowPlatforms_.clear();
    shadowPlatforms_.reserve(objects_.size());

    for (int i=0; i<(int)objects_.size(); ++i) {
        const auto corners = objects_[i].worldCorners();

        std::vector<glm::vec2> pts;
        pts.reserve(8);
        for (const auto& c : corners) pts.push_back(projectToWallZ0(light_.position, c));

        auto hull = convexHull(std::move(pts));
        if (hull.size() < 3) continue;

        ShadowPoly sp;
        sp.objectId = i;
        sp.hull = std::move(hull);
        shadowPlatforms_.push_back(std::move(sp));
    }
}

// 渲染用 mesh：画 hull（由 shader 决定与光圈交集 + 软边）
void Scene::uploadShadowMeshFromHulls() {
    std::vector<glm::vec3> verts;
    verts.reserve(shadowPlatforms_.size() * 64);

    for (const auto& sp : shadowPlatforms_) {
        const auto& poly = sp.hull;
        if (poly.size() < 3) continue;

        const glm::vec2 o = poly[0];
        for (size_t i=1; i+1<poly.size(); ++i) {
            verts.push_back(glm::vec3(o.x,         o.y,         0.02f));
            verts.push_back(glm::vec3(poly[i].x,   poly[i].y,   0.02f));
            verts.push_back(glm::vec3(poly[i+1].x, poly[i+1].y, 0.02f));
        }
    }

    shadowVertCount_ = (GLsizei)verts.size();
    glBindBuffer(GL_ARRAY_BUFFER, shadowVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(glm::vec3)), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Scene::dropBallIfOutOfLight() {
    if (!ball_.grounded()) return;

    const glm::vec2 lc = light_.footprintCenter();
    const float lr = light_.footprintRadius();

    // 用“脚点”更符合你规则：脚离开光圈就掉
    const glm::vec2 foot(ball_.pos.x, ball_.pos.y - ball_.radius);

    // 边界给一点容差，防止贴边抖动
    //const float eps = 1e-3f;
    if (glm::distance(ball_.pos, lc)+ball_.radius > lr) {
        ball_.drop();     // 这里 drop 必须清 grounded / support
    }
}


// src/scene.cpp
void Scene::stickBallToSupportAfterLightMove() {
    if (!ball_.grounded()) return;
    const glm::vec2 lc = light_.footprintCenter();
    const float lr = light_.footprintRadius();
    const glm::vec2 foot(ball_.pos.x, ball_.pos.y - ball_.radius);

    if (glm::distance(ball_.pos, lc)+ball_.radius > lr ) {
        ball_.drop();
        return;
    }

    const int objId = ball_.supportObjectId();
    const ShadowPoly* sp = findPlatformByObjectId(objId);
    if (!sp) { ball_.drop(); return; }

    float minX, maxX;
    xRange(sp->hull, minX, maxX);
    const float w = std::max(maxX - minX, 1e-5f);

    const float u = std::clamp(ball_.supportU(), 0.0f, 1.0f);
    float x = minX + u * w;
    if (x < minX || x > maxX) { ball_.drop(); return; }

    //x = std::clamp(x, minX + 1e-3f, maxX - 1e-3f);
    const float xQuery = std::clamp(x, minX, maxX);
    float yTop = 0.0f;
    if (!topYAtX(sp->hull, xQuery, yTop)) { ball_.drop(); return; }

    const glm::vec2 newPos(x, yTop + ball_.radius);

    // 关键：先判断 newPos 是否出光圈，出就直接 drop，不要瞬移到 newPos
    // const glm::vec2 lc = light_.footprintCenter();
    // const float lr = light_.footprintRadius();
    // if (glm::distance(newPos, lc) > lr) {
    //     ball_.drop();
    //     return;
    // }

    // 只有仍在光圈内，才真正“粘连到新位置”
    ball_.pos = newPos;
    ball_.vel.y = 0.0f;
    ball_.forceGrounded(true);
}


void Scene::update(GLFWwindow* window, float dt) {
    op_.update(window, dt);

    rebuildShadowPlatforms();
    uploadShadowMeshFromHulls();

    const glm::vec2 lc = light_.footprintCenter();
    const float lr = light_.footprintRadius();

    // 先检查“当前球是否已出光圈”，出就 drop，别再粘连
    // 只有仍在光圈内，并且 grounded，才做粘连/等比移动
    dropBallIfOutOfLight();
    // stickBallToSupportAfterLightMove();

    // 只在光源真的移动时才做“等比粘连”，否则会把走出边缘的动作拉回去
    bool lightMoved = false;
    if (!g_hasLastLightPos) {
        g_hasLastLightPos = true;
        g_lastLightPos = light_.position;
    } else {
        const float eps = 1e-4f;
        lightMoved = glm::distance(light_.position, g_lastLightPos) > eps;
        g_lastLightPos = light_.position;
    }

    if (lightMoved) {
        stickBallToSupportAfterLightMove();
    }

    // 物理现在会“边缘走出去就掉”，且“光圈外的平台无效”
    ball_.updatePhysics(window, dt, shadowPlatforms_, lc, lr);

    // death line
    if (ball_.pos.y - ball_.radius <= planes_.deathY()) resetLevel();

    camera_.updateFollow(ball_.pos, dt);
}

void Scene::render() {
    glClearColor(0.10f, 0.09f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 V = camera_.view();
    const glm::mat4 P = camera_.proj();

    const glm::vec2 lc = light_.footprintCenter();
    const float lr = light_.footprintRadius();

    // ----------------------------
    // PASS 0: render shadow mask (R8) with MAX blending -> no darker overlap
    // ----------------------------
    // PASS0: render shadow mask (must use SAME V/P as scene!)
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMaskFbo_);
    glViewport(0, 0, width_, height_);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);
    glBlendFunc(GL_ONE, GL_ONE);

    backgroundShader_.use();

    // 关键：必须给矩阵（否则阴影会“钉在屏幕/尺寸错乱”）
    backgroundShader_.setMat4("uModel", glm::mat4(1.0f));
    backgroundShader_.setMat4("uView",  V);
    backgroundShader_.setMat4("uProj",  P);

    backgroundShader_.setInt("uMode", 5);
    backgroundShader_.setVec2("uLightCenter", lc);
    backgroundShader_.setFloat("uLightRadius", lr);
    backgroundShader_.setFloat("uSoftness", lr * 0.32f);
    backgroundShader_.setVec2("uResolution", glm::vec2((float)width_, (float)height_));

    glBindVertexArray(shadowVao_);
    glDrawArrays(GL_TRIANGLES, 0, shadowVertCount_);
    glBindVertexArray(0);

    glDisable(GL_BLEND);

    // 必须恢复
    glBlendEquation(GL_FUNC_ADD);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);

    // ----------------------------
    // PASS 1: draw 3D objects + wall base
    // ----------------------------
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    objectShader_.use();
    objectShader_.setVec3("uViewPos", camera_.position);
    objectShader_.setVec3("uLightPos", light_.position);
    objectShader_.setVec3("uLightColor", light_.color);
    objectShader_.setVec3("uLightDir", light_.directionToPlaneCenter(lc));
    objectShader_.setFloat("uInnerCut", glm::cos(glm::radians(light_.fovDeg * 0.45f)));
    objectShader_.setFloat("uOuterCut", glm::cos(glm::radians(light_.fovDeg * 0.50f)));
    objectShader_.setFloat("uEnvAmbient", 0.48f);

    for (const auto& obj : objects_) obj.draw(objectShader_, V, P);

    planes_.drawWallLit(backgroundShader_, V, P, lc, lr, lr * 0.32f, 0.45f);

    // ----------------------------
    // PASS 2: composite shadow using mask (draw wall once) -> overlap won't get darker
    // ----------------------------
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_EQUAL);     // 只在“墙深度”的像素通过
    glDepthMask(GL_FALSE);     // 不改 depth

    backgroundShader_.use();
    backgroundShader_.setInt("uMode", 6);
    backgroundShader_.setVec2("uResolution", glm::vec2((float)width_, (float)height_));
    backgroundShader_.setVec4("uColor4", glm::vec4(0.10f, 0.07f, 0.05f, 0.95f)); // 阴影必须是暗色

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowMaskTex_);
    backgroundShader_.setInt("uShadowMask", 0);

    // IMPORTANT: draw the SAME wall geometry again
    planes_.drawWallShadowCompositeFromMask(
        backgroundShader_, V, P, shadowMaskTex_, glm::ivec2(width_, height_),
        glm::vec4(0.10f, 0.07f, 0.05f, 0.95f)
    );

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);

    // ball
    backgroundShader_.use();
    backgroundShader_.setInt("uMode", 0);
    backgroundShader_.setVec4("uColor4", glm::vec4(0.90f, 0.20f, 0.20f, 1.0f));
    ball_.draw(backgroundShader_, V, P);

}

void Scene::destroyShadowMaskResources() {
    if (shadowMaskTex_) { glDeleteTextures(1, &shadowMaskTex_); shadowMaskTex_ = 0; }
    if (shadowMaskFbo_) { glDeleteFramebuffers(1, &shadowMaskFbo_); shadowMaskFbo_ = 0; }
}

void Scene::recreateShadowMaskResources() {
    destroyShadowMaskResources();

    glGenFramebuffers(1, &shadowMaskFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMaskFbo_);

    glGenTextures(1, &shadowMaskTex_);
    glBindTexture(GL_TEXTURE_2D, shadowMaskTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadowMaskTex_, 0);

    const GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuf);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// void Scene::render() {
//     glClearColor(0.10f, 0.09f, 0.085f, 1.0f);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//     const glm::mat4 V = camera_.view();
//     const glm::mat4 P = camera_.proj();

//     // wall+floor
//     const glm::vec2 lc = light_.footprintCenter();
//     const float lr = light_.footprintRadius();
//     planes_.drawWallLit(backgroundShader_, V, P, lc, lr, lr * 0.32f /*softness更大*/, 0.45f);
//     planes_.drawFloorFlat(backgroundShader_, V, P);

//     // shadow overlay: 用 uMode=2 做 “与光圈软交集”
//     {
//         glEnable(GL_BLEND);
//         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//         glEnable(GL_DEPTH_TEST);
//         glDepthMask(GL_FALSE);

//         glEnable(GL_POLYGON_OFFSET_FILL);
//         glPolygonOffset(-1.0f, -1.0f);

//         backgroundShader_.use();
//         backgroundShader_.setMat4("uModel", glm::mat4(1.0f));
//         backgroundShader_.setMat4("uView", V);
//         backgroundShader_.setMat4("uProj", P);

//         backgroundShader_.setInt("uMode", 2); // <-- 关键：软边在 shader 里做
//         backgroundShader_.setVec2("uLightCenter", lc);
//         backgroundShader_.setFloat("uLightRadius", lr);
//         backgroundShader_.setFloat("uSoftness", lr * 0.32f);
//         backgroundShader_.setFloat("uAmbient", 0.45f);
//         backgroundShader_.setVec4("uColor", glm::vec4(0.10f, 0.07f, 0.05f, 0.92f));

//         glBindVertexArray(shadowVao_);
//         glDrawArrays(GL_TRIANGLES, 0, shadowVertCount_);
//         glBindVertexArray(0);

//         glDisable(GL_POLYGON_OFFSET_FILL);
//         glDepthMask(GL_TRUE);
//         glDisable(GL_BLEND);
//     }

//     // ball last
//     ball_.draw(backgroundShader_, V, P);
// }
