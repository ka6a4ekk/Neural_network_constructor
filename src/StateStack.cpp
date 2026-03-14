#include "StateStack.hpp"
#include "State.hpp"

StateStack::StateStack(State::Context context)
    : mContext(context)
{
}

StateStack::~StateStack() = default;


void StateStack::handleEvent(const sf::Event& event)
{
    if (!mStack.empty())
        mStack.back()->handleEvent(event);
}

void StateStack::update(sf::Time dt)
{
    if (!mStack.empty())
        mStack.back()->update(dt);
}

void StateStack::render()
{
    if (!mStack.empty())
        mStack.back()->render();
}

void StateStack::push(std::unique_ptr<State> state)
{
    mStack.push_back(std::move(state));
}

void StateStack::pop()
{
    if (!mStack.empty())
        mStack.pop_back();
}

void StateStack::clear()
{
    mStack.clear();
}

bool StateStack::empty() const
{
    return mStack.empty();
}
