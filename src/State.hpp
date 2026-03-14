#pragma once

#include <SFML/Graphics.hpp>
#include <memory>

class StateStack;
class Terrain;
class Creature;
class NeuralNetwork;

class State
{
public:
    using Ptr = std::unique_ptr<State>;

    struct Context
    {
        sf::RenderWindow* window;
        Terrain* terrain;
        Creature* creature;
        NeuralNetwork* neuralNetwork;
    };

public:
    State(StateStack& stack, Context context);
    virtual ~State() = default;

    virtual void handleEvent(const sf::Event& event) = 0;
    virtual void update(sf::Time dt) = 0;
    virtual void render() = 0;

protected:
    StateStack& getStateStack();
    void requestPush(std::unique_ptr<State> state);
    void requestPop();
    void requestClear();

protected:
    Context mContext;

private:
    StateStack& mStack;
};
