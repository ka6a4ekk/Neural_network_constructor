#include "Button.hpp"
#include <utility>

Button::Button(const sf::Font& font, const sf::String& text, std::string id)
    : Widget(std::move(id))
    , mShape({260.f, 56.f})
    , mText(font)
{
    Widget::setSize(mShape.getSize());

    mShape.setFillColor(mNormalColor);
    mShape.setOutlineThickness(2.f);
    mShape.setOutlineColor(sf::Color::White);

    mText.setString(text);
    mText.setCharacterSize(24);
    mText.setFillColor(sf::Color::White);
    centerText();
}

void Button::setString(const sf::String& text)
{
    mText.setString(text);
    centerText();
}

void Button::setCharacterSize(unsigned int size)
{
    mText.setCharacterSize(size);
    centerText();
}

void Button::setTextColor(const sf::Color& color)
{
    mText.setFillColor(color);
}

void Button::setPosition(sf::Vector2f position)
{
    Widget::setPosition(position);
    mShape.setPosition(position);
    centerText();
}

void Button::setSize(sf::Vector2f size)
{
    Widget::setSize(size);
    mShape.setSize(size);
    centerText();
}

bool Button::contains(sf::Vector2f point) const
{
    return mShape.getGlobalBounds().contains(point);
}

void Button::activate()
{
    mShape.setFillColor(mPressedColor);
    Widget::activate();
    mShape.setFillColor(isHovered() ? mHoverColor : mNormalColor);
}

void Button::onHover(bool hovered)
{
    mShape.setFillColor(hovered ? mHoverColor : mNormalColor);
}

void Button::setNormalColor(const sf::Color& color)
{
    mNormalColor = color;
    if (!isHovered())
        mShape.setFillColor(mNormalColor);
}

void Button::setHoverColor(const sf::Color& color)
{
    mHoverColor = color;
    if (isHovered())
        mShape.setFillColor(mHoverColor);
}

void Button::setPressedColor(const sf::Color& color)
{
    mPressedColor = color;
}

void Button::setOutline(float thickness, const sf::Color& color)
{
    mShape.setOutlineThickness(thickness);
    mShape.setOutlineColor(color);
}

void Button::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(mShape, states);
    target.draw(mText, states);
}

void Button::centerText()
{
    const auto textBounds = mText.getLocalBounds();
    mText.setOrigin({
        textBounds.position.x + textBounds.size.x / 2.f,
        textBounds.position.y + textBounds.size.y / 2.f
    });

    const sf::Vector2f center = {
        mShape.getPosition().x + mShape.getSize().x / 2.f,
        mShape.getPosition().y + mShape.getSize().y / 2.f
    };
    mText.setPosition(center);
}