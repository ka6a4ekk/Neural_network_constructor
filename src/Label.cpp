#include "Label.hpp"
#include <utility>

Label::Label(const sf::Font& font, const sf::String& text, std::string id)
    : Widget(std::move(id))
    , mText(font)
{
    mText.setString(text);
    syncSizeFromText();
}

void Label::setString(const sf::String& text)
{
    mText.setString(text);
    syncSizeFromText();
}

void Label::setCharacterSize(unsigned int size)
{
    mText.setCharacterSize(size);
    syncSizeFromText();
}

void Label::setTextColor(const sf::Color& color)
{
    mText.setFillColor(color);
}

void Label::setPosition(sf::Vector2f position)
{
    Widget::setPosition(position);
    mText.setPosition(position);
}

void Label::setSize(sf::Vector2f size)
{
    Widget::setSize(size);
}

void Label::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(mText, states);
}

void Label::syncSizeFromText()
{
    const auto bounds = mText.getLocalBounds();
    Widget::setSize({bounds.size.x, bounds.size.y});
}
