#pragma once

#include "State.hpp"
#include "Button.hpp"
#include <SFML/Graphics.hpp>
#include <vector>

class MenuState : public State
{
public:
    MenuState(StateStack& stack, Context context);

    void handleEvent(const sf::Event& event) override;
    void update(sf::Time dt) override;
    void render() override;

private:
    void buildButtons();
    void layoutButtons();

private:
    sf::Font mFont;
    sf::RectangleShape mBackdrop;
    sf::Text mTitle;
    std::vector<Button> mButtons;
};
