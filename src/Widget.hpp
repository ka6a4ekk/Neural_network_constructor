#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

class Widget : public sf::Drawable
{
public:
    using ActivateCallback = std::function<void()>;
    using HoverCallback = std::function<void(bool)>;

public:
    explicit Widget(std::string id = "");
    virtual ~Widget() = default;

    virtual void handleEvent(const sf::Event& event);
    virtual void update(sf::Time) {}

    virtual void setPosition(sf::Vector2f position);
    virtual sf::Vector2f getPosition() const;

    virtual void setSize(sf::Vector2f size);
    virtual sf::Vector2f getSize() const;

    virtual bool contains(sf::Vector2f point) const;

    virtual void activate();
    virtual void onHover(bool hovered);

    void setId(std::string id);
    const std::string& getId() const;

    void setOnActivate(ActivateCallback callback);
    void setOnHover(HoverCallback callback);

    bool isHovered() const;

protected:
    sf::FloatRect getBounds() const;

protected:
    std::string mId;
    sf::Vector2f mPosition;
    sf::Vector2f mSize;
    bool mHovered = false;
    ActivateCallback mOnActivate;
    HoverCallback mOnHover;
};
