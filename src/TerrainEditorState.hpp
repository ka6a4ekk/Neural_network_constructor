#pragma once

#include "State.hpp"
#include "Button.hpp"
#include "Slider.hpp"
#include <SFML/Graphics.hpp>

class TerrainEditorState : public State
{
public:
    TerrainEditorState(StateStack& stack, Context context);

    void handleEvent(const sf::Event& event) override;
    void update(sf::Time dt) override;
    void render() override;

private:
    void setupProjection();
    void drawTerrainCube();
    void generateTerrain();
    void updateHoveredCell();
    bool projectWorldToScreen(float x, float y, float z, sf::Vector2f& outScreen) const;
    void drawTopCellOverlay(int cellX, int cellZ, const sf::Color& color, float lineWidth) const;
    void updateInfoText();

private:
    bool mGlInitialized = false;
    float mYawDegrees = 35.f;
    int mHoveredCellX = -1;
    int mHoveredCellZ = -1;

    sf::Font mFont;
    sf::Text mTitleText;
    sf::Text mInfoText;
    sf::Text mLowNoiseText;
    sf::Text mHighNoiseText;
    sf::Text mMapSizeText;
    sf::Text mAggressivenessText;
    Slider mLowNoiseSlider;
    Slider mHighNoiseSlider;
    Slider mMapSizeSlider;
    Slider mAggressivenessSlider;
    int mRequestedMapSize = 32;
    Button mGenerateButton;
};
