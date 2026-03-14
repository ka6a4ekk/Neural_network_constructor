#pragma once

#include "Widget.hpp"

class Label : public Widget
{
public:
    Label(const sf::Font& font, const sf::String& text = "", std::string id = "");

    void setString(const sf::String& text);
    void setCharacterSize(unsigned int size);
    void setTextColor(const sf::Color& color);

    void setPosition(sf::Vector2f position) override;
    void setSize(sf::Vector2f size) override;

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void syncSizeFromText();

private:
    sf::Text mText;
};
