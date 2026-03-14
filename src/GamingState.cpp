#include "GamingState.hpp"
#include "Creature.hpp"
#include "NeuralNetwork.hpp"
#include "Terrain.hpp"
#include <SFML/OpenGL.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct IsoVec3
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

IsoVec3 lerpIso(const IsoVec3& a, const IsoVec3& b, float t)
{
    return IsoVec3{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

IsoVec3 interpolateIso(const IsoVec3& a, const IsoVec3& b, float va, float vb, float iso)
{
    constexpr float eps = 0.00001f;
    if (std::abs(iso - va) < eps)
        return a;
    if (std::abs(iso - vb) < eps)
        return b;
    if (std::abs(va - vb) < eps)
        return a;

    const float t = std::clamp((iso - va) / (vb - va), 0.f, 1.f);
    return lerpIso(a, b, t);
}

void polygoniseTetra(const std::array<IsoVec3, 4>& p,
                     const std::array<float, 4>& v,
                     float iso,
                     std::vector<IsoVec3>& outTriangles)
{
    std::array<int, 4> inside{};
    std::array<int, 4> outside{};
    int insideCount = 0;
    int outsideCount = 0;

    for (int i = 0; i < 4; ++i)
    {
        if (v[i] >= iso)
            inside[insideCount++] = i;
        else
            outside[outsideCount++] = i;
    }

    if (insideCount == 0 || insideCount == 4)
        return;

    auto emit = [&outTriangles](const IsoVec3& a, const IsoVec3& b, const IsoVec3& c) {
        outTriangles.push_back(a);
        outTriangles.push_back(b);
        outTriangles.push_back(c);
    };

    if (insideCount == 1)
    {
        const int i0 = inside[0];
        const int o0 = outside[0];
        const int o1 = outside[1];
        const int o2 = outside[2];

        const IsoVec3 p0 = interpolateIso(p[i0], p[o0], v[i0], v[o0], iso);
        const IsoVec3 p1 = interpolateIso(p[i0], p[o1], v[i0], v[o1], iso);
        const IsoVec3 p2 = interpolateIso(p[i0], p[o2], v[i0], v[o2], iso);
        emit(p0, p1, p2);
        return;
    }

    if (insideCount == 3)
    {
        const int o0 = outside[0];
        const int i0 = inside[0];
        const int i1 = inside[1];
        const int i2 = inside[2];

        const IsoVec3 p0 = interpolateIso(p[o0], p[i0], v[o0], v[i0], iso);
        const IsoVec3 p1 = interpolateIso(p[o0], p[i1], v[o0], v[i1], iso);
        const IsoVec3 p2 = interpolateIso(p[o0], p[i2], v[o0], v[i2], iso);
        emit(p0, p2, p1);
        return;
    }

    if (insideCount == 2)
    {
        const int i0 = inside[0];
        const int i1 = inside[1];
        const int o0 = outside[0];
        const int o1 = outside[1];

        const IsoVec3 p0 = interpolateIso(p[i0], p[o0], v[i0], v[o0], iso);
        const IsoVec3 p1 = interpolateIso(p[i0], p[o1], v[i0], v[o1], iso);
        const IsoVec3 p2 = interpolateIso(p[i1], p[o0], v[i1], v[o0], iso);
        const IsoVec3 p3 = interpolateIso(p[i1], p[o1], v[i1], v[o1], iso);

        emit(p0, p1, p2);
        emit(p2, p1, p3);
    }
}

sf::Vector3f sub3(const sf::Vector3f& a, const sf::Vector3f& b)
{
    return sf::Vector3f(a.x - b.x, a.y - b.y, a.z - b.z);
}

sf::Vector3f cross3(const sf::Vector3f& a, const sf::Vector3f& b)
{
    return sf::Vector3f(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

sf::Vector3f normalize3(const sf::Vector3f& v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.00001f)
        return sf::Vector3f(0.f, 0.f, 0.f);
    return sf::Vector3f(v.x / len, v.y / len, v.z / len);
}

void applyLookAt(const sf::Vector3f& eye, const sf::Vector3f& target, const sf::Vector3f& up)
{
    const sf::Vector3f f = normalize3(sub3(target, eye));
    sf::Vector3f s = normalize3(cross3(f, up));
    if (std::abs(s.x) + std::abs(s.y) + std::abs(s.z) < 0.0001f)
        s = sf::Vector3f(1.f, 0.f, 0.f);
    const sf::Vector3f u = cross3(s, f);

    const GLfloat m[16] = {
        s.x, u.x, -f.x, 0.f,
        s.y, u.y, -f.y, 0.f,
        s.z, u.z, -f.z, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    glMultMatrixf(m);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}

float mapSizeScale(const Terrain* terrain)
{
    const float mapSize = (terrain != nullptr) ? terrain->mapSize() : Terrain::kDefaultMapSize;
    return std::max(1.f, mapSize / Terrain::kDefaultMapSize);
}

float overviewCameraDistance(const Terrain* terrain)
{
    constexpr float baseDistance = 70.0f;
    return baseDistance * mapSizeScale(terrain);
}

float projectionFarPlane(const Terrain* terrain)
{
    constexpr float baseFarPlane = 1000.f;
    return baseFarPlane * mapSizeScale(terrain);
}
}

GamingState::GamingState(StateStack& stack, Context context)
    : State(stack, context)
    , mFont()
    , mText(mFont)
    , mHudPanel()
{
    if (!mFont.openFromFile("res/fonts/font.ttf"))
        throw std::runtime_error("Failed to load font");

    mText.setString("Generation: 0\nTime: 0.0s");
    mText.setCharacterSize(24);
    mText.setFillColor(sf::Color::White);
    mText.setPosition({30.f, 30.f});

    mHudPanel.setPosition({16.f, 16.f});
    mHudPanel.setSize({360.f, 96.f});
    mHudPanel.setFillColor(sf::Color(25, 25, 26));
    mHudPanel.setOutlineThickness(2.f);
    mHudPanel.setOutlineColor(sf::Color(54, 54, 54));

    startAgentWorkers();
}

GamingState::~GamingState()
{
    stopAgentWorkers();
    if (mContext.window != nullptr)
        mContext.window->setMouseCursorVisible(true);
}

void GamingState::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::KeyPressed>())
    {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                if (mContext.window != nullptr)
                    mContext.window->setMouseCursorVisible(true);
                requestPop();
                return;
            }

            if (mThirdPersonCamera && (key->code == sf::Keyboard::Key::Q || key->code == sf::Keyboard::Key::Up))
            {
                mThirdPersonDistance = std::clamp(mThirdPersonDistance - 0.45f, 2.0f, 20.0f);
                return;
            }

            if (mThirdPersonCamera && (key->code == sf::Keyboard::Key::E || key->code == sf::Keyboard::Key::Down))
            {
                mThirdPersonDistance = std::clamp(mThirdPersonDistance + 0.45f, 2.0f, 20.0f);
                return;
            }

            if (key->code == sf::Keyboard::Key::P)
                startPopulationTraining();

            if (key->code == sf::Keyboard::Key::T)
                mSpeedhackEnabled = !mSpeedhackEnabled;

            if (key->code == sf::Keyboard::Key::W)
            {
                mThirdPersonCamera = !mThirdPersonCamera;

                mMouseLookActive = false;
                if (mContext.window != nullptr)
                    mContext.window->setMouseCursorVisible(!mThirdPersonCamera);

                if (mThirdPersonCamera)
                {
                    sf::Vector3f focus{0.f, 1.1f, 0.f};
                    if (!mRagdollNodes.empty())
                    {
                        focus = computeCenterOfMass();
                    }
                    else if (mContext.creature != nullptr && !mContext.creature->empty())
                    {
                        const sf::Vector2f spawn = computeSpawnAnchor();
                        focus.x = spawn.x;
                        focus.z = spawn.y;
                    }

                    if (mContext.terrain != nullptr && mContext.terrain->hasDestinationCell())
                    {
                        const sf::Vector2f destination = computeDestinationAnchor();
                        const float dx = destination.x - focus.x;
                        const float dz = destination.y - focus.z;
                        if (std::abs(dx) + std::abs(dz) > 0.0001f)
                            mThirdPersonYawDegrees = std::atan2(dx, dz) * 180.f / 3.1415926535f;
                    }

                    if (mContext.window != nullptr)
                    {
                        const sf::Vector2u windowSize = mContext.window->getSize();
                        const sf::Vector2i center(
                            static_cast<int>(windowSize.x / 2u),
                            static_cast<int>(windowSize.y / 2u)
                        );
                        sf::Mouse::setPosition(center, *mContext.window);
                    }
                }
            }

            if (key->code == sf::Keyboard::Key::F1)
                mHudVisible = !mHudVisible;
        }
    }
}

void GamingState::update(sf::Time dt)
{
    if (mThirdPersonCamera && mContext.window != nullptr)
    {
        if (mContext.window->hasFocus())
        {
            const sf::Vector2u windowSize = mContext.window->getSize();
            const sf::Vector2i center(
                static_cast<int>(windowSize.x / 2u),
                static_cast<int>(windowSize.y / 2u)
            );

            if (!mMouseLookActive)
            {
                sf::Mouse::setPosition(center, *mContext.window);
                mMouseLookActive = true;
            }
            else
            {
                const sf::Vector2i mousePos = sf::Mouse::getPosition(*mContext.window);
                const sf::Vector2i delta = mousePos - center;
                constexpr float mouseSensitivity = 0.18f;
                mThirdPersonYawDegrees += static_cast<float>(delta.x) * mouseSensitivity;
                mThirdPersonPitchDegrees = std::clamp(
                    mThirdPersonPitchDegrees - static_cast<float>(delta.y) * mouseSensitivity,
                    -55.f,
                    65.f
                );

                sf::Mouse::setPosition(center, *mContext.window);
            }
        }
        else
        {
            mMouseLookActive = false;
        }
    }
    else
    {
        mMouseLookActive = false;
    }

    if (!mThirdPersonCamera)
    {
        mRotationDegrees += dt.asSeconds() * 30.f;
        if (mRotationDegrees > 360.f)
            mRotationDegrees -= 360.f;
    }

    stepPopulation(dt);

    const int timeLeftTenths = std::max(0, static_cast<int>(std::round(mGenerationTimeLeft * 10.f)));
    const std::string timerText = std::to_string(timeLeftTenths / 10) + "." + std::to_string(timeLeftTenths % 10) + "s";
    mText.setString(
        "Generation: " + std::to_string(mGenerationIndex)
        + "\nTime: " + timerText
    );
}

void GamingState::render()
{
    if (!mGlInitialized)
    {
        mContext.window->setActive(true);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1.f);
        glDisable(GL_CULL_FACE);
        mGlInitialized = true;
    }

    mContext.window->setActive(true);
    setupProjection();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    sf::Vector3f cameraFocus{0.f, 1.1f, 0.f};
    bool hasCameraFocus = false;
    if (!mRagdollNodes.empty())
    {
        cameraFocus = computeCenterOfMass();
        hasCameraFocus = true;
    }
    else if (mContext.creature != nullptr && !mContext.creature->empty())
    {
        const sf::Vector2f spawn = computeSpawnAnchor();
        cameraFocus.x = spawn.x;
        cameraFocus.z = spawn.y;
        cameraFocus.y = sampleTerrainHeight(spawn.x, spawn.y) + 0.18f;
        hasCameraFocus = true;
    }

    if (mThirdPersonCamera && hasCameraFocus)
    {
        const float yawRad = mThirdPersonYawDegrees * 3.1415926535f / 180.f;
        const float pitchRad = mThirdPersonPitchDegrees * 3.1415926535f / 180.f;
        sf::Vector3f forward{
            std::sin(yawRad) * std::cos(pitchRad),
            std::sin(pitchRad),
            std::cos(yawRad) * std::cos(pitchRad)
        };
        forward = normalize3(forward);
        if (vectorLength(forward) < 0.0001f)
            forward = sf::Vector3f(0.f, 0.f, 1.f);

        const sf::Vector3f target{
            cameraFocus.x + forward.x * 0.45f,
            cameraFocus.y + 0.10f + forward.y * 0.18f,
            cameraFocus.z + forward.z * 0.45f
        };

        sf::Vector3f eye{
            cameraFocus.x - forward.x * mThirdPersonDistance,
            cameraFocus.y + 0.48f - forward.y * 0.28f,
            cameraFocus.z - forward.z * mThirdPersonDistance
        };
        eye.y = std::max(eye.y, sampleTerrainHeight(eye.x, eye.z) + 0.25f);
        applyLookAt(eye, target, sf::Vector3f(0.f, 1.f, 0.f));
    }
    else
    {
        const float cameraDistance = overviewCameraDistance(mContext.terrain);
        glTranslatef(0.f, -0.9f, -cameraDistance);
        glRotatef(28.f, 1.f, 0.f, 0.f);
        glRotatef(mRotationDegrees, 0.f, 1.f, 0.f);
        glScalef(1.7f, 1.7f, 1.7f);
    }

    drawCubeWireframe();
    if (mContext.terrain != nullptr)
    {
        if (mContext.terrain->hasSpawnCell())
        {
            drawTopCellOverlay(
                mContext.terrain->spawnCellX(),
                mContext.terrain->spawnCellZ(),
                sf::Color(70, 130, 255),
                2.6f
            );
        }

        if (mContext.terrain->hasDestinationCell())
        {
            drawTopCellOverlay(
                mContext.terrain->destinationCellX(),
                mContext.terrain->destinationCellZ(),
                sf::Color(235, 70, 70),
                2.6f
            );
        }
    }
    drawCreature();

    mContext.window->resetGLStates();
    if (mHudVisible)
    {
        mContext.window->draw(mHudPanel);
        mContext.window->draw(mText);
    }
}

void GamingState::setupProjection()
{
    const sf::Vector2u windowSize = mContext.window->getSize();
    const float width = static_cast<float>(windowSize.x);
    const float height = static_cast<float>(windowSize.y == 0 ? 1 : windowSize.y);
    const float aspect = width / height;

    glViewport(0, 0, static_cast<GLsizei>(windowSize.x), static_cast<GLsizei>(windowSize.y));

    constexpr float nearPlane = 0.1f;
    constexpr float overviewFov = 35.f;
    constexpr float thirdPersonFov = 95.f;
    const float farPlane = projectionFarPlane(mContext.terrain);
    const float fov = mThirdPersonCamera ? thirdPersonFov : overviewFov;
    const float halfHeight = std::tan((fov * 0.5f) * 3.1415926535f / 180.f) * nearPlane;
    const float halfWidth = halfHeight * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

void GamingState::drawCubeWireframe()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.2f);
    glColor3f(1.f, 1.f, 1.f);
    const float mapSize = (mContext.terrain != nullptr) ? mContext.terrain->mapSize() : Terrain::kDefaultMapSize;
    const float mapMin = -0.5f * mapSize;
    const float mapMax = 0.5f * mapSize;

    auto emitTriangle = [](float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           float x2, float y2, float z2) {
        glVertex3f(x0, y0, z0);
        glVertex3f(x1, y1, z1);
        glVertex3f(x2, y2, z2);
    };

    glBegin(GL_TRIANGLES);

    // Bottom face (2 triangles).
    emitTriangle(mapMin, -1.f, mapMin, mapMax, -1.f, mapMin, mapMax, -1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMin, mapMax, -1.f, mapMax, mapMin, -1.f, mapMax);

    // Front face (2 triangles, z = 1).
    emitTriangle(mapMin, -1.f, mapMax, mapMax, -1.f, mapMax, mapMax, 1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMax, mapMax, 1.f, mapMax, mapMin, 1.f, mapMax);

    // Back face (2 triangles, z = -1).
    emitTriangle(mapMin, -1.f, mapMin, mapMax, 1.f, mapMin, mapMax, -1.f, mapMin);
    emitTriangle(mapMin, -1.f, mapMin, mapMin, 1.f, mapMin, mapMax, 1.f, mapMin);

    // Left face (2 triangles, x = -1).
    emitTriangle(mapMin, -1.f, mapMin, mapMin, -1.f, mapMax, mapMin, 1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMin, mapMin, 1.f, mapMax, mapMin, 1.f, mapMin);

    // Right face (2 triangles, x = 1).
    emitTriangle(mapMax, -1.f, mapMin, mapMax, 1.f, mapMax, mapMax, -1.f, mapMax);
    emitTriangle(mapMax, -1.f, mapMin, mapMax, 1.f, mapMin, mapMax, 1.f, mapMax);

    const int divisions = (mContext.terrain != nullptr)
        ? mContext.terrain->divisions()
        : Terrain::kDefaultDivisions;
    for (int z = 0; z < divisions; ++z)
    {
        for (int x = 0; x < divisions; ++x)
        {
            const float x0 = mapMin + mapSize * static_cast<float>(x) / static_cast<float>(divisions);
            const float x1 = mapMin + mapSize * static_cast<float>(x + 1) / static_cast<float>(divisions);
            const float z0 = mapMin + mapSize * static_cast<float>(z) / static_cast<float>(divisions);
            const float z1 = mapMin + mapSize * static_cast<float>(z + 1) / static_cast<float>(divisions);

            float y00 = 1.f;
            float y10 = 1.f;
            float y01 = 1.f;
            float y11 = 1.f;
            if (mContext.terrain != nullptr)
            {
                y00 = mContext.terrain->heightAt(x, z);
                y10 = mContext.terrain->heightAt(x + 1, z);
                y01 = mContext.terrain->heightAt(x, z + 1);
                y11 = mContext.terrain->heightAt(x + 1, z + 1);
            }

            emitTriangle(x0, y00, z0, x1, y10, z0, x1, y11, z1);
            emitTriangle(x0, y00, z0, x1, y11, z1, x0, y01, z1);
        }
    }

    glEnd();

    // Avoid leaking wireframe mode into SFML text rendering in other states.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void GamingState::drawTopCellOverlay(int cellX, int cellZ, const sf::Color& color, float lineWidth) const
{
    if (mContext.terrain == nullptr)
        return;

    const int divisions = (mContext.terrain != nullptr)
        ? mContext.terrain->divisions()
        : Terrain::kDefaultDivisions;
    const float mapSize = (mContext.terrain != nullptr) ? mContext.terrain->mapSize() : Terrain::kDefaultMapSize;
    const float mapMin = -0.5f * mapSize;
    if (cellX < 0 || cellZ < 0 || cellX >= divisions || cellZ >= divisions)
        return;

    const float x0 = mapMin + mapSize * static_cast<float>(cellX) / static_cast<float>(divisions);
    const float x1 = mapMin + mapSize * static_cast<float>(cellX + 1) / static_cast<float>(divisions);
    const float z0 = mapMin + mapSize * static_cast<float>(cellZ) / static_cast<float>(divisions);
    const float z1 = mapMin + mapSize * static_cast<float>(cellZ + 1) / static_cast<float>(divisions);

    const float y00 = mContext.terrain->heightAt(cellX, cellZ);
    const float y10 = mContext.terrain->heightAt(cellX + 1, cellZ);
    const float y01 = mContext.terrain->heightAt(cellX, cellZ + 1);
    const float y11 = mContext.terrain->heightAt(cellX + 1, cellZ + 1);
    constexpr float overlayLift = 0.0015f;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(lineWidth);
    glColor3f(
        static_cast<float>(color.r) / 255.f,
        static_cast<float>(color.g) / 255.f,
        static_cast<float>(color.b) / 255.f
    );

    glBegin(GL_TRIANGLES);
    glVertex3f(x0, y00 + overlayLift, z0);
    glVertex3f(x1, y10 + overlayLift, z0);
    glVertex3f(x1, y11 + overlayLift, z1);

    glVertex3f(x0, y00 + overlayLift, z0);
    glVertex3f(x1, y11 + overlayLift, z1);
    glVertex3f(x0, y01 + overlayLift, z1);
    glEnd();

    glLineWidth(1.f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void GamingState::drawCreature()
{
    if (mContext.creature == nullptr || mContext.creature->empty())
        return;

    const auto& blocks = mContext.creature->blocks();
    const auto& connections = mContext.creature->connections();
    if (mRagdollActive && mRagdollNodes.size() != blocks.size())
        mRagdollActive = false;

    int minX = blocks.front().gx;
    int maxX = blocks.front().gx;
    int minY = blocks.front().gy;
    int maxY = blocks.front().gy;
    for (const auto& block : blocks)
    {
        minX = std::min(minX, block.gx);
        maxX = std::max(maxX, block.gx);
        minY = std::min(minY, block.gy);
        maxY = std::max(maxY, block.gy);
    }

    constexpr float cellStep = Terrain::kDefaultMapSize / static_cast<float>(Terrain::kDefaultDivisions);
    const float torsoBaseRadius = std::max(0.004f, cellStep * 0.56f);

    struct Pos
    {
        float x;
        float y;
        float z;
    };

    std::vector<Pos> centers;
    centers.reserve(blocks.size());

    const float cx = static_cast<float>(minX + maxX) * 0.5f;
    const float cy = static_cast<float>(minY + maxY) * 0.5f;
    const sf::Vector2f anchor = computeSpawnAnchor();
    const float anchorX = anchor.x;
    const float anchorZ = anchor.y;

    for (const auto& block : blocks)
    {
        const float x = anchorX + (static_cast<float>(block.gx) - cx) * cellStep;
        const float z = anchorZ + (static_cast<float>(block.gy) - cy) * cellStep;
        const float y = sampleTerrainHeight(x, z) + 0.025f;
        centers.push_back({x, y, z});
    }

    if (mRagdollActive && mRagdollNodes.size() == centers.size())
    {
        for (std::size_t i = 0; i < centers.size(); ++i)
        {
            centers[i].x = mRagdollNodes[i].position.x;
            centers[i].y = mRagdollNodes[i].position.y;
            centers[i].z = mRagdollNodes[i].position.z;
        }
    }

    std::vector<bool> torsoBlock(blocks.size(), false);
    for (const auto& c : connections)
    {
        if (c.from >= blocks.size() || c.to >= blocks.size())
            continue;
        if (c.type == Creature::ConnectionType::Torso)
        {
            torsoBlock[c.from] = true;
            torsoBlock[c.to] = true;
        }
    }

    if (std::none_of(torsoBlock.begin(), torsoBlock.end(), [](bool v) { return v; }))
        std::fill(torsoBlock.begin(), torsoBlock.end(), true);

    struct FieldBlob
    {
        IsoVec3 center;
        float radius = 0.f;
        float supportRadius = 0.f;
    };

    auto addFieldBlob = [](std::vector<FieldBlob>& blobs, float x, float y, float z, float radius) {
        const float supportRadius = radius * 1.9f;
        for (const FieldBlob& existing : blobs)
        {
            const float dx = existing.center.x - x;
            const float dy = existing.center.y - y;
            const float dz = existing.center.z - z;
            const float minRadius = std::min(existing.radius, radius);
            if (dx * dx + dy * dy + dz * dz < minRadius * minRadius * 0.08f)
                return;
        }

        FieldBlob blob;
        blob.center = IsoVec3{x, y, z};
        blob.radius = radius;
        blob.supportRadius = supportRadius;
        blobs.push_back(blob);
    };

    auto extractIsoTriangles = [](const std::vector<FieldBlob>& sourceBlobs,
                                  float referenceRadius,
                                  float isoThreshold) -> std::vector<IsoVec3> {
        std::vector<IsoVec3> triangles;
        if (sourceBlobs.empty())
            return triangles;

        IsoVec3 boundsMin{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        IsoVec3 boundsMax{
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()
        };

        for (const FieldBlob& blob : sourceBlobs)
        {
            boundsMin.x = std::min(boundsMin.x, blob.center.x - blob.supportRadius);
            boundsMin.y = std::min(boundsMin.y, blob.center.y - blob.supportRadius);
            boundsMin.z = std::min(boundsMin.z, blob.center.z - blob.supportRadius);

            boundsMax.x = std::max(boundsMax.x, blob.center.x + blob.supportRadius);
            boundsMax.y = std::max(boundsMax.y, blob.center.y + blob.supportRadius);
            boundsMax.z = std::max(boundsMax.z, blob.center.z + blob.supportRadius);
        }

        const float border = referenceRadius * 0.3f;
        boundsMin.x -= border;
        boundsMin.y -= border;
        boundsMin.z -= border;
        boundsMax.x += border;
        boundsMax.y += border;
        boundsMax.z += border;

        const float extentX = std::max(0.001f, boundsMax.x - boundsMin.x);
        const float extentY = std::max(0.001f, boundsMax.y - boundsMin.y);
        const float extentZ = std::max(0.001f, boundsMax.z - boundsMin.z);

        const float targetCellSize = std::max(0.035f, referenceRadius * 1.1f);
        const int cellsX = std::clamp(static_cast<int>(std::ceil(extentX / targetCellSize)), 8, 24);
        const int cellsY = std::clamp(static_cast<int>(std::ceil(extentY / targetCellSize)), 6, 20);
        const int cellsZ = std::clamp(static_cast<int>(std::ceil(extentZ / targetCellSize)), 8, 24);

        const float stepX = extentX / static_cast<float>(cellsX);
        const float stepY = extentY / static_cast<float>(cellsY);
        const float stepZ = extentZ / static_cast<float>(cellsZ);

        const int pointsX = cellsX + 1;
        const int pointsY = cellsY + 1;
        const int pointsZ = cellsZ + 1;

        auto fieldIndex = [pointsX, pointsY](int x, int y, int z) -> std::size_t {
            return static_cast<std::size_t>(z * pointsY * pointsX + y * pointsX + x);
        };

        std::vector<float> field(static_cast<std::size_t>(pointsX * pointsY * pointsZ), 0.f);

        auto sampleField = [&sourceBlobs](const IsoVec3& p) {
            float value = 0.f;
            for (const FieldBlob& blob : sourceBlobs)
            {
                const float dx = p.x - blob.center.x;
                const float dy = p.y - blob.center.y;
                const float dz = p.z - blob.center.z;
                const float distSq = dx * dx + dy * dy + dz * dz;
                const float supportSq = blob.supportRadius * blob.supportRadius;
                if (distSq >= supportSq)
                    continue;

                const float q = distSq / supportSq;
                const float oneMinusQ = 1.f - q;
                value += oneMinusQ * oneMinusQ * oneMinusQ;
            }
            return value;
        };

        for (int z = 0; z < pointsZ; ++z)
        {
            for (int y = 0; y < pointsY; ++y)
            {
                for (int x = 0; x < pointsX; ++x)
                {
                    const IsoVec3 point{
                        boundsMin.x + static_cast<float>(x) * stepX,
                        boundsMin.y + static_cast<float>(y) * stepY,
                        boundsMin.z + static_cast<float>(z) * stepZ
                    };
                    field[fieldIndex(x, y, z)] = sampleField(point);
                }
            }
        }

        constexpr std::array<std::array<int, 3>, 8> cubeCornerOffset{{
            {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
            {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}
        }};
        constexpr std::array<std::array<int, 4>, 6> tetrahedra{{
            {0, 5, 1, 6},
            {0, 1, 2, 6},
            {0, 2, 3, 6},
            {0, 3, 7, 6},
            {0, 7, 4, 6},
            {0, 4, 5, 6}
        }};

        triangles.reserve(static_cast<std::size_t>(cellsX * cellsY * cellsZ) * 12);

        for (int z = 0; z < cellsZ; ++z)
        {
            for (int y = 0; y < cellsY; ++y)
            {
                for (int x = 0; x < cellsX; ++x)
                {
                    std::array<IsoVec3, 8> cubePos{};
                    std::array<float, 8> cubeVal{};

                    for (int c = 0; c < 8; ++c)
                    {
                        const int gx = x + cubeCornerOffset[c][0];
                        const int gy = y + cubeCornerOffset[c][1];
                        const int gz = z + cubeCornerOffset[c][2];

                        cubePos[c] = IsoVec3{
                            boundsMin.x + static_cast<float>(gx) * stepX,
                            boundsMin.y + static_cast<float>(gy) * stepY,
                            boundsMin.z + static_cast<float>(gz) * stepZ
                        };
                        cubeVal[c] = field[fieldIndex(gx, gy, gz)];
                    }

                    for (const auto& tetra : tetrahedra)
                    {
                        const std::array<IsoVec3, 4> tp{
                            cubePos[tetra[0]],
                            cubePos[tetra[1]],
                            cubePos[tetra[2]],
                            cubePos[tetra[3]]
                        };
                        const std::array<float, 4> tv{
                            cubeVal[tetra[0]],
                            cubeVal[tetra[1]],
                            cubeVal[tetra[2]],
                            cubeVal[tetra[3]]
                        };
                        polygoniseTetra(tp, tv, isoThreshold, triangles);
                    }
                }
            }
        }

        return triangles;
    };

    std::vector<FieldBlob> torsoFieldBlobs;
    torsoFieldBlobs.reserve(blocks.size() * 8);

    for (std::size_t i = 0; i < centers.size(); ++i)
    {
        if (!torsoBlock[i])
            continue;

        const Pos p = centers[i];
        addFieldBlob(torsoFieldBlobs, p.x, p.y + torsoBaseRadius * 0.62f, p.z, torsoBaseRadius);
        addFieldBlob(torsoFieldBlobs, p.x, p.y + torsoBaseRadius * 1.12f, p.z, torsoBaseRadius * 0.74f);
    }

    constexpr float pi = 3.1415926535f;
    for (const auto& c : connections)
    {
        if (c.from >= centers.size() || c.to >= centers.size())
            continue;
        if (c.type != Creature::ConnectionType::Torso)
            continue;

        const Pos a = centers[c.from];
        const Pos b = centers[c.to];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dz = b.z - a.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const int segments = std::max(4, static_cast<int>(dist / (torsoBaseRadius * 0.35f)));

        for (int i = 1; i < segments; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float bulge = 0.72f + 0.08f * std::sin(t * pi);
            const float x = a.x + dx * t;
            const float y = a.y + dy * t + torsoBaseRadius * 0.62f;
            const float z = a.z + dz * t;
            addFieldBlob(torsoFieldBlobs, x, y, z, torsoBaseRadius * bulge * 0.62f);
        }
    }

    std::vector<IsoVec3> torsoTriangles = extractIsoTriangles(torsoFieldBlobs, torsoBaseRadius, 0.52f);

    const float limbRadius = torsoBaseRadius * 0.75f;
    std::vector<FieldBlob> limbFieldBlobs;
    limbFieldBlobs.reserve(connections.size() * 10 + blocks.size() * 2);

    for (const auto& c : connections)
    {
        if (c.from >= centers.size() || c.to >= centers.size())
            continue;
        if (c.type != Creature::ConnectionType::Limb)
            continue;

        const Pos a = centers[c.from];
        const Pos b = centers[c.to];
        const float ax = a.x;
        const float ay = a.y + torsoBaseRadius * 0.5f;
        const float az = a.z;
        const float bx = b.x;
        const float by = b.y + torsoBaseRadius * 0.5f;
        const float bz = b.z;

        const float dx = bx - ax;
        const float dy = by - ay;
        const float dz = bz - az;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const int steps = std::max(2, static_cast<int>(dist / std::max(0.0001f, limbRadius * 0.55f)));

        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float x = ax + dx * t;
            const float y = ay + dy * t;
            const float z = az + dz * t;
            addFieldBlob(limbFieldBlobs, x, y, z, limbRadius * 0.95f);
        }
    }

    for (std::size_t i = 0; i < centers.size(); ++i)
    {
        if (torsoBlock[i])
            continue;
        const Pos p = centers[i];
        addFieldBlob(limbFieldBlobs, p.x, p.y + torsoBaseRadius * 0.45f, p.z, limbRadius * 0.9f);
    }

    std::vector<IsoVec3> limbTriangles = extractIsoTriangles(limbFieldBlobs, limbRadius, 0.46f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.1f);
    glColor3f(1.f, 0.9f, 0.15f);
    if (!torsoTriangles.empty())
    {
        glBegin(GL_TRIANGLES);
        for (const IsoVec3& vertex : torsoTriangles)
            glVertex3f(vertex.x, vertex.y, vertex.z);
        glEnd();
    }
    else
    {
        for (std::size_t i = 0; i < centers.size(); ++i)
        {
            if (!torsoBlock[i])
                continue;
            const Pos p = centers[i];
            drawSphere(p.x, p.y + torsoBaseRadius * 0.7f, p.z, torsoBaseRadius * 0.84f, 5, 7);
        }
    }

    if (!limbTriangles.empty())
    {
        glBegin(GL_TRIANGLES);
        for (const IsoVec3& vertex : limbTriangles)
            glVertex3f(vertex.x, vertex.y, vertex.z);
        glEnd();
    }

    // Do not leak polygon line mode into later rendering passes.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void GamingState::drawSphere(float cx, float cy, float cz, float radius, int stacks, int slices) const
{
    constexpr float pi = 3.1415926535f;

    glBegin(GL_TRIANGLES);
    for (int stack = 0; stack < stacks; ++stack)
    {
        const float v0 = static_cast<float>(stack) / static_cast<float>(stacks);
        const float v1 = static_cast<float>(stack + 1) / static_cast<float>(stacks);
        const float phi0 = -pi * 0.5f + pi * v0;
        const float phi1 = -pi * 0.5f + pi * v1;
        const float sinPhi0 = std::sin(phi0);
        const float sinPhi1 = std::sin(phi1);
        const float cosPhi0 = std::cos(phi0);
        const float cosPhi1 = std::cos(phi1);

        for (int slice = 0; slice < slices; ++slice)
        {
            const float u0 = static_cast<float>(slice) / static_cast<float>(slices);
            const float u1 = static_cast<float>(slice + 1) / static_cast<float>(slices);
            const float theta0 = 2.f * pi * u0;
            const float theta1 = 2.f * pi * u1;
            const float cosTheta0 = std::cos(theta0);
            const float sinTheta0 = std::sin(theta0);
            const float cosTheta1 = std::cos(theta1);
            const float sinTheta1 = std::sin(theta1);

            const float x00 = cx + radius * cosPhi0 * cosTheta0;
            const float y00 = cy + radius * sinPhi0;
            const float z00 = cz + radius * cosPhi0 * sinTheta0;

            const float x01 = cx + radius * cosPhi0 * cosTheta1;
            const float y01 = y00;
            const float z01 = cz + radius * cosPhi0 * sinTheta1;

            const float x10 = cx + radius * cosPhi1 * cosTheta0;
            const float y10 = cy + radius * sinPhi1;
            const float z10 = cz + radius * cosPhi1 * sinTheta0;

            const float x11 = cx + radius * cosPhi1 * cosTheta1;
            const float y11 = y10;
            const float z11 = cz + radius * cosPhi1 * sinTheta1;

            glVertex3f(x00, y00, z00);
            glVertex3f(x10, y10, z10);
            glVertex3f(x11, y11, z11);

            glVertex3f(x00, y00, z00);
            glVertex3f(x11, y11, z11);
            glVertex3f(x01, y01, z01);
        }
    }
    glEnd();
}

sf::Vector2f GamingState::computeSpawnAnchor() const
{
    if (mContext.terrain == nullptr || !mContext.terrain->hasSpawnCell())
        return sf::Vector2f(0.f, 0.f);

    const int divisions = mContext.terrain->divisions();
    const float mapSize = mContext.terrain->mapSize();
    const float mapMin = -0.5f * mapSize;
    const float cellCenterOffset = 0.5f;
    const float x = mapMin + mapSize
        * (static_cast<float>(mContext.terrain->spawnCellX()) + cellCenterOffset)
        / static_cast<float>(divisions);
    const float z = mapMin + mapSize
        * (static_cast<float>(mContext.terrain->spawnCellZ()) + cellCenterOffset)
        / static_cast<float>(divisions);
    return sf::Vector2f(x, z);
}

sf::Vector2f GamingState::computeDestinationAnchor() const
{
    if (mContext.terrain == nullptr || !mContext.terrain->hasDestinationCell())
        return sf::Vector2f(0.f, 0.f);

    const int divisions = mContext.terrain->divisions();
    const float mapSize = mContext.terrain->mapSize();
    const float mapMin = -0.5f * mapSize;
    const float cellCenterOffset = 0.5f;
    const float x = mapMin + mapSize
        * (static_cast<float>(mContext.terrain->destinationCellX()) + cellCenterOffset)
        / static_cast<float>(divisions);
    const float z = mapMin + mapSize
        * (static_cast<float>(mContext.terrain->destinationCellZ()) + cellCenterOffset)
        / static_cast<float>(divisions);
    return sf::Vector2f(x, z);
}

sf::Vector3f GamingState::computeCenterOfMass() const
{
    return computeCenterOfMass(mRagdollNodes);
}

sf::Vector3f GamingState::computeCenterOfMass(const std::vector<RagdollNode>& nodes) const
{
    if (nodes.empty())
        return sf::Vector3f(0.f, 0.f, 0.f);

    float totalMass = 0.f;
    sf::Vector3f sum{0.f, 0.f, 0.f};
    for (const auto& node : nodes)
    {
        sum.x += node.position.x * node.mass;
        sum.y += node.position.y * node.mass;
        sum.z += node.position.z * node.mass;
        totalMass += node.mass;
    }

    if (totalMass <= 0.0001f)
        return sf::Vector3f(0.f, 0.f, 0.f);

    return sf::Vector3f(sum.x / totalMass, sum.y / totalMass, sum.z / totalMass);
}

bool GamingState::initializeAgentPhysics(std::vector<RagdollNode>& nodes,
                                         std::vector<RagdollConstraint>& constraints,
                                         std::vector<LimbActuator>& actuators,
                                         std::vector<float>& limbPaddlePhase,
                                         std::vector<float>& limbDriveState) const
{
    if (mContext.creature == nullptr || mContext.creature->empty())
        return false;

    const auto& blocks = mContext.creature->blocks();
    const auto& connections = mContext.creature->connections();
    const auto scaledStiffness = [](float base, float minValue = 0.06f) {
        return std::max(minValue, base);
    };

    std::vector<std::vector<std::size_t>> torsoAdj(blocks.size());
    for (const auto& connection : connections)
    {
        if (connection.type != Creature::ConnectionType::Torso)
            continue;
        if (connection.from >= blocks.size() || connection.to >= blocks.size())
            continue;
        torsoAdj[connection.from].push_back(connection.to);
        torsoAdj[connection.to].push_back(connection.from);
    }

    std::vector<std::size_t> torsoComponentSize(blocks.size(), 1);
    std::vector<bool> visited(blocks.size(), false);
    std::vector<std::size_t> stack;
    stack.reserve(blocks.size());

    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        if (visited[i])
            continue;

        if (torsoAdj[i].empty())
        {
            visited[i] = true;
            torsoComponentSize[i] = 1;
            continue;
        }

        std::vector<std::size_t> component;
        stack.push_back(i);
        visited[i] = true;
        while (!stack.empty())
        {
            const std::size_t v = stack.back();
            stack.pop_back();
            component.push_back(v);

            for (const std::size_t n : torsoAdj[v])
            {
                if (visited[n])
                    continue;
                visited[n] = true;
                stack.push_back(n);
            }
        }

        const std::size_t componentSize = std::max<std::size_t>(1, component.size());
        for (const std::size_t idx : component)
            torsoComponentSize[idx] = componentSize;
    }

    int minX = blocks.front().gx;
    int maxX = blocks.front().gx;
    int minY = blocks.front().gy;
    int maxY = blocks.front().gy;
    for (const auto& block : blocks)
    {
        minX = std::min(minX, block.gx);
        maxX = std::max(maxX, block.gx);
        minY = std::min(minY, block.gy);
        maxY = std::max(maxY, block.gy);
    }

    constexpr float cellStep = Terrain::kDefaultMapSize / static_cast<float>(Terrain::kDefaultDivisions);
    const float cx = static_cast<float>(minX + maxX) * 0.5f;
    const float cy = static_cast<float>(minY + maxY) * 0.5f;
    const sf::Vector2f anchor = computeSpawnAnchor();

    std::vector<sf::Vector2f> horizontal;
    horizontal.reserve(blocks.size());
    float maxGround = -std::numeric_limits<float>::max();
    for (const auto& block : blocks)
    {
        const float x = anchor.x + (static_cast<float>(block.gx) - cx) * cellStep;
        const float z = anchor.y + (static_cast<float>(block.gy) - cy) * cellStep;
        horizontal.push_back(sf::Vector2f(x, z));
        maxGround = std::max(maxGround, sampleTerrainHeight(x, z) + 0.02f);
    }

    const float startY = maxGround + 1.25f;
    nodes.assign(blocks.size(), RagdollNode{});
    constexpr float baseMeatMass = 1.f;
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].position = sf::Vector3f(horizontal[i].x, startY, horizontal[i].y);
        nodes[i].previousPosition = nodes[i].position;
        const float effectiveMass = baseMeatMass * static_cast<float>(torsoComponentSize[i]);
        nodes[i].mass = effectiveMass;
        nodes[i].inverseMass = 1.f / std::max(0.0001f, effectiveMass);
    }

    constraints.clear();
    constraints.reserve(connections.size() + blocks.size() * 4);

    auto hasConstraint = [&constraints](std::size_t a, std::size_t b) {
        return std::any_of(constraints.begin(), constraints.end(),
                           [a, b](const RagdollConstraint& c) {
                               return (c.a == a && c.b == b) || (c.a == b && c.b == a);
                           });
    };

    auto addConstraint = [&nodes, &constraints, &hasConstraint, this](std::size_t a, std::size_t b, float stiffness) {
        if (a == b || a >= nodes.size() || b >= nodes.size())
            return;
        if (hasConstraint(a, b))
            return;

        const sf::Vector3f delta = sf::Vector3f(
            nodes[b].position.x - nodes[a].position.x,
            nodes[b].position.y - nodes[a].position.y,
            nodes[b].position.z - nodes[a].position.z
        );
        const float rest = vectorLength(delta);
        if (rest <= 0.0001f)
            return;

        RagdollConstraint c;
        c.a = a;
        c.b = b;
        c.restLength = rest;
        c.baseRestLength = rest;
        c.stiffness = stiffness;
        constraints.push_back(c);
    };

    for (const auto& connection : connections)
    {
        const float stiffness = (connection.type == Creature::ConnectionType::Torso)
            ? scaledStiffness(0.78f)
            : scaledStiffness(0.58f);
        addConstraint(connection.from, connection.to, stiffness);
    }

    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        for (std::size_t j = i + 1; j < blocks.size(); ++j)
        {
            const int dx = std::abs(blocks[i].gx - blocks[j].gx);
            const int dz = std::abs(blocks[i].gy - blocks[j].gy);
            if (dx + dz == 1)
                addConstraint(i, j, scaledStiffness(0.72f));
            else if (dx == 1 && dz == 1)
                addConstraint(i, j, scaledStiffness(0.34f));
        }
    }

    actuators.clear();
    actuators.reserve(connections.size());
    for (const auto& connection : connections)
    {
        if (connection.type != Creature::ConnectionType::Limb)
            continue;
        if (connection.from >= blocks.size() || connection.to >= blocks.size() || connection.from == connection.to)
            continue;

        LimbActuator actuator;
        actuator.a = connection.from;
        actuator.b = connection.to;
        actuators.push_back(actuator);
    }

    std::vector<std::vector<std::size_t>> limbNeighbors(blocks.size());
    for (const auto& actuator : actuators)
    {
        if (actuator.a >= blocks.size() || actuator.b >= blocks.size())
            continue;
        limbNeighbors[actuator.a].push_back(actuator.b);
        limbNeighbors[actuator.b].push_back(actuator.a);
    }

    auto findActuatorIndex = [&actuators](std::size_t u, std::size_t v) -> int {
        for (std::size_t i = 0; i < actuators.size(); ++i)
        {
            const bool same = (actuators[i].a == u && actuators[i].b == v)
                || (actuators[i].a == v && actuators[i].b == u);
            if (same)
                return static_cast<int>(i);
        }
        return -1;
    };

    for (std::size_t joint = 0; joint < limbNeighbors.size(); ++joint)
    {
        auto& neighbors = limbNeighbors[joint];
        if (neighbors.size() < 2)
            continue;

        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());

        for (std::size_t i = 0; i < neighbors.size(); ++i)
        {
            for (std::size_t j = i + 1; j < neighbors.size(); ++j)
            {
                const std::size_t a = neighbors[i];
                const std::size_t b = neighbors[j];
                if (a >= nodes.size() || b >= nodes.size())
                    continue;
                if (hasConstraint(a, b))
                    continue;

                const int driveA = findActuatorIndex(joint, a);
                const int driveB = findActuatorIndex(joint, b);
                if (driveA < 0 || driveB < 0 || driveA == driveB)
                    continue;

                const sf::Vector3f delta = sf::Vector3f(
                    nodes[b].position.x - nodes[a].position.x,
                    nodes[b].position.y - nodes[a].position.y,
                    nodes[b].position.z - nodes[a].position.z
                );
                const float rest = vectorLength(delta);
                if (rest <= 0.0001f)
                    continue;

                RagdollConstraint bend;
                bend.a = a;
                bend.b = b;
                bend.restLength = rest;
                bend.baseRestLength = rest;
                bend.stiffness = scaledStiffness(0.33f, 0.04f);
                bend.bendJoint = joint;
                bend.bendDriveA = driveA;
                bend.bendDriveB = driveB;
                constraints.push_back(bend);
            }
        }
    }

    limbPaddlePhase.clear();
    limbDriveState.clear();
    limbPaddlePhase.reserve(actuators.size());
    limbDriveState.reserve(actuators.size());
    constexpr float pi = 3.1415926535f;
    for (std::size_t i = 0; i < actuators.size(); ++i)
    {
        const float phase = (i % 2 == 0 ? 0.f : pi) + static_cast<float>(i / 2) * (pi * 0.35f);
        limbPaddlePhase.push_back(phase);
        limbDriveState.push_back(0.f);
    }

    return !nodes.empty();
}

NeuralNetwork::BuildSpec GamingState::mutatedGenome(const NeuralNetwork::BuildSpec& base)
{
    NeuralNetwork::BuildSpec mutated = base;
    std::bernoulli_distribution mutateRoll(kMutationRate);
    std::normal_distribution<float> noise(0.f, kMutationSigma);
    for (auto& connection : mutated.connections)
    {
        if (!mutateRoll(mPopulationRng))
            continue;
        connection.weight = std::clamp(connection.weight + noise(mPopulationRng), -4.f, 4.f);
    }
    return mutated;
}

bool GamingState::initializePopulationFromGenome(const NeuralNetwork::BuildSpec& genome, bool includeUnmutatedMother)
{
    mPopulation.clear();
    mPopulation.resize(kPopulationSize);

    for (std::size_t i = 0; i < mPopulation.size(); ++i)
    {
        NeuralNetwork::BuildSpec childGenome = includeUnmutatedMother && i == 0
            ? genome
            : mutatedGenome(genome);

        std::string buildError;
        if (!mPopulation[i].network.build(childGenome, &buildError))
            return false;

        if (!initializeAgentPhysics(mPopulation[i].nodes,
                                    mPopulation[i].constraints,
                                    mPopulation[i].actuators,
                                    mPopulation[i].limbPaddlePhase,
                                    mPopulation[i].limbDriveState))
            return false;

        mPopulation[i].distanceToGoal = 1e9f;
        mPopulation[i].reachedGoal = false;
        mPopulation[i].hasPreviousCenterOfMass = false;
    }

    mGenerationTimeLeft = kGenerationDuration;
    mPopulationAccumulator = 0.f;
    mPopulationRunning = true;
    mGenerationWinnerIndex.reset();
    mRenderAgentIndex = 0;
    mLastPopulationSubsteps = 0;

    mRagdollNodes = mPopulation.front().nodes;
    mRagdollConstraints = mPopulation.front().constraints;
    mLimbActuators = mPopulation.front().actuators;
    mLimbPaddlePhase = mPopulation.front().limbPaddlePhase;
    mLimbDriveState = mPopulation.front().limbDriveState;
    mRagdollActive = true;
    return true;
}

void GamingState::startPopulationTraining()
{
    if (mContext.neuralNetwork == nullptr || !mContext.neuralNetwork->isCompiled())
        return;
    if (mContext.creature == nullptr || mContext.creature->empty())
        return;
    if (mContext.terrain == nullptr || !mContext.terrain->hasSpawnCell() || !mContext.terrain->hasDestinationCell())
        return;

    startAgentWorkers();

    mMotherGenome = mContext.neuralNetwork->buildSpec();
    mGenerationIndex = 1;
    if (!initializePopulationFromGenome(*mMotherGenome, true))
    {
        mPopulationRunning = false;
        mPopulation.clear();
        mPopulationAccumulator = 0.f;
        mLastPopulationSubsteps = 0;
    }
}

void GamingState::stepPopulation(sf::Time dt)
{
    if (!mPopulationRunning || mPopulation.empty())
    {
        mLastPopulationSubsteps = 0;
        mPopulationAccumulator = 0.f;
        return;
    }

    const float speedMultiplier = mSpeedhackEnabled ? kTrainingSpeedMultiplier : 1.f;
    const float scaled = std::clamp(dt.asSeconds() * speedMultiplier, 0.f, 0.8f);
    mPopulationAccumulator += scaled;
    std::size_t substeps = 0;
    while (mPopulationRunning
           && mPopulationAccumulator >= kPopulationStepSeconds
           && substeps < kMaxPopulationStepsPerUpdate)
    {
        advancePopulationStep(kPopulationStepSeconds);
        mPopulationAccumulator -= kPopulationStepSeconds;
        ++substeps;
    }
    mLastPopulationSubsteps = substeps;

    if (substeps >= kMaxPopulationStepsPerUpdate)
        mPopulationAccumulator = 0.f;
}

void GamingState::advancePopulationStep(float dtSeconds)
{
    if (!mPopulationRunning || mPopulation.empty() || dtSeconds <= 0.f)
        return;

    const sf::Vector2f destination = computeDestinationAnchor();
    mGenerationTimeLeft = std::max(0.f, mGenerationTimeLeft - dtSeconds);

    float bestDistance = std::numeric_limits<float>::max();
    std::size_t bestIndex = 0;
    std::optional<std::size_t> firstWinner;
    std::size_t requiredInputCount = 0;
    std::size_t requiredOutputCount = 0;
    if (mContext.creature != nullptr)
    {
        requiredInputCount = mContext.creature->recommendedInputCount();
        requiredOutputCount = mContext.creature->recommendedOutputCount();
    }

    runPopulationWorkers(
        dtSeconds,
        destination,
        requiredInputCount,
        requiredOutputCount,
        bestDistance,
        bestIndex,
        firstWinner
    );

    if (!mGenerationWinnerIndex.has_value() && firstWinner.has_value())
        mGenerationWinnerIndex = firstWinner;

    mRenderAgentIndex = mGenerationWinnerIndex.has_value() ? mGenerationWinnerIndex.value() : bestIndex;
    if (mRenderAgentIndex < mPopulation.size())
    {
        mRagdollNodes = mPopulation[mRenderAgentIndex].nodes;
        mRagdollConstraints = mPopulation[mRenderAgentIndex].constraints;
        mLimbActuators = mPopulation[mRenderAgentIndex].actuators;
        mLimbPaddlePhase = mPopulation[mRenderAgentIndex].limbPaddlePhase;
        mLimbDriveState = mPopulation[mRenderAgentIndex].limbDriveState;
        mRagdollActive = !mRagdollNodes.empty();
    }

    if (!mGenerationWinnerIndex.has_value() && mGenerationTimeLeft > 0.f)
        return;

    const std::size_t motherIndex = mGenerationWinnerIndex.has_value()
        ? mGenerationWinnerIndex.value()
        : bestIndex;
    if (motherIndex >= mPopulation.size())
        return;

    mMotherGenome = mPopulation[motherIndex].network.buildSpec();
    ++mGenerationIndex;
    if (!initializePopulationFromGenome(*mMotherGenome, true))
    {
        mPopulationRunning = false;
        mPopulation.clear();
        mPopulationAccumulator = 0.f;
        mLastPopulationSubsteps = 0;
    }
}

void GamingState::runPopulationWorkers(float dtSeconds,
                                       const sf::Vector2f& destination,
                                       std::size_t requiredInputCount,
                                       std::size_t requiredOutputCount,
                                       float& bestDistance,
                                       std::size_t& bestIndex,
                                       std::optional<std::size_t>& firstWinner)
{
    startAgentWorkers();

    if (mPopulation.empty())
    {
        bestDistance = std::numeric_limits<float>::max();
        bestIndex = 0;
        firstWinner.reset();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mWorkerMutex);
        mWorkerDtSeconds = dtSeconds;
        mWorkerDestination = destination;
        mWorkerRequiredInputCount = requiredInputCount;
        mWorkerRequiredOutputCount = requiredOutputCount;
        mWorkerDoneCount = 0;
        ++mWorkerEpoch;
    }

    mWorkerCv.notify_all();

    {
        std::unique_lock<std::mutex> lock(mWorkerMutex);
        mWorkerDoneCv.wait(lock, [this]() { return mWorkerDoneCount >= kAgentWorkerCount; });
    }

    bestDistance = std::numeric_limits<float>::max();
    bestIndex = 0;
    firstWinner.reset();
    for (std::size_t i = 0; i < kAgentWorkerCount; ++i)
    {
        if (mWorkerBestDistance[i] < bestDistance)
        {
            bestDistance = mWorkerBestDistance[i];
            bestIndex = mWorkerBestIndex[i];
        }

        if (mWorkerWinner[i].has_value())
        {
            if (!firstWinner.has_value() || mWorkerWinner[i].value() < firstWinner.value())
                firstWinner = mWorkerWinner[i];
        }
    }
}

void GamingState::agentWorkerLoop(std::size_t workerIndex)
{
    std::size_t seenEpoch = 0;
    while (true)
    {
        float dtSeconds = 0.f;
        sf::Vector2f destination{0.f, 0.f};
        std::size_t requiredInputCount = 0;
        std::size_t requiredOutputCount = 0;

        {
            std::unique_lock<std::mutex> lock(mWorkerMutex);
            mWorkerCv.wait(lock, [this, seenEpoch]() { return mWorkerStop || mWorkerEpoch != seenEpoch; });
            if (mWorkerStop)
                return;
            seenEpoch = mWorkerEpoch;
            dtSeconds = mWorkerDtSeconds;
            destination = mWorkerDestination;
            requiredInputCount = mWorkerRequiredInputCount;
            requiredOutputCount = mWorkerRequiredOutputCount;
        }

        const std::size_t begin = workerIndex * kAgentsPerWorker;
        const std::size_t end = std::min(begin + kAgentsPerWorker, mPopulation.size());

        float localBestDistance = std::numeric_limits<float>::max();
        std::size_t localBestIndex = begin;
        std::optional<std::size_t> localWinner;
        constexpr float goalRadius = 0.06f;

        for (std::size_t i = begin; i < end; ++i)
        {
            auto& agent = mPopulation[i];
            simulateRagdollForAgent(
                sf::seconds(dtSeconds),
                agent,
                destination,
                requiredInputCount,
                requiredOutputCount
            );

            const sf::Vector3f center = computeCenterOfMass(agent.nodes);
            const float dx = destination.x - center.x;
            const float dz = destination.y - center.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            agent.distanceToGoal = distance;

            if (distance < localBestDistance)
            {
                localBestDistance = distance;
                localBestIndex = i;
            }

            if (!localWinner.has_value() && distance <= goalRadius)
            {
                localWinner = i;
                agent.reachedGoal = true;
            }
        }

        std::lock_guard<std::mutex> lock(mWorkerMutex);
        mWorkerBestDistance[workerIndex] = localBestDistance;
        mWorkerBestIndex[workerIndex] = localBestIndex;
        mWorkerWinner[workerIndex] = localWinner;
        ++mWorkerDoneCount;
        if (mWorkerDoneCount >= kAgentWorkerCount)
            mWorkerDoneCv.notify_one();
    }
}

void GamingState::simulateRagdollForAgent(sf::Time dt,
                                          Agent& agent,
                                          const sf::Vector2f& destination,
                                          std::size_t requiredInputCount,
                                          std::size_t requiredOutputCount)
{
    if (agent.nodes.empty())
        return;

    const float dtSeconds = std::clamp(dt.asSeconds(), 0.f, 0.05f);
    if (dtSeconds <= 0.f)
        return;

    const float dtSquared = dtSeconds * dtSeconds;
    constexpr float gravity = 8.6f;
    constexpr float damping = 0.993f;
    std::vector<sf::Vector3f> motorAcceleration(agent.nodes.size(), sf::Vector3f(0.f, 0.f, 0.f));
    applyNeuralMotorControlForAgent(
        dt,
        agent,
        motorAcceleration,
        destination,
        requiredInputCount,
        requiredOutputCount
    );

    for (std::size_t i = 0; i < agent.nodes.size(); ++i)
    {
        auto& node = agent.nodes[i];
        const float nodeDamping = 1.f - (1.f - damping) * std::clamp(node.inverseMass, 0.08f, 1.f);
        const float vx = (node.position.x - node.previousPosition.x) * nodeDamping;
        const float vy = (node.position.y - node.previousPosition.y) * nodeDamping;
        const float vz = (node.position.z - node.previousPosition.z) * nodeDamping;

        node.previousPosition = node.position;
        node.position.x += vx + motorAcceleration[i].x * dtSquared;
        node.position.y += vy + motorAcceleration[i].y * dtSquared - gravity * dtSquared;
        node.position.z += vz + motorAcceleration[i].z * dtSquared;
    }

    constexpr int solverIterations = 6;
    constexpr float epsilon = 0.0001f;
    for (int iter = 0; iter < solverIterations; ++iter)
    {
        for (const auto& c : agent.constraints)
        {
            if (c.a >= agent.nodes.size() || c.b >= agent.nodes.size())
                continue;
            auto& a = agent.nodes[c.a];
            auto& b = agent.nodes[c.b];

            const float dx = b.position.x - a.position.x;
            const float dy = b.position.y - a.position.y;
            const float dz = b.position.z - a.position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float inverseMassSum = a.inverseMass + b.inverseMass;
            if (dist <= epsilon || inverseMassSum <= epsilon)
                continue;

            const float error = (dist - c.restLength) / dist;
            const float correctionScale = error * c.stiffness;
            const float weightA = a.inverseMass / inverseMassSum;
            const float weightB = b.inverseMass / inverseMassSum;
            const float cxA = dx * correctionScale * weightA;
            const float cyA = dy * correctionScale * weightA;
            const float czA = dz * correctionScale * weightA;
            const float cxB = dx * correctionScale * weightB;
            const float cyB = dy * correctionScale * weightB;
            const float czB = dz * correctionScale * weightB;

            a.position.x += cxA;
            a.position.y += cyA;
            a.position.z += czA;
            b.position.x -= cxB;
            b.position.y -= cyB;
            b.position.z -= czB;
        }

        for (auto& node : agent.nodes)
            applyGroundAndBounds(node);
    }
}

void GamingState::applyNeuralMotorControlForAgent(sf::Time dt,
                                                  Agent& agent,
                                                  std::vector<sf::Vector3f>& motorAcceleration,
                                                  const sf::Vector2f& destination,
                                                  std::size_t requiredInputCount,
                                                  std::size_t requiredOutputCount)
{
    if (!agent.network.isCompiled())
        return;
    if (mContext.creature == nullptr || mContext.creature->empty())
        return;
    if (mContext.terrain == nullptr || !mContext.terrain->hasDestinationCell())
        return;
    if (motorAcceleration.size() != agent.nodes.size())
        return;

    if (requiredInputCount == 0 || requiredOutputCount == 0)
        return;
    if (agent.network.inputCount() != requiredInputCount
        || agent.network.outputCount() != requiredOutputCount)
        return;

    const float dtSeconds = std::clamp(dt.asSeconds(), 0.0001f, 0.05f);
    const sf::Vector3f centerOfMass = computeCenterOfMass(agent.nodes);
    sf::Vector3f velocity{0.f, 0.f, 0.f};
    if (agent.hasPreviousCenterOfMass)
    {
        velocity.x = (centerOfMass.x - agent.previousCenterOfMass.x) / dtSeconds;
        velocity.y = (centerOfMass.y - agent.previousCenterOfMass.y) / dtSeconds;
        velocity.z = (centerOfMass.z - agent.previousCenterOfMass.z) / dtSeconds;
    }
    agent.previousCenterOfMass = centerOfMass;
    agent.hasPreviousCenterOfMass = true;

    const float toGoalX = destination.x - centerOfMass.x;
    const float toGoalZ = destination.y - centerOfMass.z;
    const float goalDistance = std::sqrt(toGoalX * toGoalX + toGoalZ * toGoalZ);
    const float invGoalDistance = (goalDistance > 0.0001f) ? (1.f / goalDistance) : 0.f;
    const float goalDirX = toGoalX * invGoalDistance;
    const float goalDirZ = toGoalZ * invGoalDistance;
    const float centerGround = sampleTerrainHeight(centerOfMass.x, centerOfMass.z) + 0.02f;
    const float centerHeight = centerOfMass.y - centerGround;

    std::vector<float> inputs;
    inputs.reserve(requiredInputCount);
    inputs.push_back(goalDirX);
    inputs.push_back(goalDirZ);
    inputs.push_back(std::clamp(goalDistance / 2.f, 0.f, 1.f));
    inputs.push_back(std::clamp(velocity.x, -1.5f, 1.5f));
    inputs.push_back(std::clamp(velocity.z, -1.5f, 1.5f));
    inputs.push_back(std::clamp(centerHeight, -1.f, 1.f));

    const auto nodeGroundContact = [this](const RagdollNode& node) {
        const float ground = sampleTerrainHeight(node.position.x, node.position.z) + 0.02f;
        return node.position.y <= ground + 0.05f;
    };

    if (agent.actuators.empty())
    {
        const bool grounded = centerOfMass.y <= centerGround + 0.06f;
        inputs.push_back(grounded ? 1.f : 0.f);
    }
    else
    {
        for (std::size_t i = 0; i < requiredOutputCount; ++i)
        {
            if (i >= agent.actuators.size())
            {
                inputs.push_back(0.f);
                continue;
            }

            const auto& actuator = agent.actuators[i];
            if (actuator.a >= agent.nodes.size() || actuator.b >= agent.nodes.size())
            {
                inputs.push_back(0.f);
                continue;
            }

            const bool contactA = nodeGroundContact(agent.nodes[actuator.a]);
            const bool contactB = nodeGroundContact(agent.nodes[actuator.b]);
            const float contact = (contactA && contactB) ? 1.f : ((contactA || contactB) ? 0.5f : 0.f);
            inputs.push_back(contact);
        }
    }

    if (inputs.size() < requiredInputCount)
        inputs.resize(requiredInputCount, 0.f);
    if (inputs.size() != requiredInputCount)
        return;

    const std::vector<float> outputs = agent.network.forward(inputs);
    if (outputs.size() != requiredOutputCount)
        return;

    if (agent.limbPaddlePhase.size() != agent.actuators.size())
        agent.limbPaddlePhase.assign(agent.actuators.size(), 0.f);
    if (agent.limbDriveState.size() != agent.actuators.size())
        agent.limbDriveState.assign(agent.actuators.size(), 0.f);

    for (auto& constraint : agent.constraints)
        constraint.restLength = constraint.baseRestLength;

    if (goalDistance < 0.02f)
        return;

    constexpr float fallbackMotorStrength = 10.5f;
    if (agent.actuators.empty())
    {
        if (outputs.empty())
            return;

        const float contact = std::clamp(inputs[Creature::kBaseSensorInputCount], 0.f, 1.f);
        const float drive = std::clamp(outputs[0] * 2.f - 1.f, -1.f, 1.f);
        const float accel = drive * contact * fallbackMotorStrength;
        for (std::size_t i = 0; i < agent.nodes.size(); ++i)
        {
            const float mobility = std::clamp(agent.nodes[i].inverseMass, 0.08f, 1.f);
            motorAcceleration[i].x += goalDirX * accel * mobility;
            motorAcceleration[i].z += goalDirZ * accel * mobility;
        }
        return;
    }

    const std::size_t actuatorCount = std::min(outputs.size(), agent.actuators.size());
    constexpr float pi = 3.1415926535f;
    constexpr float twoPi = 2.f * pi;
    constexpr float baseCadenceHz = 1.25f;
    constexpr float cadenceGainHz = 2.6f;
    for (std::size_t i = 0; i < actuatorCount; ++i)
    {
        const auto& actuator = agent.actuators[i];
        if (actuator.a >= agent.nodes.size() || actuator.b >= agent.nodes.size())
            continue;

        const float rawDrive = std::clamp(outputs[i] * 2.f - 1.f, -1.f, 1.f);
        const float driveAlpha = std::clamp(dtSeconds * 10.f, 0.f, 1.f);
        agent.limbDriveState[i] += (rawDrive - agent.limbDriveState[i]) * driveAlpha;
        const float drive = std::clamp(agent.limbDriveState[i], -1.f, 1.f);
        const float contact = std::clamp(inputs[Creature::kBaseSensorInputCount + i], 0.f, 1.f);

        const float cadence = baseCadenceHz + cadenceGainHz * std::abs(drive);
        agent.limbPaddlePhase[i] += dtSeconds * cadence * twoPi;
        while (agent.limbPaddlePhase[i] >= twoPi)
            agent.limbPaddlePhase[i] -= twoPi;

        const float stroke = std::sin(agent.limbPaddlePhase[i]);
        const float amplitude = 0.35f + 0.65f * std::abs(drive);
        const float powerPhase = std::max(0.f, stroke) * amplitude;
        const float recoveryPhase = std::max(0.f, -stroke) * amplitude;

        for (auto& constraint : agent.constraints)
        {
            const bool samePair = (constraint.a == actuator.a && constraint.b == actuator.b)
                || (constraint.a == actuator.b && constraint.b == actuator.a);
            if (!samePair)
                continue;

            const float extensionScale = std::clamp(1.f + 0.13f * powerPhase - 0.22f * recoveryPhase, 0.70f, 1.28f);
            constraint.restLength = constraint.baseRestLength * extensionScale;
            break;
        }

        const std::size_t foot = (agent.nodes[actuator.a].position.y <= agent.nodes[actuator.b].position.y)
            ? actuator.a
            : actuator.b;
        const std::size_t hip = (foot == actuator.a) ? actuator.b : actuator.a;
        const bool footGrounded = nodeGroundContact(agent.nodes[foot]);

        const float mobilityFoot = std::clamp(agent.nodes[foot].inverseMass, 0.08f, 1.f);
        const float mobilityHip = std::clamp(agent.nodes[hip].inverseMass, 0.08f, 1.f);
        const float sideX = -goalDirZ;
        const float sideZ = goalDirX;
        const float sideSign = (i % 2 == 0) ? 1.f : -1.f;

        if (footGrounded && powerPhase > 0.f)
        {
            const float push = powerPhase * (0.35f + 0.65f * contact) * 11.5f;
            motorAcceleration[hip].x += goalDirX * push * mobilityHip;
            motorAcceleration[hip].z += goalDirZ * push * mobilityHip;
            motorAcceleration[hip].x += sideX * sideSign * (powerPhase * 1.1f) * mobilityHip;
            motorAcceleration[hip].z += sideZ * sideSign * (powerPhase * 1.1f) * mobilityHip;
            motorAcceleration[foot].x -= goalDirX * push * 0.58f * mobilityFoot;
            motorAcceleration[foot].z -= goalDirZ * push * 0.58f * mobilityFoot;
            motorAcceleration[foot].x -= sideX * sideSign * (powerPhase * 1.6f) * mobilityFoot;
            motorAcceleration[foot].z -= sideZ * sideSign * (powerPhase * 1.6f) * mobilityFoot;
            motorAcceleration[foot].y -= powerPhase * 1.1f * mobilityFoot;
            motorAcceleration[hip].y += powerPhase * 0.4f * mobilityHip;
        }

        const float recoveryIntent = footGrounded ? recoveryPhase : std::max(0.12f, recoveryPhase);
        if (recoveryIntent > 0.f)
        {
            motorAcceleration[foot].y += recoveryIntent * 8.8f * mobilityFoot;
            motorAcceleration[foot].x += goalDirX * recoveryIntent * 4.6f * mobilityFoot;
            motorAcceleration[foot].z += goalDirZ * recoveryIntent * 4.6f * mobilityFoot;
            motorAcceleration[foot].x += sideX * sideSign * (recoveryIntent * 2.5f) * mobilityFoot;
            motorAcceleration[foot].z += sideZ * sideSign * (recoveryIntent * 2.5f) * mobilityFoot;

            const float dx = agent.nodes[hip].position.x - agent.nodes[foot].position.x;
            const float dy = agent.nodes[hip].position.y - agent.nodes[foot].position.y;
            const float dz = agent.nodes[hip].position.z - agent.nodes[foot].position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > 0.0001f)
            {
                const float invDist = 1.f / dist;
                const float retract = (0.38f + recoveryIntent) * 3.6f;
                const float dirX = dx * invDist;
                const float dirY = dy * invDist;
                const float dirZ = dz * invDist;
                motorAcceleration[foot].x += dirX * retract * mobilityFoot;
                motorAcceleration[foot].y += dirY * retract * mobilityFoot;
                motorAcceleration[foot].z += dirZ * retract * mobilityFoot;
                motorAcceleration[hip].x -= dirX * retract * 0.26f * mobilityHip;
                motorAcceleration[hip].y -= dirY * retract * 0.26f * mobilityHip;
                motorAcceleration[hip].z -= dirZ * retract * 0.26f * mobilityHip;
            }
        }

        if (!footGrounded && powerPhase > 0.f)
        {
            motorAcceleration[foot].x -= goalDirX * powerPhase * 2.7f * mobilityFoot;
            motorAcceleration[foot].z -= goalDirZ * powerPhase * 2.7f * mobilityFoot;
            motorAcceleration[foot].y -= powerPhase * 0.45f * mobilityFoot;
        }
    }

    for (auto& constraint : agent.constraints)
    {
        if (constraint.bendDriveA < 0 || constraint.bendDriveB < 0)
            continue;
        if (constraint.bendJoint >= agent.nodes.size()
            || constraint.a >= agent.nodes.size()
            || constraint.b >= agent.nodes.size())
            continue;

        const std::size_t driveA = static_cast<std::size_t>(constraint.bendDriveA);
        const std::size_t driveB = static_cast<std::size_t>(constraint.bendDriveB);
        if (driveA >= agent.limbDriveState.size()
            || driveB >= agent.limbDriveState.size()
            || driveA >= agent.limbPaddlePhase.size()
            || driveB >= agent.limbPaddlePhase.size())
            continue;

        const float drive = 0.5f * (std::abs(agent.limbDriveState[driveA]) + std::abs(agent.limbDriveState[driveB]));
        const float phaseBlend = 0.5f * (std::sin(agent.limbPaddlePhase[driveA]) + std::sin(agent.limbPaddlePhase[driveB]));
        const float cyclePulse = std::clamp(0.5f + 0.5f * phaseBlend, 0.f, 1.f);

        const std::size_t inputA = Creature::kBaseSensorInputCount + driveA;
        const std::size_t inputB = Creature::kBaseSensorInputCount + driveB;
        const float contactA = (inputA < inputs.size()) ? std::clamp(inputs[inputA], 0.f, 1.f) : 0.f;
        const float contactB = (inputB < inputs.size()) ? std::clamp(inputs[inputB], 0.f, 1.f) : 0.f;
        const float contact = 0.5f * (contactA + contactB);

        const float airborneBias = 1.f - contact;
        const float bendStrength = (0.07f + 0.30f * drive)
            * (0.45f + 0.55f * cyclePulse)
            * (0.5f + 0.5f * airborneBias);
        const float bendScale = std::clamp(1.f - bendStrength, 0.62f, 1.f);
        constraint.restLength = constraint.baseRestLength * bendScale;

        const sf::Vector3f span = sf::Vector3f(
            agent.nodes[constraint.b].position.x - agent.nodes[constraint.a].position.x,
            agent.nodes[constraint.b].position.y - agent.nodes[constraint.a].position.y,
            agent.nodes[constraint.b].position.z - agent.nodes[constraint.a].position.z
        );
        sf::Vector3f side = normalize3(sf::Vector3f(-span.z, 0.f, span.x));
        if (vectorLength(side) < 0.0001f)
            side = sf::Vector3f(1.f, 0.f, 0.f);

        const float swingSign = (std::sin(0.5f * (agent.limbPaddlePhase[driveA] + agent.limbPaddlePhase[driveB])) >= 0.f)
            ? 1.f
            : -1.f;
        const float lateralPush = bendStrength * (2.1f + 1.6f * cyclePulse) * swingSign;
        const float lift = bendStrength * (4.8f + 2.4f * cyclePulse);

        const float mobilityA = std::clamp(agent.nodes[constraint.a].inverseMass, 0.08f, 1.f);
        const float mobilityB = std::clamp(agent.nodes[constraint.b].inverseMass, 0.08f, 1.f);
        const float mobilityJoint = std::clamp(agent.nodes[constraint.bendJoint].inverseMass, 0.08f, 1.f);

        motorAcceleration[constraint.a].x += side.x * lateralPush * mobilityA;
        motorAcceleration[constraint.a].z += side.z * lateralPush * mobilityA;
        motorAcceleration[constraint.b].x -= side.x * lateralPush * mobilityB;
        motorAcceleration[constraint.b].z -= side.z * lateralPush * mobilityB;

        motorAcceleration[constraint.bendJoint].y += lift * mobilityJoint;
        motorAcceleration[constraint.a].y += lift * 0.25f * mobilityA;
        motorAcceleration[constraint.b].y += lift * 0.25f * mobilityB;
    }
}

void GamingState::startAgentWorkers()
{
    for (std::size_t i = 0; i < kAgentWorkerCount; ++i)
    {
        if (mWorkers[i].joinable())
            return;
    }

    mWorkerStop = false;
    mWorkerEpoch = 0;
    mWorkerDoneCount = 0;
    mWorkerBestDistance.fill(std::numeric_limits<float>::max());
    mWorkerBestIndex.fill(0);
    for (auto& winner : mWorkerWinner)
        winner.reset();

    for (std::size_t i = 0; i < kAgentWorkerCount; ++i)
        mWorkers[i] = std::thread(&GamingState::agentWorkerLoop, this, i);
}

void GamingState::stopAgentWorkers()
{
    {
        std::lock_guard<std::mutex> lock(mWorkerMutex);
        mWorkerStop = true;
        ++mWorkerEpoch;
    }
    mWorkerCv.notify_all();

    for (auto& worker : mWorkers)
    {
        if (worker.joinable())
            worker.join();
    }
}

void GamingState::startRagdollDrop()
{
    if (!initializeAgentPhysics(mRagdollNodes,
                                mRagdollConstraints,
                                mLimbActuators,
                                mLimbPaddlePhase,
                                mLimbDriveState))
        return;
    mHasPreviousCenterOfMass = false;
    mRagdollActive = !mRagdollNodes.empty();
}

void GamingState::applyNeuralMotorControl(sf::Time dt,
                                          std::vector<sf::Vector3f>& motorAcceleration,
                                          const NeuralNetwork* network)
{
    if (network == nullptr || !network->isCompiled())
        return;
    if (mContext.creature == nullptr || mContext.creature->empty())
        return;
    if (mContext.terrain == nullptr || !mContext.terrain->hasDestinationCell())
        return;
    if (motorAcceleration.size() != mRagdollNodes.size())
        return;

    const std::size_t requiredInputCount = mContext.creature->recommendedInputCount();
    const std::size_t requiredOutputCount = mContext.creature->recommendedOutputCount();
    if (requiredInputCount == 0 || requiredOutputCount == 0)
        return;
    if (network->inputCount() != requiredInputCount
        || network->outputCount() != requiredOutputCount)
        return;

    const float dtSeconds = std::clamp(dt.asSeconds(), 0.0001f, 0.05f);
    const sf::Vector3f centerOfMass = computeCenterOfMass();
    sf::Vector3f velocity{0.f, 0.f, 0.f};
    if (mHasPreviousCenterOfMass)
    {
        velocity.x = (centerOfMass.x - mPreviousCenterOfMass.x) / dtSeconds;
        velocity.y = (centerOfMass.y - mPreviousCenterOfMass.y) / dtSeconds;
        velocity.z = (centerOfMass.z - mPreviousCenterOfMass.z) / dtSeconds;
    }
    mPreviousCenterOfMass = centerOfMass;
    mHasPreviousCenterOfMass = true;

    const sf::Vector2f destination = computeDestinationAnchor();
    const float toGoalX = destination.x - centerOfMass.x;
    const float toGoalZ = destination.y - centerOfMass.z;
    const float goalDistance = std::sqrt(toGoalX * toGoalX + toGoalZ * toGoalZ);
    const float invGoalDistance = (goalDistance > 0.0001f) ? (1.f / goalDistance) : 0.f;
    const float goalDirX = toGoalX * invGoalDistance;
    const float goalDirZ = toGoalZ * invGoalDistance;
    const float centerGround = sampleTerrainHeight(centerOfMass.x, centerOfMass.z) + 0.02f;
    const float centerHeight = centerOfMass.y - centerGround;

    std::vector<float> inputs;
    inputs.reserve(requiredInputCount);
    inputs.push_back(goalDirX);
    inputs.push_back(goalDirZ);
    inputs.push_back(std::clamp(goalDistance / 2.f, 0.f, 1.f));
    inputs.push_back(std::clamp(velocity.x, -1.5f, 1.5f));
    inputs.push_back(std::clamp(velocity.z, -1.5f, 1.5f));
    inputs.push_back(std::clamp(centerHeight, -1.f, 1.f));

    const auto nodeGroundContact = [this](const RagdollNode& node) {
        const float ground = sampleTerrainHeight(node.position.x, node.position.z) + 0.02f;
        return node.position.y <= ground + 0.05f;
    };

    if (mLimbActuators.empty())
    {
        const bool grounded = centerOfMass.y <= centerGround + 0.06f;
        inputs.push_back(grounded ? 1.f : 0.f);
    }
    else
    {
        for (std::size_t i = 0; i < requiredOutputCount; ++i)
        {
            if (i >= mLimbActuators.size())
            {
                inputs.push_back(0.f);
                continue;
            }

            const auto& actuator = mLimbActuators[i];
            if (actuator.a >= mRagdollNodes.size() || actuator.b >= mRagdollNodes.size())
            {
                inputs.push_back(0.f);
                continue;
            }

            const bool contactA = nodeGroundContact(mRagdollNodes[actuator.a]);
            const bool contactB = nodeGroundContact(mRagdollNodes[actuator.b]);
            const float contact = (contactA && contactB) ? 1.f : ((contactA || contactB) ? 0.5f : 0.f);
            inputs.push_back(contact);
        }
    }

    if (inputs.size() < requiredInputCount)
        inputs.resize(requiredInputCount, 0.f);
    if (inputs.size() != requiredInputCount)
        return;

    const std::vector<float> outputs = network->forward(inputs);
    if (outputs.size() != requiredOutputCount)
        return;

    if (mLimbPaddlePhase.size() != mLimbActuators.size())
        mLimbPaddlePhase.assign(mLimbActuators.size(), 0.f);
    if (mLimbDriveState.size() != mLimbActuators.size())
        mLimbDriveState.assign(mLimbActuators.size(), 0.f);

    for (auto& constraint : mRagdollConstraints)
        constraint.restLength = constraint.baseRestLength;

    if (goalDistance < 0.02f)
        return;

    constexpr float fallbackMotorStrength = 10.5f;
    if (mLimbActuators.empty())
    {
        if (outputs.empty())
            return;

        const float contact = std::clamp(inputs[Creature::kBaseSensorInputCount], 0.f, 1.f);
        const float drive = std::clamp(outputs[0] * 2.f - 1.f, -1.f, 1.f);
        const float accel = drive * contact * fallbackMotorStrength;
        for (std::size_t i = 0; i < mRagdollNodes.size(); ++i)
        {
            const float mobility = std::clamp(mRagdollNodes[i].inverseMass, 0.08f, 1.f);
            motorAcceleration[i].x += goalDirX * accel * mobility;
            motorAcceleration[i].z += goalDirZ * accel * mobility;
        }
        return;
    }

    const std::size_t actuatorCount = std::min(outputs.size(), mLimbActuators.size());
    constexpr float pi = 3.1415926535f;
    constexpr float twoPi = 2.f * pi;
    constexpr float baseCadenceHz = 1.25f;
    constexpr float cadenceGainHz = 2.6f;
    for (std::size_t i = 0; i < actuatorCount; ++i)
    {
        const auto& actuator = mLimbActuators[i];
        if (actuator.a >= mRagdollNodes.size() || actuator.b >= mRagdollNodes.size())
            continue;

        const float rawDrive = std::clamp(outputs[i] * 2.f - 1.f, -1.f, 1.f);
        const float driveAlpha = std::clamp(dtSeconds * 10.f, 0.f, 1.f);
        mLimbDriveState[i] += (rawDrive - mLimbDriveState[i]) * driveAlpha;
        const float drive = std::clamp(mLimbDriveState[i], -1.f, 1.f);
        const float contact = std::clamp(inputs[Creature::kBaseSensorInputCount + i], 0.f, 1.f);

        const float cadence = baseCadenceHz + cadenceGainHz * std::abs(drive);
        mLimbPaddlePhase[i] += dtSeconds * cadence * twoPi;
        while (mLimbPaddlePhase[i] >= twoPi)
            mLimbPaddlePhase[i] -= twoPi;

        const float stroke = std::sin(mLimbPaddlePhase[i]);
        const float amplitude = 0.35f + 0.65f * std::abs(drive);
        const float powerPhase = std::max(0.f, stroke) * amplitude;
        const float recoveryPhase = std::max(0.f, -stroke) * amplitude;

        // Output i directly controls limb i with paddle cycle:
        // power stroke pushes body, recovery lifts and swings limb forward.
        for (auto& constraint : mRagdollConstraints)
        {
            const bool samePair = (constraint.a == actuator.a && constraint.b == actuator.b)
                || (constraint.a == actuator.b && constraint.b == actuator.a);
            if (!samePair)
                continue;

            const float extensionScale = std::clamp(1.f + 0.13f * powerPhase - 0.22f * recoveryPhase, 0.70f, 1.28f);
            constraint.restLength = constraint.baseRestLength * extensionScale;
            break;
        }

        const std::size_t foot = (mRagdollNodes[actuator.a].position.y <= mRagdollNodes[actuator.b].position.y)
            ? actuator.a
            : actuator.b;
        const std::size_t hip = (foot == actuator.a) ? actuator.b : actuator.a;
        const bool footGrounded = nodeGroundContact(mRagdollNodes[foot]);

        const float mobilityFoot = std::clamp(mRagdollNodes[foot].inverseMass, 0.08f, 1.f);
        const float mobilityHip = std::clamp(mRagdollNodes[hip].inverseMass, 0.08f, 1.f);
        const float sideX = -goalDirZ;
        const float sideZ = goalDirX;
        const float sideSign = (i % 2 == 0) ? 1.f : -1.f;

        // Power stroke: grounded "paddle" drives torso forward with small lateral sweep.
        if (footGrounded && powerPhase > 0.f)
        {
            const float push = powerPhase * (0.35f + 0.65f * contact) * 11.5f;
            motorAcceleration[hip].x += goalDirX * push * mobilityHip;
            motorAcceleration[hip].z += goalDirZ * push * mobilityHip;
            motorAcceleration[hip].x += sideX * sideSign * (powerPhase * 1.1f) * mobilityHip;
            motorAcceleration[hip].z += sideZ * sideSign * (powerPhase * 1.1f) * mobilityHip;
            motorAcceleration[foot].x -= goalDirX * push * 0.58f * mobilityFoot;
            motorAcceleration[foot].z -= goalDirZ * push * 0.58f * mobilityFoot;
            motorAcceleration[foot].x -= sideX * sideSign * (powerPhase * 1.6f) * mobilityFoot;
            motorAcceleration[foot].z -= sideZ * sideSign * (powerPhase * 1.6f) * mobilityFoot;
            motorAcceleration[foot].y -= powerPhase * 1.1f * mobilityFoot;
            motorAcceleration[hip].y += powerPhase * 0.4f * mobilityHip;
        }

        // Recovery stroke: lift and swing limb forward to prepare next push.
        const float recoveryIntent = footGrounded ? recoveryPhase : std::max(0.12f, recoveryPhase);
        if (recoveryIntent > 0.f)
        {
            motorAcceleration[foot].y += recoveryIntent * 8.8f * mobilityFoot;
            motorAcceleration[foot].x += goalDirX * recoveryIntent * 4.6f * mobilityFoot;
            motorAcceleration[foot].z += goalDirZ * recoveryIntent * 4.6f * mobilityFoot;
            motorAcceleration[foot].x += sideX * sideSign * (recoveryIntent * 2.5f) * mobilityFoot;
            motorAcceleration[foot].z += sideZ * sideSign * (recoveryIntent * 2.5f) * mobilityFoot;

            const float dx = mRagdollNodes[hip].position.x - mRagdollNodes[foot].position.x;
            const float dy = mRagdollNodes[hip].position.y - mRagdollNodes[foot].position.y;
            const float dz = mRagdollNodes[hip].position.z - mRagdollNodes[foot].position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > 0.0001f)
            {
                const float invDist = 1.f / dist;
                const float retract = (0.38f + recoveryIntent) * 3.6f;
                const float dirX = dx * invDist;
                const float dirY = dy * invDist;
                const float dirZ = dz * invDist;
                motorAcceleration[foot].x += dirX * retract * mobilityFoot;
                motorAcceleration[foot].y += dirY * retract * mobilityFoot;
                motorAcceleration[foot].z += dirZ * retract * mobilityFoot;
                motorAcceleration[hip].x -= dirX * retract * 0.26f * mobilityHip;
                motorAcceleration[hip].y -= dirY * retract * 0.26f * mobilityHip;
                motorAcceleration[hip].z -= dirZ * retract * 0.26f * mobilityHip;
            }
        }

        // If foot is airborne during power stroke, sweep backward to find next contact.
        if (!footGrounded && powerPhase > 0.f)
        {
            motorAcceleration[foot].x -= goalDirX * powerPhase * 2.7f * mobilityFoot;
            motorAcceleration[foot].z -= goalDirZ * powerPhase * 2.7f * mobilityFoot;
            motorAcceleration[foot].y -= powerPhase * 0.45f * mobilityFoot;
        }
    }

    // Dynamic bend constraints across limb chains:
    // shorten second-neighbor distance and add lateral lift so limbs arc instead of worm-sliding.
    for (auto& constraint : mRagdollConstraints)
    {
        if (constraint.bendDriveA < 0 || constraint.bendDriveB < 0)
            continue;
        if (constraint.bendJoint >= mRagdollNodes.size()
            || constraint.a >= mRagdollNodes.size()
            || constraint.b >= mRagdollNodes.size())
            continue;

        const std::size_t driveA = static_cast<std::size_t>(constraint.bendDriveA);
        const std::size_t driveB = static_cast<std::size_t>(constraint.bendDriveB);
        if (driveA >= mLimbDriveState.size()
            || driveB >= mLimbDriveState.size()
            || driveA >= mLimbPaddlePhase.size()
            || driveB >= mLimbPaddlePhase.size())
            continue;

        const float drive = 0.5f * (std::abs(mLimbDriveState[driveA]) + std::abs(mLimbDriveState[driveB]));
        const float phaseBlend = 0.5f * (std::sin(mLimbPaddlePhase[driveA]) + std::sin(mLimbPaddlePhase[driveB]));
        const float cyclePulse = std::clamp(0.5f + 0.5f * phaseBlend, 0.f, 1.f);

        const std::size_t inputA = Creature::kBaseSensorInputCount + driveA;
        const std::size_t inputB = Creature::kBaseSensorInputCount + driveB;
        const float contactA = (inputA < inputs.size()) ? std::clamp(inputs[inputA], 0.f, 1.f) : 0.f;
        const float contactB = (inputB < inputs.size()) ? std::clamp(inputs[inputB], 0.f, 1.f) : 0.f;
        const float contact = 0.5f * (contactA + contactB);

        const float airborneBias = 1.f - contact;
        const float bendStrength = (0.07f + 0.30f * drive)
            * (0.45f + 0.55f * cyclePulse)
            * (0.5f + 0.5f * airborneBias);
        const float bendScale = std::clamp(1.f - bendStrength, 0.62f, 1.f);
        constraint.restLength = constraint.baseRestLength * bendScale;

        const sf::Vector3f span = sf::Vector3f(
            mRagdollNodes[constraint.b].position.x - mRagdollNodes[constraint.a].position.x,
            mRagdollNodes[constraint.b].position.y - mRagdollNodes[constraint.a].position.y,
            mRagdollNodes[constraint.b].position.z - mRagdollNodes[constraint.a].position.z
        );
        sf::Vector3f side = normalize3(sf::Vector3f(-span.z, 0.f, span.x));
        if (vectorLength(side) < 0.0001f)
            side = sf::Vector3f(1.f, 0.f, 0.f);

        const float swingSign = (std::sin(0.5f * (mLimbPaddlePhase[driveA] + mLimbPaddlePhase[driveB])) >= 0.f)
            ? 1.f
            : -1.f;
        const float lateralPush = bendStrength * (2.1f + 1.6f * cyclePulse) * swingSign;
        const float lift = bendStrength * (4.8f + 2.4f * cyclePulse);

        const float mobilityA = std::clamp(mRagdollNodes[constraint.a].inverseMass, 0.08f, 1.f);
        const float mobilityB = std::clamp(mRagdollNodes[constraint.b].inverseMass, 0.08f, 1.f);
        const float mobilityJoint = std::clamp(mRagdollNodes[constraint.bendJoint].inverseMass, 0.08f, 1.f);

        motorAcceleration[constraint.a].x += side.x * lateralPush * mobilityA;
        motorAcceleration[constraint.a].z += side.z * lateralPush * mobilityA;
        motorAcceleration[constraint.b].x -= side.x * lateralPush * mobilityB;
        motorAcceleration[constraint.b].z -= side.z * lateralPush * mobilityB;

        motorAcceleration[constraint.bendJoint].y += lift * mobilityJoint;
        motorAcceleration[constraint.a].y += lift * 0.25f * mobilityA;
        motorAcceleration[constraint.b].y += lift * 0.25f * mobilityB;
    }
}

void GamingState::simulateRagdoll(sf::Time dt, const NeuralNetwork* network)
{
    if (!mRagdollActive || mRagdollNodes.empty())
        return;

    const float dtSeconds = std::clamp(dt.asSeconds(), 0.f, 0.05f);
    if (dtSeconds <= 0.f)
        return;

    const float dtSquared = dtSeconds * dtSeconds;
    constexpr float gravity = 8.6f;
    constexpr float damping = 0.993f;
    std::vector<sf::Vector3f> motorAcceleration(mRagdollNodes.size(), sf::Vector3f(0.f, 0.f, 0.f));
    applyNeuralMotorControl(dt, motorAcceleration, network);

    for (std::size_t i = 0; i < mRagdollNodes.size(); ++i)
    {
        auto& node = mRagdollNodes[i];
        const float nodeDamping = 1.f - (1.f - damping) * std::clamp(node.inverseMass, 0.08f, 1.f);
        const float vx = (node.position.x - node.previousPosition.x) * nodeDamping;
        const float vy = (node.position.y - node.previousPosition.y) * nodeDamping;
        const float vz = (node.position.z - node.previousPosition.z) * nodeDamping;

        node.previousPosition = node.position;
        node.position.x += vx + motorAcceleration[i].x * dtSquared;
        node.position.y += vy + motorAcceleration[i].y * dtSquared - gravity * dtSquared;
        node.position.z += vz + motorAcceleration[i].z * dtSquared;
    }

    constexpr int solverIterations = 6;
    constexpr float epsilon = 0.0001f;
    for (int iter = 0; iter < solverIterations; ++iter)
    {
        for (const auto& c : mRagdollConstraints)
        {
            auto& a = mRagdollNodes[c.a];
            auto& b = mRagdollNodes[c.b];

            const float dx = b.position.x - a.position.x;
            const float dy = b.position.y - a.position.y;
            const float dz = b.position.z - a.position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float inverseMassSum = a.inverseMass + b.inverseMass;
            if (dist <= epsilon || inverseMassSum <= epsilon)
                continue;

            const float error = (dist - c.restLength) / dist;
            const float correctionScale = error * c.stiffness;
            const float weightA = a.inverseMass / inverseMassSum;
            const float weightB = b.inverseMass / inverseMassSum;
            const float cxA = dx * correctionScale * weightA;
            const float cyA = dy * correctionScale * weightA;
            const float czA = dz * correctionScale * weightA;
            const float cxB = dx * correctionScale * weightB;
            const float cyB = dy * correctionScale * weightB;
            const float czB = dz * correctionScale * weightB;

            a.position.x += cxA;
            a.position.y += cyA;
            a.position.z += czA;
            b.position.x -= cxB;
            b.position.y -= cyB;
            b.position.z -= czB;
        }

        for (auto& node : mRagdollNodes)
            applyGroundAndBounds(node);
    }
}

void GamingState::applyGroundAndBounds(RagdollNode& node) const
{
    const float mapHalf = (mContext.terrain != nullptr)
        ? (mContext.terrain->mapSize() * 0.5f)
        : (Terrain::kDefaultMapSize * 0.5f);
    const float minBound = -mapHalf + 0.02f;
    const float maxBound = mapHalf - 0.02f;
    node.position.x = std::clamp(node.position.x, minBound, maxBound);
    node.position.z = std::clamp(node.position.z, minBound, maxBound);

    const float ground = sampleTerrainHeight(node.position.x, node.position.z) + 0.02f;
    if (node.position.y >= ground)
        return;

    const float vx = node.position.x - node.previousPosition.x;
    const float vy = node.position.y - node.previousPosition.y;
    const float vz = node.position.z - node.previousPosition.z;
    const float clampedInverseMass = std::clamp(node.inverseMass, 0.05f, 1.f);
    const float horizontalKeep = 0.36f + 0.38f * (1.f - clampedInverseMass);
    const float restitution = 0.06f + 0.12f * clampedInverseMass;
    node.position.y = ground;
    const float postImpactVy = (vy < 0.f) ? (-vy * restitution) : 0.f;
    node.previousPosition.y = node.position.y - postImpactVy;
    node.previousPosition.x = node.position.x - vx * horizontalKeep;
    node.previousPosition.z = node.position.z - vz * horizontalKeep;
}

float GamingState::vectorLength(sf::Vector3f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float GamingState::sampleTerrainHeight(float x, float z) const
{
    if (mContext.terrain == nullptr)
        return 1.f;

    const int divisions = (mContext.terrain != nullptr)
        ? mContext.terrain->divisions()
        : Terrain::kDefaultDivisions;
    const float mapSize = (mContext.terrain != nullptr) ? mContext.terrain->mapSize() : Terrain::kDefaultMapSize;
    const float mapMin = -0.5f * mapSize;
    const float u = std::clamp((x - mapMin) / std::max(0.0001f, mapSize), 0.f, 1.f);
    const float v = std::clamp((z - mapMin) / std::max(0.0001f, mapSize), 0.f, 1.f);
    const int ix = static_cast<int>(std::round(u * static_cast<float>(divisions)));
    const int iz = static_cast<int>(std::round(v * static_cast<float>(divisions)));
    return mContext.terrain->heightAt(ix, iz);
}
