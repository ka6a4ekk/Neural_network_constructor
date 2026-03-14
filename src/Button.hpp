#pragma once

#include "Widget.hpp"

class Button : public Widget
{
public:
    Button(const sf::Font& font, const sf::String& text = "", std::string id = "");

    void setString(const sf::String& text);
    void setCharacterSize(unsigned int size);
    void setTextColor(const sf::Color& color);

    void setPosition(sf::Vector2f position) override;
    void setSize(sf::Vector2f size) override;
    bool contains(sf::Vector2f point) const override;

    void activate() override;
    void onHover(bool hovered) override;

    void setNormalColor(const sf::Color& color);
    void setHoverColor(const sf::Color& color);
    void setPressedColor(const sf::Color& color);
    void setOutline(float thickness, const sf::Color& color);

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void centerText();

private:
    sf::RectangleShape mShape;
    sf::Text mText;
    sf::Color mNormalColor{60, 60, 60};
    sf::Color mHoverColor{90, 90, 90};
    sf::Color mPressedColor{120, 120, 120};
};
