#include "Engine.hpp"
#include "MenuState.hpp"

Engine::Engine()
    : mWindow(sf::VideoMode(sf::Vector2u(1280, 720)), "RPG Engine")
    , mTerrain()
    , mCreature()
    , mNeuralNetwork()
    , mStateStack(State::Context{ &mWindow, &mTerrain, &mCreature, &mNeuralNetwork })
{
    mWindow.setFramerateLimit(0);

    // Push first state
    mStateStack.push(std::make_unique<MenuState>(
        mStateStack,
        State::Context{ &mWindow, &mTerrain, &mCreature, &mNeuralNetwork }
    ));
}

void Engine::run()
{
    sf::Clock clock;

    while (mWindow.isOpen() && !mStateStack.empty())
    {
        sf::Time dt = clock.restart();

        processEvents();
        update(dt);
        render();
    }
}

void Engine::processEvents()
{
    while (const std::optional event = mWindow.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
            mWindow.close();

        mStateStack.handleEvent(*event);
    }
}

void Engine::update(sf::Time dt)
{
    mStateStack.update(dt);
}

void Engine::render()
{
    mWindow.clear();
    mWindow.resetGLStates();
    mStateStack.render();
    mWindow.display();
}
