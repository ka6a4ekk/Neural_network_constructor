#pragma once

#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>
#include "State.hpp"

class StateStack
{
public:
    explicit StateStack(State::Context context);
    ~StateStack();

    void handleEvent(const sf::Event& event);
    void update(sf::Time dt);
    void render();

    void push(std::unique_ptr<State> state);
    void pop();
    void clear();

    bool empty() const;

private:
    std::vector<std::unique_ptr<State>> mStack;
    State::Context mContext;
};
