#include "Slider.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

Slider::Slider(std::string id)
    : Widget(std::move(id))
    , mTrack({260.f, 8.f})
    , mFill({0.f, 8.f})
    , mKnob(10.f)
{
    Widget::setSize({260.f, 24.f});

    mTrack.setFillColor(mTrackColor);
    mTrack.setOutlineThickness(1.f);
    mTrack.setOutlineColor(sf::Color(30, 30, 35));

    mFill.setFillColor(mFillColor);

    mKnob.setOrigin({10.f, 10.f});
    mKnob.setFillColor(mKnobColor);
    mKnob.setOutlineThickness(2.f);
    mKnob.setOutlineColor(mKnobNormalOutline);

    updateGeometry();
}

void Slider::handleEvent(const sf::Event& event)
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

            if (mDragging)
                updateValueFromMouseX(point.x, true);
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
            if (!contains(point))
                return;

            mDragging = true;
            updateValueFromMouseX(point.x, true);
            activate();
        }
        return;
    }

    if (event.is<sf::Event::MouseButtonReleased>())
    {
        if (const auto* mouse = event.getIf<sf::Event::MouseButtonReleased>())
        {
            if (mouse->button == sf::Mouse::Button::Left)
                mDragging = false;
        }
    }
}

void Slider::setPosition(sf::Vector2f position)
{
    Widget::setPosition(position);
    updateGeometry();
}

void Slider::setSize(sf::Vector2f size)
{
    const sf::Vector2f clamped{
        std::max(40.f, size.x),
        std::max(18.f, size.y)
    };
    Widget::setSize(clamped);
    updateGeometry();
}

bool Slider::contains(sf::Vector2f point) const
{
    const sf::FloatRect trackBounds = mTrack.getGlobalBounds();
    const sf::FloatRect expandedTrack(
        {trackBounds.position.x, trackBounds.position.y - 8.f},
        {trackBounds.size.x, trackBounds.size.y + 16.f}
    );
    return expandedTrack.contains(point) || mKnob.getGlobalBounds().contains(point);
}

void Slider::onHover(bool hovered)
{
    mKnob.setOutlineColor(hovered ? mKnobHoverOutline : mKnobNormalOutline);
}

void Slider::setRange(float minValue, float maxValue)
{
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    mMinValue = minValue;
    mMaxValue = maxValue;
    setValue(mValue, false);
}

float Slider::getMin() const
{
    return mMinValue;
}

float Slider::getMax() const
{
    return mMaxValue;
}

void Slider::setValue(float value, bool emitCallback)
{
    const float clampedValue = std::clamp(value, mMinValue, mMaxValue);
    if (std::abs(clampedValue - mValue) <= 0.0001f)
    {
        updateGeometry();
        return;
    }

    mValue = clampedValue;
    updateGeometry();
    if (emitCallback && mOnValueChanged)
        mOnValueChanged(mValue);
}

float Slider::getValue() const
{
    return mValue;
}

void Slider::setTrackColor(const sf::Color& color)
{
    mTrackColor = color;
    mTrack.setFillColor(mTrackColor);
}

void Slider::setFillColor(const sf::Color& color)
{
    mFillColor = color;
    mFill.setFillColor(mFillColor);
}

void Slider::setKnobColor(const sf::Color& color)
{
    mKnobColor = color;
    mKnob.setFillColor(mKnobColor);
}

void Slider::setOnValueChanged(ValueChangedCallback callback)
{
    mOnValueChanged = std::move(callback);
}

void Slider::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(mTrack, states);
    target.draw(mFill, states);
    target.draw(mKnob, states);
}

void Slider::updateGeometry()
{
    constexpr float trackHeight = 8.f;
    const sf::Vector2f size = getSize();
    const sf::Vector2f position = getPosition();

    const float trackY = position.y + (size.y - trackHeight) * 0.5f;
    mTrack.setPosition({position.x, trackY});
    mTrack.setSize({size.x, trackHeight});

    const float t = clampedT(mValue);
    const float knobX = position.x + t * size.x;
    const float knobY = position.y + size.y * 0.5f;
    mKnob.setPosition({knobX, knobY});

    mFill.setPosition({position.x, trackY});
    mFill.setSize({t * size.x, trackHeight});
}

void Slider::updateValueFromMouseX(float x, bool emitCallback)
{
    const sf::Vector2f size = getSize();
    const sf::Vector2f position = getPosition();
    const float width = std::max(1.f, size.x);
    const float t = std::clamp((x - position.x) / width, 0.f, 1.f);
    setValue(valueFromT(t), emitCallback);
}

float Slider::clampedT(float value) const
{
    if (std::abs(mMaxValue - mMinValue) <= 0.0001f)
        return 0.f;
    return std::clamp((value - mMinValue) / (mMaxValue - mMinValue), 0.f, 1.f);
}

float Slider::valueFromT(float t) const
{
    return mMinValue + std::clamp(t, 0.f, 1.f) * (mMaxValue - mMinValue);
}
