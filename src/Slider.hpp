#pragma once

#include "Widget.hpp"
#include <functional>

class Slider : public Widget
{
public:
    using ValueChangedCallback = std::function<void(float)>;

public:
    explicit Slider(std::string id = "");

    void handleEvent(const sf::Event& event) override;
    void setPosition(sf::Vector2f position) override;
    void setSize(sf::Vector2f size) override;
    bool contains(sf::Vector2f point) const override;
    void onHover(bool hovered) override;

    void setRange(float minValue, float maxValue);
    float getMin() const;
    float getMax() const;

    void setValue(float value, bool emitCallback = false);
    float getValue() const;

    void setTrackColor(const sf::Color& color);
    void setFillColor(const sf::Color& color);
    void setKnobColor(const sf::Color& color);

    void setOnValueChanged(ValueChangedCallback callback);

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void updateGeometry();
    void updateValueFromMouseX(float x, bool emitCallback);
    float clampedT(float value) const;
    float valueFromT(float t) const;

private:
    sf::RectangleShape mTrack;
    sf::RectangleShape mFill;
    sf::CircleShape mKnob;
    sf::Color mTrackColor{70, 70, 80};
    sf::Color mFillColor{95, 175, 235};
    sf::Color mKnobColor{230, 230, 235};
    sf::Color mKnobHoverOutline{255, 255, 170};
    sf::Color mKnobNormalOutline{35, 35, 40};
    float mMinValue = 0.f;
    float mMaxValue = 1.f;
    float mValue = 0.f;
    bool mDragging = false;
    ValueChangedCallback mOnValueChanged;
};
