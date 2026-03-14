#include "TerrainEditorState.hpp"
#include "Terrain.hpp"
#include <SFML/OpenGL.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
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

TerrainEditorState::TerrainEditorState(StateStack& stack, Context context)
    : State(stack, context)
    , mFont()
    , mTitleText(mFont)
    , mInfoText(mFont)
    , mLowNoiseText(mFont)
    , mHighNoiseText(mFont)
    , mMapSizeText(mFont)
    , mAggressivenessText(mFont)
    , mLowNoiseSlider("noise_low_slider")
    , mHighNoiseSlider("noise_high_slider")
    , mMapSizeSlider("map_size_slider")
    , mAggressivenessSlider("aggressiveness_slider")
    , mGenerateButton(mFont, "Generate terrain", "generate_terrain")
{
    if (!mFont.openFromFile("res/fonts/font.ttf"))
        throw std::runtime_error("Failed to load font");

    mTitleText.setString("Terrain Generator");
    mTitleText.setCharacterSize(34);
    mTitleText.setFillColor(sf::Color::White);
    mTitleText.setPosition({30.f, 26.f});

    mInfoText.setCharacterSize(18);
    mInfoText.setFillColor(sf::Color::White);
    mInfoText.setPosition({30.f, 74.f});

    const float generateX = static_cast<float>(mContext.window->getSize().x) - 280.f;
    const float sliderX = generateX - 250.f;
    const float sliderWidth = 230.f;
    auto setupLabel = [sliderX](sf::Text& text, float y) {
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);
        text.setPosition({sliderX, y});
    };
    setupLabel(mLowNoiseText, 22.f);
    setupLabel(mHighNoiseText, 72.f);
    setupLabel(mMapSizeText, 122.f);
    setupLabel(mAggressivenessText, 172.f);

    auto setupSliderStyle = [](Slider& slider) {
        slider.setTrackColor(sf::Color(25, 25, 26));
        slider.setFillColor(sf::Color(54, 54, 54));
        slider.setKnobColor(sf::Color::White);
    };

    mLowNoiseSlider.setPosition({sliderX, 42.f});
    mLowNoiseSlider.setSize({sliderWidth, 24.f});
    mLowNoiseSlider.setRange(-0.35f, 0.35f);
    setupSliderStyle(mLowNoiseSlider);
    mLowNoiseSlider.setValue(-0.17f, false);

    mHighNoiseSlider.setPosition({sliderX, 92.f});
    mHighNoiseSlider.setSize({sliderWidth, 24.f});
    mHighNoiseSlider.setRange(-0.35f, 0.35f);
    setupSliderStyle(mHighNoiseSlider);
    mHighNoiseSlider.setValue(0.17f, false);

    mMapSizeSlider.setPosition({sliderX, 142.f});
    mMapSizeSlider.setSize({sliderWidth, 24.f});
    mMapSizeSlider.setRange(
        Terrain::kMinMapSize,
        Terrain::kMaxMapSize
    );
    setupSliderStyle(mMapSizeSlider);
    mMapSizeSlider.setValue(Terrain::kDefaultMapSize, false);

    mAggressivenessSlider.setPosition({sliderX, 192.f});
    mAggressivenessSlider.setSize({sliderWidth, 24.f});
    mAggressivenessSlider.setRange(
        Terrain::kMinAggressiveness,
        Terrain::kMaxAggressiveness
    );
    setupSliderStyle(mAggressivenessSlider);
    mAggressivenessSlider.setValue(Terrain::kDefaultAggressiveness, false);

    if (mContext.terrain != nullptr)
    {
        mLowNoiseSlider.setValue(mContext.terrain->noiseLowLimit(), false);
        mHighNoiseSlider.setValue(mContext.terrain->noiseHighLimit(), false);
        mRequestedMapSize = std::clamp(
            static_cast<int>(std::lround(mContext.terrain->mapSize())),
            static_cast<int>(Terrain::kMinMapSize),
            static_cast<int>(Terrain::kMaxMapSize)
        );
        mContext.terrain->setMapSize(static_cast<float>(mRequestedMapSize));
        mContext.terrain->setDivisions(mRequestedMapSize);
        mMapSizeSlider.setValue(static_cast<float>(mRequestedMapSize), false);
        mAggressivenessSlider.setValue(mContext.terrain->aggressiveness(), false);
    }

    mLowNoiseSlider.setOnValueChanged([this](float low) {
        float high = mHighNoiseSlider.getValue();
        if (low > high)
        {
            high = low;
            mHighNoiseSlider.setValue(high, false);
        }
        if (mContext.terrain != nullptr)
            mContext.terrain->setNoiseRange(low, high);
        updateInfoText();
    });

    mHighNoiseSlider.setOnValueChanged([this](float high) {
        float low = mLowNoiseSlider.getValue();
        if (high < low)
        {
            low = high;
            mLowNoiseSlider.setValue(low, false);
        }
        if (mContext.terrain != nullptr)
            mContext.terrain->setNoiseRange(low, high);
        updateInfoText();
    });

    mMapSizeSlider.setOnValueChanged([this](float sizeValue) {
        const int mapSize = std::clamp(
            static_cast<int>(std::lround(sizeValue)),
            static_cast<int>(Terrain::kMinMapSize),
            static_cast<int>(Terrain::kMaxMapSize)
        );
        mRequestedMapSize = mapSize;
        mMapSizeSlider.setValue(static_cast<float>(mapSize), false);
        if (mContext.terrain != nullptr)
        {
            mContext.terrain->setMapSize(static_cast<float>(mapSize));
            mContext.terrain->setDivisions(mapSize);
        }
        updateInfoText();
    });

    mAggressivenessSlider.setOnValueChanged([this](float value) {
        if (mContext.terrain != nullptr)
            mContext.terrain->setAggressiveness(value);
        updateInfoText();
    });

    mGenerateButton.setSize({260.f, 52.f});
    mGenerateButton.setPosition({
        generateX,
        24.f
    });
    mGenerateButton.setOnActivate([this]() {
        generateTerrain();
    });
    mGenerateButton.setNormalColor(sf::Color(25, 25, 26));
    mGenerateButton.setHoverColor(sf::Color(54, 54, 54));
    mGenerateButton.setPressedColor(sf::Color(54, 54, 54));
    mGenerateButton.setTextColor(sf::Color::White);
    mGenerateButton.setOutline(0.f, sf::Color(54, 54, 54));

    generateTerrain();
    updateInfoText();
}

void TerrainEditorState::handleEvent(const sf::Event& event)
{
    mLowNoiseSlider.handleEvent(event);
    mHighNoiseSlider.handleEvent(event);
    mMapSizeSlider.handleEvent(event);
    mAggressivenessSlider.handleEvent(event);
    mGenerateButton.handleEvent(event);

    if (event.is<sf::Event::KeyPressed>())
    {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                requestPop();
                return;
            }

            if (key->code == sf::Keyboard::Key::R)
            {
                generateTerrain();
                updateInfoText();
                return;
            }

            if (key->code == sf::Keyboard::Key::Q)
            {
                if (mContext.terrain != nullptr && mHoveredCellX >= 0 && mHoveredCellZ >= 0)
                    mContext.terrain->setSpawnCell(mHoveredCellX, mHoveredCellZ);
                updateInfoText();
                return;
            }

            if (key->code == sf::Keyboard::Key::E)
            {
                if (mContext.terrain != nullptr && mHoveredCellX >= 0 && mHoveredCellZ >= 0)
                    mContext.terrain->setDestinationCell(mHoveredCellX, mHoveredCellZ);
                updateInfoText();
                return;
            }
        }
    }
}

void TerrainEditorState::update(sf::Time dt)
{
    constexpr float rotationSpeed = 85.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
        mYawDegrees -= rotationSpeed * dt.asSeconds();
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
        mYawDegrees += rotationSpeed * dt.asSeconds();

    updateHoveredCell();
    updateInfoText();
}

void TerrainEditorState::render()
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
    const float cameraDistance = overviewCameraDistance(mContext.terrain);
    glTranslatef(0.f, -0.9f, -cameraDistance);
    glRotatef(28.f, 1.f, 0.f, 0.f);
    glRotatef(mYawDegrees, 0.f, 1.f, 0.f);
    glScalef(1.7f, 1.7f, 1.7f);

    drawTerrainCube();
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

    mContext.window->resetGLStates();
    const sf::Vector2u size = mContext.window->getSize();
    (void)size;
    mContext.window->draw(mTitleText);
    mContext.window->draw(mInfoText);
    mContext.window->draw(mLowNoiseText);
    mContext.window->draw(mHighNoiseText);
    mContext.window->draw(mMapSizeText);
    mContext.window->draw(mAggressivenessText);
    mContext.window->draw(mLowNoiseSlider);
    mContext.window->draw(mHighNoiseSlider);
    mContext.window->draw(mMapSizeSlider);
    mContext.window->draw(mAggressivenessSlider);
    mContext.window->draw(mGenerateButton);
}

void TerrainEditorState::setupProjection()
{
    const sf::Vector2u windowSize = mContext.window->getSize();
    const float width = static_cast<float>(windowSize.x);
    const float height = static_cast<float>(windowSize.y == 0 ? 1 : windowSize.y);
    const float aspect = width / height;

    glViewport(0, 0, static_cast<GLsizei>(windowSize.x), static_cast<GLsizei>(windowSize.y));

    constexpr float fov = 35.f;
    constexpr float nearPlane = 0.1f;
    const float farPlane = projectionFarPlane(mContext.terrain);
    const float halfHeight = std::tan((fov * 0.5f) * 3.1415926535f / 180.f) * nearPlane;
    const float halfWidth = halfHeight * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

void TerrainEditorState::drawTerrainCube()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.1f);
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

    emitTriangle(mapMin, -1.f, mapMin, mapMax, -1.f, mapMin, mapMax, -1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMin, mapMax, -1.f, mapMax, mapMin, -1.f, mapMax);

    emitTriangle(mapMin, -1.f, mapMax, mapMax, -1.f, mapMax, mapMax, 1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMax, mapMax, 1.f, mapMax, mapMin, 1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMin, mapMax, 1.f, mapMin, mapMax, -1.f, mapMin);
    emitTriangle(mapMin, -1.f, mapMin, mapMin, 1.f, mapMin, mapMax, 1.f, mapMin);
    emitTriangle(mapMin, -1.f, mapMin, mapMin, -1.f, mapMax, mapMin, 1.f, mapMax);
    emitTriangle(mapMin, -1.f, mapMin, mapMin, 1.f, mapMax, mapMin, 1.f, mapMin);
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
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void TerrainEditorState::updateHoveredCell()
{
    mHoveredCellX = -1;
    mHoveredCellZ = -1;

    if (mContext.window == nullptr)
        return;

    const sf::Vector2u windowSize = mContext.window->getSize();
    if (windowSize.x == 0 || windowSize.y == 0)
        return;

    const sf::Vector2i mousePixel = sf::Mouse::getPosition(*mContext.window);
    const float mouseX = static_cast<float>(mousePixel.x);
    const float mouseY = static_cast<float>(mousePixel.y);
    if (mouseX < 0.f || mouseY < 0.f
        || mouseX >= static_cast<float>(windowSize.x)
        || mouseY >= static_cast<float>(windowSize.y))
    {
        return;
    }

    const int divisions = (mContext.terrain != nullptr)
        ? mContext.terrain->divisions()
        : Terrain::kDefaultDivisions;
    const float mapSize = (mContext.terrain != nullptr) ? mContext.terrain->mapSize() : Terrain::kDefaultMapSize;
    const float mapMin = -0.5f * mapSize;
    float bestDist2 = std::numeric_limits<float>::max();
    int bestX = -1;
    int bestZ = -1;

    for (int z = 0; z < divisions; ++z)
    {
        for (int x = 0; x < divisions; ++x)
        {
            const float worldX = mapMin
                + mapSize * (static_cast<float>(x) + 0.5f) / static_cast<float>(divisions);
            const float worldZ = mapMin
                + mapSize * (static_cast<float>(z) + 0.5f) / static_cast<float>(divisions);

            float worldY = 1.f;
            if (mContext.terrain != nullptr)
            {
                const float h00 = mContext.terrain->heightAt(x, z);
                const float h10 = mContext.terrain->heightAt(x + 1, z);
                const float h01 = mContext.terrain->heightAt(x, z + 1);
                const float h11 = mContext.terrain->heightAt(x + 1, z + 1);
                worldY = (h00 + h10 + h01 + h11) * 0.25f;
            }

            sf::Vector2f projected;
            if (!projectWorldToScreen(worldX, worldY, worldZ, projected))
                continue;

            const float dx = projected.x - mouseX;
            const float dy = projected.y - mouseY;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 < bestDist2)
            {
                bestDist2 = dist2;
                bestX = x;
                bestZ = z;
            }
        }
    }

    constexpr float maxPickDistancePx = 26.f;
    if (bestX >= 0 && bestDist2 <= maxPickDistancePx * maxPickDistancePx)
    {
        mHoveredCellX = bestX;
        mHoveredCellZ = bestZ;
    }
}

bool TerrainEditorState::projectWorldToScreen(float x, float y, float z, sf::Vector2f& outScreen) const
{
    if (mContext.window == nullptr)
        return false;

    const sf::Vector2u windowSize = mContext.window->getSize();
    const float width = static_cast<float>(windowSize.x);
    const float height = static_cast<float>(windowSize.y == 0 ? 1u : windowSize.y);
    const float aspect = width / height;

    constexpr float degToRad = 3.1415926535f / 180.f;
    const float yaw = mYawDegrees * degToRad;
    constexpr float pitch = 28.f * degToRad;
    constexpr float scale = 1.7f;

    float px = x * scale;
    float py = y * scale;
    float pz = z * scale;

    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    const float ryX = px * cosYaw + pz * sinYaw;
    const float ryY = py;
    const float ryZ = -px * sinYaw + pz * cosYaw;

    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);
    const float rxX = ryX;
    const float rxY = ryY * cosPitch - ryZ * sinPitch;
    const float rxZ = ryY * sinPitch + ryZ * cosPitch;

    const float cx = rxX;
    const float cy = rxY - 0.9f;
    const float cz = rxZ - overviewCameraDistance(mContext.terrain);

    constexpr float nearPlane = 0.1f;
    if (cz >= -nearPlane)
        return false;

    constexpr float fov = 35.f;
    const float tanHalf = std::tan((fov * 0.5f) * degToRad);
    const float ndcX = cx / (-cz * tanHalf * aspect);
    const float ndcY = cy / (-cz * tanHalf);

    if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
        return false;

    outScreen.x = (ndcX * 0.5f + 0.5f) * width;
    outScreen.y = (1.f - (ndcY * 0.5f + 0.5f)) * height;
    return true;
}

void TerrainEditorState::drawTopCellOverlay(int cellX, int cellZ, const sf::Color& color, float lineWidth) const
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

    // Avoid leaking wireframe state into SFML text rendering.
    glLineWidth(1.f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void TerrainEditorState::updateInfoText()
{
    std::string spawn = "not set";
    std::string destination = "not set";
    if (mContext.terrain != nullptr)
    {
        if (mContext.terrain->hasSpawnCell())
        {
            spawn = "("
                + std::to_string(mContext.terrain->spawnCellX()) + ", "
                + std::to_string(mContext.terrain->spawnCellZ()) + ")";
        }
        if (mContext.terrain->hasDestinationCell())
        {
            destination = "("
                + std::to_string(mContext.terrain->destinationCellX()) + ", "
                + std::to_string(mContext.terrain->destinationCellZ()) + ")";
        }
    }

    const float low = mLowNoiseSlider.getValue();
    const float high = mHighNoiseSlider.getValue();
    std::ostringstream lowText;
    std::ostringstream highText;
    std::ostringstream aggressivenessText;
    lowText << std::fixed << std::setprecision(3) << low;
    highText << std::fixed << std::setprecision(3) << high;
    aggressivenessText << std::fixed << std::setprecision(2) << mAggressivenessSlider.getValue();
    mLowNoiseText.setString("Noise low: " + lowText.str());
    mHighNoiseText.setString("Noise high: " + highText.str());
    mMapSizeText.setString(
        "Map size: " + std::to_string(mRequestedMapSize)
        + " x " + std::to_string(mRequestedMapSize)
    );
    mAggressivenessText.setString("Aggressiveness: " + aggressivenessText.str());

    mInfoText.setString("Spawn: " + spawn + "   Destination: " + destination);
}

void TerrainEditorState::generateTerrain()
{
    if (mContext.terrain != nullptr)
    {
        mContext.terrain->setMapSize(static_cast<float>(mRequestedMapSize));
        mContext.terrain->setDivisions(mRequestedMapSize);
        mContext.terrain->setNoiseRange(mLowNoiseSlider.getValue(), mHighNoiseSlider.getValue());
        mContext.terrain->setAggressiveness(mAggressivenessSlider.getValue());
        mContext.terrain->generate();
    }
}
