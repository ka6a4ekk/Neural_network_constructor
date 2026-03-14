#include "MenuState.hpp"
#include "StateStack.hpp"
#include "NNConfigState.hpp"
#include "CreatureEditorState.hpp"
#include "TerrainEditorState.hpp"
#include "GamingState.hpp"
#include <stdexcept>

MenuState::MenuState(StateStack& stack, Context context)
    : State(stack, context)
    , mFont()
    , mBackdrop()
    , mTitle(mFont)
{
    if (!mFont.openFromFile("res/fonts/font.ttf"))
        throw std::runtime_error("Failed to load font");

    const sf::Vector2u size = mContext.window->getSize();
    mBackdrop.setSize({static_cast<float>(size.x), static_cast<float>(size.y)});
    mBackdrop.setFillColor(sf::Color(10, 10, 10));

    mTitle.setString("Neural Network Constructor");
    mTitle.setCharacterSize(52);
    mTitle.setFillColor(sf::Color::White);

    const auto bounds = mTitle.getLocalBounds();
    mTitle.setOrigin({
        bounds.size.x / 2.f,
        bounds.size.y / 2.f
    });

    buildButtons();
    layoutButtons();
}

void MenuState::handleEvent(const sf::Event& event)
{
    for (auto& button : mButtons)
        button.handleEvent(event);

    if (event.is<sf::Event::KeyPressed>())
    {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Num1)
                mButtons[0].activate();

            if (key->code == sf::Keyboard::Key::Num2)
                mButtons[1].activate();

            if (key->code == sf::Keyboard::Key::Num3)
                mButtons[2].activate();

            if (key->code == sf::Keyboard::Key::Num4)
                mButtons[3].activate();

            if (key->code == sf::Keyboard::Key::Escape)
                requestClear();
        }
    }
}

void MenuState::update(sf::Time dt)
{
    for (auto& button : mButtons)
        button.update(dt);
}

void MenuState::render()
{
    mContext.window->draw(mBackdrop);
    mContext.window->draw(mTitle);
    for (const auto& button : mButtons)
        mContext.window->draw(button);
}

void MenuState::buildButtons()
{
    mButtons.clear();
    mButtons.reserve(5);

    mButtons.emplace_back(mFont, "Terrain editor", "terrain_editor");
    mButtons.emplace_back(mFont, "Creature editor", "creature_editor");
    mButtons.emplace_back(mFont, "Configure neural network", "nn_config");
    mButtons.emplace_back(mFont, "Train neural network", "gaming");
    mButtons.emplace_back(mFont, "Exit", "exit");

    mButtons[0].setOnActivate([this]() {
        requestPush(std::make_unique<TerrainEditorState>(getStateStack(), mContext));
    });
    mButtons[1].setOnActivate([this]() {
        requestPush(std::make_unique<CreatureEditorState>(getStateStack(), mContext));
    });
    mButtons[2].setOnActivate([this]() {
        requestPush(std::make_unique<NNConfigState>(getStateStack(), mContext));
    });
    mButtons[3].setOnActivate([this]() {
        requestPush(std::make_unique<GamingState>(getStateStack(), mContext));
    });
    mButtons[4].setOnActivate([this]() {
        requestClear();
    });

    for (auto& button : mButtons)
    {
        button.setCharacterSize(24);
        button.setTextColor(sf::Color::White);
        button.setNormalColor(sf::Color(25, 25, 26));
        button.setHoverColor(sf::Color(54, 54, 54));
        button.setPressedColor(sf::Color(54, 54, 54));
        button.setOutline(0.f, sf::Color(54, 54, 54));
    }

    mButtons.back().setNormalColor(sf::Color(25, 25, 26));
    mButtons.back().setHoverColor(sf::Color(54, 54, 54));
    mButtons.back().setPressedColor(sf::Color(54, 54, 54));
    mButtons.back().setOutline(0.f, sf::Color(54, 54, 54));
}

void MenuState::layoutButtons()
{
    const float width = 460.f;
    const float height = 56.f;
    const float spacing = 18.f;
    const float startY = 220.f;
    const float centerX = static_cast<float>(mContext.window->getSize().x) * 0.5f;
    mTitle.setPosition({centerX, 110.f});

    for (std::size_t i = 0; i < mButtons.size(); ++i)
    {
        mButtons[i].setSize({width, height});
        mButtons[i].setPosition({
            centerX - width / 2.f,
            startY + static_cast<float>(i) * (height + spacing)
        });
    }
}
