#pragma once

#include <SFML/Graphics.hpp>
#include "StateStack.hpp"
#include "Terrain.hpp"
#include "Creature.hpp"
#include "NeuralNetwork.hpp"

class Engine
{
public:
    Engine();
    ~Engine() = default;

    void run();

private:
    void processEvents();
    void update(sf::Time dt);
    void render();

private:
    sf::RenderWindow mWindow;
    Terrain mTerrain;
    Creature mCreature;
    NeuralNetwork mNeuralNetwork;
    StateStack mStateStack;
};
