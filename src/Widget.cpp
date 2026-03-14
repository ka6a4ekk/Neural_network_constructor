#include "Widget.hpp"
#include <utility>

Widget::Widget(std::string id)
    : mId(std::move(id))
{
}

void Widget::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::MouseMoved>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f point{
                static_cast<float>(mouse->position.x),
                static_cast<float>(mouse->position.y)
            };
            const bool hovered = contains(point);
            if (hovered != mHovered)
            {
                mHovered = hovered;
                onHover(mHovered);
                if (mOnHover)
                    mOnHover(mHovered);
            }
        }
        return;
    }

    if (event.is<sf::Event::MouseButtonPressed>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouse->button != sf::Mouse::Button::Left)
                return;

            const sf::Vector2f point{
                static_cast<float>(mouse->position.x),
                static_cast<float>(mouse->position.y)
            };
            if (contains(point))
                activate();
        }
    }
}

void Widget::setPosition(sf::Vector2f position)
{
    mPosition = position;
}

sf::Vector2f Widget::getPosition() const
{
    return mPosition;
}

void Widget::setSize(sf::Vector2f size)
{
    mSize = size;
}

sf::Vector2f Widget::getSize() const
{
    return mSize;
}

bool Widget::contains(sf::Vector2f point) const
{
    return getBounds().contains(point);
}

void Widget::activate()
{
    if (mOnActivate)
        mOnActivate();
}

void Widget::onHover(bool)
{
}

void Widget::setId(std::string id)
{
    mId = std::move(id);
}

const std::string& Widget::getId() const
{
    return mId;
}

void Widget::setOnActivate(ActivateCallback callback)
{
    mOnActivate = std::move(callback);
}

void Widget::setOnHover(HoverCallback callback)
{
    mOnHover = std::move(callback);
}

bool Widget::isHovered() const
{
    return mHovered;
}

sf::FloatRect Widget::getBounds() const
{
    return sf::FloatRect(mPosition, mSize);
}
